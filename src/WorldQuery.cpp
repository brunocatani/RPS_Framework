#include "RPS/Runtime/WorldQuery.h"

#include "RPS/Runtime/Memory.h"
#include "RPS/Runtime/PhysicsScale.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>

namespace RPS::Runtime::Physics
{
    namespace
    {
        namespace Layout = Addresses::Layouts::Query;

        struct Point3
        {
            float x{};
            float y{};
            float z{};
        };

        struct alignas(16) NativePickData
        {
            std::array<std::byte, Layout::PickDataSize> bytes{};
        };
        static_assert(sizeof(NativePickData) == Layout::PickDataSize);

        struct alignas(16) NativeCollisionResult
        {
            std::array<std::byte, Layout::CollisionResultSize> bytes{};
        };
        static_assert(sizeof(NativeCollisionResult) == Layout::CollisionResultSize);

        class alignas(16) NativeCollectorBase
        {
        public:
            virtual ~NativeCollectorBase() noexcept = default;
            virtual void reset() noexcept = 0;
            virtual void addHit(const NativeCollisionResult& hit) noexcept = 0;
            [[nodiscard]] virtual bool hasHit() const noexcept = 0;
            [[nodiscard]] virtual std::int32_t getNumHits() const noexcept = 0;
            [[nodiscard]] virtual const NativeCollisionResult* getHits() const noexcept = 0;

        protected:
            void initializeBase() noexcept
            {
                const std::int32_t hints{};
                const auto maximum = (std::numeric_limits<float>::max)();
                const std::array threshold{ maximum, maximum, maximum, maximum };
                auto* const base = reinterpret_cast<std::byte*>(this);
                std::memcpy(
                    base + Addresses::Layouts::Query::Collector_Hints,
                    &hints,
                    sizeof(hints));
                std::memcpy(
                    base + Addresses::Layouts::Query::Collector_EarlyOutThreshold,
                    threshold.data(),
                    sizeof(threshold));
            }

        private:
            std::array<std::byte, Addresses::Layouts::Query::CollectorBaseSize - sizeof(void*)> _nativeState{};
        };
        static_assert(sizeof(NativeCollectorBase) == Addresses::Layouts::Query::CollectorBaseSize);

        class FixedHitCollector final : public NativeCollectorBase
        {
        public:
            explicit FixedHitCollector(const std::span<NativeCollisionResult> storage) noexcept :
                _storage(storage.data()), _capacity(storage.size())
            {
                reset();
            }

            void reset() noexcept override
            {
                initializeBase();
                _size = 0;
                _dropped = 0;
            }

            void addHit(const NativeCollisionResult& hit) noexcept override
            {
                if (_size < _capacity) {
                    _storage[_size++] = hit;
                } else {
                    ++_dropped;
                }
            }

            [[nodiscard]] bool hasHit() const noexcept override { return _size != 0; }
            [[nodiscard]] std::int32_t getNumHits() const noexcept override
            {
                return static_cast<std::int32_t>(_size);
            }
            [[nodiscard]] const NativeCollisionResult* getHits() const noexcept override { return _storage; }
            [[nodiscard]] std::size_t size() const noexcept { return _size; }
            [[nodiscard]] std::size_t dropped() const noexcept { return _dropped; }

        private:
            NativeCollisionResult* _storage{};
            std::size_t _capacity{};
            std::size_t _size{};
            std::size_t _dropped{};
        };

        using PickDataCtorFunction = void* (*)(void*);
        using PickDataSetStartEndFunction = void (*)(void*, const Point3*, const Point3*);
        using PickObjectFunction = bool (*)(void*, void*);
        using PickDataHasHitFunction = bool (*)(void*);
        using PickDataGetHitFractionFunction = float (*)(void*);
        using CastShapeFunction = void (*)(void*, void*, void*, NativeCollectorBase*, NativeCollectorBase*);

