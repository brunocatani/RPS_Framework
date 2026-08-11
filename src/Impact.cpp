#include "RPS/Runtime/Impact.h"

#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/Memory.h"
#include "RPS/Runtime/NativeReference.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace RPS::Runtime::Physics
{
    namespace
    {
        namespace Layout = Addresses::Layouts::Impact;

        struct alignas(16) DamageImpactData
        {
            Vector4 location{};
            Vector4 normal{};
            Vector4 velocity{};
            void* collisionObject{};
        };
        static_assert(sizeof(DamageImpactData) == Layout::DamageImpactDataSize);
        static_assert(offsetof(DamageImpactData, location) == Layout::DamageImpactData_Location);
        static_assert(offsetof(DamageImpactData, normal) == Layout::DamageImpactData_Normal);
        static_assert(offsetof(DamageImpactData, velocity) == Layout::DamageImpactData_Velocity);
        static_assert(offsetof(DamageImpactData, collisionObject) == Layout::DamageImpactData_CollisionObject);

        using GetDamageForImpactFunction = float (*)(float, float);
        using GetCollisionObjectFunction = void* (*)(void*, std::uint32_t*);
        using HitDataLifecycleFunction = void (*)(void*);
        using InitializeImpactFunction = void (*)(void*, void*, void*, float, DamageImpactData*);
        using GetActorHandleFunction = std::uint32_t (*)(void*);
        using ActorHitMeFunction = void (*)(void*, void*);

        template <class Result, class Function, class... Arguments>
        [[nodiscard]] bool invoke(Result& result, const Function function, Arguments... arguments) noexcept
        {
            result = {};
            if (!function || !Memory::rangeHasAccess(
                    reinterpret_cast<const void*>(function), 1, Memory::Access::Execute)) {
                return false;
            }
#if defined(_MSC_VER)
            __try {
                result = function(arguments...);
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                result = {};
                return false;
            }
#else
            result = function(arguments...);
            return true;
#endif
        }

        template <class Function, class... Arguments>
        [[nodiscard]] bool invokeVoid(const Function function, Arguments... arguments) noexcept
        {
            if (!function || !Memory::rangeHasAccess(
                    reinterpret_cast<const void*>(function), 1, Memory::Access::Execute)) {
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

        [[nodiscard]] bool validEngineObject(const void* const object) noexcept
        {
            return Memory::rangeHasAccess(object, sizeof(void*), Memory::Access::Read);
        }

        [[nodiscard]] bool validDamageSettings(const ImpactDamageSettings& settings) noexcept
        {
            return std::isfinite(settings.multiplier) && settings.multiplier >= 0.0f &&
                   std::isfinite(settings.maximumDamage) && settings.maximumDamage >= 0.0f;
        }
    }

    ImpactGeometryResult preparePhysicsImpactGeometry(
        const PhysicsImpactContact& contact,
        const float havokToGameScale) noexcept
    {
        ImpactGeometryResult result{};
        if (!contact.sourceBodyId.valid() ||
            contact.reserved[0] != 0 || contact.reserved[1] != 0 || contact.reserved[2] != 0) {
            result.error = ImpactError::InvalidInput;
            return result;
        }
        if (!std::isfinite(havokToGameScale) || havokToGameScale <= 0.0f) {
            result.error = ImpactError::InvalidScale;
            return result;
        }
        if (!contact.pointHavok.finite() || !contact.normalHavok.finite() ||
            !contact.sourceVelocityHavok.finite()) {
            result.error = ImpactError::NonFiniteContact;
            return result;
        }

        const double normalLength = std::sqrt(
            static_cast<double>(contact.normalHavok.x) * contact.normalHavok.x +
            static_cast<double>(contact.normalHavok.y) * contact.normalHavok.y +
            static_cast<double>(contact.normalHavok.z) * contact.normalHavok.z);
        if (!std::isfinite(normalLength) || normalLength <= 0.000001) {
            result.error = ImpactError::InvalidNormal;
            return result;
        }
        const auto inverseNormalLength = static_cast<float>(1.0 / normalLength);
        result.geometry.pointGame = {
            contact.pointHavok.x * havokToGameScale,
            contact.pointHavok.y * havokToGameScale,
            contact.pointHavok.z * havokToGameScale,
            0.0f,
        };
        result.geometry.normal = {
            contact.normalHavok.x * inverseNormalLength,
            contact.normalHavok.y * inverseNormalLength,
            contact.normalHavok.z * inverseNormalLength,
            0.0f,
        };
        result.geometry.sourceVelocityGame = {
            contact.sourceVelocityHavok.x * havokToGameScale,
            contact.sourceVelocityHavok.y * havokToGameScale,
            contact.sourceVelocityHavok.z * havokToGameScale,
            0.0f,
        };
        if (!result.geometry.pointGame.finite() || !result.geometry.normal.finite() ||
            !result.geometry.sourceVelocityGame.finite()) {
            result.geometry = {};
            result.error = ImpactError::NonFiniteContact;
            return result;
        }
        result.error = ImpactError::None;
        return result;
    }

    ImpactDamageResult calculateImpactDamage(
        const RuntimeModule& module,
        const float mass,
        const float speedGame,
        const ImpactDamageSettings settings) noexcept
    {
        ImpactDamageResult result{};
        if (!std::isfinite(mass) || mass <= 0.0f || !std::isfinite(speedGame) || speedGame <= 0.0f ||
            !validDamageSettings(settings)) {
            result.error = ImpactError::InvalidInput;
            return result;
        }
        if (!module) {
            result.error = ImpactError::InvalidRuntime;
            return result;
        }
        const auto function = module.resolveFunction<GetDamageForImpactFunction>(
            Addresses::Symbol::Physics_GetDamageForImpact);
        if (!function) {
            result.error = ImpactError::FunctionUnavailable;
            return result;
        }
        if (!invoke(result.nativeDamage, function, mass, speedGame)) {
            result.error = ImpactError::NativeCallFailed;
            return result;
        }
        if (!std::isfinite(result.nativeDamage)) {
            result.error = ImpactError::InvalidNativeOutput;
            return result;
        }
        result.invoked = true;
        if (result.nativeDamage <= 0.0f || settings.multiplier == 0.0f) {
            result.error = ImpactError::NoDamage;
            return result;
        }
        result.damage = result.nativeDamage * settings.multiplier;
        if (settings.maximumDamage > 0.0f) {
            result.damage = (std::min)(result.damage, settings.maximumDamage);
        }
        if (!std::isfinite(result.damage)) {
            result.damage = 0.0f;
            result.error = ImpactError::InvalidNativeOutput;
            return result;
        }
        if (result.damage <= 0.0f) {
            result.error = ImpactError::NoDamage;
            return result;
        }
        result.error = ImpactError::None;
        return result;
    }

    PhysicsHitResult deliverPhysicsHit(
        const RuntimeModule& module,
        const PhysicsHitRequest& request) noexcept
    {
        PhysicsHitResult result{};
        if (!module) {
            result.error = ImpactError::InvalidRuntime;
            return result;
        }
        if (!validEngineObject(request.bhkWorld) || !validEngineObject(request.targetActor) ||
            (request.aggressorActor && !validEngineObject(request.aggressorActor)) ||
            !std::isfinite(request.damage) || request.damage <= 0.0f) {
            result.error = ImpactError::InvalidInput;
            return result;
        }
        const auto geometry = preparePhysicsImpactGeometry(request.contact, request.havokToGameScale);
        result.geometry = geometry.geometry;
        if (!geometry) {
            result.error = geometry.error;
            return result;
        }

        const auto getCollisionObject = module.resolveFunction<GetCollisionObjectFunction>(
            Addresses::Symbol::Impact_GetCollisionObjectForBody);
        const auto hitDataCtor = module.resolveFunction<HitDataLifecycleFunction>(Addresses::Symbol::Impact_HitDataCtor);
        const auto hitDataDtor = module.resolveFunction<HitDataLifecycleFunction>(Addresses::Symbol::Impact_HitDataDtor);
        const auto initializeImpact = module.resolveFunction<InitializeImpactFunction>(
            Addresses::Symbol::Impact_HitDataInitialize);
        const auto getActorHandle = module.resolveFunction<GetActorHandleFunction>(Addresses::Symbol::Impact_GetActorHandle);
        const auto actorHitMe = module.resolveFunction<ActorHitMeFunction>(Addresses::Symbol::Impact_ActorHitMe);
        if (!getCollisionObject || !hitDataCtor || !hitDataDtor || !initializeImpact || !getActorHandle || !actorHitMe) {
            result.error = ImpactError::FunctionUnavailable;
            return result;
        }

        auto bodyId = request.contact.sourceBodyId.value;
        void* collisionObject{};
        if (!invoke(collisionObject, getCollisionObject, request.bhkWorld, &bodyId) || !collisionObject) {
            result.error = ImpactError::MissingSourceCollisionObject;
            return result;
        }
        result.sourceCollisionObject = reinterpret_cast<std::uintptr_t>(collisionObject);
        if (!addBethesdaReference(collisionObject)) {
            result.error = ImpactError::CollisionReferenceFailed;
            return result;
        }

        DamageImpactData impactData{
            result.geometry.pointGame,
            result.geometry.normal,
            result.geometry.sourceVelocityGame,
            collisionObject,
        };
        alignas(Layout::HitDataAlignment) std::array<std::byte, Layout::HitDataSize> hitDataStorage{};
        void* const hitData = hitDataStorage.data();

        if (!invokeVoid(hitDataCtor, hitData)) {
            result.error = ImpactError::HitDataConstructionFailed;
        } else {
            result.hitDataConstructed = true;
            if (!invokeVoid(initializeImpact, hitData, nullptr, request.targetActor, request.damage, &impactData)) {
                result.error = ImpactError::ImpactInitializationFailed;
            } else {
                bool aggressorReady = true;
                if (request.aggressorActor) {
                    std::uint32_t aggressorHandle{};
                    aggressorReady = invoke(aggressorHandle, getActorHandle, request.aggressorActor) &&
                                     aggressorHandle != 0 &&
                                     Memory::write(
                                         hitDataStorage.data() + Layout::HitData_AggressorHandle,
                                         aggressorHandle);
                }
                if (!aggressorReady) {
                    result.error = ImpactError::AggressorHandleFailed;
                } else if (!invokeVoid(actorHitMe, request.targetActor, hitData)) {
                    result.error = ImpactError::ActorDispatchFailed;
                } else {
                    result.applied = true;
                    result.error = ImpactError::None;
                }
            }
        }

        if (result.hitDataConstructed) {
            result.hitDataDestroyed = invokeVoid(hitDataDtor, hitData);
            if (!result.hitDataDestroyed) {
                result.error = ImpactError::HitDataDestructionFailed;
            }
        }
        result.collisionReferenceReleased = releaseBethesdaReference(collisionObject);
        if (!result.collisionReferenceReleased) {
            result.error = ImpactError::CollisionReferenceReleaseFailed;
        }
        return result;
    }
}
