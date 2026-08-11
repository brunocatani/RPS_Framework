#include "RPS/Runtime/CollisionFilter.h"

namespace RPS::Runtime::Collision
{
    bool Matrix::validLayer(const std::uint32_t layer) const noexcept
    {
        return _rows != nullptr && layer < MatrixLayerCount;
    }

    bool Matrix::pairEnabled(const std::uint32_t layerA, const std::uint32_t layerB) const noexcept
    {
        return validLayer(layerA) && validLayer(layerB) && (_rows[layerA] & (std::uint64_t{ 1 } << layerB)) != 0;
    }

    bool Matrix::pairSymmetric(const std::uint32_t layerA, const std::uint32_t layerB) const noexcept
    {
        return validLayer(layerA) && validLayer(layerB) && pairEnabled(layerA, layerB) == pairEnabled(layerB, layerA);
    }

    bool Matrix::setPair(const std::uint32_t layerA, const std::uint32_t layerB, const bool enabled) noexcept
    {
        if (!validLayer(layerA) || !validLayer(layerB)) {
            return false;
        }
        const auto bitA = std::uint64_t{ 1 } << layerA;
        const auto bitB = std::uint64_t{ 1 } << layerB;
        if (enabled) {
            _rows[layerA] |= bitB;
            _rows[layerB] |= bitA;
        } else {
            _rows[layerA] &= ~bitB;
            _rows[layerB] &= ~bitA;
        }
        return true;
    }

    bool Matrix::applyMask(const std::uint32_t layer, const std::uint64_t enabledLayers) noexcept
    {
        if (!validLayer(layer)) {
            return false;
        }
        for (std::uint32_t other = 0; other < MatrixLayerCount; ++other) {
            if (!setPair(layer, other, (enabledLayers & (std::uint64_t{ 1 } << other)) != 0)) {
                return false;
            }
        }
        return true;
    }

    bool Matrix::matchesMask(const std::uint32_t layer, const std::uint64_t enabledLayers) const noexcept
    {
        if (!validLayer(layer)) {
            return false;
        }
        for (std::uint32_t other = 0; other < MatrixLayerCount; ++other) {
            const auto expected = (enabledLayers & (std::uint64_t{ 1 } << other)) != 0;
            if (pairEnabled(layer, other) != expected || !pairSymmetric(layer, other)) {
                return false;
            }
        }
        return true;
    }
}
