#include "RPS/Runtime/Shape.h"

#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/HavokAllocator.h"
#include "RPS/Runtime/Memory.h"
#include "RPS/Runtime/NativeReference.h"
#include "RPS/Runtime/PhysicsScale.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <new>

namespace RPS::Runtime::Physics
{
    namespace
    {
        namespace Layout = Addresses::Layouts::Shape;

        struct StridedPointArray
        {
            const void* data{};
            std::int32_t count{};
            std::int32_t stride{};
        };

        struct alignas(16) ShapeInstance
        {
            std::array<std::byte, Layout::InstanceSize> bytes{};
        };
        static_assert(sizeof(ShapeInstance) == Layout::InstanceSize);

        struct CompoundCinfo
        {
            ShapeInstance* instances{};
            std::int32_t count{};
            std::int32_t capacityAndFlags{};
            std::uint8_t flags{};
            std::array<std::byte, 7> reserved{};
            void* massConfig{};
            void* outputIds{};
        };
        static_assert(sizeof(CompoundCinfo) == Layout::CompoundCinfoSize);

        using InitConvexConfigFunction = void* (*)(void*);
        using BuildConvexFunction = void* (*)(const StridedPointArray*, float, void*);
        using BuildSphereFunction = void* (*)(Vector4*, float);
        using ConstructCinfoFunction = CompoundCinfo* (*)(CompoundCinfo*, ShapeInstance*, std::int32_t, void*);
        using ConstructStaticFunction = void* (*)(void*, CompoundCinfo*, std::uint64_t, void*);
        using ConstructDynamicFunction = void* (*)(void*, CompoundCinfo*);
        using SetShapeFunction = void (*)(ShapeInstance*, const void*);
        using SetTransformFunction = void (*)(ShapeInstance*, const ChildTransform*);
        using SetScaleFunction = void (*)(ShapeInstance*, const Vector4*, int);
        using UpdateInstancesFunction = void (*)(void*, const std::int16_t*, std::int32_t, const ShapeInstance*);

        struct CompoundApi
        {
            ConstructCinfoFunction constructCinfo{};
            ConstructStaticFunction constructStatic{};
            ConstructDynamicFunction constructDynamic{};
            SetShapeFunction setShape{};
            SetTransformFunction setTransform{};
            SetScaleFunction setScale{};
            UpdateInstancesFunction updateInstances{};

            [[nodiscard]] bool staticReady() const noexcept
            {
                return constructCinfo && constructStatic && setShape && setTransform && setScale;
            }
            [[nodiscard]] bool dynamicReady() const noexcept
            {
                return constructCinfo && constructDynamic && setShape && setTransform && setScale && updateInstances;
            }
        };

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

        template <std::size_t Size>
        [[nodiscard]] bool matches(
            const RuntimeModule& module,
            const Addresses::Symbol symbol,
            const std::array<std::byte, Size>& bytes,
            const std::array<std::byte, Size>& mask = {}) noexcept
        {
            const bool defaultMask = std::all_of(mask.begin(), mask.end(), [](const std::byte value) {
                return value == std::byte{};
            });
            return module.matches(symbol, RPS::Runtime::BytePattern{ bytes, defaultMask ? std::span<const std::byte>{} : mask });
        }

