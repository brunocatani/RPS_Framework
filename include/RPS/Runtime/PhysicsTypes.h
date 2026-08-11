#pragma once

#include <compare>
#include <cmath>
#include <cstdint>
#include <limits>

namespace RPS::Runtime::Physics
{
    inline constexpr std::uint32_t InvalidBodyId = static_cast<std::uint32_t>((std::numeric_limits<std::int32_t>::max)());

    struct BodyId
    {
        std::uint32_t value{ InvalidBodyId };

        [[nodiscard]] constexpr bool valid() const noexcept { return value != InvalidBodyId; }
        auto operator<=>(const BodyId&) const = default;
    };
    static_assert(sizeof(BodyId) == sizeof(std::uint32_t));

    struct alignas(16) Vector4
    {
        float x{};
        float y{};
        float z{};
        float w{};

        [[nodiscard]] bool finite() const noexcept
        {
            return std::isfinite(x) && std::isfinite(y) && std::isfinite(z) && std::isfinite(w);
        }
    };
    static_assert(sizeof(Vector4) == 0x10);

    struct alignas(16) Transform
    {
        Vector4 column0{ 1.0f, 0.0f, 0.0f, 0.0f };
        Vector4 column1{ 0.0f, 1.0f, 0.0f, 0.0f };
        Vector4 column2{ 0.0f, 0.0f, 1.0f, 0.0f };
        Vector4 translation{ 0.0f, 0.0f, 0.0f, 1.0f };

        [[nodiscard]] bool finite() const noexcept
        {
            return column0.finite() && column1.finite() && column2.finite() && translation.finite();
        }
    };
    static_assert(sizeof(Transform) == 0x40);
}
