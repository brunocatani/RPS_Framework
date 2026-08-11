#include "RPS/Runtime/PhysicsApi.h"
#include "RPS/Runtime/PhysicsScale.h"
#include "RPS/Runtime/PhysicsTiming.h"

#include <cmath>
#include <iostream>

namespace
{
    [[nodiscard]] bool near(const float left, const float right, const float epsilon = 0.00001f)
    {
        return std::fabs(left - right) <= epsilon;
    }
}

int main()
{
    using namespace RPS::Runtime;
    using namespace Physics;

    ScaleSnapshot scale{};
    const Vector4 point{ 70.0f, -35.0f, 7.0f, 1.0f };
    const auto havok = scale.toHavokPoint(point);
    const auto roundTrip = scale.toGamePoint(havok);
    if (!near(havok.x, 1.0f) || !near(roundTrip.x, point.x) || !near(roundTrip.y, point.y) ||
        roundTrip.w != point.w || scale.reciprocalDriftGameUnits() > 0.0001f || usableScale(0.0f)) {
        std::cerr << "physics scale policy failed\n";
        return 1;
    }

    const auto timing = makeTimingSample(1.0f / 45.0f, 1.0f / 90.0f, 0.0f, 1.0f / 45.0f, 2);
    if (!timing.valid || timing.usedFallback || timing.substepCount != 2 || !near(timing.simulatedDeltaSeconds, 1.0f / 45.0f) ||
        !near(timing.driveDeltaSeconds(), 1.0f / 45.0f)) {
        std::cerr << "whole-step timing policy failed\n";
        return 1;
    }
    auto substep = timing;
    substep.phase = StepPhase::SubstepPreCollide;
    if (!near(substep.driveDeltaSeconds(), 1.0f / 90.0f)) {
        std::cerr << "substep timing policy failed\n";
        return 1;
    }
    const auto fallbackTiming = makeTimingSample(0.0f, 0.0f, 0.0f, 0.0f, 0);
    if (!fallbackTiming.valid || !fallbackTiming.usedFallback || fallbackTiming.substepCount != 1 ||
        !near(fallbackTiming.driveDeltaSeconds(), FallbackPhysicsDeltaSeconds)) {
        std::cerr << "fallback timing policy failed\n";
        return 1;
    }

    const auto module = RuntimeModule::detect();
    int dummyWorld{};
    WorldReadGuard readGuard(module, &dummyWorld);
    WorldWriteGuard writeGuard(module, &dummyWorld);
    Api api(module, &dummyWorld);
    Vector4 outputLinear{};
    Vector4 outputAngular{};
    if (module || currentThreadPhysicsStepState(module) != PhysicsStepState::Unknown ||
        currentThreadInsidePhysicsStep(module) || readGuard.active() || writeGuard.active() ||
        api.snapshot(readGuard, BodyId{ 1 }).valid || api.activate(writeGuard, BodyId{ 1 }) ||
        api.setCollisionFilterInfo(writeGuard, BodyId{ 1 }, 43) ||
        api.computeHardKeyFrame(
            writeGuard, BodyId{ 1 }, Vector4{}, Vector4{ 0.0f, 0.0f, 0.0f, 1.0f }, 0.01f, outputLinear, outputAngular) ||
        readScaleSnapshot(module).runtimeBacked || !readTimingSample(module).usedFallback) {
        std::cerr << "invalid runtime did not fail closed\n";
        return 1;
    }

    return 0;
}
