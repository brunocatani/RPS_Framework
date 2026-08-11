#include "RPS/Runtime/BodyGravity.h"

#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/Memory.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <cmath>
#include <cstdint>
#include <limits>

namespace RPS::Runtime::Physics
{
    namespace
    {
        using SetBodyGravityFactorFunction = void (*)(void*, std::uint32_t, float);

        [[nodiscard]] bool indexedAddress(
            const std::uintptr_t base,
            const std::uint16_t index,
            const std::size_t stride,
            std::uintptr_t& result) noexcept
        {
            const auto maximum = (std::numeric_limits<std::uintptr_t>::max)();
            if (base == 0 || index > maximum / stride) {
                return false;
            }
            const auto offset = static_cast<std::uintptr_t>(index) * stride;
            if (base > maximum - offset) {
                return false;
            }
            result = base + offset;
            return true;
        }

        [[nodiscard]] bool invokeGravitySetter(
            const SetBodyGravityFactorFunction function,
            void* const world,
            const BodyId bodyId,
            const float gravityFactor) noexcept
        {
            if (!function) {
                return false;
            }
#if defined(_MSC_VER)
            __try {
                function(world, bodyId.value, gravityFactor);
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
#else
            function(world, bodyId.value, gravityFactor);
            return true;
#endif
        }
    }

    bool validGravityFactor(const float gravityFactor) noexcept
    {
        // The native supports intentional zero and signed/custom factors. The
        // framework owns memory safety, not a consumer's gameplay policy.
        return std::isfinite(gravityFactor);
    }

    BodyGravitySnapshot snapshotBodyGravityDuringSafeEpoch(void* const hknpWorld, const BodyId bodyId) noexcept
    {
        using namespace Addresses::Layouts;

        BodyGravitySnapshot result{};
        result.bodyId = bodyId;
        const auto body = snapshotBodyDuringSafeEpoch(hknpWorld, bodyId);
        if (!body.valid || !body.motion.valid || body.motion.motionPropertiesId >= Havok::MaxUsableMotionIndex) {
            return result;
        }

        const auto world = reinterpret_cast<std::uintptr_t>(hknpWorld);
        std::uintptr_t library{};
        std::uintptr_t properties{};
        std::uintptr_t property{};
        if (!Memory::read(reinterpret_cast<const void*>(world + Havok::HknpWorld_MotionPropertiesLibrary), library) ||
            library == 0 ||
            !Memory::read(reinterpret_cast<const void*>(library + Havok::MotionPropertiesLibrary_Data), properties) ||
            !indexedAddress(properties, body.motion.motionPropertiesId, Havok::MotionProperties_Stride, property) ||
            !Memory::read(
                reinterpret_cast<const void*>(property + Havok::MotionProperties_GravityFactor),
                result.gravityFactor) ||
            !validGravityFactor(result.gravityFactor)) {
            return result;
        }

        result.motionIndex = body.body.motionIndex;
        result.motionPropertiesId = body.motion.motionPropertiesId;
        result.valid = true;
        return result;
    }

    std::string_view toString(const BodyGravityWriteStatus status) noexcept
    {
        switch (status) {
        case BodyGravityWriteStatus::Applied:
            return "applied";
        case BodyGravityWriteStatus::InvalidRuntime:
            return "invalid-runtime";
        case BodyGravityWriteStatus::InvalidWorld:
            return "invalid-world";
        case BodyGravityWriteStatus::InvalidBody:
            return "invalid-body";
        case BodyGravityWriteStatus::InvalidGravityFactor:
            return "invalid-gravity-factor";
        case BodyGravityWriteStatus::PhysicsStepActive:
            return "physics-step-active";
        case BodyGravityWriteStatus::PhysicsStepStateUnavailable:
            return "physics-step-state-unavailable";
        case BodyGravityWriteStatus::ReadAccessUnavailable:
            return "read-access-unavailable";
        case BodyGravityWriteStatus::BodyUnavailable:
            return "body-unavailable";
        case BodyGravityWriteStatus::NativeUnavailable:
            return "native-unavailable";
        case BodyGravityWriteStatus::NativeFault:
            return "native-fault";
        case BodyGravityWriteStatus::VerificationFailed:
            return "verification-failed";
        default:
            return "unknown";
        }
    }

    BodyGravitySnapshot BodyGravityApi::read(const WorldReadGuard& guard, const BodyId bodyId) const noexcept
    {
        return guard.owns(_world) ? snapshotBodyGravityDuringSafeEpoch(_world, bodyId) : BodyGravitySnapshot{};
    }

    BodyGravityWriteResult BodyGravityApi::setBodyFactor(
        const BodyId bodyId,
        const float gravityFactor) const noexcept
    {
        BodyGravityWriteResult result{};
        result.requestedGravityFactor = gravityFactor;
        if (!_module || !*_module) {
            return result;
        }
        if (!_world) {
            result.status = BodyGravityWriteStatus::InvalidWorld;
            return result;
        }
        if (!bodyId.valid()) {
            result.status = BodyGravityWriteStatus::InvalidBody;
            return result;
        }
        if (!validGravityFactor(gravityFactor)) {
            result.status = BodyGravityWriteStatus::InvalidGravityFactor;
            return result;
        }

        switch (currentThreadPhysicsStepState(*_module)) {
        case PhysicsStepState::Inside:
            result.status = BodyGravityWriteStatus::PhysicsStepActive;
            return result;
        case PhysicsStepState::Unknown:
            result.status = BodyGravityWriteStatus::PhysicsStepStateUnavailable;
            return result;
        case PhysicsStepState::Outside:
            break;
        }

        {
            WorldReadGuard guard{ *_module, _world };
            if (!guard.active()) {
                result.status = BodyGravityWriteStatus::ReadAccessUnavailable;
                return result;
            }
            result.before = read(guard, bodyId);
        }
        if (!result.before.valid) {
            result.status = BodyGravityWriteStatus::BodyUnavailable;
            return result;
        }

        const auto function =
            _module->resolveFunction<SetBodyGravityFactorFunction>(Addresses::Symbol::Character_SetWaterGravityFactor);
        if (!function) {
            result.status = BodyGravityWriteStatus::NativeUnavailable;
            return result;
        }
        result.invoked = true;
        if (!invokeGravitySetter(function, _world, bodyId, gravityFactor)) {
            result.status = BodyGravityWriteStatus::NativeFault;
            return result;
        }

        {
            WorldReadGuard guard{ *_module, _world };
            if (guard.active()) {
                result.after = read(guard, bodyId);
            }
        }
        if (!result.after.valid || result.after.bodyId != result.before.bodyId ||
            result.after.motionIndex != result.before.motionIndex ||
            std::fabs(result.after.gravityFactor - gravityFactor) > 0.0001f) {
            result.status = BodyGravityWriteStatus::VerificationFailed;
            return result;
        }

        result.status = BodyGravityWriteStatus::Applied;
        return result;
    }
}
