#pragma once

#include <compare>
#include <cstdint>

namespace RPS::Addresses
{
    struct RuntimeVersion
    {
        std::uint16_t major{};
        std::uint16_t minor{};
        std::uint16_t patch{};
        std::uint16_t build{};

        auto operator<=>(const RuntimeVersion&) const = default;
    };

    inline constexpr RuntimeVersion Fallout4Vr_1_2_72{ 1, 2, 72, 0 };
}
