#pragma once

#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/PhysicsTypes.h"
#include "RPS/Runtime/RuntimeModule.h"
#include "RPS/Runtime/WorldAccess.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace RPS::Runtime::Physics
{
    class ShapeHandle
    {
    public:
        ShapeHandle() = default;
        ~ShapeHandle() noexcept;

        ShapeHandle(const ShapeHandle&) = delete;
        ShapeHandle& operator=(const ShapeHandle&) = delete;
        ShapeHandle(ShapeHandle&& other) noexcept;
        ShapeHandle& operator=(ShapeHandle&& other) noexcept;

        [[nodiscard]] static ShapeHandle adopt(void* shape) noexcept;
        [[nodiscard]] void* get() const noexcept { return _shape; }
        [[nodiscard]] explicit operator bool() const noexcept { return _shape != nullptr; }
        [[nodiscard]] void* release() noexcept;
        void reset() noexcept;

    private:
        explicit ShapeHandle(void* shape) noexcept : _shape(shape) {}

        void* _shape{};
    };

    struct alignas(16) ChildTransform
    {
        Transform transform{};
        Vector4 scale{ 1.0f, 1.0f, 1.0f, 1.0f };
        int scaleMode{};
        std::array<std::byte, 12> reserved{};

        [[nodiscard]] bool finite() const noexcept { return transform.finite() && scale.finite(); }
    };
    static_assert(sizeof(ChildTransform) == Addresses::Layouts::Shape::ChildTransformSize);

    struct CompoundChild
    {
        const void* shape{};
        ChildTransform transform{};
    };

    class ShapeFactory
    {
    public:
        explicit ShapeFactory(RuntimeModule module) noexcept : _module(module) {}

        [[nodiscard]] ShapeHandle buildConvex(std::span<const Vector4> localHavokPoints, float convexRadius) const noexcept;
        [[nodiscard]] ShapeHandle buildStaticCompound(std::span<const CompoundChild> children) const noexcept;

    private:
        RuntimeModule _module{};
    };

    struct DynamicCompoundUpdateResult
    {
        bool succeeded{};
        std::size_t changedChildCount{};
    };

    class DynamicCompoundShape
    {
    public:
        explicit DynamicCompoundShape(RuntimeModule module) noexcept : _module(module) {}
        ~DynamicCompoundShape() = default;

        DynamicCompoundShape(const DynamicCompoundShape&) = delete;
        DynamicCompoundShape& operator=(const DynamicCompoundShape&) = delete;
        DynamicCompoundShape(DynamicCompoundShape&&) noexcept = default;
        DynamicCompoundShape& operator=(DynamicCompoundShape&&) noexcept = default;

        [[nodiscard]] bool create(std::span<const CompoundChild> children) noexcept;
        [[nodiscard]] DynamicCompoundUpdateResult updateTransforms(
            const WorldWriteGuard& guard,
            void* hknpWorld,
            std::span<const ChildTransform> transforms) noexcept;
        void reset() noexcept;

        [[nodiscard]] void* get() const noexcept { return _shape.get(); }
        [[nodiscard]] std::size_t childCount() const noexcept { return _instances.size(); }
        [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(_shape); }

    private:
        struct alignas(16) ShapeInstanceStorage
        {
            std::array<std::byte, Addresses::Layouts::Shape::InstanceSize> bytes{};
        };

        using SetTransformFunction = void (*)(void*, const ChildTransform*);
        using SetScaleFunction = void (*)(void*, const Vector4*, int);
        using UpdateInstancesFunction = void (*)(void*, const std::int16_t*, std::int32_t, const void*);

        RuntimeModule _module{};
        ShapeHandle _shape{};
        std::vector<ShapeInstanceStorage> _instances{};
        std::vector<std::int16_t> _instanceIds{};
        std::vector<ChildTransform> _lastTransforms{};
        std::vector<ShapeInstanceStorage> _updateInstances{};
        std::vector<std::int16_t> _updateIds{};
        SetTransformFunction _setTransform{};
        SetScaleFunction _setScale{};
        UpdateInstancesFunction _updateInstancesFunction{};
    };
}
