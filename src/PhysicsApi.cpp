#include "RPS/Runtime/PhysicsApi.h"

#include "RPS/Addresses/Layouts.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <cmath>

namespace RPS::Runtime::Physics
{
    namespace
    {
        template <class Function, class... Arguments>
        [[nodiscard]] bool invokeNative(const Function function, Arguments... arguments) noexcept
        {
            if (!function) {
                return false;
            }
#if defined(_MSC_VER)
            __try {
                function(arguments...);
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
#else
            function(arguments...);
            return true;
#endif
        }
    }

    BodySnapshot Api::snapshot(const WorldReadGuard& guard, const BodyId bodyId) const noexcept
    {
        return guard.owns(_world) ? snapshotBodyDuringSafeEpoch(_world, bodyId) : BodySnapshot{};
    }

    bool Api::canMutate(const WorldWriteGuard& guard, const BodyId bodyId) const noexcept
    {
        return _module && *_module && _world && guard.owns(_world) && bodyId.valid() &&
               snapshotBodyDuringSafeEpoch(_world, bodyId).valid;
    }

    bool Api::setCollisionFilterInfo(
        const WorldWriteGuard& guard,
        const BodyId bodyId,
        const std::uint32_t filterInfo,
        const std::uint32_t rebuildMode) const noexcept
    {
        if (!canMutate(guard, bodyId)) {
            return false;
        }
        using Function = void (*)(void*, std::uint32_t, std::uint32_t, std::uint32_t);
        return invokeNative(
            _module->resolveFunction<Function>(Addresses::Symbol::Physics_SetBodyCollisionFilterInfo),
            _world,
            bodyId.value,
            filterInfo,
            rebuildMode);
    }

    bool Api::setVelocityDeferred(
        const WorldWriteGuard& guard,
        const BodyId bodyId,
        const Vector4& linearVelocity,
        const Vector4& angularVelocity) const noexcept
    {
        const auto body = canMutate(guard, bodyId) ? snapshotBodyDuringSafeEpoch(_world, bodyId) : BodySnapshot{};
        if (!body.motion.valid || !linearVelocity.finite() || !angularVelocity.finite()) {
            return false;
        }
        using Function = void (*)(void*, std::uint32_t, const float*, const float*);
        return invokeNative(
            _module->resolveFunction<Function>(Addresses::Symbol::Physics_SetBodyVelocityDeferred),
            _world,
            bodyId.value,
            &linearVelocity.x,
            &angularVelocity.x);
    }

    bool Api::setTransformDeferred(
        const WorldWriteGuard& guard,
        const BodyId bodyId,
        const Transform& transform,
        const int mode) const noexcept
    {
        if (!canMutate(guard, bodyId) || !transform.finite()) {
            return false;
        }
        using Function = void (*)(void*, std::uint32_t, const float*, int);
        return invokeNative(
            _module->resolveFunction<Function>(Addresses::Symbol::Physics_SetBodyTransformDeferred),
            _world,
            bodyId.value,
            &transform.column0.x,
            mode);
    }

    bool Api::setKeyframed(const WorldWriteGuard& guard, const BodyId bodyId) const noexcept
    {
        if (!canMutate(guard, bodyId)) {
            return false;
        }
        using Function = void (*)(void*, std::uint32_t);
        return invokeNative(
            _module->resolveFunction<Function>(Addresses::Symbol::Physics_SetBodyKeyframed), _world, bodyId.value);
    }

    bool Api::activate(const WorldWriteGuard& guard, const BodyId bodyId) const noexcept
    {
        if (!canMutate(guard, bodyId)) {
            return false;
        }
        using Function = void (*)(void*, std::uint32_t);
        return invokeNative(_module->resolveFunction<Function>(Addresses::Symbol::Physics_ActivateBody), _world, bodyId.value);
    }

