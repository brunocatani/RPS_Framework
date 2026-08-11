#include "RPS/Runtime/CollisionFilter.h"
#include "RPS/Runtime/CollisionPairPolicy.h"

#include <array>
#include <cstddef>
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

    const std::array rules{
        SuppressionRule{ layerEndpoint(RockHandLayer), layerEndpoint(RockBodyLayer) },
        SuppressionRule{ groupEndpoint(17), groupEndpoint(23) },
        SuppressionRule{ layerGroupEndpoint(RockWeaponLayer, 41), anyEndpoint() },
        SuppressionRule{ layerEndpoint(RockBodyLayer), layerEndpoint(RockHandLayer) },
        SuppressionRule{},
    };
    CollisionPairPolicy policy;
    const auto published = policy.publish(rules, 77);
    if (!published.enabled || published.publishedCount != 3 || published.duplicateCount != 1 ||
        published.invalidCount != 1 || published.truncatedCount != 0 || published ||
        published.generation != 77) {
        std::cerr << "collision suppression publication failed\n";
        return 1;
    }

    const auto layerSuppressed = policy.evaluate(true, FilterInfo{ RockBodyLayer }, FilterInfo{ RockHandLayer });
    const auto groupSuppressed = policy.evaluate(
        true,
        FilterInfo{}.withLayer(3).withGroup(23),
        FilterInfo{}.withLayer(5).withGroup(17));
    const auto wildcardSuppressed = policy.evaluate(
        true,
        FilterInfo{}.withLayer(9).withGroup(2),
        FilterInfo{}.withLayer(RockWeaponLayer).withGroup(41));
    const auto untouched = policy.evaluate(true, FilterInfo{ 1 }, FilterInfo{ 2 });
    const auto vanillaRejected = policy.evaluate(false, FilterInfo{ RockHandLayer }, FilterInfo{ RockBodyLayer });
    if (!layerSuppressed.suppressed || layerSuppressed.collides || !layerSuppressed.snapshotStable ||
        layerSuppressed.generation != 77 || !groupSuppressed.suppressed || !wildcardSuppressed.suppressed ||
        untouched.suppressed || !untouched.collides || vanillaRejected.suppressed || vanillaRejected.collides) {
        std::cerr << "collision suppression evaluation failed\n";
        return 1;
    }

    policy.clear(78);
    const auto cleared = policy.evaluate(true, FilterInfo{ RockHandLayer }, FilterInfo{ RockBodyLayer });
    const auto stats = policy.stats();
    if (cleared.suppressed || !cleared.collides || cleared.snapshotEnabled || cleared.generation != 78 ||
        stats.comparisons != 6 || stats.suppressions != 3) {
        std::cerr << "collision suppression clear/stats failed\n";
        return 1;
    }

    const auto pattern = compareFilterInfoEntryPattern();
    if (pattern.bytes.size() != 15 || !pattern.mask.empty() ||
        pattern.bytes.front() != std::byte{ 0x8B } || pattern.bytes.back() != std::byte{ 0x7F }) {
        std::cerr << "collision compare signature contract failed\n";
        return 1;
    }

    return 0;
}
