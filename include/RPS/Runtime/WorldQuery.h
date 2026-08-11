#pragma once

#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/PhysicsTypes.h"
#include "RPS/Runtime/RuntimeModule.h"
#include "RPS/Runtime/WorldAccess.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace RPS::Runtime::Physics
{
    enum class QueryError : std::uint8_t
    {
        None,
        InvalidRuntime,
        InvalidWorld,
        InvalidReadGuard,
        InvalidInput,
        OutputUnavailable,
        ScaleUnavailable,
        QueryFilterUnavailable,
        FunctionUnavailable,
        NativeCallFailed,
        NativeResultInvalid,
    };

    struct RayRequest
    {
        Vector4 startGame{};
        Vector4 directionGame{};
        float maxDistanceGame{};
        std::uint32_t collisionFilterInfo{};
    };

    struct QueryHit
    {
        Vector4 positionGame{};
        Vector4 normalGame{};
        float fraction{ 1.0f };
        float distanceGame{};
        BodyId body{};
        std::uint16_t materialId{ Addresses::Layouts::Query::AnyMaterialId };
        std::uint16_t reserved{};
        std::uint32_t shapeKey{};
        std::uint32_t collisionFilterInfo{};
        std::uintptr_t userData{};
        bool normalValid{};
    };

    struct ClosestRayResult
    {
        QueryError error{ QueryError::InvalidInput };
        bool hit{};
        QueryHit closest{};

        [[nodiscard]] explicit operator bool() const noexcept { return error == QueryError::None; }
    };

    struct ShapeCastRequest
    {
        Vector4 startGame{};
        Vector4 directionGame{};
        const void* shape{};
        float distanceGame{};
        std::uint32_t collisionFilterInfo{};
    };

    struct ShapeCastResult
    {
        QueryError error{ QueryError::InvalidInput };
        std::size_t hitCount{};
        std::size_t droppedHitCount{};
        std::size_t invalidHitCount{};

        [[nodiscard]] explicit operator bool() const noexcept { return error == QueryError::None; }
    };

    struct alignas(16) ShapeCastQuery
    {
        void* filter{};
        std::uint16_t materialId{ Addresses::Layouts::Query::AnyMaterialId };
        std::uint16_t reserved0A{};
        std::uint32_t collisionFilterInfo{};
        std::uint64_t reserved10{};
        std::uint8_t reserved18{};
        std::array<std::byte, 7> reserved19{};
        const void* shape{};
        std::uint64_t reserved28{};
        Vector4 start{};
        Vector4 displacement{};
        Vector4 inverseDisplacementAndSign{};
        float tolerance{ Addresses::Layouts::Query::ShapeCastTolerance };
        std::array<std::byte, 28> reserved64{};
    };
    static_assert(sizeof(ShapeCastQuery) == Addresses::Layouts::Query::ShapeCastQuerySize);
    static_assert(offsetof(ShapeCastQuery, filter) == Addresses::Layouts::Query::ShapeCast_Filter);
    static_assert(offsetof(ShapeCastQuery, collisionFilterInfo) == Addresses::Layouts::Query::ShapeCast_CollisionFilterInfo);
    static_assert(offsetof(ShapeCastQuery, shape) == Addresses::Layouts::Query::ShapeCast_Shape);
    static_assert(offsetof(ShapeCastQuery, start) == Addresses::Layouts::Query::ShapeCast_Start);
    static_assert(offsetof(ShapeCastQuery, displacement) == Addresses::Layouts::Query::ShapeCast_Displacement);
    static_assert(offsetof(ShapeCastQuery, inverseDisplacementAndSign) == Addresses::Layouts::Query::ShapeCast_InverseDisplacementAndSign);
    static_assert(offsetof(ShapeCastQuery, tolerance) == Addresses::Layouts::Query::ShapeCast_Tolerance);

    [[nodiscard]] bool normalizeQueryDirection(const Vector4& direction, Vector4& normalized) noexcept;
    [[nodiscard]] ShapeCastQuery buildShapeCastQuery(
        void* filter,
        const void* shape,
        std::uint32_t collisionFilterInfo,
        const Vector4& startHavok,
        const Vector4& displacementHavok) noexcept;
    void sortQueryHitsByFraction(std::span<QueryHit> hits) noexcept;

    class WorldQueryApi
    {
    public:
        WorldQueryApi(RuntimeModule module, void* bhkWorld, void* hknpWorld) noexcept :
            _module(module), _bhkWorld(bhkWorld), _hknpWorld(hknpWorld)
        {}

        [[nodiscard]] ClosestRayResult castClosestRayGame(const RayRequest& request) const noexcept;
        [[nodiscard]] ShapeCastResult castShapeGame(
            const WorldReadGuard& guard,
            const ShapeCastRequest& request,
            std::span<QueryHit> output) const noexcept;

    private:
        RuntimeModule _module{};
        void* _bhkWorld{};
        void* _hknpWorld{};
    };
}
