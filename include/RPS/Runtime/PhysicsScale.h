#pragma once

#include "RPS/Runtime/PhysicsTypes.h"
#include "RPS/Runtime/RuntimeModule.h"

#include <cmath>

namespace RPS::Runtime::Physics
{
    inline constexpr float FallbackHavokToGame = 70.0f;
    inline constexpr float FallbackGameToHavok = 1.0f / FallbackHavokToGame;

    struct ScaleSnapshot
    {
        float gameToHavok{ FallbackGameToHavok };
        float havokToGame{ FallbackHavokToGame };
        float vrGlobalScale{};
        float raycastScale{};
        bool runtimeBacked{};

        [[nodiscard]] Vector4 toHavokPoint(const Vector4& value) const noexcept;
        [[nodiscard]] Vector4 toGamePoint(const Vector4& value) const noexcept;
        [[nodiscard]] float reciprocalDriftGameUnits() const noexcept;
    };

    [[nodiscard]] bool usableScale(float value) noexcept;
    [[nodiscard]] ScaleSnapshot readScaleSnapshot(const RuntimeModule& module) noexcept;
}