        [[nodiscard]] bool compoundEntryPatternsMatch(const RuntimeModule& module) noexcept
        {
            const std::array setShape{
                std::byte{ 0x48 }, std::byte{ 0x89 }, std::byte{ 0x5C }, std::byte{ 0x24 }, std::byte{ 0x08 },
                std::byte{ 0x57 }, std::byte{ 0x48 }, std::byte{ 0x83 }, std::byte{ 0xEC }, std::byte{ 0x20 },
            };
            const std::array setTransform{
                std::byte{ 0x48 }, std::byte{ 0x89 }, std::byte{ 0x5C }, std::byte{ 0x24 }, std::byte{ 0x08 },
                std::byte{ 0x48 }, std::byte{ 0x89 }, std::byte{ 0x6C }, std::byte{ 0x24 }, std::byte{ 0x10 },
                std::byte{ 0x48 }, std::byte{ 0x89 }, std::byte{ 0x74 }, std::byte{ 0x24 }, std::byte{ 0x18 },
            };
            const std::array setScale{
                std::byte{ 0x0F }, std::byte{ 0x28 }, std::byte{ 0x12 }, std::byte{ 0x0F }, std::byte{ 0x28 },
                std::byte{ 0x1D }, std::byte{}, std::byte{}, std::byte{}, std::byte{}, std::byte{ 0x44 },
                std::byte{ 0x8B }, std::byte{ 0x49 }, std::byte{ 0x0C },
            };
            auto setScaleMask = std::array<std::byte, 14>{};
            setScaleMask.fill(std::byte{ 0xFF });
            std::fill(setScaleMask.begin() + 6, setScaleMask.begin() + 10, std::byte{});
            const std::array cinfo{
                std::byte{ 0x33 }, std::byte{ 0xC0 }, std::byte{ 0x48 }, std::byte{ 0x89 }, std::byte{ 0x11 },
                std::byte{ 0x44 }, std::byte{ 0x89 }, std::byte{ 0x41 }, std::byte{ 0x08 }, std::byte{ 0x89 },
                std::byte{ 0x41 }, std::byte{ 0x0C },
            };
            return matches(module, Addresses::Symbol::Shape_InstanceSetShape, setShape) &&
                   matches(module, Addresses::Symbol::Shape_InstanceSetTransform, setTransform) &&
                   matches(module, Addresses::Symbol::Shape_InstanceSetScale, setScale, setScaleMask) &&
                   matches(module, Addresses::Symbol::Shape_CompoundCinfoFromInstances, cinfo);
        }

        [[nodiscard]] bool dynamicEntryPatternsMatch(const RuntimeModule& module) noexcept
        {
            const std::array constructor{
                std::byte{ 0x48 }, std::byte{ 0x89 }, std::byte{ 0x5C }, std::byte{ 0x24 }, std::byte{ 0x10 },
                std::byte{ 0x48 }, std::byte{ 0x89 }, std::byte{ 0x74 }, std::byte{ 0x24 }, std::byte{ 0x18 },
                std::byte{ 0x57 }, std::byte{ 0x48 }, std::byte{ 0x83 }, std::byte{ 0xEC }, std::byte{ 0x60 },
            };
            const std::array update{
                std::byte{ 0x48 }, std::byte{ 0x89 }, std::byte{ 0x5C }, std::byte{ 0x24 }, std::byte{ 0x18 },
                std::byte{ 0x55 }, std::byte{ 0x41 }, std::byte{ 0x54 }, std::byte{ 0x41 }, std::byte{ 0x55 },
                std::byte{ 0x41 }, std::byte{ 0x56 }, std::byte{ 0x41 }, std::byte{ 0x57 }, std::byte{ 0x48 },
                std::byte{ 0x83 }, std::byte{ 0xEC },
            };
            return matches(module, Addresses::Symbol::Shape_DynamicCompoundCtor, constructor) &&
                   matches(module, Addresses::Symbol::Shape_DynamicCompoundUpdateInstances, update);
        }

        [[nodiscard]] CompoundApi resolveCompoundApi(const RuntimeModule& module, const bool dynamic) noexcept
        {
            if (!module || !compoundEntryPatternsMatch(module) || (dynamic && !dynamicEntryPatternsMatch(module))) {
                return {};
            }
            return {
                .constructCinfo = module.resolveFunction<ConstructCinfoFunction>(Addresses::Symbol::Shape_CompoundCinfoFromInstances),
                .constructStatic = module.resolveFunction<ConstructStaticFunction>(Addresses::Symbol::Shape_StaticCompoundCtor),
                .constructDynamic = module.resolveFunction<ConstructDynamicFunction>(Addresses::Symbol::Shape_DynamicCompoundCtor),
                .setShape = module.resolveFunction<SetShapeFunction>(Addresses::Symbol::Shape_InstanceSetShape),
                .setTransform = module.resolveFunction<SetTransformFunction>(Addresses::Symbol::Shape_InstanceSetTransform),
                .setScale = module.resolveFunction<SetScaleFunction>(Addresses::Symbol::Shape_InstanceSetScale),
                .updateInstances = module.resolveFunction<UpdateInstancesFunction>(Addresses::Symbol::Shape_DynamicCompoundUpdateInstances),
            };
        }

