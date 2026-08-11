#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/MovementController.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>

namespace
{
    template <class T, std::size_t Size>
    void write(std::array<std::byte, Size>& storage, const std::size_t offset, const T& value)
    {
        if (offset > storage.size() || sizeof(T) > storage.size() - offset) {
            std::abort();
        }
        std::memcpy(storage.data() + offset, &value, sizeof(T));
    }
}

int main()
{
    using namespace RPS;
    using namespace Runtime;
    using namespace Character;
    namespace Layout = Addresses::Layouts::Movement;

    alignas(16) std::array<std::byte, 0x400> actor{};
    alignas(16) std::array<std::byte, Layout::Controller_MinimumReadableSize> controller{};
    const auto controllerAddress = reinterpret_cast<std::uintptr_t>(controller.data());
    write(actor, Layout::Actor_ControllerSmartPointer, controllerAddress);
    write(controller, Layout::Controller_Mode, std::uint32_t{ static_cast<std::uint32_t>(MovementMode::PathFollowing) });
    const std::array<std::uint8_t, MovementPathingFlagCount> flags{ 1, 2, 3, 4, 0, 1, 0, 0 };
    write(controller, Layout::Controller_PathingFlags, flags);

    const auto snapshot = snapshotMovementController(actor.data());
    if (!snapshot.complete() || snapshot.actorAddress != reinterpret_cast<std::uintptr_t>(actor.data()) ||
        snapshot.controllerAddress != controllerAddress || snapshot.mode != MovementMode::PathFollowing ||
        snapshot.pathingFlags != flags ||
        snapshot.motionDrivenInterfaceAddress != controllerAddress + Layout::Controller_MotionDrivenInterface ||
        snapshot.plannerDirectInterfaceAddress != controllerAddress + Layout::Controller_PlannerDirectInterface) {
        std::cerr << "movement-controller snapshot failed\n";
        return 1;
    }

    write(actor, Layout::Actor_ControllerSmartPointer, std::uintptr_t{});
    if (snapshotMovementController(actor.data()).controllerResolved || snapshotMovementController(nullptr).actorReadable ||
        !validMovementTransition(MovementTransition::PlannerDirectControl) ||
        validMovementTransition(static_cast<MovementTransition>(99)) ||
        !transitionHasFixedMode(MovementTransition::PathFollowing) ||
        transitionHasFixedMode(MovementTransition::ReconcileMotionDrivenControl) ||
        transitionMode(MovementTransition::PathFollowing) != MovementMode::PathFollowing) {
        std::cerr << "movement-controller validation failed\n";
        return 1;
    }

    write(actor, Layout::Actor_ControllerSmartPointer, controllerAddress);
    const auto module = RuntimeModule::detect();
    MovementControllerApi api{ module, actor.data() };
    const auto transitionResult = api.transition(MovementTransition::PlannerDirectControl);
    const auto yawResult = api.setPlannerTargetYaw((std::numeric_limits<float>::quiet_NaN)());
    if (module || transitionResult.status != MovementCommandStatus::InvalidRuntime || transitionResult.invoked ||
        transitionResult || yawResult.status != MovementCommandStatus::InvalidArgument || yawResult.invoked || yawResult ||
        toString(MovementCommandStatus::VerificationFailed) != "verification-failed") {
        std::cerr << "movement commands did not fail closed without FO4VR\n";
        return 1;
    }

    return 0;
}
