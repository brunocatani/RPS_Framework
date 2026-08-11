#pragma once

#include "RPS/Runtime/AnimationPose.h"
#include "RPS/Runtime/RuntimeModule.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace RPS::Runtime::Animation
{
    enum class RootPoseStatus : std::uint8_t
    {
        Ready,
        InvalidRuntime,
        InvalidDriver,
        InvalidContext,
        InvalidRagdollInterface,
        VtableUnavailable,
        FunctionUnavailable,
        InvalidSkeleton,
        OutputTooSmall,
        InvalidBoneIndex,
        BodyUnavailable,
        InvalidPose,
        InvalidScale,
        NativeFault,
        NonFiniteResult,
        GenerationChanged,
    };

    struct RootPoseResult
    {
        RootPoseStatus status{ RootPoseStatus::InvalidRuntime };
        std::size_t count{};
        std::size_t requiredCount{};

        [[nodiscard]] explicit operator bool() const noexcept { return status == RootPoseStatus::Ready; }
    };

    struct LowSkeletonResult
    {
        RootPoseStatus status{ RootPoseStatus::InvalidRuntime };
        std::uintptr_t ragdollInterfaceAddress{};
        std::uintptr_t skeletonAddress{};
        std::size_t boneCount{};
        std::size_t copiedCount{};

        [[nodiscard]] explicit operator bool() const noexcept { return status == RootPoseStatus::Ready; }
    };

    struct RootBodyAnchorResult
    {
        HkQsTransform worldTransform{};
        std::array<float, 4> localOffset{};
        std::uintptr_t ragdollInterfaceAddress{};
        std::uintptr_t physicsInterfaceAddress{};
        std::uintptr_t bodyHandleAddress{};
        std::uint32_t boneIndex{};
        std::uint32_t bodyId{};
        RootPoseStatus status{ RootPoseStatus::InvalidRuntime };
        std::array<std::byte, 15> reserved{};

        [[nodiscard]] explicit operator bool() const noexcept { return status == RootPoseStatus::Ready; }
    };
    static_assert(sizeof(RootBodyAnchorResult) == 0x70);

    [[nodiscard]] std::string_view toString(RootPoseStatus status) noexcept;

    /**
     * Checked primitives from SCISSORS' shipped FO4VR root-pose path. Driver,
     * context, graph, skeleton, and body handles are borrowed and may only be
     * used synchronously during the callback that supplied them. No pointer is
     * retained and no world-from-model track is mutated by this API.
     */
    class RootPoseApi
    {
    public:
        RootPoseApi(RuntimeModule module, const void* driver, const void* context) noexcept :
            _module(module), _driver(driver), _context(context)
        {}

        [[nodiscard]] LowSkeletonResult copyLowSkeletonParents(
            std::span<std::int16_t> output) const noexcept;
        [[nodiscard]] RootBodyAnchorResult readBodyAnchor(std::uint32_t boneIndex = 0) const noexcept;
        [[nodiscard]] RootPoseResult mapHighToLow(
            std::span<const HkQsTransform> highPoseLocal,
            std::span<HkQsTransform> lowPoseLocal) const noexcept;
        [[nodiscard]] RootPoseResult copyAndApplyScale(
            std::span<const HkQsTransform> input,
            std::span<HkQsTransform> output,
            float worldScale) const noexcept;
        [[nodiscard]] RootPoseResult copyAndScaleTransform(
            const HkQsTransform& input,
            HkQsTransform& output) const noexcept;
        [[nodiscard]] RootPoseResult localToWorld(
            std::span<const std::int16_t> parentIndices,
            const HkQsTransform& worldFromModel,
            std::span<const HkQsTransform> localPose,
            std::span<HkQsTransform> worldPose) const noexcept;

    private:
        RuntimeModule _module{};
        const void* _driver{};
        const void* _context{};
    };

    enum class RootMathStatus : std::uint8_t
    {
        Ready,
        NonFiniteTransform,
        InvalidQuaternion,
        NonFiniteOffset,
        InvalidMaximumOffset,
        OffsetTooLarge,
        NonFiniteBias,
        InvalidMaximumSeparation,
        SpaceMismatch,
    };

    struct RootAnchorResult
    {
        HkQsTransform transform{};
        float offsetLength{};
        RootMathStatus status{ RootMathStatus::NonFiniteTransform };
        std::array<std::byte, 11> reserved{};

        [[nodiscard]] explicit operator bool() const noexcept { return status == RootMathStatus::Ready; }
    };
    static_assert(sizeof(RootAnchorResult) == 0x40);

    struct RootBiasResult
    {
        HkQsTransform adjustedAnimationAnchor{};
        std::array<float, 4> bias{};
        float separation{};
        RootMathStatus status{ RootMathStatus::NonFiniteTransform };
        std::array<std::byte, 11> reserved{};

        [[nodiscard]] explicit operator bool() const noexcept { return status == RootMathStatus::Ready; }
    };
    static_assert(sizeof(RootBiasResult) == 0x50);

    struct RootDeltaResult
    {
        RootMathStatus status{ RootMathStatus::NonFiniteTransform };
        std::array<float, 4> offset{};
        float length{};
        float horizontalLength{};
        float yawRadians{};

        [[nodiscard]] explicit operator bool() const noexcept { return status == RootMathStatus::Ready; }
    };

    [[nodiscard]] HkQsTransform identityHkQsTransform() noexcept;
    [[nodiscard]] RootAnchorResult applyBodyAnchorOffset(
        const HkQsTransform& transform,
        std::span<const float, 3> localOffset,
        float maximumOffset = 10.0f) noexcept;
    [[nodiscard]] RootBiasResult computeRootTranslationBias(
        const HkQsTransform& animationAnchor,
        const HkQsTransform& actualAnchor) noexcept;
    [[nodiscard]] RootBiasResult applyRootTranslationBias(
        const HkQsTransform& animationAnchor,
        const HkQsTransform& actualAnchor,
        std::span<const float, 3> bias,
        float maximumSeparation) noexcept;
    [[nodiscard]] RootDeltaResult sampleRootDelta(
        const HkQsTransform& previousAnimationRoot,
        const HkQsTransform& actualRoot) noexcept;
    [[nodiscard]] std::string_view toString(RootMathStatus status) noexcept;
}
