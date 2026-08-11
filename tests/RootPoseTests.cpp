#include "RPS/Runtime/RootPose.h"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>

namespace
{
    [[nodiscard]] bool near(const float first, const float second) noexcept
    {
        return std::abs(first - second) <= 1.0e-4f;
    }
}

int main()
{
    using namespace RPS::Runtime;
    using namespace RPS::Runtime::Animation;

    auto anchor = identityHkQsTransform();
    anchor.translation[0] = 10.0f;
    anchor.translation[1] = 20.0f;
    anchor.translation[2] = 30.0f;
    const std::array offset{ 1.0f, 2.0f, 3.0f };
    const auto adjusted = applyBodyAnchorOffset(anchor, offset);
    if (!adjusted || !near(adjusted.transform.translation[0], 11.0f) ||
        !near(adjusted.transform.translation[1], 22.0f) ||
        !near(adjusted.transform.translation[2], 33.0f)) {
        std::cerr << "identity body-anchor adjustment failed\n";
        return 1;
    }

    anchor.rotation[2] = std::sqrt(0.5f);
    anchor.rotation[3] = std::sqrt(0.5f);
    const std::array xOffset{ 1.0f, 0.0f, 0.0f };
    const auto rotated = applyBodyAnchorOffset(anchor, xOffset);
    if (!rotated || !near(rotated.transform.translation[0], 10.0f) ||
        !near(rotated.transform.translation[1], 21.0f)) {
        std::cerr << "rotated body-anchor adjustment failed\n";
        return 1;
    }

    auto invalidQuaternion = identityHkQsTransform();
    invalidQuaternion.rotation[3] = 0.0f;
    if (applyBodyAnchorOffset(invalidQuaternion, xOffset).status != RootMathStatus::InvalidQuaternion) {
        std::cerr << "invalid anchor quaternion did not fail closed\n";
        return 1;
    }
    const std::array hugeOffset{ 11.0f, 0.0f, 0.0f };
    if (applyBodyAnchorOffset(identityHkQsTransform(), hugeOffset).status != RootMathStatus::OffsetTooLarge) {
        std::cerr << "oversized anchor offset did not fail closed\n";
        return 1;
    }

    auto animationAnchor = identityHkQsTransform();
    animationAnchor.translation[0] = 0.02f;
    animationAnchor.translation[1] = 0.01f;
    animationAnchor.translation[2] = 0.99f;
    auto actualAnchor = identityHkQsTransform();
    actualAnchor.translation[0] = 38.57f;
    actualAnchor.translation[1] = 55.19f;
    actualAnchor.translation[2] = -21.82f;
    const auto bias = computeRootTranslationBias(animationAnchor, actualAnchor);
    if (!bias || !near(bias.bias[0], 38.55f) || !near(bias.bias[1], 55.18f) ||
        !near(bias.bias[2], -22.81f) || !near(bias.separation, 0.0f)) {
        std::cerr << "root-space translation bias failed\n";
        return 1;
    }

    const std::array staleBias{ 1.0f, 2.0f, 3.0f };
    if (applyRootTranslationBias(animationAnchor, actualAnchor, staleBias, 3.0f).status !=
        RootMathStatus::SpaceMismatch) {
        std::cerr << "stale root-space bias was not rejected\n";
        return 1;
    }

    auto previous = identityHkQsTransform();
    auto actual = identityHkQsTransform();
    actual.translation[0] = 3.0f;
    actual.translation[1] = 4.0f;
    actual.translation[2] = 12.0f;
    actual.rotation[2] = std::sqrt(0.5f);
    actual.rotation[3] = std::sqrt(0.5f);
    const auto delta = sampleRootDelta(previous, actual);
    if (!delta || !near(delta.length, 13.0f) || !near(delta.horizontalLength, 5.0f) ||
        !near(delta.yawRadians, -1.5707963f)) {
        std::cerr << "root delta sampling failed\n";
        return 1;
    }

    animationAnchor.translation[0] = (std::numeric_limits<float>::quiet_NaN)();
    if (computeRootTranslationBias(animationAnchor, actualAnchor).status != RootMathStatus::NonFiniteTransform) {
        std::cerr << "non-finite root anchor did not fail closed\n";
        return 1;
    }

    const auto module = RuntimeModule::detect();
    RootPoseApi api{ module, nullptr, nullptr };
    std::array<std::int16_t, 4> parents{};
    std::array<HkQsTransform, 4> pose{};
    HkQsTransform transform{};
    if (module || api.copyLowSkeletonParents(parents).status != RootPoseStatus::InvalidRuntime ||
        api.readBodyAnchor().status != RootPoseStatus::InvalidRuntime ||
        api.mapHighToLow(pose, pose).status != RootPoseStatus::InvalidRuntime ||
        api.copyAndApplyScale(pose, pose, 1.0f).status != RootPoseStatus::InvalidRuntime ||
        api.copyAndScaleTransform(transform, transform).status != RootPoseStatus::InvalidRuntime ||
        api.localToWorld(parents, transform, pose, pose).status != RootPoseStatus::InvalidRuntime ||
        toString(RootPoseStatus::GenerationChanged) != "generation-changed" ||
        toString(RootMathStatus::SpaceMismatch) != "space-mismatch") {
        std::cerr << "root pose API did not fail closed without FO4VR\n";
        return 1;
    }

    return 0;
}
