#include "RPS/Runtime/PhysicsTiming.h"

#include "RPS/Runtime/Memory.h"

#include <algorithm>
#include <cmath>

namespace RPS::Runtime::Physics
{
    namespace
    {
        template <class T>
        [[nodiscard]] T readOr(const RuntimeModule& module, const Addresses::Symbol symbol, const T fallback) noexcept
        {
            T value{};
            const auto address = module.resolve(symbol);
            return address != 0 && Memory::read(reinterpret_cast<const void*>(address), value) ? value : fallback;
        }
    }

    float TimingSample::driveDeltaSeconds() const noexcept
    {
        if ((phase == StepPhase::SubstepPreCollide || phase == StepPhase::BetweenCollideAndSolve ||
                phase == StepPhase::SubstepPostSolve) &&
            usableDelta(substepDeltaSeconds)) {
            return substepDeltaSeconds;
        }
        if (valid && usableDelta(rawDeltaSeconds)) {
            return rawDeltaSeconds;
        }
        return FallbackPhysicsDeltaSeconds;
    }

    bool usableDelta(const float value) noexcept
    {
        return std::isfinite(value) && value > 0.000001f && value <= 0.25f;
    }

    TimingSample makeTimingSample(
        const float rawDeltaSeconds,
        const float substepDeltaSeconds,
        const float remainderDeltaSeconds,
        const float accumulatedDeltaSeconds,
        const std::uint32_t substepCount) noexcept
    {
        TimingSample sample{};
        sample.rawDeltaSeconds = usableDelta(rawDeltaSeconds) ? rawDeltaSeconds : FallbackPhysicsDeltaSeconds;
        sample.substepDeltaSeconds = usableDelta(substepDeltaSeconds) ? substepDeltaSeconds : sample.rawDeltaSeconds;
        sample.remainderDeltaSeconds = std::isfinite(remainderDeltaSeconds) ? remainderDeltaSeconds : 0.0f;
        sample.accumulatedDeltaSeconds = usableDelta(accumulatedDeltaSeconds) ? accumulatedDeltaSeconds : sample.rawDeltaSeconds;
        sample.substepCount = (std::min)(substepCount, 6u);
        if (sample.substepCount == 0) {
            sample.substepCount = 1;
        }

        const auto simulated = sample.substepDeltaSeconds * static_cast<float>(sample.substepCount);
        sample.simulatedDeltaSeconds = usableDelta(simulated) ? simulated : sample.rawDeltaSeconds;
        sample.valid = usableDelta(sample.simulatedDeltaSeconds);
        sample.usedFallback = !usableDelta(rawDeltaSeconds) || !usableDelta(substepDeltaSeconds) || substepCount == 0;
        return sample;
    }

    TimingSample readTimingSample(const RuntimeModule& module) noexcept
    {
        if (!module) {
            return {};
        }
        return makeTimingSample(
            readOr(module, Addresses::Symbol::Timing_RawDeltaSeconds, FallbackPhysicsDeltaSeconds),
            readOr(module, Addresses::Symbol::Timing_SubstepDeltaSeconds, FallbackPhysicsDeltaSeconds),
            readOr(module, Addresses::Symbol::Timing_RemainderDeltaSeconds, 0.0f),
            readOr(module, Addresses::Symbol::Timing_AccumulatedDeltaSeconds, FallbackPhysicsDeltaSeconds),
            readOr(module, Addresses::Symbol::Timing_SubstepCount, std::uint32_t{ 1 }));
    }
}
