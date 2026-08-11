#pragma once

#include "RPS/Runtime/RuntimeModule.h"

#include <cstdint>

namespace RPS::Runtime::Physics
{
    inline constexpr float FallbackPhysicsDeltaSeconds = 1.0f / 90.0f;

    enum class StepPhase : std::uint8_t
    {
        WholePreStep,
        SubstepPreCollide,
        BetweenCollideAndSolve,
        SubstepPostSolve,
    };

    struct TimingSample
    {
        float rawDeltaSeconds{ FallbackPhysicsDeltaSeconds };
        float substepDeltaSeconds{ FallbackPhysicsDeltaSeconds };
        float remainderDeltaSeconds{};
        float accumulatedDeltaSeconds{ FallbackPhysicsDeltaSeconds };
        float simulatedDeltaSeconds{ FallbackPhysicsDeltaSeconds };
        std::uint32_t substepCount{ 1 };
        StepPhase phase{ StepPhase::WholePreStep };
        bool valid{};
        bool usedFallback{ true };

        [[nodiscard]] float driveDeltaSeconds() const noexcept;
    };

    [[nodiscard]] bool usableDelta(float value) noexcept;
    [[nodiscard]] TimingSample makeTimingSample(
        float rawDeltaSeconds,
        float substepDeltaSeconds,
        float remainderDeltaSeconds,
        float accumulatedDeltaSeconds,
        std::uint32_t substepCount) noexcept;
    [[nodiscard]] TimingSample readTimingSample(const RuntimeModule& module) noexcept;
}
