#include "RPS/Runtime/CollisionFilter.h"

#include <array>
#include <iostream>

int main()
{
    using namespace RPS::Runtime::Collision;

    constexpr FilterInfo initial{ 0x1234002Au };
    static_assert(initial.layer() == 42);
    static_assert(initial.group() == 0x1234);
    static_assert(initial.withLayer(RockWeaponLayer).layer() == RockWeaponLayer);
    static_assert(initial.withLayer(RockWeaponLayer).group() == initial.group());
    static_assert(initial.withGroup(0xBEEF).group() == 0xBEEF);
    static_assert(initial.withGroup(0xBEEF).layer() == initial.layer());

    std::array<std::uint64_t, MatrixLayerCount> rows{};
    Matrix matrix(rows.data());
    if (!matrix.setPair(1, 2, true) || !matrix.pairEnabled(1, 2) || !matrix.pairEnabled(2, 1) || !matrix.pairSymmetric(1, 2)) {
        std::cerr << "symmetric pair enable failed\n";
        return 1;
    }
    if (!matrix.setPair(1, 2, false) || matrix.pairEnabled(1, 2) || matrix.pairEnabled(2, 1)) {
        std::cerr << "symmetric pair disable failed\n";
        return 1;
    }

    constexpr auto mask = (std::uint64_t{ 1 } << 3) | (std::uint64_t{ 1 } << RockHandLayer) |
                          (std::uint64_t{ 1 } << RockWeaponLayer);
    if (!matrix.applyMask(RockBodyLayer, mask) || !matrix.matchesMask(RockBodyLayer, mask) ||
        matrix.setPair(MatrixLayerCount, 0, true) || matrix.applyMask(MatrixLayerCount, mask)) {
        std::cerr << "matrix mask contract failed\n";
        return 1;
    }

    return 0;
}
