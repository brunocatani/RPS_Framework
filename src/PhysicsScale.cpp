#include "RPS/Runtime/PhysicsScale.h"

#include "RPS/Runtime/Memory.h"

#include <limits>

namespace RPS::Runtime::Physics
{
    namespace
    {
        [[nodiscard]] bool readFloat(const RuntimeModule& module, const Addresses::Symbol symbol, float& value) noexcept
        {
            const auto address = module.resolve(symbol);
            return address != 0 && Memory::read(reinterpret_cast<const void*>(address), value);
        }
    }

    Vector4 ScaleSnapshot::toHavokPoint(const Vector4& value) const noexcept
    {
        return { value.x * gameToHavok, value.y * gameToHavok, value.z * gameToHavok, value.w };
    }

    Vector4 ScaleSnapshot::toGamePoint(const Vector4& value) const noexcept
    {
        return { value.x * havokToGame, value.y * havokToGame, value.z * havokToGame, value.w };
    }

    float ScaleSnapshot::reciprocalDriftGameUnits() const noexcept
    {
        if (!usableScale(gameToHavok) || !usableScale(havokToGame)) {
            return (std::numeric_limits<float>::max)();
        }
        return std::fabs((1.0f / gameToHavok) - havokToGame);
    }

    bool usableScale(const float value) noexcept
    {
        return std::isfinite(value) && value > 0.000001f && value < 10000.0f;
    }

    ScaleSnapshot readScaleSnapshot(const RuntimeModule& module) noexcept
    {
        ScaleSnapshot result{};
        if (!module) {
            return result;
        }

        float gameToHavok{};
        float havokToGame{};
        const bool readGameToHavok = readFloat(module, Addresses::Symbol::Scale_GameToHavok, gameToHavok);
        const bool readHavokToGame = readFloat(module, Addresses::Symbol::Scale_HavokToGame, havokToGame);

        if (readGameToHavok && usableScale(gameToHavok)) {
            result.gameToHavok = gameToHavok;
        } else if (readHavokToGame && usableScale(havokToGame)) {
            result.gameToHavok = 1.0f / havokToGame;
        }
        if (readHavokToGame && usableScale(havokToGame)) {
            result.havokToGame = havokToGame;
        } else if (readGameToHavok && usableScale(gameToHavok)) {
            result.havokToGame = 1.0f / gameToHavok;
        }

        float diagnostic{};
        if (readFloat(module, Addresses::Symbol::Scale_VrPrimary, diagnostic) && std::isfinite(diagnostic)) {
            result.vrGlobalScale = diagnostic;
        }
        if (readFloat(module, Addresses::Symbol::Scale_RaycastResult, diagnostic) && std::isfinite(diagnostic)) {
            result.raycastScale = diagnostic;
        }
        result.runtimeBacked = (readGameToHavok && usableScale(gameToHavok)) ||
                               (readHavokToGame && usableScale(havokToGame));
        return result;
    }
}