    bool Api::enableFlags(
        const WorldWriteGuard& guard,
        const BodyId bodyId,
        const std::uint32_t flags,
        const std::uint32_t mode) const noexcept
    {
        if (!canMutate(guard, bodyId) || flags == 0) {
            return false;
        }
        using Function = void (*)(void*, std::uint32_t, std::uint32_t, std::uint32_t);
        return invokeNative(
            _module->resolveFunction<Function>(Addresses::Symbol::Physics_EnableBodyFlags),
            _world,
            bodyId.value,
            flags,
            mode);
    }

    bool Api::disableFlags(
        const WorldWriteGuard& guard,
        const BodyId bodyId,
        const std::uint32_t flags,
        const std::uint32_t mode) const noexcept
    {
        if (!canMutate(guard, bodyId) || flags == 0) {
            return false;
        }
        using Function = void (*)(void*, std::uint32_t, std::uint32_t, std::uint32_t);
        return invokeNative(
            _module->resolveFunction<Function>(Addresses::Symbol::Physics_DisableBodyFlags),
            _world,
            bodyId.value,
            flags,
            mode);
    }

    bool Api::rebuildMotionMassProperties(
        const WorldWriteGuard& guard,
        const std::uint32_t motionIndex,
        const int rebuildMode) const noexcept
    {
        if (!_module || !*_module || !_world || !guard.owns(_world) || motionIndex == 0 ||
            motionIndex >= Addresses::Layouts::Havok::MaxUsableMotionIndex) {
            return false;
        }
        using Function = void (*)(void*, std::uint32_t, int);
        return invokeNative(
            _module->resolveFunction<Function>(Addresses::Symbol::Physics_RebuildMotionMassProperties),
            _world,
            motionIndex,
            rebuildMode);
    }

    bool Api::applyImpulseAt(
        const WorldWriteGuard& guard,
        const BodyId bodyId,
        const Vector4& impulse,
        const Vector4& worldPoint) const noexcept
    {
        const auto body = canMutate(guard, bodyId) ? snapshotBodyDuringSafeEpoch(_world, bodyId) : BodySnapshot{};
        if (!body.motion.valid || !impulse.finite() || !worldPoint.finite()) {
            return false;
        }
        auto mutableImpulse = impulse;
        auto mutablePoint = worldPoint;
        using Function = void (*)(void*, std::uint32_t, float*, float*);
        return invokeNative(
            _module->resolveFunction<Function>(Addresses::Symbol::Physics_ApplyBodyImpulseAt),
            _world,
            bodyId.value,
            &mutableImpulse.x,
            &mutablePoint.x);
    }

    bool Api::computeHardKeyFrame(
        const WorldWriteGuard& guard,
        const BodyId bodyId,
        const Vector4& targetPosition,
        const Vector4& targetRotation,
        const float deltaSeconds,
        Vector4& outLinearVelocity,
        Vector4& outAngularVelocity) const noexcept
    {
        outLinearVelocity = {};
        outAngularVelocity = {};
        const auto body = canMutate(guard, bodyId) ? snapshotBodyDuringSafeEpoch(_world, bodyId) : BodySnapshot{};
        if (!body.motion.valid || !targetPosition.finite() || !targetRotation.finite() || !std::isfinite(deltaSeconds) ||
            deltaSeconds <= 0.000001f || deltaSeconds > 0.25f) {
            return false;
        }

        auto mutablePosition = targetPosition;
        auto mutableRotation = targetRotation;
        using Function = void (*)(void*, std::uint32_t, float*, float*, float, float*, float*);
        const bool invoked = invokeNative(
            _module->resolveFunction<Function>(Addresses::Symbol::Physics_ComputeHardKeyFrame),
            _world,
            bodyId.value,
            &mutablePosition.x,
            &mutableRotation.x,
            deltaSeconds,
            &outLinearVelocity.x,
            &outAngularVelocity.x);
        if (!invoked || !outLinearVelocity.finite() || !outAngularVelocity.finite()) {
            outLinearVelocity = {};
            outAngularVelocity = {};
            return false;
        }
        return true;
    }
}
