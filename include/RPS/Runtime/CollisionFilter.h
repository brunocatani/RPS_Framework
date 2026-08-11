#pragma once

#include <array>
#include <cstdint>
#include <limits>

namespace RPS::Runtime::Collision
{
    inline constexpr std::uint32_t LayerMask = 0x7Fu;
    inline constexpr std::uint32_t MatrixLayerCount = 64;

    struct FilterInfo
    {
        std::uint32_t value{};

        [[nodiscard]] constexpr std::uint32_t layer() const noexcept { return value & LayerMask; }
        [[nodiscard]] constexpr std::uint16_t group() const noexcept { return static_cast<std::uint16_t>(value >> 16); }
        [[nodiscard]] constexpr FilterInfo withLayer(std::uint32_t newLayer) const noexcept
        {
            return { (value & ~LayerMask) | (newLayer & LayerMask) };
        }
        [[nodiscard]] constexpr FilterInfo withGroup(std::uint16_t newGroup) const noexcept
        {
            return { (value & static_cast<std::uint32_t>((std::numeric_limits<std::uint16_t>::max)())) |
                     (static_cast<std::uint32_t>(newGroup) << 16) };
        }
    };

    class Matrix
    {
    public:
        explicit Matrix(std::uint64_t* rows) noexcept : _rows(rows) {}

        [[nodiscard]] bool validLayer(std::uint32_t layer) const noexcept;
        [[nodiscard]] bool pairEnabled(std::uint32_t layerA, std::uint32_t layerB) const noexcept;
        [[nodiscard]] bool pairSymmetric(std::uint32_t layerA, std::uint32_t layerB) const noexcept;
        [[nodiscard]] bool setPair(std::uint32_t layerA, std::uint32_t layerB, bool enabled) noexcept;
        [[nodiscard]] bool applyMask(std::uint32_t layer, std::uint64_t enabledLayers) noexcept;
        [[nodiscard]] bool matchesMask(std::uint32_t layer, std::uint64_t enabledLayers) const noexcept;

    private:
        std::uint64_t* _rows{};
    };

    inline constexpr std::uint32_t RockHandLayer = 43;
    inline constexpr std::uint32_t RockWeaponLayer = 44;
    inline constexpr std::uint32_t RockBodyLayer = 47;
}
