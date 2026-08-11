#include "RPS/Runtime/CollisionPairPolicy.h"

#include <algorithm>

namespace RPS::Runtime::Collision
{
    namespace
    {
        inline constexpr std::uint32_t EndpointLayerMask = LayerMask;
        inline constexpr std::uint32_t EndpointMatchLayer = 1u << 7;
        inline constexpr std::uint32_t EndpointGroupShift = 8;
        inline constexpr std::uint32_t EndpointMatchGroup = 1u << 24;

        static_assert(std::atomic<std::uint64_t>::is_always_lock_free);

        [[nodiscard]] bool encodeEndpoint(
            const PairEndpoint& endpoint,
            std::uint32_t& encoded) noexcept
        {
            encoded = 0;
            if (endpoint.matchLayer) {
                if (endpoint.layer > LayerMask) {
                    return false;
                }
                encoded |= EndpointMatchLayer | endpoint.layer;
            }
            if (endpoint.matchGroup) {
                encoded |= EndpointMatchGroup |
                           (static_cast<std::uint32_t>(endpoint.group) << EndpointGroupShift);
            }
            return true;
        }

        [[nodiscard]] bool encodeRule(
            const SuppressionRule& rule,
            std::uint64_t& encoded) noexcept
        {
            std::uint32_t first{};
            std::uint32_t second{};
            if (!encodeEndpoint(rule.first, first) || !encodeEndpoint(rule.second, second) ||
                (first == 0 && second == 0)) {
                encoded = 0;
                return false;
            }
            if (second < first) {
                std::swap(first, second);
            }
            encoded = static_cast<std::uint64_t>(first) |
                      (static_cast<std::uint64_t>(second) << 32);
            return true;
        }

        [[nodiscard]] bool endpointMatches(
            const std::uint32_t encoded,
            const FilterInfo filter) noexcept
        {
            if ((encoded & EndpointMatchLayer) != 0 &&
                filter.layer() != (encoded & EndpointLayerMask)) {
                return false;
            }
            if ((encoded & EndpointMatchGroup) != 0 &&
                filter.group() != static_cast<std::uint16_t>(encoded >> EndpointGroupShift)) {
                return false;
            }
            return true;
        }

        [[nodiscard]] bool ruleMatches(
            const std::uint64_t encoded,
            const FilterInfo first,
            const FilterInfo second) noexcept
        {
            const auto endpointA = static_cast<std::uint32_t>(encoded);
            const auto endpointB = static_cast<std::uint32_t>(encoded >> 32);
            return (endpointMatches(endpointA, first) && endpointMatches(endpointB, second)) ||
                   (endpointMatches(endpointA, second) && endpointMatches(endpointB, first));
        }

        [[nodiscard]] bool stableSequence(
            const std::uint64_t before,
            const std::uint64_t after) noexcept
        {
            return before == after && (before & 1u) == 0;
        }
    }

    std::uint64_t CollisionPairPolicy::beginWrite() noexcept
    {
        auto sequence = _sequence.load(std::memory_order_relaxed);
        if ((sequence & 1u) != 0) {
            ++sequence;
        }
        const auto writeSequence = sequence + 1;
        _sequence.store(writeSequence, std::memory_order_seq_cst);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        return writeSequence;
    }

    void CollisionPairPolicy::finishWrite(const std::uint64_t writeSequence) noexcept
    {
        std::atomic_thread_fence(std::memory_order_seq_cst);
        _sequence.store(writeSequence + 1, std::memory_order_seq_cst);
    }

    PolicyPublishResult CollisionPairPolicy::publish(
        const std::span<const SuppressionRule> rules,
        const std::uint64_t generation) noexcept
    {
        std::scoped_lock lock(_writeMutex);
        const auto writeSequence = beginWrite();

        PolicyPublishResult result{};
        result.generation = generation;
        const auto considered = (std::min)(rules.size(), MaximumSuppressionRules);
        result.truncatedCount = static_cast<std::uint32_t>((std::min)(
            rules.size() - considered,
            static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)())));

        for (std::size_t index = 0; index < considered; ++index) {
            std::uint64_t encoded{};
            if (!encodeRule(rules[index], encoded)) {
                ++result.invalidCount;
                continue;
            }

            bool duplicate = false;
            for (std::uint32_t existing = 0; existing < result.publishedCount; ++existing) {
                if (_rules[existing].load(std::memory_order_relaxed) == encoded) {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate) {
                ++result.duplicateCount;
                continue;
            }
            _rules[result.publishedCount].store(encoded, std::memory_order_relaxed);
            ++result.publishedCount;
        }

        result.enabled = result.publishedCount != 0;
        _generation.store(generation, std::memory_order_relaxed);
        _count.store(result.publishedCount, std::memory_order_relaxed);
        _enabled.store(result.enabled, std::memory_order_relaxed);
        finishWrite(writeSequence);
        return result;
    }

    void CollisionPairPolicy::clear(const std::uint64_t generation) noexcept
    {
        std::scoped_lock lock(_writeMutex);
        const auto writeSequence = beginWrite();
        _enabled.store(false, std::memory_order_relaxed);
        _count.store(0, std::memory_order_relaxed);
        _generation.store(generation, std::memory_order_relaxed);
        finishWrite(writeSequence);
    }

    PairPolicyDecision CollisionPairPolicy::evaluate(
        const bool vanillaCollides,
        const FilterInfo first,
        const FilterInfo second) const noexcept
    {
        _comparisons.fetch_add(1, std::memory_order_relaxed);
        PairPolicyDecision result{};
        result.vanillaCollides = vanillaCollides;
        result.collides = vanillaCollides;
        if (!vanillaCollides) {
            return result;
        }

        const auto sequenceBefore = _sequence.load(std::memory_order_seq_cst);
        if ((sequenceBefore & 1u) != 0) {
            result.snapshotStable = false;
            return result;
        }

        result.snapshotEnabled = _enabled.load(std::memory_order_relaxed);
        result.generation = _generation.load(std::memory_order_relaxed);
        if (result.snapshotEnabled) {
            const auto count = (std::min)(
                _count.load(std::memory_order_relaxed),
                static_cast<std::uint32_t>(MaximumSuppressionRules));
            for (std::uint32_t index = 0; index < count; ++index) {
                if (ruleMatches(_rules[index].load(std::memory_order_relaxed), first, second)) {
                    result.matchingRule = index;
                    break;
                }
            }
        }

        const auto sequenceAfter = _sequence.load(std::memory_order_seq_cst);
        if (!stableSequence(sequenceBefore, sequenceAfter)) {
            result.snapshotStable = false;
            result.snapshotEnabled = false;
            result.matchingRule = NoSuppressionRule;
            return result;
        }
        if (result.snapshotEnabled && result.matchingRule != NoSuppressionRule) {
            result.collides = false;
            result.suppressed = true;
            _suppressions.fetch_add(1, std::memory_order_relaxed);
        }
        return result;
    }

    PairPolicyStats CollisionPairPolicy::stats() const noexcept
    {
        return {
            _comparisons.load(std::memory_order_relaxed),
            _suppressions.load(std::memory_order_relaxed),
        };
    }
}
