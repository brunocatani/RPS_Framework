#include "RPS/Runtime/ScenePhysics.h"
#include "RPS/Runtime/WorldAccess.h"

#include <cstdint>
#include <iostream>

int main()
{
    using namespace RPS::Runtime;
    using namespace Physics;

    constexpr RecursiveMotionRequest motion{
        .preset = MotionPreset::Keyframed,
        .recursive = true,
        .force = true,
        .activate = false,
    };
    constexpr RecursiveCollisionRequest collision{
        .enable = false,
        .recursive = true,
        .force = true,
    };
    if (!validMotionPreset(MotionPreset::Static) || !validMotionPreset(MotionPreset::Dynamic) ||
        !validMotionPreset(MotionPreset::Keyframed) || validMotionPreset(static_cast<MotionPreset>(3)) ||
        motion.preset != MotionPreset::Keyframed || !motion.recursive || !motion.force || motion.activate ||
        collision.enable || !collision.recursive || !collision.force) {
        std::cerr << "recursive command policy failed\n";
        return 1;
    }

    const auto module = RuntimeModule::detect();
    std::uintptr_t fakeVtable{};
    const auto motionResult = setMotionRecursive(module, &fakeVtable, motion);
    const auto collisionResult = enableCollisionRecursive(module, &fakeVtable, collision);
    if (module || currentThreadPhysicsStepState(module) != PhysicsStepState::Unknown ||
        motionResult.status != RecursiveCommandStatus::InvalidRuntime || motionResult.invoked || motionResult ||
        collisionResult.status != RecursiveCommandStatus::InvalidRuntime || collisionResult.invoked || collisionResult ||
        toString(RecursiveCommandStatus::NativeRejected) != "native-rejected") {
        std::cerr << "recursive commands did not fail closed without FO4VR\n";
        return 1;
    }

    return 0;
}
