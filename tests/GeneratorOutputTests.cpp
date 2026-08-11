#include "RPS/Runtime/GeneratorOutput.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>

namespace
{
    constexpr std::size_t BlobBytes = 0x300;

    struct alignas(16) Blob
    {
        std::array<std::byte, BlobBytes> bytes{};
    };

    void initialize(Blob& blob)
    {
        using namespace RPS::Runtime::Animation;
        auto* master = reinterpret_cast<TrackMasterHeader*>(blob.bytes.data());
        master->numBytes = static_cast<std::int32_t>(blob.bytes.size());
        master->numTracks = 18;

        auto* headers = reinterpret_cast<TrackHeader*>(blob.bytes.data() + sizeof(TrackMasterHeader));
        auto& pose = headers[PoseTrack];
        pose.capacity = 2;
        pose.numData = 2;
        pose.dataOffset = 0x200;
        pose.elementSizeBytes = 0x30;
        pose.onFraction = 1.0f;
    }
}

int main()
{
    using namespace RPS::Runtime::Animation;

    if (viewTrack(nullptr, PoseTrack, 0x30).status != TrackStatus::MissingOutput) {
        std::cerr << "missing output contract failed\n";
        return 1;
    }

    GeneratorOutput missing{};
    if (viewTrack(&missing, PoseTrack, 0x30).status != TrackStatus::MissingTracks) {
        std::cerr << "missing tracks contract failed\n";
        return 1;
    }

    Blob blob{};
    initialize(blob);
    GeneratorOutput output{ blob.bytes.data(), false };
    auto pose = viewTrack(&output, PoseTrack, 0x30);
    if (!pose || pose.data.size() != 0x60 || !pose.indices.empty() || pose.header->numData != 2) {
        std::cerr << "valid pose track contract failed\n";
        return 1;
    }

    pose.header->flags = TrackDisabledFlag;
    if (viewTrack(&output, PoseTrack, 0x30).status != TrackStatus::Disabled) {
        std::cerr << "disabled track contract failed\n";
        return 1;
    }
    pose.header->flags = 0;
    pose.header->dataOffset = 0x2F0;
    if (viewTrack(&output, PoseTrack, 0x30).status != TrackStatus::InvalidRange) {
        std::cerr << "overflowing track contract failed\n";
        return 1;
    }

    initialize(blob);
    pose = viewTrack(&output, PoseTrack, 0x30);
    pose.header->flags = TrackPaletteFlag;
    if (!viewTrack(&output, PoseTrack, 0x30)) {
        std::cerr << "valid palette contract failed\n";
        return 1;
    }
    pose.header->dataOffset = 0x2A0;
    if (viewTrack(&output, PoseTrack, 0x30).status != TrackStatus::PaletteRangeInvalid) {
        std::cerr << "palette bounds contract failed\n";
        return 1;
    }

    return 0;
}
