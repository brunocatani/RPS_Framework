#pragma once

#include "RPS/Runtime/RuntimeModule.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace RPS::Runtime::Scene
{
    struct Point3
    {
        float x{};
        float y{};
        float z{};

        [[nodiscard]] bool finite() const noexcept;
    };
    static_assert(sizeof(Point3) == 0x0C);

    struct Matrix3
    {
        float entry[3][3]{
            { 1.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f },
        };

        [[nodiscard]] bool finite() const noexcept;
        [[nodiscard]] bool properRotation(float tolerance = 1.0e-3f) const noexcept;
    };
    static_assert(sizeof(Matrix3) == 0x24);

    struct Transform
    {
        Matrix3 rotate{};
        Point3 translate{};
        float scale{ 1.0f };

        [[nodiscard]] bool finite() const noexcept;
    };
    static_assert(sizeof(Transform) == 0x34);

    enum class TransformStatus : std::uint8_t
    {
        Converted,
        NonFiniteParent,
        DegenerateParentScale,
        InvalidParentRotation,
        NonFiniteInput,
        DegenerateInputScale,
        InvalidInputRotation,
        NonFiniteResult,
        DegenerateResultScale,
        InvalidResultRotation,
    };

    struct TransformResult
    {
        TransformStatus status{ TransformStatus::NonFiniteInput };
        Transform value{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return status == TransformStatus::Converted;
        }
    };

    /** Matches the NiTransform convention shipped by ROCK Emitters. */
    [[nodiscard]] TransformResult worldToParentLocal(
        const Transform& parentWorld,
        const Transform& desiredWorld) noexcept;
    [[nodiscard]] TransformResult parentLocalToWorld(
        const Transform& parentWorld,
        const Transform& local) noexcept;

    enum class ObjectCommandStatus : std::uint8_t
    {
        Completed,
        InvalidRuntime,
        InvalidObject,
        PhysicsStepActive,
        PhysicsStepStateUnavailable,
        VtableUnavailable,
        NativeFault,
    };

    struct ObjectCommandResult
    {
        ObjectCommandStatus status{ ObjectCommandStatus::InvalidRuntime };
        std::uintptr_t objectAddress{};
        std::uintptr_t vtableAddress{};
        std::uintptr_t functionAddress{};
        bool invoked{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return status == ObjectCommandStatus::Completed;
        }
    };

    [[nodiscard]] std::string_view toString(TransformStatus status) noexcept;
    [[nodiscard]] std::string_view toString(ObjectCommandStatus status) noexcept;

    /**
     * Calls the three proven FO4VR NiAVObject virtuals used by ROCK's scene
     * helpers. The object is borrowed for one synchronous call. Commands must
     * run on the owning game/frame thread outside physics; attachment,
     * renderer-proxy, and NiPointer lifetime remain consumer responsibilities.
     */
    class ObjectApi
    {
    public:
        ObjectApi(const RuntimeModule& module, void* object) noexcept : _module(&module), _object(object) {}

        [[nodiscard]] ObjectCommandResult setMaterialNeedsUpdate(bool value) const noexcept;
        [[nodiscard]] ObjectCommandResult setAppCulled(bool value) const noexcept;
        [[nodiscard]] ObjectCommandResult updateWorldBound() const noexcept;

    private:
        [[nodiscard]] bool executionContextValid(ObjectCommandResult& result) const noexcept;
        [[nodiscard]] bool resolveVirtual(
            std::size_t index,
            ObjectCommandResult& result) const noexcept;

        const RuntimeModule* _module{};
        void* _object{};
    };
}
