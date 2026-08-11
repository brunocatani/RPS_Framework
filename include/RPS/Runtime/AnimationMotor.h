#pragma once

#include <cstdint>

namespace RPS::Runtime::Animation
{
    struct MotorControlSettings
    {
        bool forceKeyframedControls{ true };
        bool forceKeyframedBones{ false };
        bool forcePoweredControls{ true };
        bool tuneKeyframedControls{ true };
        bool tunePoweredControls{ true };
        float keyframedOnFraction{ 1.0f };
        float keyframedBonesOnFraction{ 1.1f };
        float keyframedPoweredBlendOnFraction{ 1.1f };
        float poweredOnFraction{ 0.05f };
        float poweredOnlyOnFraction{ 0.05f };
        float snapBlendSeconds{ 0.10f };
        float hierarchyGain{ 0.6f };
        float velocityDamping{};
        float accelerationGain{ 1.0f };
        float velocityGain{ 0.6f };
        float positionGain{ 0.05f };
        float positionMaxLinearVelocity{ 1.4f };
        float positionMaxAngularVelocity{ 1.8f };
        float snapGain{ 1.0f };
        float snapMaxLinearVelocity{ 100.0f };
        float snapMaxAngularVelocity{ 100.0f };
        float snapMaxLinearDistance{ 100.0f };
        float snapMaxAngularDistance{ 100.0f };
        float poweredMaxForce{ 500.0f };
        float poweredTau{ 0.8f };
        float poweredDamping{ 1.0f };
        float poweredProportionalRecoveryVelocity{ 5.0f };
        float poweredConstantRecoveryVelocity{ 0.2f };
    };

    [[nodiscard]] MotorControlSettings sanitizeMotorControlSettings(MotorControlSettings settings) noexcept;

    enum class WorldFromModelMode : std::uint8_t
    {
        UseOld = 0,
        UseInput = 1,
        Compute = 2,
        None = 3,
        UseRootBone = 4,
        Unknown = 0xFF,
    };

    struct MotorInspectionResult
    {
        bool tracksResolved{};
        bool keyframedTrackUsable{};
        bool poweredTrackUsable{};
        bool worldFromModelModeTrackUsable{};
        bool initiallyPoweredOnly{};
        bool poweredAllNoForce{};
        bool poweredOnlyAllNoForce{};
        bool computingWorldFromModel{};
        bool usingRootBoneAsWorldFromModel{};
        std::uint16_t trackCount{};
        std::uint16_t keyframedCapacity{};
        std::uint16_t keyframedNumData{};
        std::uint16_t poweredCapacity{};
        std::uint16_t poweredNumData{};
        std::uint16_t poweredPositiveForceControls{};
        float initialKeyframedOnFraction{};
        float initialPoweredOnFraction{};
        float maxPoweredForce{};
        WorldFromModelMode worldFromModelMode{ WorldFromModelMode::Unknown };
    };

    struct MotorApplyResult
    {
        bool tracksResolved{};
        bool keyframedTrackUsable{};
        bool keyframedBonesTrackUsable{};
        bool poweredTrackUsable{};
        bool keyframedForced{};
        bool keyframedBonesForced{};
        bool poweredForced{};
        bool keyframedPoweredBlendRaised{};
        bool keyframedTuned{};
        bool poweredTuned{};
        bool poweredOnFractionClamped{};
        bool initiallyPoweredOnly{};
        bool poweredSuppressedForOneFrame{};
        bool snapActive{};
        std::uint16_t trackCount{};
        std::uint16_t keyframedElementSize{};
        std::uint16_t keyframedBonesElementSize{};
        std::uint16_t poweredElementSize{};
        std::uint16_t keyframedCapacity{};
        std::uint16_t keyframedNumData{};
        std::uint16_t keyframedBonesCapacity{};
        std::uint16_t keyframedBonesNumData{};
        std::uint16_t keyframedBonesAppliedCount{};
        std::uint16_t poweredCapacity{};
        std::uint16_t poweredNumData{};
        float keyframedOnFraction{};
        float keyframedBonesOnFraction{};
        float poweredOnFraction{};
        float initialKeyframedOnFraction{};
        float initialPoweredOnFraction{};
    };

    // These views are valid only during the synchronous generator callback that owns the output.
    [[nodiscard]] MotorInspectionResult inspectDriveToPoseMotorState(void* generatorOutput) noexcept;
    [[nodiscard]] MotorApplyResult applyDriveToPoseMotorControl(
        void* generatorOutput,
        const MotorControlSettings& settings,
        float activeDriveSeconds,
        std::uint32_t ragdollBodyCount,
        bool suppressPoweredControlsForOneFrame = false) noexcept;
}
