#pragma once

#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/PhysicsTypes.h"
#include "RPS/Runtime/RuntimeModule.h"

#include <array>
#include <cstdint>

namespace RPS::Runtime::Physics
{
    enum class ImpactError : std::uint8_t
    {
        None,
        InvalidRuntime,
        InvalidInput,
        InvalidScale,
        NonFiniteContact,
        InvalidNormal,
        FunctionUnavailable,
        NativeCallFailed,
        InvalidNativeOutput,
        NoDamage,
        MissingSourceCollisionObject,
        CollisionReferenceFailed,
        HitDataConstructionFailed,
        ImpactInitializationFailed,
        AggressorHandleFailed,
        ActorDispatchFailed,
        HitDataDestructionFailed,
        CollisionReferenceReleaseFailed,
    };

    struct PhysicsImpactContact
    {
        BodyId sourceBodyId{};
        std::array<std::uint32_t, 3> reserved{};
        Vector4 pointHavok{};
        Vector4 normalHavok{};
        Vector4 sourceVelocityHavok{};
    };
    static_assert(sizeof(PhysicsImpactContact) == Addresses::Layouts::Impact::PhysicsImpactContactSize);

    struct PhysicsImpactGeometry
    {
        Vector4 pointGame{};
        Vector4 normal{};
        Vector4 sourceVelocityGame{};
    };

    struct ImpactGeometryResult
    {
        ImpactError error{ ImpactError::InvalidInput };
        PhysicsImpactGeometry geometry{};

        [[nodiscard]] explicit operator bool() const noexcept { return error == ImpactError::None; }
    };

    [[nodiscard]] ImpactGeometryResult preparePhysicsImpactGeometry(
        const PhysicsImpactContact& contact,
        float havokToGameScale) noexcept;

    struct ImpactDamageSettings
    {
        float multiplier{ 1.0f };
        // Zero leaves Bethesda's native result uncapped.
        float maximumDamage{};
    };

    struct ImpactDamageResult
    {
        ImpactError error{ ImpactError::InvalidInput };
        bool invoked{};
        float nativeDamage{};
        float damage{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return error == ImpactError::None && invoked && damage > 0.0f;
        }
    };

    [[nodiscard]] ImpactDamageResult calculateImpactDamage(
        const RuntimeModule& module,
        float mass,
        float speedGame,
        ImpactDamageSettings settings = {}) noexcept;

    struct PhysicsHitRequest
    {
        void* bhkWorld{};
        void* targetActor{};
        void* aggressorActor{};
        PhysicsImpactContact contact{};
        float damage{};
        float havokToGameScale{};
    };

    struct PhysicsHitResult
    {
        ImpactError error{ ImpactError::InvalidInput };
        bool applied{};
        bool hitDataConstructed{};
        bool hitDataDestroyed{};
        bool collisionReferenceReleased{};
        std::uintptr_t sourceCollisionObject{};
        PhysicsImpactGeometry geometry{};

        // Actor::HitMe may already have applied the hit even when cleanup later
        // reports an error, so truth reflects the irreversible side effect.
        [[nodiscard]] explicit operator bool() const noexcept { return applied; }
    };

    // This invokes Actor::HitMe and must run synchronously on the owning game
    // thread, never from a physics/contact callback. Actor, bhkWorld, and body
    // identity are borrowed for the duration of the call only.
    [[nodiscard]] PhysicsHitResult deliverPhysicsHit(
        const RuntimeModule& module,
        const PhysicsHitRequest& request) noexcept;
}
