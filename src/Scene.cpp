#include "RPS/Runtime/Scene.h"

#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/Memory.h"
#include "RPS/Runtime/WorldAccess.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <cmath>
#include <limits>

namespace RPS::Runtime::Scene
{
    namespace
    {
        inline constexpr float MinimumUsableScale = 1.0e-5f;

        using BooleanObjectCommand = void (*)(void*, bool);
        using UnaryObjectCommand = void (*)(void*);

        [[nodiscard]] bool invokeBoolean(
            const BooleanObjectCommand function,
            void* const object,
            const bool value) noexcept
        {
#if defined(_MSC_VER)
            __try {
                function(object, value);
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
#else
            function(object, value);
            return true;
#endif
        }

        [[nodiscard]] bool invokeUnary(
            const UnaryObjectCommand function,
            void* const object) noexcept
        {
#if defined(_MSC_VER)
            __try {
                function(object);
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
#else
            function(object);
            return true;
#endif
        }
    }

    bool Point3::finite() const noexcept
    {
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
    }

    bool Matrix3::finite() const noexcept
    {
        for (const auto& row : entry) {
            for (const auto value : row) {
                if (!std::isfinite(value)) {
                    return false;
                }
            }
        }
        return true;
    }

    bool Matrix3::properRotation(const float tolerance) const noexcept
    {
        if (!finite() || !std::isfinite(tolerance) || tolerance < 0.0f) {
            return false;
        }

        for (std::size_t row = 0; row < 3; ++row) {
            float lengthSquared{};
            for (std::size_t column = 0; column < 3; ++column) {
                lengthSquared += entry[row][column] * entry[row][column];
            }
            if (std::abs(lengthSquared - 1.0f) > tolerance) {
                return false;
            }
            for (std::size_t other = row + 1; other < 3; ++other) {
                float dot{};
                for (std::size_t column = 0; column < 3; ++column) {
                    dot += entry[row][column] * entry[other][column];
                }
                if (std::abs(dot) > tolerance) {
                    return false;
                }
            }
        }

        const auto determinant =
            entry[0][0] * (entry[1][1] * entry[2][2] - entry[1][2] * entry[2][1]) -
            entry[0][1] * (entry[1][0] * entry[2][2] - entry[1][2] * entry[2][0]) +
            entry[0][2] * (entry[1][0] * entry[2][1] - entry[1][1] * entry[2][0]);
        return std::isfinite(determinant) && std::abs(determinant - 1.0f) <= tolerance;
    }

    bool Transform::finite() const noexcept
    {
        return rotate.finite() && translate.finite() && std::isfinite(scale);
    }

    TransformResult worldToParentLocal(
        const Transform& parentWorld,
        const Transform& desiredWorld) noexcept
    {
        TransformResult result{};
        if (!parentWorld.rotate.finite() || !parentWorld.translate.finite() || !std::isfinite(parentWorld.scale)) {
            result.status = TransformStatus::NonFiniteParent;
            return result;
        }
        if (std::abs(parentWorld.scale) <= MinimumUsableScale) {
            result.status = TransformStatus::DegenerateParentScale;
            return result;
        }
        if (!parentWorld.rotate.properRotation()) {
            result.status = TransformStatus::InvalidParentRotation;
            return result;
        }
        if (!desiredWorld.finite()) {
            result.status = TransformStatus::NonFiniteInput;
            return result;
        }
        if (std::abs(desiredWorld.scale) <= MinimumUsableScale) {
            result.status = TransformStatus::DegenerateInputScale;
            return result;
        }
        if (!desiredWorld.rotate.properRotation()) {
            result.status = TransformStatus::InvalidInputRotation;
            return result;
        }

        const auto inverseScale = 1.0f / parentWorld.scale;
        const Point3 offset{
            desiredWorld.translate.x - parentWorld.translate.x,
            desiredWorld.translate.y - parentWorld.translate.y,
            desiredWorld.translate.z - parentWorld.translate.z,
        };
        result.value.translate = {
            (parentWorld.rotate.entry[0][0] * offset.x + parentWorld.rotate.entry[0][1] * offset.y +
             parentWorld.rotate.entry[0][2] * offset.z) * inverseScale,
            (parentWorld.rotate.entry[1][0] * offset.x + parentWorld.rotate.entry[1][1] * offset.y +
             parentWorld.rotate.entry[1][2] * offset.z) * inverseScale,
            (parentWorld.rotate.entry[2][0] * offset.x + parentWorld.rotate.entry[2][1] * offset.y +
             parentWorld.rotate.entry[2][2] * offset.z) * inverseScale,
        };
        for (std::size_t row = 0; row < 3; ++row) {
            for (std::size_t column = 0; column < 3; ++column) {
                result.value.rotate.entry[row][column] =
                    desiredWorld.rotate.entry[row][0] * parentWorld.rotate.entry[column][0] +
                    desiredWorld.rotate.entry[row][1] * parentWorld.rotate.entry[column][1] +
                    desiredWorld.rotate.entry[row][2] * parentWorld.rotate.entry[column][2];
            }
        }
        result.value.scale = desiredWorld.scale * inverseScale;
        if (!result.value.finite()) {
            result.status = TransformStatus::NonFiniteResult;
        } else if (std::abs(result.value.scale) <= MinimumUsableScale) {
            result.status = TransformStatus::DegenerateResultScale;
        } else if (!result.value.rotate.properRotation()) {
            result.status = TransformStatus::InvalidResultRotation;
        } else {
            result.status = TransformStatus::Converted;
        }
        return result;
    }

    TransformResult parentLocalToWorld(
        const Transform& parentWorld,
        const Transform& local) noexcept
    {
        TransformResult result{};
        if (!parentWorld.rotate.finite() || !parentWorld.translate.finite() || !std::isfinite(parentWorld.scale)) {
            result.status = TransformStatus::NonFiniteParent;
            return result;
        }
        if (std::abs(parentWorld.scale) <= MinimumUsableScale) {
            result.status = TransformStatus::DegenerateParentScale;
            return result;
        }
        if (!parentWorld.rotate.properRotation()) {
            result.status = TransformStatus::InvalidParentRotation;
            return result;
        }
        if (!local.finite()) {
            result.status = TransformStatus::NonFiniteInput;
            return result;
        }
        if (std::abs(local.scale) <= MinimumUsableScale) {
            result.status = TransformStatus::DegenerateInputScale;
            return result;
        }
        if (!local.rotate.properRotation()) {
            result.status = TransformStatus::InvalidInputRotation;
            return result;
        }

        const Point3 scaledLocal{
            local.translate.x * parentWorld.scale,
            local.translate.y * parentWorld.scale,
            local.translate.z * parentWorld.scale,
        };
        result.value.translate = {
            parentWorld.translate.x + parentWorld.rotate.entry[0][0] * scaledLocal.x +
                parentWorld.rotate.entry[1][0] * scaledLocal.y +
                parentWorld.rotate.entry[2][0] * scaledLocal.z,
            parentWorld.translate.y + parentWorld.rotate.entry[0][1] * scaledLocal.x +
                parentWorld.rotate.entry[1][1] * scaledLocal.y +
                parentWorld.rotate.entry[2][1] * scaledLocal.z,
            parentWorld.translate.z + parentWorld.rotate.entry[0][2] * scaledLocal.x +
                parentWorld.rotate.entry[1][2] * scaledLocal.y +
                parentWorld.rotate.entry[2][2] * scaledLocal.z,
        };
        for (std::size_t row = 0; row < 3; ++row) {
            for (std::size_t column = 0; column < 3; ++column) {
                result.value.rotate.entry[row][column] =
                    local.rotate.entry[row][0] * parentWorld.rotate.entry[0][column] +
                    local.rotate.entry[row][1] * parentWorld.rotate.entry[1][column] +
                    local.rotate.entry[row][2] * parentWorld.rotate.entry[2][column];
            }
        }
        result.value.scale = local.scale * parentWorld.scale;
        if (!result.value.finite()) {
            result.status = TransformStatus::NonFiniteResult;
        } else if (std::abs(result.value.scale) <= MinimumUsableScale) {
            result.status = TransformStatus::DegenerateResultScale;
        } else if (!result.value.rotate.properRotation()) {
            result.status = TransformStatus::InvalidResultRotation;
        } else {
            result.status = TransformStatus::Converted;
        }
        return result;
    }

    std::string_view toString(const TransformStatus status) noexcept
    {
        switch (status) {
        case TransformStatus::Converted:
            return "converted";
        case TransformStatus::NonFiniteParent:
            return "non-finite-parent";
        case TransformStatus::DegenerateParentScale:
            return "degenerate-parent-scale";
        case TransformStatus::InvalidParentRotation:
            return "invalid-parent-rotation";
        case TransformStatus::NonFiniteInput:
            return "non-finite-input";
        case TransformStatus::DegenerateInputScale:
            return "degenerate-input-scale";
        case TransformStatus::InvalidInputRotation:
            return "invalid-input-rotation";
        case TransformStatus::NonFiniteResult:
            return "non-finite-result";
        case TransformStatus::DegenerateResultScale:
            return "degenerate-result-scale";
        case TransformStatus::InvalidResultRotation:
            return "invalid-result-rotation";
        default:
            return "unknown";
        }
    }

    std::string_view toString(const ObjectCommandStatus status) noexcept
    {
        switch (status) {
        case ObjectCommandStatus::Completed:
            return "completed";
        case ObjectCommandStatus::InvalidRuntime:
            return "invalid-runtime";
        case ObjectCommandStatus::InvalidObject:
            return "invalid-object";
        case ObjectCommandStatus::PhysicsStepActive:
            return "physics-step-active";
        case ObjectCommandStatus::PhysicsStepStateUnavailable:
            return "physics-step-state-unavailable";
        case ObjectCommandStatus::VtableUnavailable:
            return "vtable-unavailable";
        case ObjectCommandStatus::NativeFault:
            return "native-fault";
        default:
            return "unknown";
        }
    }

    bool ObjectApi::executionContextValid(ObjectCommandResult& result) const noexcept
    {
        result.objectAddress = reinterpret_cast<std::uintptr_t>(_object);
        if (!_module || !*_module) {
            result.status = ObjectCommandStatus::InvalidRuntime;
            return false;
        }
        if (!Memory::rangeHasAccess(
                _object,
                Addresses::Layouts::Scene::NiAVObject_MinimumReadableSize,
                Memory::Access::Read)) {
            result.status = ObjectCommandStatus::InvalidObject;
            return false;
        }
        switch (Physics::currentThreadPhysicsStepState(*_module)) {
        case Physics::PhysicsStepState::Inside:
            result.status = ObjectCommandStatus::PhysicsStepActive;
            return false;
        case Physics::PhysicsStepState::Unknown:
            result.status = ObjectCommandStatus::PhysicsStepStateUnavailable;
            return false;
        case Physics::PhysicsStepState::Outside:
            return true;
        }
        result.status = ObjectCommandStatus::PhysicsStepStateUnavailable;
        return false;
    }

    bool ObjectApi::resolveVirtual(const std::size_t index, ObjectCommandResult& result) const noexcept
    {
        if (!executionContextValid(result) || index > (std::numeric_limits<std::uintptr_t>::max)() / sizeof(void*)) {
            return false;
        }
        if (!Memory::read(_object, result.vtableAddress) || result.vtableAddress == 0) {
            result.status = ObjectCommandStatus::VtableUnavailable;
            return false;
        }
        const auto slotAddress = result.vtableAddress + index * sizeof(void*);
        if (!Memory::read(reinterpret_cast<const void*>(slotAddress), result.functionAddress) ||
            result.functionAddress == 0 ||
            !Memory::rangeHasAccess(
                reinterpret_cast<const void*>(result.functionAddress),
                1,
                Memory::Access::Execute)) {
            result.status = ObjectCommandStatus::VtableUnavailable;
            return false;
        }
        return true;
    }

    ObjectCommandResult ObjectApi::setMaterialNeedsUpdate(const bool value) const noexcept
    {
        ObjectCommandResult result{};
        if (!resolveVirtual(Addresses::Layouts::Scene::NiAVObject_SetMaterialNeedsUpdateVtableIndex, result)) {
            return result;
        }
        result.invoked = true;
        result.status = invokeBoolean(
                            reinterpret_cast<BooleanObjectCommand>(result.functionAddress),
                            _object,
                            value) ?
            ObjectCommandStatus::Completed :
            ObjectCommandStatus::NativeFault;
        return result;
    }

    ObjectCommandResult ObjectApi::setAppCulled(const bool value) const noexcept
    {
        ObjectCommandResult result{};
        if (!resolveVirtual(Addresses::Layouts::Scene::NiAVObject_SetAppCulledVtableIndex, result)) {
            return result;
        }
        result.invoked = true;
        result.status = invokeBoolean(
                            reinterpret_cast<BooleanObjectCommand>(result.functionAddress),
                            _object,
                            value) ?
            ObjectCommandStatus::Completed :
            ObjectCommandStatus::NativeFault;
        return result;
    }

    ObjectCommandResult ObjectApi::updateWorldBound() const noexcept
    {
        ObjectCommandResult result{};
        if (!resolveVirtual(Addresses::Layouts::Scene::NiAVObject_UpdateWorldBoundVtableIndex, result)) {
            return result;
        }
        result.invoked = true;
        result.status = invokeUnary(
                            reinterpret_cast<UnaryObjectCommand>(result.functionAddress),
                            _object) ?
            ObjectCommandStatus::Completed :
            ObjectCommandStatus::NativeFault;
        return result;
    }
}