        template <class Function, class... Arguments>
        [[nodiscard]] bool invokeVoid(const Function function, Arguments... arguments) noexcept
        {
            if (!function || !Memory::rangeHasAccess(
                    reinterpret_cast<const void*>(function), 1, Memory::Access::Execute)) {
                return false;
            }
#if defined(_MSC_VER)
            __try {
                function(arguments...);
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
#else
            function(arguments...);
            return true;
#endif
        }

        template <class Result, class Function, class... Arguments>
        [[nodiscard]] bool invokeResult(Result& result, const Function function, Arguments... arguments) noexcept
        {
            result = {};
            if (!function || !Memory::rangeHasAccess(
                    reinterpret_cast<const void*>(function), 1, Memory::Access::Execute)) {
                return false;
            }
#if defined(_MSC_VER)
            __try {
                result = function(arguments...);
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                result = {};
                return false;
            }
#else
            result = function(arguments...);
            return true;
#endif
        }

        template <class T>
        [[nodiscard]] T readField(const NativeCollisionResult& result, const std::ptrdiff_t offset) noexcept
        {
            T value{};
            std::memcpy(&value, result.bytes.data() + offset, sizeof(value));
            return value;
        }

        [[nodiscard]] bool finitePoint(const Vector4& value) noexcept
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
        }

        [[nodiscard]] bool convertHit(
            const NativeCollisionResult& native,
            const ScaleSnapshot& scale,
            const float distanceGame,
            QueryHit& output) noexcept
        {
            const auto positionHavok = readField<Vector4>(native, Layout::CollisionResult_Position);
            const auto normal = readField<Vector4>(native, Layout::CollisionResult_Normal);
            const auto fraction = readField<float>(native, Layout::CollisionResult_Fraction);
            if (!finitePoint(positionHavok) || !finitePoint(normal) || !std::isfinite(fraction)) {
                return false;
            }

            output = {};
            output.positionGame = scale.toGamePoint(positionHavok);
            output.positionGame.w = 1.0f;
            output.fraction = std::clamp(fraction, 0.0f, 1.0f);
            output.distanceGame = output.fraction * distanceGame;

            const float normalLengthSquared = normal.x * normal.x + normal.y * normal.y + normal.z * normal.z;
            if (std::isfinite(normalLengthSquared) && normalLengthSquared > 0.00000001f) {
                const float inverseLength = 1.0f / std::sqrt(normalLengthSquared);
                output.normalGame = {
                    normal.x * inverseLength,
                    normal.y * inverseLength,
                    normal.z * inverseLength,
                    0.0f,
                };
                output.normalValid = true;
            }

            const auto hitInfo = Layout::CollisionResult_HitBodyInfo;
            output.body.value = readField<std::uint32_t>(native, hitInfo + Layout::BodyInfo_BodyId);
            output.materialId = readField<std::uint16_t>(native, hitInfo + Layout::BodyInfo_MaterialId);
            output.shapeKey = readField<std::uint32_t>(native, hitInfo + Layout::BodyInfo_ShapeKey);
            output.collisionFilterInfo = readField<std::uint32_t>(
                native, hitInfo + Layout::BodyInfo_CollisionFilterInfo);
            output.userData = readField<std::uintptr_t>(native, hitInfo + Layout::BodyInfo_UserData);
            return output.positionGame.finite();
        }

        [[nodiscard]] void* queryFilter(void* const hknpWorld) noexcept
        {
            if (!hknpWorld) {
                return nullptr;
            }

            std::uintptr_t modifierManager{};
            if (!Memory::read(
                    reinterpret_cast<const std::byte*>(hknpWorld) +
                        Addresses::Layouts::Collision::HknpWorld_ModifierManager,
                    modifierManager) ||
                modifierManager == 0) {
                return nullptr;
            }

            void* filter{};
            if (!Memory::read(
                    reinterpret_cast<const std::byte*>(modifierManager) +
                        Addresses::Layouts::Collision::ModifierManager_Filter,
                    filter) ||
                !Memory::rangeHasAccess(filter, 1, Memory::Access::Read)) {
                return nullptr;
            }
            return filter;
        }

        [[nodiscard]] std::uint32_t positiveSignMask(const Vector4& displacement) noexcept
        {
            std::uint32_t result{};
            if (displacement.x >= 0.0f) {
                result |= 1u << 0;
            }
            if (displacement.y >= 0.0f) {
                result |= 1u << 1;
            }
            if (displacement.z >= 0.0f) {
                result |= 1u << 2;
            }
            return result;
        }

        [[nodiscard]] float inverseOrMaximum(const float value) noexcept
        {
            return value != 0.0f ? 1.0f / value : (std::numeric_limits<float>::max)();
        }
    }

    bool normalizeQueryDirection(const Vector4& direction, Vector4& normalized) noexcept
    {
        normalized = {};
        if (!finitePoint(direction)) {
            return false;
        }
        const float lengthSquared = direction.x * direction.x + direction.y * direction.y + direction.z * direction.z;
        if (!std::isfinite(lengthSquared) || lengthSquared <= 0.00000001f) {
            return false;
        }
        const float inverseLength = 1.0f / std::sqrt(lengthSquared);
        normalized = {
            direction.x * inverseLength,
            direction.y * inverseLength,
            direction.z * inverseLength,
            0.0f,
        };
        return normalized.finite();
    }

