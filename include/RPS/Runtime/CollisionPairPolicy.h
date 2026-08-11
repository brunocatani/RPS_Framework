#pragma once

#include "RPS/Runtime/CollisionFilter.h"
#include "RPS/Runtime/RuntimeModule.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <span>

namespace RPS::Runtime::Collision
{
    inline constexpr std::size_t MaximumSuppressionRules = 256;
    inline constexpr std::uint32_t NoSuppressionRule = (std::numeric_limits<std::uint32_t>::max)();

    inline constexpr std::array<std::byte, 15> CompareFilterInfoExpectedPrefix{
        std::byte{ 0x8B }, std::byte{ 0xC2 },
        std::byte{ 0x44 }, std::byte{ 0x8B }, std::byte{ 0xD2 },
        std::byte{ 0x4C }, std::byte{ 0x8B }, std::byte{ 0xD9 },
        std::byte{ 0xC1 }, std::byte{ 0xE8 }, std::byte{ 0x0E },
        std::byte{ 0x41 }, std::byte{ 0x83 }, std::byte{ 0xE2 }, std::byte{ 0x7F }
    };

    [[nodiscard]] constexpr BytePattern compareFilterInfoEntryPattern() noexcept
    {
        return { CompareFilterInfoExpectedPrefix, {} };
    }

    struct PairEndpoint
    {
        bool matchLayer{};
        bool matchGroup{};
        std::uint8_t layer{};
        std::uint16_t group{};
    };

    struct SuppressionRule
    {
        PairEndpoint first{};
        PairEndpoint second{};
    };

    [[nodiscard]] constexpr PairEndpoint anyEndpoint() noexcept { return {}; }
    [[nodiscard]] constexpr PairEndpoint layerEndpoint(const std::uint8_t layer) noexcept
    {
        return { true, false, layer, 0 };
    }
    [[nodiscard]] constexpr PairEndpoint groupEndpoint(const std::uint16_t group) noexcept
    {
        return { false, true, 0, group };
    }
    [[nodiscard]] constexpr PairEndpoint layerGroupEndpoint(
        const std::uint8_t layer,
        const std::uint16_t group) noexcept
    {
        return { true, true, layer, group };
    }

    struct PolicyPublishResult
    {
        bool enabled{};
        std::uint32_t publishedCount{};
        std::uint32_t invalidCount{};
        std::uint32_t duplicateCount{};
        std::uint32_t truncatedCount{};
        std::uint64_t generation{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return invalidCount == 0 && truncatedCount == 0;
        }
    };

    struct PairPolicyDecision
    {
        bool vanillaCollides{};
        bool collides{};
        bool suppressed{};
        bool snapshotEnabled{};
        bool snapshotStable{ true };
        std::uint32_t matchingRule{ NoSuppressionRule };
        std::uint64_t generation{};
    };

    struct PairPolicyStats
    {
        std::uint64_t comparisons{};
        std::uint64_t suppressions{};
    };

    /**
     * Process-lifetime, consumer-owned collision suppression snapshot. Writers
     * serialize publication; physics readers are lock-free, allocation-free,
     * and fail open to the vanilla result whenever publication is unstable.
     * Rules are immutable while published and match symmetrically. The policy
     * can only turn a vanilla collision off, never create a collision.
     *
     * This object does not install a hook. A hook owner must validate
     * Collision_CompareFilterInfo with compareFilterInfoEntryPattern(), call
     * the original first, and keep this object alive until every reader and
     * detour has quiesced.
     */
    class CollisionPairPolicy
    {
    public:
        CollisionPairPolicy() noexcept = default;
        CollisionPairPolicy(const CollisionPairPolicy&) = delete;
        CollisionPairPolicy& operator=(const CollisionPairPolicy&) = delete;
        CollisionPairPolicy(CollisionPairPolicy&&) = delete;
        CollisionPairPolicy& operator=(CollisionPairPolicy&&) = delete;

        [[nodiscard]] PolicyPublishResult publish(
            std::span<const SuppressionRule> rules,
            std::uint64_t generation) noexcept;
        void clear(std::uint64_t generation = 0) noexcept;

        [[nodiscard]] PairPolicyDecision evaluate(
            bool vanillaCollides,
            FilterInfo first,
            FilterInfo second) const noexcept;
        [[nodiscard]] PairPolicyStats stats() const noexcept;

    private:
        [[nodiscard]] std::uint64_t beginWrite() noexcept;
        void finishWrite(std::uint64_t writeSequence) noexcept;

        mutable std::mutex _writeMutex;
        std::array<std::atomic<std::uint64_t>, MaximumSuppressionRules> _rules{};
        std::atomic<std::uint64_t> _sequence{};
        std::atomic<std::uint64_t> _generation{};
        std::atomic<std::uint32_t> _count{};
        std::atomic_bool _enabled{};
        mutable std::atomic<std::uint64_t> _comparisons{};
        mutable std::atomic<std::uint64_t> _suppressions{};
    };
}
