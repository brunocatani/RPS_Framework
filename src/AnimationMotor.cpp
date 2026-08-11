#include "RPS/Runtime/AnimationMotor.h"

#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/GeneratorOutput.h"
#include "RPS/Runtime/Memory.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace RPS::Runtime::Animation
{
    namespace
    {
        struct KeyframedControlData
        {
            float hierarchyGain{ 0.17f };
            float velocityDamping{};
            float accelerationGain{ 1.0f };
            float velocityGain{ 0.6f };
            float positionGain{ 0.05f };
            float positionMaxLinearVelocity{ 1.4f };
            float positionMaxAngularVelocity{ 1.8f };
            float snapGain{ 0.1f };
            float snapMaxLinearVelocity{ 0.3f };
            float snapMaxAngularVelocity{ 0.3f };
            float snapMaxLinearDistance{ 0.03f };
            float snapMaxAngularDistance{ 0.1f };
        };

        struct PoweredControlData
        {
            float maxForce{ 200.0f };
            float tau{ 0.8f };
            float damping{ 1.0f };
            float proportionalRecoveryVelocity{ 2.0f };
            float constantRecoveryVelocity{ 1.0f };
        };

        static_assert(sizeof(KeyframedControlData) == Addresses::Layouts::Animation::KeyframedControlData);
        static_assert(sizeof(PoweredControlData) == Addresses::Layouts::Animation::PoweredControlData);

        [[nodiscard]] float unit(const float value, const float fallback) noexcept
        {
            return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : fallback;
        }

        [[nodiscard]] float nonNegative(const float value, const float fallback) noexcept
        {
            return std::isfinite(value) ? (std::max)(value, 0.0f) : fallback;
        }

        [[nodiscard]] std::uint16_t positive16(const std::int16_t value) noexcept
        {
            return static_cast<std::uint16_t>((std::max)(value, std::int16_t{}));
        }

        [[nodiscard]] bool paletteUsable(const TrackView& track) noexcept
        {
            return track && track.header && track.indices.size() == static_cast<std::size_t>(track.header->capacity);
        }

        [[nodiscard]] std::byte* element(TrackView& track, const std::size_t index) noexcept
        {
            return track.data.data() + index * static_cast<std::size_t>(track.header->elementSizeBytes);
        }

        [[nodiscard]] const std::byte* element(const TrackView& track, const std::size_t index) noexcept
        {
            return track.data.data() + index * static_cast<std::size_t>(track.header->elementSizeBytes);
        }

        void forcePaletteIndexZero(TrackView& track, const float onFraction) noexcept
        {
            std::fill(track.indices.begin(), track.indices.end(), std::int8_t{});
            track.header->numData = 1;
            track.header->onFraction = onFraction;
        }

        [[nodiscard]] bool writeDefaultKeyframedControl(TrackView& track) noexcept
        {
            KeyframedControlData defaults{};
            std::memset(track.data.data(), 0, static_cast<std::size_t>(track.header->elementSizeBytes));
            return Memory::copyTo(track.data.data(), &defaults, sizeof(defaults));
        }

        [[nodiscard]] bool writeDefaultPoweredControl(TrackView& track) noexcept
        {
            PoweredControlData defaults{};
            std::memset(track.data.data(), 0, static_cast<std::size_t>(track.header->elementSizeBytes));
            return Memory::copyTo(track.data.data(), &defaults, sizeof(defaults));
        }

        [[nodiscard]] bool tuneKeyframedControls(
            TrackView& track,
            const MotorControlSettings& settings,
            const bool snapActive) noexcept
        {
            const auto count = static_cast<std::size_t>((std::min)(track.header->numData, track.header->capacity));
            for (std::size_t index = 0; index < count; ++index) {
                KeyframedControlData control{};
                if (!Memory::copyFrom(element(track, index), &control, sizeof(control))) {
                    return false;
                }
                control.hierarchyGain = settings.hierarchyGain;
                control.velocityDamping = settings.velocityDamping;
                control.accelerationGain = settings.accelerationGain;
                control.velocityGain = settings.velocityGain;
                control.positionGain = settings.positionGain;
                control.positionMaxLinearVelocity = settings.positionMaxLinearVelocity;
                control.positionMaxAngularVelocity = settings.positionMaxAngularVelocity;
                if (snapActive) {
                    control.snapGain = settings.snapGain;
                    control.snapMaxLinearVelocity = settings.snapMaxLinearVelocity;
                    control.snapMaxAngularVelocity = settings.snapMaxAngularVelocity;
                    control.snapMaxLinearDistance = settings.snapMaxLinearDistance;
                    control.snapMaxAngularDistance = settings.snapMaxAngularDistance;
                }
                if (!Memory::copyTo(element(track, index), &control, sizeof(control))) {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] bool tunePoweredControls(
            TrackView& track,
            const MotorControlSettings& settings) noexcept
        {
            const auto count = static_cast<std::size_t>((std::min)(track.header->numData, track.header->capacity));
            for (std::size_t index = 0; index < count; ++index) {
                PoweredControlData control{};
                if (!Memory::copyFrom(element(track, index), &control, sizeof(control))) {
                    return false;
                }
                control.maxForce = settings.poweredMaxForce;
                control.tau = settings.poweredTau;
                control.damping = settings.poweredDamping;
                control.proportionalRecoveryVelocity = settings.poweredProportionalRecoveryVelocity;
                control.constantRecoveryVelocity = settings.poweredConstantRecoveryVelocity;
                if (!Memory::copyTo(element(track, index), &control, sizeof(control))) {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] WorldFromModelMode readWorldFromModelMode(const TrackView& track) noexcept
        {
            if (!track || !track.header || track.header->numData <= 0 ||
                track.data.size() < Addresses::Layouts::Animation::WorldFromModelModeData) {
                return WorldFromModelMode::Unknown;
            }
            const auto raw = static_cast<std::uint8_t>(
                track.data[Addresses::Layouts::Animation::WorldFromModelMode_Value]);
            switch (raw) {
            case static_cast<std::uint8_t>(WorldFromModelMode::UseOld):
                return WorldFromModelMode::UseOld;
            case static_cast<std::uint8_t>(WorldFromModelMode::UseInput):
                return WorldFromModelMode::UseInput;
            case static_cast<std::uint8_t>(WorldFromModelMode::Compute):
                return WorldFromModelMode::Compute;
            case static_cast<std::uint8_t>(WorldFromModelMode::None):
                return WorldFromModelMode::None;
            case static_cast<std::uint8_t>(WorldFromModelMode::UseRootBone):
                return WorldFromModelMode::UseRootBone;
            default:
                return WorldFromModelMode::Unknown;
            }
        }

        void inspectPoweredForces(const TrackView& track, MotorInspectionResult& result) noexcept
        {
            result.poweredAllNoForce = true;
            const auto count = static_cast<std::size_t>((std::min)(track.header->numData, track.header->capacity));
            for (std::size_t index = 0; index < count; ++index) {
                PoweredControlData control{};
                if (!Memory::copyFrom(element(track, index), &control, sizeof(control))) {
                    result.poweredTrackUsable = false;
                    result.poweredAllNoForce = false;
                    return;
                }
                if (std::isfinite(control.maxForce)) {
                    result.maxPoweredForce = (std::max)(result.maxPoweredForce, control.maxForce);
                }
                if (control.maxForce > 0.0001f) {
                    ++result.poweredPositiveForceControls;
                    result.poweredAllNoForce = false;
                }
            }
        }

        [[nodiscard]] std::uint16_t keyframedBoneCount(
            const TrackView& track,
            const std::uint32_t ragdollBodyCount) noexcept
        {
            if (!track || ragdollBodyCount == 0) {
                return 0;
            }
            const auto count = (std::min)(ragdollBodyCount, static_cast<std::uint32_t>(track.header->capacity));
            return static_cast<std::uint16_t>((std::min)(
                count,
                static_cast<std::uint32_t>((std::numeric_limits<std::int16_t>::max)())));
        }
    }

    MotorControlSettings sanitizeMotorControlSettings(MotorControlSettings settings) noexcept
    {
        const MotorControlSettings defaults{};
        settings.keyframedOnFraction = unit(settings.keyframedOnFraction, defaults.keyframedOnFraction);
        settings.keyframedBonesOnFraction = std::isfinite(settings.keyframedBonesOnFraction) ?
                                                (std::max)(settings.keyframedBonesOnFraction, 1.0f) :
                                                defaults.keyframedBonesOnFraction;
        settings.keyframedPoweredBlendOnFraction = std::isfinite(settings.keyframedPoweredBlendOnFraction) ?
                                                        std::clamp(settings.keyframedPoweredBlendOnFraction, 0.0f, 2.0f) :
                                                        defaults.keyframedPoweredBlendOnFraction;
        settings.poweredOnFraction = unit(settings.poweredOnFraction, defaults.poweredOnFraction);
        settings.poweredOnlyOnFraction = unit(settings.poweredOnlyOnFraction, defaults.poweredOnlyOnFraction);
        if (settings.keyframedPoweredBlendOnFraction > 1.0f) {
            if (settings.poweredOnFraction >= 1.0f) {
                settings.poweredOnFraction = defaults.poweredOnFraction;
            }
            if (settings.poweredOnlyOnFraction >= 1.0f) {
                settings.poweredOnlyOnFraction = defaults.poweredOnlyOnFraction;
            }
        }
        settings.snapBlendSeconds = nonNegative(settings.snapBlendSeconds, defaults.snapBlendSeconds);
        settings.hierarchyGain = unit(settings.hierarchyGain, defaults.hierarchyGain);
        settings.velocityDamping = unit(settings.velocityDamping, defaults.velocityDamping);
        settings.accelerationGain = unit(settings.accelerationGain, defaults.accelerationGain);
        settings.velocityGain = unit(settings.velocityGain, defaults.velocityGain);
        settings.positionGain = unit(settings.positionGain, defaults.positionGain);
        settings.positionMaxLinearVelocity =
            nonNegative(settings.positionMaxLinearVelocity, defaults.positionMaxLinearVelocity);
        settings.positionMaxAngularVelocity =
            nonNegative(settings.positionMaxAngularVelocity, defaults.positionMaxAngularVelocity);
        settings.snapGain = unit(settings.snapGain, defaults.snapGain);
        settings.snapMaxLinearVelocity = nonNegative(settings.snapMaxLinearVelocity, defaults.snapMaxLinearVelocity);
        settings.snapMaxAngularVelocity = nonNegative(settings.snapMaxAngularVelocity, defaults.snapMaxAngularVelocity);
        settings.snapMaxLinearDistance = nonNegative(settings.snapMaxLinearDistance, defaults.snapMaxLinearDistance);
        settings.snapMaxAngularDistance = nonNegative(settings.snapMaxAngularDistance, defaults.snapMaxAngularDistance);
        settings.poweredMaxForce = nonNegative(settings.poweredMaxForce, defaults.poweredMaxForce);
        settings.poweredTau = unit(settings.poweredTau, defaults.poweredTau);
        settings.poweredDamping = unit(settings.poweredDamping, defaults.poweredDamping);
        settings.poweredProportionalRecoveryVelocity = nonNegative(
            settings.poweredProportionalRecoveryVelocity, defaults.poweredProportionalRecoveryVelocity);
        settings.poweredConstantRecoveryVelocity = nonNegative(
            settings.poweredConstantRecoveryVelocity, defaults.poweredConstantRecoveryVelocity);
        return settings;
    }

    MotorInspectionResult inspectDriveToPoseMotorState(void* const generatorOutput) noexcept
    {
        MotorInspectionResult result{};
        auto keyframed = viewTrack(
            generatorOutput,
            KeyframedRagdollControlTrack,
            static_cast<std::uint16_t>(Addresses::Layouts::Animation::KeyframedControlData),
            TrackAccess::StorageRead);
        result.tracksResolved = keyframed.trackCount != 0;
        result.trackCount = static_cast<std::uint16_t>((std::min)(
            keyframed.trackCount,
            static_cast<std::uint32_t>((std::numeric_limits<std::uint16_t>::max)())));
        if (keyframed.header) {
            result.initialKeyframedOnFraction = keyframed.header->onFraction;
            result.keyframedCapacity = positive16(keyframed.header->capacity);
            result.keyframedNumData = positive16(keyframed.header->numData);
            result.keyframedTrackUsable = paletteUsable(keyframed);
        }

        auto powered = viewTrack(
            generatorOutput,
            PoweredRagdollControlTrack,
            static_cast<std::uint16_t>(Addresses::Layouts::Animation::PoweredControlData),
            TrackAccess::StorageRead);
        if (powered.header) {
            result.initialPoweredOnFraction = powered.header->onFraction;
            result.poweredCapacity = positive16(powered.header->capacity);
            result.poweredNumData = positive16(powered.header->numData);
            result.poweredTrackUsable = paletteUsable(powered);
            if (result.poweredTrackUsable) {
                inspectPoweredForces(powered, result);
            }
        }

        auto mode = viewTrack(
            generatorOutput,
            PoweredWorldFromModelModeTrack,
            static_cast<std::uint16_t>(Addresses::Layouts::Animation::WorldFromModelModeData),
            TrackAccess::StorageRead);
        result.worldFromModelModeTrackUsable = static_cast<bool>(mode);
        if (result.worldFromModelModeTrackUsable) {
            result.worldFromModelMode = readWorldFromModelMode(mode);
            result.computingWorldFromModel = result.worldFromModelMode == WorldFromModelMode::Compute;
            result.usingRootBoneAsWorldFromModel = result.worldFromModelMode == WorldFromModelMode::UseRootBone;
        }

        result.initiallyPoweredOnly = result.keyframedTrackUsable && result.poweredTrackUsable &&
                                      result.initialKeyframedOnFraction <= 0.0f &&
                                      result.initialPoweredOnFraction > 0.0f;
        result.poweredOnlyAllNoForce = result.initiallyPoweredOnly && result.poweredAllNoForce;
        return result;
    }

    MotorApplyResult applyDriveToPoseMotorControl(
        void* const generatorOutput,
        const MotorControlSettings& requestedSettings,
        const float activeDriveSeconds,
        const std::uint32_t ragdollBodyCount,
        const bool suppressPoweredControlsForOneFrame) noexcept
    {
        MotorApplyResult result{};
        const auto initial = inspectDriveToPoseMotorState(generatorOutput);
        const auto settings = sanitizeMotorControlSettings(requestedSettings);
        result.tracksResolved = initial.tracksResolved;
        result.trackCount = initial.trackCount;
        result.initialKeyframedOnFraction = initial.initialKeyframedOnFraction;
        result.initialPoweredOnFraction = initial.initialPoweredOnFraction;
        result.initiallyPoweredOnly = initial.initiallyPoweredOnly;
        result.snapActive = std::isfinite(activeDriveSeconds) && activeDriveSeconds >= 0.0f &&
                            activeDriveSeconds <= settings.snapBlendSeconds;
        if (!result.tracksResolved) {
            return result;
        }

        auto keyframed = viewTrack(
            generatorOutput,
            KeyframedRagdollControlTrack,
            static_cast<std::uint16_t>(Addresses::Layouts::Animation::KeyframedControlData),
            TrackAccess::MutableStorage);
        if (keyframed.header) {
            result.keyframedElementSize = positive16(keyframed.header->elementSizeBytes);
            result.keyframedCapacity = positive16(keyframed.header->capacity);
            result.keyframedTrackUsable = paletteUsable(keyframed) && keyframed.mutableStorage();
            if (result.keyframedTrackUsable) {
                if (settings.forceKeyframedControls &&
                    (keyframed.header->onFraction <= 0.0f || keyframed.header->numData <= 0) &&
                    writeDefaultKeyframedControl(keyframed)) {
                    forcePaletteIndexZero(keyframed, settings.keyframedOnFraction);
                    result.keyframedForced = true;
                }
                if (settings.tuneKeyframedControls && keyframed.header->onFraction > 0.0f &&
                    keyframed.header->numData > 0 && tuneKeyframedControls(keyframed, settings, result.snapActive)) {
                    result.keyframedTuned = true;
                }
                result.keyframedNumData = positive16(keyframed.header->numData);
            }
            result.keyframedOnFraction = keyframed.header->onFraction;
        }

        auto bones = viewTrack(
            generatorOutput,
            KeyframedRagdollBonesTrack,
            static_cast<std::uint16_t>(sizeof(float)),
            TrackAccess::MutableStorage);
        if (bones.header) {
            result.keyframedBonesElementSize = positive16(bones.header->elementSizeBytes);
            result.keyframedBonesCapacity = positive16(bones.header->capacity);
            result.keyframedBonesOnFraction = bones.header->onFraction;
            result.keyframedBonesAppliedCount = keyframedBoneCount(bones, ragdollBodyCount);
            result.keyframedBonesTrackUsable = bones.mutableStorage() && result.keyframedBonesAppliedCount > 0 &&
                                               bones.header->elementSizeBytes == sizeof(float);
            if (result.keyframedBonesTrackUsable && settings.forceKeyframedBones) {
                bool written = true;
                for (std::size_t index = 0; index < result.keyframedBonesAppliedCount; ++index) {
                    written = Memory::write(element(bones, index), settings.keyframedBonesOnFraction) && written;
                }
                if (written) {
                    bones.header->numData = static_cast<std::int16_t>(result.keyframedBonesAppliedCount);
                    bones.header->onFraction = settings.keyframedBonesOnFraction;
                    result.keyframedBonesForced = true;
                }
            }
            result.keyframedBonesNumData = positive16(bones.header->numData);
            result.keyframedBonesOnFraction = bones.header->onFraction;
        }

        auto powered = viewTrack(
            generatorOutput,
            PoweredRagdollControlTrack,
            static_cast<std::uint16_t>(Addresses::Layouts::Animation::PoweredControlData),
            TrackAccess::MutableStorage);
        if (powered.header) {
            result.poweredElementSize = positive16(powered.header->elementSizeBytes);
            result.poweredCapacity = positive16(powered.header->capacity);
            result.poweredTrackUsable = paletteUsable(powered) && powered.mutableStorage();
            if (result.poweredTrackUsable) {
                if (settings.forcePoweredControls &&
                    (powered.header->onFraction <= 0.0f || powered.header->numData <= 0) &&
                    writeDefaultPoweredControl(powered)) {
                    forcePaletteIndexZero(powered, settings.poweredOnFraction);
                    result.poweredForced = true;
                }
                if (suppressPoweredControlsForOneFrame && powered.header->onFraction > 0.0f) {
                    powered.header->onFraction = 0.0f;
                    result.poweredSuppressedForOneFrame = true;
                }
                if (settings.forcePoweredControls && powered.header->onFraction > 0.0f && powered.header->numData > 0 &&
                    std::fabs(powered.header->onFraction - settings.poweredOnFraction) > 0.0001f) {
                    powered.header->onFraction = settings.poweredOnFraction;
                    result.poweredOnFractionClamped = true;
                }
                if (settings.tunePoweredControls && powered.header->onFraction > 0.0f &&
                    powered.header->numData > 0 && tunePoweredControls(powered, settings)) {
                    result.poweredTuned = true;
                }
                result.poweredNumData = positive16(powered.header->numData);
            }
            result.poweredOnFraction = powered.header->onFraction;
        }

        if (result.keyframedTrackUsable && result.poweredTrackUsable && keyframed.header && powered.header &&
            keyframed.header->onFraction > 0.0f && powered.header->onFraction > 0.0f &&
            keyframed.header->onFraction < settings.keyframedPoweredBlendOnFraction) {
            keyframed.header->onFraction = settings.keyframedPoweredBlendOnFraction;
            result.keyframedPoweredBlendRaised = true;
        }
        if (keyframed.header) {
            result.keyframedOnFraction = keyframed.header->onFraction;
        }
        if (powered.header) {
            result.poweredOnFraction = powered.header->onFraction;
        }
        return result;
    }
}
