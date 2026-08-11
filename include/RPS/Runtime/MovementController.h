#pragma once

#include "RPS/Runtime/RuntimeModule.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace RPS::Runtime::Character
{
    inline constexpr std::size_t MovementPathingFlagCount = 8;

    enum class MovementMode : std::uint32_t
    {
        AnimationDriven = 0,
        MotionDrivenControls = 1,
        PathFollowing = 2,
        AnimationDrivenAllowPlannerRotation = 4,
        PathingTargetAngle = 5,
        PlannerDirectControl = 8,
        MotionDrivenControlsAlternate = 15,
    };

    enum class MovementTransition : std::uint8_t
    {
        AnimationDriven,
        PathFollowing,
        AnimationDrivenAllowPlannerRotation,
        PlannerDirectControl,
        ReconcileMotionDrivenControl,
    };

    [[nodiscard]] constexpr bool validMovementTransition(const MovementTransition transition) noexcept
    {
        return transition == MovementTransition::AnimationDriven || transition == MovementTransition::PathFollowing ||
               transition == MovementTransition::AnimationDrivenAllowPlannerRotation ||
               transition == MovementTransition::PlannerDirectControl ||
               transition == MovementTransition::ReconcileMotionDrivenControl;
    }

    [[nodiscard]] bool transitionHasFixedMode(MovementTransition transition) noexcept;
    [[nodiscard]] MovementMode transitionMode(MovementTransition transition) noexcept;

    struct MovementControllerSnapshot
    {
        bool actorReadable{};
        bool controllerResolved{};
        bool modeReadable{};
        bool pathingFlagsReadable{};
        std::uintptr_t actorAddress{};
        std::uintptr_t controllerAddress{};
        std::uintptr_t motionDrivenInterfaceAddress{};
        std::uintptr_t plannerDirectInterfaceAddress{};
        MovementMode mode{};
        std::array<std::uint8_t, MovementPathingFlagCount> pathingFlags{};

        [[nodiscard]] bool complete() const noexcept
        {
            return actorReadable && controllerResolved && modeReadable && pathingFlagsReadable;
        }
    };

    /** Returns a copied view; no engine pointer from the snapshot may be dereferenced by a consumer. */
    [[nodiscard]] MovementControllerSnapshot snapshotMovementController(void* actor) noexcept;

    enum class MovementCommandStatus : std::uint8_t
    {
        Applied,
        InvalidRuntime,
        InvalidActor,
        InvalidArgument,
        ControllerUnavailable,
        ModeUnavailable,
        WrongMode,
        PhysicsStepActive,
        PhysicsStepStateUnavailable,
        NativeUnavailable,
        NativeFault,
        VerificationFailed,
    };

    struct MovementCommandResult
    {
        MovementCommandStatus status{ MovementCommandStatus::InvalidRuntime };
        MovementControllerSnapshot before{};
        MovementControllerSnapshot after{};
        MovementTransition transition{ MovementTransition::AnimationDriven };
        float requestedYawRadians{};
        bool invoked{};

        [[nodiscard]] bool applied() const noexcept { return status == MovementCommandStatus::Applied; }
        [[nodiscard]] explicit operator bool() const noexcept { return applied(); }
    };

    [[nodiscard]] std::string_view toString(MovementCommandStatus status) noexcept;

    /**
     * Checked access to MovementControllerNPC. The actor and its current
     * controller are borrowed only for each synchronous call and re-resolved
     * afterward. Commands require the owning game/frame thread outside the
     * physics step. Modes are changed only through native transitions; the
     * framework never writes Controller_Mode directly.
     */
    class MovementControllerApi
    {
    public:
        MovementControllerApi(const RuntimeModule& module, void* actor) noexcept : _module(&module), _actor(actor) {}

        [[nodiscard]] MovementControllerSnapshot inspect() const noexcept;
        [[nodiscard]] MovementCommandResult transition(MovementTransition transition) const noexcept;
        [[nodiscard]] MovementCommandResult setPlannerTargetYaw(float yawRadians) const noexcept;
        [[nodiscard]] MovementCommandResult clearPlannerTargetYaw() const noexcept;

    private:
        [[nodiscard]] MovementCommandStatus preflight(MovementControllerSnapshot& snapshot) const noexcept;

        const RuntimeModule* _module{};
        void* _actor{};
    };
}
