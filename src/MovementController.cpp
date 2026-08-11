#include "RPS/Runtime/MovementController.h"

#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/Memory.h"
#include "RPS/Runtime/WorldAccess.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <cmath>
#include <limits>

namespace RPS::Runtime::Character
{
    namespace
    {
        using MovementControllerModeFunction = void (*)(void*);
        using MovementPlannerSetTargetAngleFunction = void (*)(void*, void*);

        struct NativePoint3
        {
            float x{};
            float y{};
            float z{};
        };
        static_assert(sizeof(NativePoint3) == 0x0C);
        static_assert(MovementPathingFlagCount == Addresses::Layouts::Movement::Controller_PathingFlagCount);

        [[nodiscard]] bool addOffset(
            const std::uintptr_t base,
            const std::ptrdiff_t offset,
            std::uintptr_t& result) noexcept
        {
            if (base == 0 || offset < 0 ||
                static_cast<std::uintptr_t>(offset) > (std::numeric_limits<std::uintptr_t>::max)() - base) {
                return false;
            }
            result = base + static_cast<std::uintptr_t>(offset);
            return true;
        }

        [[nodiscard]] bool invokeVoid(const MovementControllerModeFunction function, void* const argument) noexcept
        {
            if (!function || !argument) {
                return false;
            }
#if defined(_MSC_VER)
            __try {
                function(argument);
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
#else
            function(argument);
            return true;
#endif
        }

        [[nodiscard]] bool invokeTargetAngle(
            const MovementPlannerSetTargetAngleFunction function,
            void* const plannerInterface,
            NativePoint3& targetAngle) noexcept
        {
            if (!function || !plannerInterface) {
                return false;
            }
#if defined(_MSC_VER)
            __try {
                function(plannerInterface, &targetAngle);
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
#else
            function(plannerInterface, &targetAngle);
            return true;
#endif
        }

        [[nodiscard]] Addresses::Symbol transitionSymbol(const MovementTransition transition) noexcept
        {
            switch (transition) {
            case MovementTransition::AnimationDriven:
                return Addresses::Symbol::Character_SetAnimationDriven;
            case MovementTransition::PathFollowing:
                return Addresses::Symbol::Character_RequestPathFollowing;
            case MovementTransition::AnimationDrivenAllowPlannerRotation:
                return Addresses::Symbol::Character_SetAnimationDrivenAllowPlannerRotation;
            case MovementTransition::PlannerDirectControl:
                return Addresses::Symbol::Character_SetPlannerDirectControl;
            case MovementTransition::ReconcileMotionDrivenControl:
            default:
                return Addresses::Symbol::Character_MotionDrivenControlUpdate;
            }
        }

        [[nodiscard]] void* transitionArgument(
            const MovementControllerSnapshot& snapshot,
            const MovementTransition transition) noexcept
        {
            if (transition == MovementTransition::ReconcileMotionDrivenControl) {
                return reinterpret_cast<void*>(snapshot.motionDrivenInterfaceAddress);
            }
            return reinterpret_cast<void*>(snapshot.controllerAddress);
        }

        [[nodiscard]] bool sameController(
            const MovementControllerSnapshot& before,
            const MovementControllerSnapshot& after) noexcept
        {
            return after.controllerResolved && before.controllerAddress != 0 &&
                   after.controllerAddress == before.controllerAddress;
        }
    }

    bool transitionHasFixedMode(const MovementTransition transition) noexcept
    {
        return transition != MovementTransition::ReconcileMotionDrivenControl;
    }

    MovementMode transitionMode(const MovementTransition transition) noexcept
    {
        switch (transition) {
        case MovementTransition::AnimationDriven:
            return MovementMode::AnimationDriven;
        case MovementTransition::PathFollowing:
            return MovementMode::PathFollowing;
        case MovementTransition::AnimationDrivenAllowPlannerRotation:
            return MovementMode::AnimationDrivenAllowPlannerRotation;
        case MovementTransition::PlannerDirectControl:
            return MovementMode::PlannerDirectControl;
        case MovementTransition::ReconcileMotionDrivenControl:
        default:
            return MovementMode::MotionDrivenControls;
        }
    }

    MovementControllerSnapshot snapshotMovementController(void* const actor) noexcept
    {
        namespace Layout = Addresses::Layouts::Movement;

        MovementControllerSnapshot result{};
        result.actorAddress = reinterpret_cast<std::uintptr_t>(actor);
        std::uintptr_t controllerPointerAddress{};
        std::uintptr_t controller{};
        if (!addOffset(result.actorAddress, Layout::Actor_ControllerSmartPointer, controllerPointerAddress) ||
            !Memory::read(reinterpret_cast<const void*>(controllerPointerAddress), controller)) {
            return result;
        }
        result.actorReadable = true;
        if (controller == 0 ||
            !Memory::rangeHasAccess(
                reinterpret_cast<const void*>(controller),
                Layout::Controller_MinimumReadableSize,
                Memory::Access::Read)) {
            return result;
        }

        result.controllerResolved = true;
        result.controllerAddress = controller;
        result.motionDrivenInterfaceAddress = controller + Layout::Controller_MotionDrivenInterface;
        result.plannerDirectInterfaceAddress = controller + Layout::Controller_PlannerDirectInterface;

        std::uint32_t rawMode{};
        result.modeReadable = Memory::read(
            reinterpret_cast<const void*>(controller + Layout::Controller_Mode), rawMode);
        result.mode = static_cast<MovementMode>(rawMode);
        result.pathingFlagsReadable = Memory::copyFrom(
            reinterpret_cast<const void*>(controller + Layout::Controller_PathingFlags),
            result.pathingFlags.data(),
            result.pathingFlags.size());
        return result;
    }

    std::string_view toString(const MovementCommandStatus status) noexcept
    {
        switch (status) {
        case MovementCommandStatus::Applied:
            return "applied";
        case MovementCommandStatus::InvalidRuntime:
            return "invalid-runtime";
        case MovementCommandStatus::InvalidActor:
            return "invalid-actor";
        case MovementCommandStatus::InvalidArgument:
            return "invalid-argument";
        case MovementCommandStatus::ControllerUnavailable:
            return "controller-unavailable";
        case MovementCommandStatus::ModeUnavailable:
            return "mode-unavailable";
        case MovementCommandStatus::WrongMode:
            return "wrong-mode";
        case MovementCommandStatus::PhysicsStepActive:
            return "physics-step-active";
        case MovementCommandStatus::PhysicsStepStateUnavailable:
            return "physics-step-state-unavailable";
        case MovementCommandStatus::NativeUnavailable:
            return "native-unavailable";
        case MovementCommandStatus::NativeFault:
            return "native-fault";
        case MovementCommandStatus::VerificationFailed:
            return "verification-failed";
        default:
            return "unknown";
        }
    }

    MovementControllerSnapshot MovementControllerApi::inspect() const noexcept
    {
        return snapshotMovementController(_actor);
    }

    MovementCommandStatus MovementControllerApi::preflight(MovementControllerSnapshot& snapshot) const noexcept
    {
        if (!_module || !*_module) {
            return MovementCommandStatus::InvalidRuntime;
        }
        if (!_actor) {
            return MovementCommandStatus::InvalidActor;
        }
        switch (Physics::currentThreadPhysicsStepState(*_module)) {
        case Physics::PhysicsStepState::Inside:
            return MovementCommandStatus::PhysicsStepActive;
        case Physics::PhysicsStepState::Unknown:
            return MovementCommandStatus::PhysicsStepStateUnavailable;
        case Physics::PhysicsStepState::Outside:
            break;
        }

        snapshot = inspect();
        if (!snapshot.controllerResolved) {
            return MovementCommandStatus::ControllerUnavailable;
        }
        return snapshot.modeReadable ? MovementCommandStatus::Applied : MovementCommandStatus::ModeUnavailable;
    }

    MovementCommandResult MovementControllerApi::transition(const MovementTransition requestedTransition) const noexcept
    {
        MovementCommandResult result{};
        result.transition = requestedTransition;
        if (!validMovementTransition(requestedTransition)) {
            result.status = MovementCommandStatus::InvalidArgument;
            return result;
        }
        result.status = preflight(result.before);
        if (result.status != MovementCommandStatus::Applied) {
            return result;
        }

        const auto function = _module->resolveFunction<MovementControllerModeFunction>(transitionSymbol(requestedTransition));
        if (!function) {
            result.status = MovementCommandStatus::NativeUnavailable;
            return result;
        }
        result.invoked = true;
        if (!invokeVoid(function, transitionArgument(result.before, requestedTransition))) {
            result.status = MovementCommandStatus::NativeFault;
            return result;
        }

        result.after = inspect();
        if (!sameController(result.before, result.after) || !result.after.modeReadable ||
            (transitionHasFixedMode(requestedTransition) && result.after.mode != transitionMode(requestedTransition))) {
            result.status = MovementCommandStatus::VerificationFailed;
            return result;
        }
        result.status = MovementCommandStatus::Applied;
        return result;
    }

    MovementCommandResult MovementControllerApi::setPlannerTargetYaw(const float yawRadians) const noexcept
    {
        MovementCommandResult result{};
        result.transition = MovementTransition::PlannerDirectControl;
        result.requestedYawRadians = yawRadians;
        if (!std::isfinite(yawRadians)) {
            result.status = MovementCommandStatus::InvalidArgument;
            return result;
        }
        result.status = preflight(result.before);
        if (result.status != MovementCommandStatus::Applied) {
            return result;
        }
        if (result.before.mode != MovementMode::PlannerDirectControl) {
            result.status = MovementCommandStatus::WrongMode;
            return result;
        }

        const auto function = _module->resolveFunction<MovementPlannerSetTargetAngleFunction>(
            Addresses::Symbol::Character_SetPlannerTargetAngle);
        if (!function) {
            result.status = MovementCommandStatus::NativeUnavailable;
            return result;
        }
        NativePoint3 targetAngle{ .z = yawRadians };
        result.invoked = true;
        if (!invokeTargetAngle(
                function,
                reinterpret_cast<void*>(result.before.plannerDirectInterfaceAddress),
                targetAngle)) {
            result.status = MovementCommandStatus::NativeFault;
            return result;
        }

        result.after = inspect();
        result.status = sameController(result.before, result.after) && result.after.modeReadable &&
                                result.after.mode == MovementMode::PlannerDirectControl ?
            MovementCommandStatus::Applied :
            MovementCommandStatus::VerificationFailed;
        return result;
    }

    MovementCommandResult MovementControllerApi::clearPlannerTargetYaw() const noexcept
    {
        MovementCommandResult result{};
        result.transition = MovementTransition::PlannerDirectControl;
        result.status = preflight(result.before);
        if (result.status != MovementCommandStatus::Applied) {
            return result;
        }
        if (result.before.mode != MovementMode::PlannerDirectControl) {
            result.status = MovementCommandStatus::WrongMode;
            return result;
        }

        const auto function = _module->resolveFunction<MovementControllerModeFunction>(
            Addresses::Symbol::Character_ClearPlannerDirectControl);
        if (!function) {
            result.status = MovementCommandStatus::NativeUnavailable;
            return result;
        }
        result.invoked = true;
        if (!invokeVoid(function, reinterpret_cast<void*>(result.before.plannerDirectInterfaceAddress))) {
            result.status = MovementCommandStatus::NativeFault;
            return result;
        }

        result.after = inspect();
        result.status = sameController(result.before, result.after) && result.after.modeReadable ?
            MovementCommandStatus::Applied :
            MovementCommandStatus::VerificationFailed;
        return result;
    }
}