        void initializeInstance(ShapeInstance& instance) noexcept
        {
            instance.bytes.fill(std::byte{});
            const std::uint32_t flags = Addresses::Layouts::Shape::InstanceDefaultFlags;
            const std::uint32_t invalidIndex = (std::numeric_limits<std::uint32_t>::max)();
            std::memcpy(instance.bytes.data() + Layout::Instance_Flags, &flags, sizeof(flags));
            std::memcpy(instance.bytes.data() + Layout::Instance_Index, &invalidIndex, sizeof(invalidIndex));
        }

        [[nodiscard]] const void* instanceShape(const ShapeInstance& instance) noexcept
        {
            const void* shape{};
            std::memcpy(&shape, instance.bytes.data() + Layout::Instance_Shape, sizeof(shape));
            return shape;
        }

        void releaseTemporaryShapeReferences(const std::span<ShapeInstance> instances) noexcept
        {
            for (auto& instance : instances) {
                (void)releaseHavokReference(const_cast<void*>(instanceShape(instance)));
            }
        }

        [[nodiscard]] bool childrenValid(const std::span<const CompoundChild> children) noexcept
        {
            if (children.empty() || children.size() > Layout::MaximumCompoundChildren ||
                children.size() > static_cast<std::size_t>((std::numeric_limits<std::int32_t>::max)())) {
                return false;
            }
            return std::all_of(children.begin(), children.end(), [](const CompoundChild& child) {
                return child.shape && child.transform.finite();
            });
        }

        [[nodiscard]] bool vectorsNear(const Vector4& left, const Vector4& right) noexcept
        {
            constexpr float epsilon = 0.00001f;
            return std::fabs(left.x - right.x) <= epsilon && std::fabs(left.y - right.y) <= epsilon &&
                   std::fabs(left.z - right.z) <= epsilon && std::fabs(left.w - right.w) <= epsilon;
        }

        [[nodiscard]] bool transformsNear(const ChildTransform& left, const ChildTransform& right) noexcept
        {
            return vectorsNear(left.transform.column0, right.transform.column0) &&
                   vectorsNear(left.transform.column1, right.transform.column1) &&
                   vectorsNear(left.transform.column2, right.transform.column2) &&
                   vectorsNear(left.transform.translation, right.transform.translation) && vectorsNear(left.scale, right.scale) &&
                   left.scaleMode == right.scaleMode;
        }

        [[nodiscard]] bool configureInstance(
            ShapeInstance& instance,
            const CompoundChild& child,
            const CompoundApi& api) noexcept
        {
            initializeInstance(instance);
            return invokeVoid(api.setTransform, &instance, &child.transform) &&
                   invokeVoid(api.setScale, &instance, &child.transform.scale, child.transform.scaleMode) &&
                   invokeVoid(api.setShape, &instance, child.shape);
        }
    }

    ShapeHandle::~ShapeHandle() noexcept
    {
        reset();
    }

    ShapeHandle::ShapeHandle(ShapeHandle&& other) noexcept : _shape(other.release()) {}

    ShapeHandle& ShapeHandle::operator=(ShapeHandle&& other) noexcept
    {
        if (this != std::addressof(other)) {
            reset();
            _shape = other.release();
        }
        return *this;
    }

    ShapeHandle ShapeHandle::adopt(void* const shape) noexcept
    {
        return ShapeHandle{ shape };
    }

    void* ShapeHandle::release() noexcept
    {
        void* const result = _shape;
        _shape = nullptr;
        return result;
    }

    void ShapeHandle::reset() noexcept
    {
        if (_shape) {
            (void)releaseHavokReference(_shape);
            _shape = nullptr;
        }
    }

