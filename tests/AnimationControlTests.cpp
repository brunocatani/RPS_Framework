#include "RPS/Runtime/AnimationMotor.h"
#include "RPS/Runtime/AnimationPose.h"
#include "RPS/Runtime/GeneratorOutput.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>

namespace
{
    using namespace RPS::Runtime::Animation;

    constexpr std::size_t BlobBytes = 0x1000;

    struct alignas(16) Blob
    {
        std::array<std::byte, BlobBytes> bytes{};
        GeneratorOutput output{ bytes.data(), false };

        TrackHeader& track(const std::uint32_t id)
        {
            auto* headers = reinterpret_cast<TrackHeader*>(bytes.data() + sizeof(TrackMasterHeader));
            return headers[id];
        }

        std::byte* data(const std::uint32_t id)
        {
            return bytes.data() + track(id).dataOffset;
        }

        std::int8_t* indices(const std::uint32_t id)
        {
            auto& header = track(id);
            const auto rawBytes = static_cast<std::uint32_t>(header.elementSizeBytes) *
                                  static_cast<std::uint32_t>(header.capacity);
            const auto alignedBytes = (rawBytes + 15u) & ~15u;
            return reinterpret_cast<std::int8_t*>(data(id) + alignedBytes);
        }
    };

    void initialize(Blob& blob)
    {
        auto* master = reinterpret_cast<TrackMasterHeader*>(blob.bytes.data());
        master->numBytes = static_cast<std::int32_t>(blob.bytes.size());
        master->numTracks = 18;

        auto configure = [&blob](
                             const std::uint32_t id,
                             const std::int16_t offset,
                             const std::int16_t elementBytes,
                             const std::int16_t capacity,
                             const std::int16_t numData,
                             const float fraction,
                             const std::uint8_t flags = 0) {
            auto& header = blob.track(id);
            header.dataOffset = offset;
            header.elementSizeBytes = elementBytes;
            header.capacity = capacity;
            header.numData = numData;
            header.onFraction = fraction;
            header.flags = flags;
        };

        configure(WorldFromModelTrack, 0x200, 0x30, 1, 1, 1.0f);
        configure(PoseTrack, 0x240, 0x30, 2, 2, 1.0f);
        configure(KeyframedRagdollControlTrack, 0x300, 0x30, 1, 0, 0.0f, TrackPaletteFlag);
        configure(PoweredRagdollControlTrack, 0x380, 0x14, 1, 0, 0.0f, TrackPaletteFlag);
        configure(PoweredWorldFromModelModeTrack, 0x3C0, 0x08, 1, 1, 1.0f);
        configure(KeyframedRagdollBonesTrack, 0x3D0, sizeof(float), 4, 0, 0.0f);
    }

    HkQsTransform transform(const float translation, const float quaternionW = 1.0f)
    {
        HkQsTransform value{};
        value.translation[0] = translation;
        value.rotation[3] = quaternionW;
        value.scale[0] = 1.0f;
        value.scale[1] = 1.0f;
        value.scale[2] = 1.0f;
        value.scale[3] = 1.0f;
        return value;
    }

    bool near(const float lhs, const float rhs)
    {
        return std::fabs(lhs - rhs) < 0.0001f;
    }

    bool poseContracts()
    {
        Blob blob{};
        initialize(blob);
        const std::array source{ transform(2.0f), transform(4.0f) };
        auto copied = copyPoseToOutput(&blob.output, source);
        std::array<HkQsTransform, 2> captured{};
        auto read = capturePose(&blob.output, captured);
        if (!copied || !read || copied.count != 2 || read.count != 2 || !near(captured[1].translation[0], 4.0f)) {
            std::cerr << "pose copy/capture contract failed\n";
            return false;
        }

        const std::array target{ transform(10.0f, -1.0f), transform(20.0f, -1.0f) };
        const auto blended = blendPoseToOutput(&blob.output, source, target, 0.5f);
        read = capturePose(&blob.output, captured);
        if (!blended || !read || !near(captured[0].translation[0], 6.0f) ||
            !near(std::fabs(captured[0].rotation[3]), 1.0f)) {
            std::cerr << "pose shortest-hemisphere blend failed\n";
            return false;
        }

        std::array<HkQsTransform, 1> tooSmall{};
        if (capturePose(&blob.output, tooSmall).status != PoseStatus::TargetTooSmall) {
            std::cerr << "pose target bounds contract failed\n";
            return false;
        }
        auto invalid = source;
        invalid[0].translation[0] = (std::numeric_limits<float>::quiet_NaN)();
        if (copyPoseToOutput(&blob.output, invalid).status != PoseStatus::NonFiniteInput) {
            std::cerr << "pose finite-input contract failed\n";
            return false;
        }
        return true;
    }

