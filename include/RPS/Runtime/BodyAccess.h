#pragma once

#include "RPS/Runtime/PhysicsTypes.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace RPS::Runtime::Physics
{
    struct BodyState
    {
        Transform transform{};
        std::uint32_t flags{};
        std::uint32_t collisionFilterInfo{};
        std::uintptr_t shapeAddress{};
        BodyId bodyId{};
        std::uint32_t nextAttachedBodyId{};
        std::uint32_t motionIndex{};
        std::int32_t deactivationIslandId{};
        std::uint16_t materialId{};
        std::uint8_t motionPropertiesId{};
        std::uint8_t qualityId{};
        std::uintptr_t userDataAddress{};
    };

    struct MotionState
    {
        Vector4 position{};
        Vector4 orientation{};
        Vector4 linearVelocity{};
        Vector4 angularVelocity{};
        Vector4 previousStepLinearVelocity{};
        Vector4 previousStepAngularVelocity{};
        std::int16_t packedInverseInertia[4]{};
        BodyId firstBodyId{};
        std::uint16_t motionPropertiesId{};
        std::uint16_t maxLinearVelocityPacked{};
        std::uint16_t maxAngularVelocityPacked{};
        std::uint8_t deactivationState{};
        bool valid{};
        std::array<std::byte, 12> reserved{};
    };

    struct BodySnapshot
    {
        bool valid{};
        std::uint32_t bodyHighWaterMark{};
        BodyState body{};
        MotionState motion{};
    };

    enum class CollisionBodyResolveStage : std::uint8_t
    {
        None,
        CollisionObject,
        SceneOwner,
        PhysicsSystem,
        PhysicsInstance,
        CurrentWorld,
        BodyTable,
        BodyIndex,
        BodyId,
        Verified,
    };

    enum class CollisionBodyResolveStatus : std::uint8_t
    {
        Resolved,
        MissingCollisionObject,
        MissingSceneOwner,
        MissingExpectedWorld,
        UnreadableCollisionObject,
        SceneOwnerMismatch,
        MissingPhysicsSystem,
        UnreadablePhysicsSystem,
        MissingPhysicsInstance,
        UnreadablePhysicsInstance,
        WorldMismatch,
        InvalidBodyCount,
        MissingBodyIds,
        BodyIndexOutOfRange,
        UnreadableBodyTable,
        UnreadableBodyId,
        InvalidBodyId,
        GenerationChanged,
    };

    struct CollisionBodyResolveResult
    {
        CollisionBodyResolveStatus status{ CollisionBodyResolveStatus::MissingCollisionObject };
        CollisionBodyResolveStage stage{ CollisionBodyResolveStage::None };
        BodyId bodyId{};
        std::uintptr_t collisionObjectAddress{};
        std::uintptr_t sceneOwnerAddress{};
        std::uintptr_t physicsSystemAddress{};
        std::uintptr_t physicsInstanceAddress{};
        std::uintptr_t hknpWorldAddress{};
        std::uintptr_t bodyIdsAddress{};
        std::uint32_t bodyIndex{};
        std::int32_t bodyCount{};

        [[nodiscard]] bool resolved() const noexcept
        {
            return status == CollisionBodyResolveStatus::Resolved && bodyId.valid();
        }
        [[nodiscard]] explicit operator bool() const noexcept { return resolved(); }
    };

    [[nodiscard]] BodySnapshot snapshotBodyDuringSafeEpoch(void* hknpWorld, BodyId bodyId) noexcept;

    /**
     * Resolves one borrowed bhkNPCollisionObject to its current hknp body ID.
     * The expected scene owner and world are mandatory identity witnesses.
     * Every native record is copied completely and the pointer chain is read a
     * second time before success. The caller must keep the scene object,
     * collision object, and physics system stable for this synchronous call.
     */
    [[nodiscard]] CollisionBodyResolveResult resolveCollisionObjectBody(
        void* collisionObject,
        void* expectedSceneOwner,
        void* expectedHknpWorld) noexcept;

    [[nodiscard]] const char* toString(CollisionBodyResolveStage stage) noexcept;
    [[nodiscard]] const char* toString(CollisionBodyResolveStatus status) noexcept;
}