    ShapeHandle ShapeFactory::buildConvex(
        const std::span<const Vector4> localHavokPoints,
        const float convexRadius) const noexcept
    {
        if (!_module || localHavokPoints.size() < 4 || localHavokPoints.size() > Layout::MaximumConvexPointCount ||
            localHavokPoints.size() > static_cast<std::size_t>((std::numeric_limits<std::int32_t>::max)()) ||
            !std::isfinite(convexRadius) || !std::all_of(localHavokPoints.begin(), localHavokPoints.end(), [](const Vector4& point) {
                return point.finite();
            })) {
            return {};
        }

        const auto initialize = _module.resolveFunction<InitConvexConfigFunction>(Addresses::Symbol::Shape_ConvexBuildConfigInit);
        const auto build = _module.resolveFunction<BuildConvexFunction>(Addresses::Symbol::Shape_ConvexFromPoints);
        alignas(16) std::array<std::byte, Layout::ConvexBuildConfigSize> config{};
        if (!invokeVoid(initialize, config.data())) {
            return {};
        }

        const StridedPointArray points{
            localHavokPoints.data(),
            static_cast<std::int32_t>(localHavokPoints.size()),
            static_cast<std::int32_t>(sizeof(Vector4)),
        };
        void* shape{};
        if (!invokeResult(shape, build, &points, (std::max)(0.0f, convexRadius), config.data())) {
            return {};
        }
        return ShapeHandle::adopt(shape);
    }

    ShapeHandle ShapeFactory::buildSphereHavok(const float radiusHavok) const noexcept
    {
        if (!_module || !std::isfinite(radiusHavok) || radiusHavok <= 0.000001f) {
            return {};
        }

        const auto build = _module.resolveFunction<BuildSphereFunction>(Addresses::Symbol::Shape_CreateSphere);
        Vector4 center{};
        void* shape{};
        if (!invokeResult(shape, build, &center, radiusHavok) || !shape) {
            return {};
        }
        return ShapeHandle::adopt(shape);
    }

    ShapeHandle ShapeFactory::buildSphereGame(const float radiusGame) const noexcept
    {
        if (!std::isfinite(radiusGame) || radiusGame <= 0.000001f) {
            return {};
        }
        const auto scale = readScaleSnapshot(_module);
        if (!scale.runtimeBacked) {
            return {};
        }
        return buildSphereHavok(radiusGame * scale.gameToHavok);
    }

    ShapeHandle ShapeFactory::buildStaticCompound(const std::span<const CompoundChild> children) const noexcept
    {
        if (!childrenValid(children)) {
            return {};
        }
        const auto api = resolveCompoundApi(_module, false);
        if (!api.staticReady()) {
            return {};
        }

        std::vector<ShapeInstance> instances;
        try {
            instances.resize(children.size());
        } catch (...) {
            return {};
        }

        for (std::size_t index = 0; index < children.size(); ++index) {
            if (!configureInstance(instances[index], children[index], api)) {
                releaseTemporaryShapeReferences(instances);
                return {};
            }
        }

        CompoundCinfo cinfo{};
        CompoundCinfo* cinfoResult{};
        const auto childCount = static_cast<std::int32_t>(instances.size());
        if (!invokeResult(cinfoResult, api.constructCinfo, &cinfo, instances.data(), childCount, nullptr) ||
            cinfoResult != &cinfo) {
            releaseTemporaryShapeReferences(instances);
            return {};
        }

        HavokAllocator allocator{ _module };
        void* const storage = allocator.allocate(Layout::CompoundStorageSize);
        if (!storage) {
            releaseTemporaryShapeReferences(instances);
            return {};
        }
        std::memset(storage, 0, Layout::CompoundStorageSize);

        void* compound{};
        const bool invoked = invokeResult(
            compound, api.constructStatic, storage, &cinfo, static_cast<std::uint64_t>(childCount), nullptr);
        releaseTemporaryShapeReferences(instances);
        if (!invoked || !compound) {
            (void)allocator.deallocate(storage, Layout::CompoundStorageSize);
            return {};
        }
        return ShapeHandle::adopt(compound);
    }

