#include "RPS/Runtime/AnimationPose.h"

#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/Memory.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace RPS::Runtime::Animation
{
    namespace
    {
        [[nodiscard]] bool finiteVector(const float (&values)[4]) noexcept
        {
            return std::isfinite(values[0]) && std::isfinite(values[1]) && std::isfinite(values[2]) &&
                   std::isfinite(values[3]);
        }

        [[nodiscard]] float unitBlend(const float value) noexcept
        {
            return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f;
        }

        void blendVector(const float* source, const float* target, const float amount, float* output) noexcept
        {
            const float inverse = 1.0f - amount;
            for (std::size_t index = 0; index < 4; ++index) {
                output[index] = source[index] * inverse + target[index] * amount;
            }
        }

        void blendQuaternion(const float* source, const float* target, const float amount, float* output) noexcept
        {
            float adjusted[4]{ target[0], target[1], target[2], target[3] };
            const float dot = source[0] * target[0] + source[1] * target[1] + source[2] * target[2] +
                              source[3] * target[3];
            if (dot < 0.0f) {
                for (float& value : adjusted) {
                    value = -value;
                }
            }
            blendVector(source, adjusted, amount, output);

            const float lengthSquared = output[0] * output[0] + output[1] * output[1] + output[2] * output[2] +
                                        output[3] * output[3];
            if (!std::isfinite(lengthSquared) || lengthSquared <= 0.000001f) {
                output[0] = 0.0f;
                output[1] = 0.0f;
                output[2] = 0.0f;
                output[3] = 1.0f;
                return;
            }
            const float inverseLength = 1.0f / std::sqrt(lengthSquared);
            for (std::size_t index = 0; index < 4; ++index) {
                output[index] *= inverseLength;
            }
        }

        [[nodiscard]] PoseResult describe(
            void* const generatorOutput,
            const std::uint32_t trackId,
            const TrackAccess access,
            TrackView& track) noexcept
        {
            track = viewTrack(
                generatorOutput,
                trackId,
                static_cast<std::uint16_t>(Addresses::Layouts::Animation::HkQsTransform),
                access);
            if (!track) {
                return { PoseStatus::TrackUnavailable, track.status };
            }
            if (track.header->onFraction <= 0.0f) {
                return { PoseStatus::TrackUnavailable, TrackStatus::Disabled };
            }
            const auto count = static_cast<std::size_t>(track.header->numData);
            if (count == 0) {
                return { PoseStatus::EmptyTrack, TrackStatus::Ready };
            }
            return { PoseStatus::Ready, TrackStatus::Ready, 0, count };
        }

        [[nodiscard]] bool allFinite(const std::span<const HkQsTransform> transforms) noexcept
        {
            return std::all_of(transforms.begin(), transforms.end(), [](const HkQsTransform& value) {
                return value.finite();
            });
        }

        [[nodiscard]] std::byte* element(TrackView& track, const std::size_t index) noexcept
        {
            return track.data.data() + index * static_cast<std::size_t>(track.header->elementSizeBytes);
        }
    }

    bool HkQsTransform::finite() const noexcept
    {
        return finiteVector(translation) && finiteVector(rotation) && finiteVector(scale);
    }

    HkQsTransform blendTransform(
        const HkQsTransform& source,
        const HkQsTransform& target,
        const float blendAmount) noexcept
    {
        HkQsTransform result{};
        const float amount = unitBlend(blendAmount);
        blendVector(source.translation, target.translation, amount, result.translation);
        blendQuaternion(source.rotation, target.rotation, amount, result.rotation);
        blendVector(source.scale, target.scale, amount, result.scale);
        return result;
    }

    PoseResult capturePose(void* const generatorOutput, const std::span<HkQsTransform> output) noexcept
    {
        TrackView track{};
        auto result = describe(generatorOutput, PoseTrack, TrackAccess::ActiveRead, track);
        if (!result) {
            return result;
        }
        if (output.size() < result.requiredCount) {
            result.status = PoseStatus::TargetTooSmall;
            return result;
        }
        for (std::size_t index = 0; index < result.requiredCount; ++index) {
            if (!Memory::copyFrom(element(track, index), &output[index], sizeof(HkQsTransform))) {
                result.status = PoseStatus::MemoryFailure;
                return result;
            }
        }
        result.count = result.requiredCount;
        return result;
    }

    PoseResult captureWorldFromModel(void* const generatorOutput, HkQsTransform& output) noexcept
    {
        TrackView track{};
        auto result = describe(generatorOutput, WorldFromModelTrack, TrackAccess::ActiveRead, track);
        if (!result) {
            return result;
        }
        result.requiredCount = 1;
        if (!Memory::copyFrom(track.data.data(), &output, sizeof(output))) {
            result.status = PoseStatus::MemoryFailure;
            return result;
        }
        result.count = 1;
        return result;
    }

    PoseResult copyPoseToOutput(
        void* const generatorOutput,
        const std::span<const HkQsTransform> pose) noexcept
    {
        TrackView track{};
        auto result = describe(generatorOutput, PoseTrack, TrackAccess::MutableStorage, track);
        if (!result) {
            return result;
        }
        if (pose.size() < result.requiredCount) {
            result.status = PoseStatus::SourceTooSmall;
            return result;
        }
        if (!allFinite(pose.first(result.requiredCount))) {
            result.status = PoseStatus::NonFiniteInput;
            return result;
        }
        for (std::size_t index = 0; index < result.requiredCount; ++index) {
            if (!Memory::copyTo(element(track, index), &pose[index], sizeof(HkQsTransform))) {
                result.status = PoseStatus::MemoryFailure;
                return result;
            }
        }
        result.count = result.requiredCount;
        return result;
    }

    PoseResult copyWorldFromModelToOutput(
        void* const generatorOutput,
        const HkQsTransform& transform) noexcept
    {
        TrackView track{};
        auto result = describe(generatorOutput, WorldFromModelTrack, TrackAccess::MutableStorage, track);
        if (!result) {
            return result;
        }
        result.requiredCount = 1;
        if (!transform.finite()) {
            result.status = PoseStatus::NonFiniteInput;
            return result;
        }
        if (!Memory::copyTo(track.data.data(), &transform, sizeof(transform))) {
            result.status = PoseStatus::MemoryFailure;
            return result;
        }
        result.count = 1;
        return result;
    }

    PoseResult blendPoseToOutput(
        void* const generatorOutput,
        const std::span<const HkQsTransform> source,
        const std::span<const HkQsTransform> target,
        const float blendAmount) noexcept
    {
        TrackView track{};
        auto result = describe(generatorOutput, PoseTrack, TrackAccess::MutableStorage, track);
        result.blendAmount = unitBlend(blendAmount);
        if (!result) {
            return result;
        }
        if (source.size() < result.requiredCount) {
            result.status = PoseStatus::SourceTooSmall;
            return result;
        }
        if (target.size() < result.requiredCount) {
            result.status = PoseStatus::TargetTooSmall;
            return result;
        }
        if (!allFinite(source.first(result.requiredCount)) || !allFinite(target.first(result.requiredCount))) {
            result.status = PoseStatus::NonFiniteInput;
            return result;
        }
        for (std::size_t index = 0; index < result.requiredCount; ++index) {
            const auto blended = blendTransform(source[index], target[index], result.blendAmount);
            if (!Memory::copyTo(element(track, index), &blended, sizeof(blended))) {
                result.status = PoseStatus::MemoryFailure;
                return result;
            }
        }
        result.count = result.requiredCount;
        return result;
    }

    PoseResult blendWorldFromModelToOutput(
        void* const generatorOutput,
        const HkQsTransform& source,
        const HkQsTransform& target,
        const float blendAmount) noexcept
    {
        if (!source.finite() || !target.finite()) {
            return { PoseStatus::NonFiniteInput, TrackStatus::Ready, 0, 1, unitBlend(blendAmount) };
        }
        const auto blended = blendTransform(source, target, blendAmount);
        auto result = copyWorldFromModelToOutput(generatorOutput, blended);
        result.blendAmount = unitBlend(blendAmount);
        return result;
    }
}