    ShapeCastQuery buildShapeCastQuery(
        void* const filter,
        const void* const shape,
        const std::uint32_t collisionFilterInfo,
        const Vector4& startHavok,
        const Vector4& displacementHavok) noexcept
    {
        ShapeCastQuery query{};
        query.filter = filter;
        query.collisionFilterInfo = collisionFilterInfo;
        query.shape = shape;
        query.start = startHavok;
        query.displacement = displacementHavok;
        query.displacement.w = 1.0f;
        query.inverseDisplacementAndSign = {
            inverseOrMaximum(query.displacement.x),
            inverseOrMaximum(query.displacement.y),
            inverseOrMaximum(query.displacement.z),
            std::bit_cast<float>(Layout::PositiveSignMaskBase | positiveSignMask(query.displacement)),
        };
        return query;
    }

    void sortQueryHitsByFraction(const std::span<QueryHit> hits) noexcept
    {
        std::sort(hits.begin(), hits.end(), [](const QueryHit& left, const QueryHit& right) {
            const auto leftFraction = std::isfinite(left.fraction) ?
                                          left.fraction :
                                          (std::numeric_limits<float>::max)();
            const auto rightFraction = std::isfinite(right.fraction) ?
                                           right.fraction :
                                           (std::numeric_limits<float>::max)();
            return leftFraction < rightFraction;
        });
    }

    ClosestRayResult WorldQueryApi::castClosestRayGame(const RayRequest& request) const noexcept
    {
        ClosestRayResult result{};
        if (!_module) {
            result.error = QueryError::InvalidRuntime;
            return result;
        }
        if (!_bhkWorld || !Memory::rangeHasAccess(_bhkWorld, sizeof(void*), Memory::Access::Read)) {
            result.error = QueryError::InvalidWorld;
            return result;
        }

        Vector4 direction{};
        if (!request.startGame.finite() || !normalizeQueryDirection(request.directionGame, direction) ||
            !std::isfinite(request.maxDistanceGame) || request.maxDistanceGame <= 0.001f) {
            result.error = QueryError::InvalidInput;
            return result;
        }
        const Point3 start{ request.startGame.x, request.startGame.y, request.startGame.z };
        const Point3 end{
            start.x + direction.x * request.maxDistanceGame,
            start.y + direction.y * request.maxDistanceGame,
            start.z + direction.z * request.maxDistanceGame,
        };
        if (!std::isfinite(end.x) || !std::isfinite(end.y) || !std::isfinite(end.z)) {
            result.error = QueryError::InvalidInput;
            return result;
        }

        const auto constructor = _module.resolveFunction<PickDataCtorFunction>(Addresses::Symbol::Query_PickDataCtor);
        const auto setStartEnd = _module.resolveFunction<PickDataSetStartEndFunction>(Addresses::Symbol::Query_PickDataSetStartEnd);
        const auto pick = _module.resolveFunction<PickObjectFunction>(Addresses::Symbol::World_PickObject);
        const auto hasHit = _module.resolveFunction<PickDataHasHitFunction>(Addresses::Symbol::Query_PickDataHasHit);
        const auto getFraction = _module.resolveFunction<PickDataGetHitFractionFunction>(Addresses::Symbol::Query_PickDataGetHitFraction);
        if (!constructor || !setStartEnd || !pick || !hasHit || !getFraction) {
            result.error = QueryError::FunctionUnavailable;
            return result;
        }

        NativePickData pickData{};
        void* constructed{};
        if (!invokeResult(constructed, constructor, &pickData) || constructed != &pickData ||
            !invokeVoid(setStartEnd, &pickData, &start, &end)) {
            result.error = QueryError::NativeCallFailed;
            return result;
        }
        std::memcpy(
            pickData.bytes.data() + Layout::PickData_CollisionFilterInfo,
            &request.collisionFilterInfo,
            sizeof(request.collisionFilterInfo));

        bool pickReportedHit{};
        if (!invokeResult(pickReportedHit, pick, _bhkWorld, &pickData)) {
            result.error = QueryError::NativeCallFailed;
            return result;
        }
        if (!pickReportedHit) {
            result.error = QueryError::None;
            return result;
        }

        bool dataHasHit{};
        if (!invokeResult(dataHasHit, hasHit, &pickData)) {
            result.error = QueryError::NativeCallFailed;
            return result;
        }
        if (!dataHasHit) {
            result.error = QueryError::None;
            return result;
        }

        float fraction{};
        if (!invokeResult(fraction, getFraction, &pickData) || !std::isfinite(fraction)) {
            result.error = QueryError::NativeResultInvalid;
            return result;
        }

        NativeCollisionResult native{};
        std::memcpy(
            native.bytes.data(),
            pickData.bytes.data() + Layout::PickData_Result,
            native.bytes.size());
        const ScaleSnapshot identityScale{ 1.0f, 1.0f, 0.0f, 0.0f, true };
        if (!convertHit(native, identityScale, request.maxDistanceGame, result.closest)) {
            result.error = QueryError::NativeResultInvalid;
            return result;
        }

        result.closest.fraction = std::clamp(fraction, 0.0f, 1.0f);
        result.closest.distanceGame = result.closest.fraction * request.maxDistanceGame;
        result.closest.positionGame = {
            start.x + direction.x * result.closest.distanceGame,
            start.y + direction.y * result.closest.distanceGame,
            start.z + direction.z * result.closest.distanceGame,
            1.0f,
        };
        result.error = QueryError::None;
        result.hit = true;
        return result;
    }

