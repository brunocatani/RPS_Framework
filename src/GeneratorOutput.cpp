#include "RPS/Runtime/GeneratorOutput.h"

#include "RPS/Runtime/Memory.h"

#include <cmath>
#include <limits>

namespace RPS::Runtime::Animation
{
    namespace
    {
        [[nodiscard]] constexpr std::uint32_t align16(const std::uint32_t value) noexcept
        {
            return (value + 15u) & ~15u;
        }

        [[nodiscard]] bool checkedAdd(const std::uint32_t lhs, const std::uint32_t rhs, std::uint32_t& result) noexcept
        {
            if (lhs > (std::numeric_limits<std::uint32_t>::max)() - rhs) {
                return false;
            }
            result = lhs + rhs;
            return true;
        }

        [[nodiscard]] bool checkedMultiply(const std::uint32_t lhs, const std::uint32_t rhs, std::uint32_t& result) noexcept
        {
            if (lhs != 0 && rhs > (std::numeric_limits<std::uint32_t>::max)() / lhs) {
                return false;
            }
            result = lhs * rhs;
            return true;
        }
    }

    TrackView viewTrack(
        void* generatorOutput,
        const std::uint32_t trackId,
        const std::uint16_t minimumElementSize,
        const TrackAccess access) noexcept
    {
        TrackView result{};
        result.access = access;
        if (!generatorOutput) {
            return result;
        }

        GeneratorOutput output{};
        if (!Memory::read(generatorOutput, output) || !output.tracks) {
            result.status = TrackStatus::MissingTracks;
            return result;
        }

        TrackMasterHeader master{};
        if (!Memory::read(output.tracks, master) || master.numBytes < static_cast<std::int32_t>(sizeof(master)) ||
            master.numBytes > static_cast<std::int32_t>(MaxReasonableTrackBytes) || master.numTracks <= 0 || master.numTracks >= 128) {
            result.status = TrackStatus::InvalidMasterHeader;
            return result;
        }

        const auto blobBytes = static_cast<std::uint32_t>(master.numBytes);
        const auto trackCount = static_cast<std::uint32_t>(master.numTracks);
        std::uint32_t headerBytes = 0;
        std::uint32_t tableBytes = 0;
        if (!checkedMultiply(static_cast<std::uint32_t>(sizeof(TrackHeader)), trackCount, headerBytes) ||
            !checkedAdd(static_cast<std::uint32_t>(sizeof(TrackMasterHeader)), headerBytes, tableBytes) || tableBytes > blobBytes ||
            !Memory::rangeHasAccess(output.tracks, blobBytes, Memory::Access::Read)) {
            result.status = TrackStatus::InvalidMasterHeader;
            return result;
        }
        if (access == TrackAccess::MutableStorage &&
            !Memory::rangeHasAccess(output.tracks, blobBytes, Memory::Access::Write)) {
            result.status = TrackStatus::ReadOnlyStorage;
            return result;
        }
        result.blobBytes = blobBytes;
        result.trackCount = trackCount;
        if (trackId >= trackCount) {
            result.status = TrackStatus::MissingTrack;
            return result;
        }

        auto* base = static_cast<std::byte*>(output.tracks);
        auto* header = reinterpret_cast<TrackHeader*>(base + sizeof(TrackMasterHeader) + sizeof(TrackHeader) * trackId);
        TrackHeader copy{};
        if (!Memory::read(header, copy)) {
            result.status = TrackStatus::InvalidHeader;
            return result;
        }
        if ((copy.flags & TrackDisabledFlag) != 0 ||
            (access == TrackAccess::ActiveRead && copy.onFraction <= 0.0f)) {
            result.status = TrackStatus::Disabled;
            return result;
        }
        if (copy.capacity <= 0 || copy.numData < 0 || copy.numData > copy.capacity || copy.dataOffset <= 0 ||
            !std::isfinite(copy.onFraction) ||
            copy.elementSizeBytes < static_cast<std::int16_t>(minimumElementSize)) {
            result.status = TrackStatus::InvalidHeader;
            return result;
        }

        const auto dataOffset = static_cast<std::uint32_t>(copy.dataOffset);
        const auto capacity = static_cast<std::uint32_t>(copy.capacity);
        const auto elementSize = static_cast<std::uint32_t>(copy.elementSizeBytes);
        std::uint32_t rawDataBytes = 0;
        if (dataOffset < tableBytes || !checkedMultiply(elementSize, capacity, rawDataBytes)) {
            result.status = TrackStatus::InvalidRange;
            return result;
        }
        const auto dataBytes = align16(rawDataBytes);
        std::uint32_t dataEnd = 0;
        if (!checkedAdd(dataOffset, dataBytes, dataEnd) || dataEnd > blobBytes) {
            result.status = TrackStatus::InvalidRange;
            return result;
        }

        result.header = header;
        result.data = { base + dataOffset, rawDataBytes };
        if ((copy.flags & TrackPaletteFlag) != 0) {
            std::uint32_t indicesEnd = 0;
            if (!checkedAdd(dataEnd, capacity, indicesEnd) || indicesEnd > blobBytes) {
                result.status = TrackStatus::PaletteRangeInvalid;
                result.header = nullptr;
                result.data = {};
                return result;
            }
            result.indices = { reinterpret_cast<std::int8_t*>(base + dataEnd), capacity };
        }
        result.status = TrackStatus::Ready;
        return result;
    }
}
