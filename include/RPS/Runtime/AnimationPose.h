#pragma once

#include "RPS/Runtime/GeneratorOutput.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace RPS::Runtime::Animation
{
    struct alignas(16) HkQsTransform
    {
        float translation[4]{};
        float rotation[4]{};
        float scale[4]{};

        [[nodiscard]] bool finite() const noexcept;
    };
    static_assert(sizeof(HkQsTransform) == 0x30);

    enum class PoseStatus : std::uint8_t
    {
        Ready,
        TrackUnavailable,
        EmptyTrack,
        SourceTooSmall,
        TargetTooSmall,
        NonFiniteInput,
        MemoryFailure,
    };

    struct PoseResult
    {
        PoseStatus status{ PoseStatus::TrackUnavailable };
        TrackStatus trackStatus{ TrackStatus::MissingOutput };
        std::size_t count{};
        std::size_t requiredCount{};
        float blendAmount{};

        [[nodiscard]] explicit operator bool() const noexcept { return status == PoseStatus::Ready; }
    };

    // Generator-output storage is callback-owned; these functions never retain its pointers.
    [[nodiscard]] PoseResult capturePose(void* generatorOutput, std::span<HkQsTransform> output) noexcept;
    [[nodiscard]] PoseResult captureWorldFromModel(void* generatorOutput, HkQsTransform& output) noexcept;
    [[nodiscard]] PoseResult copyPoseToOutput(
        void* generatorOutput,
        std::span<const HkQsTransform> pose) noexcept;
    [[nodiscard]] PoseResult copyWorldFromModelToOutput(
        void* generatorOutput,
        const HkQsTransform& transform) noexcept;
    [[nodiscard]] PoseResult blendPoseToOutput(
        void* generatorOutput,
        std::span<const HkQsTransform> source,
        std::span<const HkQsTransform> target,
        float blendAmount) noexcept;
    [[nodiscard]] PoseResult blendWorldFromModelToOutput(
        void* generatorOutput,
        const HkQsTransform& source,
        const HkQsTransform& target,
        float blendAmount) noexcept;
    [[nodiscard]] HkQsTransform blendTransform(
        const HkQsTransform& source,
        const HkQsTransform& target,
        float blendAmount) noexcept;
}