    bool motorContracts()
    {
        Blob blob{};
        initialize(blob);
        const MotorControlSettings settings{};
        auto applied = applyDriveToPoseMotorControl(&blob.output, settings, 0.05f, 3);
        float hierarchy{};
        float poweredForce{};
        std::memcpy(&hierarchy, blob.data(KeyframedRagdollControlTrack), sizeof(float));
        std::memcpy(&poweredForce, blob.data(PoweredRagdollControlTrack), sizeof(float));
        if (!applied.tracksResolved || !applied.keyframedForced || !applied.poweredForced ||
            !applied.keyframedTuned || !applied.poweredTuned || !applied.keyframedPoweredBlendRaised ||
            applied.keyframedBonesForced || blob.indices(KeyframedRagdollControlTrack)[0] != 0 ||
            blob.indices(PoweredRagdollControlTrack)[0] != 0 || !near(hierarchy, settings.hierarchyGain) ||
            !near(poweredForce, settings.poweredMaxForce) ||
            !near(blob.track(KeyframedRagdollControlTrack).onFraction, settings.keyframedPoweredBlendOnFraction) ||
            !near(blob.track(PoweredRagdollControlTrack).onFraction, settings.poweredOnFraction)) {
            std::cerr << "motor force/tune contract failed\n";
            return false;
        }

        Blob wide{};
        initialize(wide);
        wide.track(KeyframedRagdollControlTrack).elementSizeBytes = 0x40;
        wide.track(PoweredRagdollControlTrack).elementSizeBytes = 0x20;
        std::fill(
            wide.data(KeyframedRagdollControlTrack),
            wide.data(KeyframedRagdollControlTrack) + 0x40,
            std::byte{ 0x7F });
        std::fill(
            wide.data(PoweredRagdollControlTrack),
            wide.data(PoweredRagdollControlTrack) + 0x20,
            std::byte{ 0x7F });
        applied = applyDriveToPoseMotorControl(&wide.output, settings, 1.0f, 3);
        const bool keyframedTailZero = std::all_of(
            wide.data(KeyframedRagdollControlTrack) + 0x30,
            wide.data(KeyframedRagdollControlTrack) + 0x40,
            [](const std::byte value) { return value == std::byte{}; });
        const bool poweredTailZero = std::all_of(
            wide.data(PoweredRagdollControlTrack) + 0x14,
            wide.data(PoweredRagdollControlTrack) + 0x20,
            [](const std::byte value) { return value == std::byte{}; });
        if (!applied.keyframedForced || !applied.poweredForced || !keyframedTailZero || !poweredTailZero) {
            std::cerr << "wide motor element initialization contract failed\n";
            return false;
        }

        auto forceBones = settings;
        forceBones.forceKeyframedBones = true;
        applied = applyDriveToPoseMotorControl(&blob.output, forceBones, 1.0f, 3);
        std::array<float, 4> bones{};
        std::memcpy(bones.data(), blob.data(KeyframedRagdollBonesTrack), sizeof(bones));
        if (!applied.keyframedBonesForced || applied.keyframedBonesAppliedCount != 3 ||
            blob.track(KeyframedRagdollBonesTrack).numData != 3 || !near(bones[2], settings.keyframedBonesOnFraction) ||
            !near(bones[3], 0.0f)) {
            std::cerr << "keyframed bone control contract failed\n";
            return false;
        }

        blob.track(KeyframedRagdollControlTrack).numData = 0;
        blob.track(KeyframedRagdollControlTrack).onFraction = 0.0f;
        blob.track(PoweredRagdollControlTrack).numData = 1;
        blob.track(PoweredRagdollControlTrack).onFraction = 1.0f;
        const float noForce{};
        std::memcpy(blob.data(PoweredRagdollControlTrack), &noForce, sizeof(noForce));
        blob.data(PoweredWorldFromModelModeTrack)[6] =
            static_cast<std::byte>(static_cast<std::uint8_t>(WorldFromModelMode::UseRootBone));
        const auto inspection = inspectDriveToPoseMotorState(&blob.output);
        if (!inspection.initiallyPoweredOnly || !inspection.poweredAllNoForce ||
            !inspection.poweredOnlyAllNoForce || !inspection.usingRootBoneAsWorldFromModel) {
            std::cerr << "motor inspection contract failed\n";
            return false;
        }

        blob.track(KeyframedRagdollControlTrack).numData = 1;
        blob.track(KeyframedRagdollControlTrack).onFraction = 1.0f;
        applied = applyDriveToPoseMotorControl(&blob.output, settings, 1.0f, 3, true);
        if (!applied.poweredSuppressedForOneFrame || !near(blob.track(PoweredRagdollControlTrack).onFraction, 0.0f)) {
            std::cerr << "one-frame powered suppression contract failed\n";
            return false;
        }

        auto invalid = settings;
        invalid.hierarchyGain = (std::numeric_limits<float>::quiet_NaN)();
        invalid.poweredMaxForce = -1.0f;
        const auto sanitized = sanitizeMotorControlSettings(invalid);
        if (!near(sanitized.hierarchyGain, settings.hierarchyGain) || !near(sanitized.poweredMaxForce, 0.0f)) {
            std::cerr << "motor sanitization contract failed\n";
            return false;
        }
        return true;
    }
}

int main()
{
    return poseContracts() && motorContracts() ? 0 : 1;
}
