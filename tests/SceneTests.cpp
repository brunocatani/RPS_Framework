#include "RPS/Runtime/Scene.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace
{
    [[nodiscard]] bool near(const float first, const float second) noexcept
    {
        return std::abs(first - second) <= 1.0e-5f;
    }

    [[nodiscard]] bool nearTransform(
        const RPS::Runtime::Scene::Transform& first,
        const RPS::Runtime::Scene::Transform& second) noexcept
    {
        for (std::size_t row = 0; row < 3; ++row) {
            for (std::size_t column = 0; column < 3; ++column) {
                if (!near(first.rotate.entry[row][column], second.rotate.entry[row][column])) {
                    return false;
                }
            }
        }
        return near(first.translate.x, second.translate.x) && near(first.translate.y, second.translate.y) &&
               near(first.translate.z, second.translate.z) && near(first.scale, second.scale);
    }
}

int main()
{
    using namespace RPS::Runtime;
    using namespace Scene;

    const Transform identity{};
    Transform desired{};
    desired.translate = { 4.0f, 6.0f, 8.0f };
    desired.scale = 3.0f;
    const auto identityLocal = worldToParentLocal(identity, desired);
    if (!identityLocal || !nearTransform(identityLocal.value, desired)) {
        std::cerr << "identity scene conversion failed\n";
        return 1;
    }

    Transform parent{};
    parent.rotate.entry[0][0] = 0.0f;
    parent.rotate.entry[0][1] = -1.0f;
    parent.rotate.entry[1][0] = 1.0f;
    parent.rotate.entry[1][1] = 0.0f;
    parent.translate = { 10.0f, 20.0f, 30.0f };
    parent.scale = 2.0f;

    desired.translate = { 14.0f, 26.0f, 38.0f };
    const auto local = worldToParentLocal(parent, desired);
    const auto roundTrip = local ? parentLocalToWorld(parent, local.value) : TransformResult{};
    if (!local || !near(local.value.translate.x, -3.0f) || !near(local.value.translate.y, 2.0f) ||
        !near(local.value.translate.z, 4.0f) || !near(local.value.scale, 1.5f) || !roundTrip ||
        !nearTransform(roundTrip.value, desired)) {
        std::cerr << "rotated/scaled scene conversion failed\n";
        return 1;
    }

    parent.scale = 0.0f;
    if (worldToParentLocal(parent, desired).status != TransformStatus::DegenerateParentScale) {
        std::cerr << "degenerate parent scale did not fail closed\n";
        return 1;
    }
    parent.scale = 1.0f;
    desired.translate.x = (std::numeric_limits<float>::quiet_NaN)();
    if (worldToParentLocal(parent, desired).status != TransformStatus::NonFiniteInput) {
        std::cerr << "non-finite scene input did not fail closed\n";
        return 1;
    }

    desired.translate.x = 14.0f;
    parent = Transform{};
    parent.rotate.entry[0][0] = 2.0f;
    if (worldToParentLocal(parent, desired).status != TransformStatus::InvalidParentRotation) {
        std::cerr << "non-orthonormal parent rotation did not fail closed\n";
        return 1;
    }
    parent = Transform{};
    parent.rotate.entry[2][2] = -1.0f;
    if (worldToParentLocal(parent, desired).status != TransformStatus::InvalidParentRotation) {
        std::cerr << "reflected parent rotation did not fail closed\n";
        return 1;
    }
    parent = Transform{};
    desired.rotate.entry[0][0] = 0.5f;
    if (worldToParentLocal(parent, desired).status != TransformStatus::InvalidInputRotation) {
        std::cerr << "non-orthonormal input rotation did not fail closed\n";
        return 1;
    }
    desired = Transform{};
    desired.scale = 0.0f;
    if (worldToParentLocal(parent, desired).status != TransformStatus::DegenerateInputScale) {
        std::cerr << "degenerate input scale did not fail closed\n";
        return 1;
    }

    const auto module = RuntimeModule::detect();
    int object{};
    ObjectApi api{ module, &object };
    const auto material = api.setMaterialNeedsUpdate(true);
    const auto culled = api.setAppCulled(false);
    const auto bound = api.updateWorldBound();
    if (module || material.status != ObjectCommandStatus::InvalidRuntime || material.invoked || material ||
        culled.status != ObjectCommandStatus::InvalidRuntime || culled.invoked || culled ||
        bound.status != ObjectCommandStatus::InvalidRuntime || bound.invoked || bound ||
        toString(ObjectCommandStatus::VtableUnavailable) != "vtable-unavailable" ||
        toString(TransformStatus::InvalidParentRotation) != "invalid-parent-rotation") {
        std::cerr << "scene object API did not fail closed without FO4VR\n";
        return 1;
    }

    return 0;
}