    bool DynamicCompoundShape::create(const std::span<const CompoundChild> children) noexcept
    {
        reset();
        if (!childrenValid(children)) {
            return false;
        }
        const auto api = resolveCompoundApi(_module, true);
        if (!api.dynamicReady()) {
            return false;
        }

        try {
            _instances.resize(children.size());
            _instanceIds.assign(children.size(), (std::numeric_limits<std::int16_t>::max)());
            _lastTransforms.resize(children.size());
            _updateInstances.reserve(children.size());
            _updateIds.reserve(children.size());
        } catch (...) {
            reset();
            return false;
        }

        auto* const instances = reinterpret_cast<ShapeInstance*>(_instances.data());
        for (std::size_t index = 0; index < children.size(); ++index) {
            if (!configureInstance(instances[index], children[index], api)) {
                releaseTemporaryShapeReferences({ instances, children.size() });
                reset();
                return false;
            }
            _lastTransforms[index] = children[index].transform;
        }

        CompoundCinfo cinfo{};
        CompoundCinfo* cinfoResult{};
        const auto childCount = static_cast<std::int32_t>(children.size());
        if (!invokeResult(cinfoResult, api.constructCinfo, &cinfo, instances, childCount, nullptr) || cinfoResult != &cinfo) {
            releaseTemporaryShapeReferences({ instances, children.size() });
            reset();
            return false;
        }
        cinfo.outputIds = _instanceIds.data();

        HavokAllocator allocator{ _module };
        void* const storage = allocator.allocate(Layout::CompoundStorageSize);
        if (!storage) {
            releaseTemporaryShapeReferences({ instances, children.size() });
            reset();
            return false;
        }
        std::memset(storage, 0, Layout::CompoundStorageSize);

        void* compound{};
        const bool invoked = invokeResult(compound, api.constructDynamic, storage, &cinfo);
        releaseTemporaryShapeReferences({ instances, children.size() });
        if (!invoked || !compound) {
            (void)allocator.deallocate(storage, Layout::CompoundStorageSize);
            reset();
            return false;
        }
        _shape = ShapeHandle::adopt(compound);

        for (std::size_t index = 0; index < _instanceIds.size(); ++index) {
            if (_instanceIds[index] < 0 || _instanceIds[index] == (std::numeric_limits<std::int16_t>::max)() ||
                std::find(_instanceIds.begin(), _instanceIds.begin() + static_cast<std::ptrdiff_t>(index), _instanceIds[index]) !=
                    _instanceIds.begin() + static_cast<std::ptrdiff_t>(index)) {
                reset();
                return false;
            }
        }

        _setTransform = reinterpret_cast<SetTransformFunction>(api.setTransform);
        _setScale = reinterpret_cast<SetScaleFunction>(api.setScale);
        _updateInstancesFunction = reinterpret_cast<UpdateInstancesFunction>(api.updateInstances);
        return true;
    }

    DynamicCompoundUpdateResult DynamicCompoundShape::updateTransforms(
        const WorldWriteGuard& guard,
        void* const hknpWorld,
        const std::span<const ChildTransform> transforms) noexcept
    {
        DynamicCompoundUpdateResult result{};
        if (!_shape || !guard.owns(hknpWorld) || transforms.size() != _instances.size() ||
            _instanceIds.size() != _instances.size() || !_setTransform || !_setScale || !_updateInstancesFunction ||
            std::any_of(transforms.begin(), transforms.end(), [](const ChildTransform& transform) {
                return !transform.finite();
            })) {
            return result;
        }

        _updateInstances.clear();
        _updateIds.clear();
        auto* const instances = reinterpret_cast<ShapeInstance*>(_instances.data());
        for (std::size_t index = 0; index < transforms.size(); ++index) {
            if (transformsNear(_lastTransforms[index], transforms[index])) {
                continue;
            }
            if (!invokeVoid(_setTransform, &instances[index], &transforms[index]) ||
                !invokeVoid(_setScale, &instances[index], &transforms[index].scale, transforms[index].scaleMode)) {
                return result;
            }
            _updateInstances.push_back(_instances[index]);
            _updateIds.push_back(_instanceIds[index]);
        }

        result.changedChildCount = _updateIds.size();
        if (!_updateIds.empty() && !invokeVoid(
                _updateInstancesFunction,
                _shape.get(),
                _updateIds.data(),
                static_cast<std::int32_t>(_updateIds.size()),
                _updateInstances.data())) {
            return result;
        }
        for (std::size_t index = 0; index < transforms.size(); ++index) {
            if (!transformsNear(_lastTransforms[index], transforms[index])) {
                _lastTransforms[index] = transforms[index];
            }
        }
        result.succeeded = true;
        return result;
    }

    void DynamicCompoundShape::reset() noexcept
    {
        _updateIds.clear();
        _updateInstances.clear();
        _lastTransforms.clear();
        _instanceIds.clear();
        _instances.clear();
        _shape.reset();
        _setTransform = nullptr;
        _setScale = nullptr;
        _updateInstancesFunction = nullptr;
    }
}
