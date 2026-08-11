#pragma once

#include "RPS/Runtime/RuntimeModule.h"

#include <cstdint>
#include <string_view>

namespace RPS::Runtime::Physics
{
    enum class MotionPreset : std::uint32_t
    {
        Static = 0,
        Dynamic = 1,
        Keyframed = 2,
    };

    [[nodiscard]] constexpr bool validMotionPreset(const MotionPreset preset) noexcept
    {
        return preset == MotionPreset::Static || preset == MotionPreset::Dynamic || preset == MotionPreset::Keyframed;
    }

    struct RecursiveMotionRequest
    {
        MotionPreset preset{ MotionPreset::Dynamic };
        bool recursive{ true };
        bool force{};
        bool activate{};
    };

    struct RecursiveCollisionRequest
    {
        bool enable{ true };
        bool recursive{ true };
        bool force{};
    };

    enum class RecursiveCommandStatus : std::uint8_t
    {
        Accepted,
        InvalidRuntime,
        InvalidSceneRoot,
        InvalidMotionPreset,
        PhysicsStepActive,
        PhysicsStepStateUnavailable,
        NativeUnavailable,
        NativeRejected,
        NativeFault,
    };

    struct RecursiveCommandResult
    {
        RecursiveCommandStatus status{ RecursiveCommandStatus::InvalidRuntime };
        bool invoked{};

        [[nodiscard]] bool accepted() const noexcept { return status == RecursiveCommandStatus::Accepted; }
        [[nodiscard]] explicit operator bool() const noexcept { return accepted(); }
    };

    [[nodiscard]] std::string_view toString(RecursiveCommandStatus status) noexcept;

    /**
     * Calls Fallout 4 VR's NiAVObject recursive physics commands. These are
     * the correct boundary for multi-child props; a collision-object motion
     * call only updates one child. The scene root is borrowed for the call.
     * Invoke from the owning game/frame thread, outside the physics step.
     */
    [[nodiscard]] RecursiveCommandResult setMotionRecursive(
        const RuntimeModule& module,
        void* sceneRoot,
        RecursiveMotionRequest request) noexcept;

    [[nodiscard]] RecursiveCommandResult enableCollisionRecursive(
        const RuntimeModule& module,
        void* sceneRoot,
        RecursiveCollisionRequest request) noexcept;
}