    ShapeCastResult WorldQueryApi::castShapeGame(
        const WorldReadGuard& guard,
        const ShapeCastRequest& request,
        const std::span<QueryHit> output) const noexcept
    {
        ShapeCastResult result{};
        if (!_module) {
            result.error = QueryError::InvalidRuntime;
            return result;
        }
        if (!_hknpWorld || !Memory::rangeHasAccess(_hknpWorld, sizeof(void*), Memory::Access::Read)) {
            result.error = QueryError::InvalidWorld;
            return result;
        }
        if (!guard.owns(_hknpWorld)) {
            result.error = QueryError::InvalidReadGuard;
            return result;
        }
        if (output.empty()) {
            result.error = QueryError::OutputUnavailable;
            return result;
        }

        Vector4 direction{};
        if (!request.shape || !Memory::rangeHasAccess(request.shape, sizeof(void*), Memory::Access::Read) ||
            !request.startGame.finite() || !normalizeQueryDirection(request.directionGame, direction) ||
            !std::isfinite(request.distanceGame) || request.distanceGame <= 0.001f) {
            result.error = QueryError::InvalidInput;
            return result;
        }

        const auto scale = readScaleSnapshot(_module);
        if (!scale.runtimeBacked) {
            result.error = QueryError::ScaleUnavailable;
            return result;
        }
        auto* const filter = queryFilter(_hknpWorld);
        if (!filter) {
            result.error = QueryError::QueryFilterUnavailable;
            return result;
        }
        const auto cast = _module.resolveFunction<CastShapeFunction>(Addresses::Symbol::World_CastShape);
        if (!cast) {
            result.error = QueryError::FunctionUnavailable;
            return result;
        }

        auto startHavok = scale.toHavokPoint(request.startGame);
        startHavok.w = request.startGame.w;
        const Vector4 displacementHavok{
            direction.x * request.distanceGame * scale.gameToHavok,
            direction.y * request.distanceGame * scale.gameToHavok,
            direction.z * request.distanceGame * scale.gameToHavok,
            1.0f,
        };
        auto query = buildShapeCastQuery(
            filter,
            request.shape,
            request.collisionFilterInfo,
            startHavok,
            displacementHavok);
        Transform identity{};

        std::array<NativeCollisionResult, Layout::MaximumCollectedHits> nativeHits{};
        const auto collectorCapacity = (std::min)(output.size(), nativeHits.size());
        FixedHitCollector collector{ std::span{ nativeHits }.first(collectorCapacity) };
        if (!invokeVoid(cast, _hknpWorld, &query, &identity, &collector, &collector)) {
            result.error = QueryError::NativeCallFailed;
            return result;
        }

        result.droppedHitCount = collector.dropped();
        for (std::size_t index = 0; index < collector.size(); ++index) {
            QueryHit hit{};
            if (!convertHit(nativeHits[index], scale, request.distanceGame, hit)) {
                ++result.invalidHitCount;
                continue;
            }
            output[result.hitCount++] = hit;
        }
        result.error = QueryError::None;
        return result;
    }
}
