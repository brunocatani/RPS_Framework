#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace RPS::Runtime::Animation
{
    inline constexpr std::uint32_t MaxReasonableTrackBytes = 1024u * 1024u;
    inline constexpr std::uint16_t TrackDisabledFlag = 0x10;
    inline constexpr std::uint8_t TrackPaletteFlag = 0x02;
    inline constexpr std::uint8_t TrackSparseFlag = 0x04;
    inline constexpr std::uint32_t WorldFromModelTrack = 0;
    inline constexpr std::uint32_t PoseTrack = 2;
    inline constexpr std::uint32_t KeyframedRagdollControlTrack = 13;
    inline constexpr std::uint32_t PoweredRagdollControlTrack = 15;
    inline constexpr std::uint32_t PoweredWorldFromModelModeTrack = 16;
    inline constexpr std::uint32_t KeyframedRagdollBonesTrack = 17;

    struct GeneratorOutput
    {
        void* tracks{};
        bool deleteTracks{};
    };

    struct TrackMasterHeader
    {
        std::int32_t numBytes{};
        std::int32_t numTracks{};
        std::int8_t unused[8]{};
    };

    struct TrackHeader
    {
        std::int16_t capacity{};
        std::int16_t numData{};
        std::int16_t dataOffset{};
        std::int16_t elementSizeBytes{};
        float onFraction{};
        std::uint8_t flags{};
        std::uint8_t type{};
        std::uint16_t pad{};
    };

    static_assert(sizeof(TrackMasterHeader) == 0x10);
    static_assert(sizeof(TrackHeader) == 0x10);

    enum class TrackStatus : std::uint8_t
    {
        Ready,
        MissingOutput,
        MissingTracks,
        InvalidMasterHeader,
        MissingTrack,
        Disabled,
        InvalidHeader,
        InvalidRange,
        PaletteRangeInvalid,
    };

    struct TrackView
    {
        TrackStatus status{ TrackStatus::MissingOutput };
        TrackHeader* header{};
        std::span<std::byte> data{};
        std::span<std::int8_t> indices{};

        [[nodiscard]] explicit operator bool() const noexcept { return status == TrackStatus::Ready; }
    };

    [[nodiscard]] TrackView viewTrack(void* generatorOutput, std::uint32_t trackId, std::uint16_t minimumElementSize) noexcept;
}
