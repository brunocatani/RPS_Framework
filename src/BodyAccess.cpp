#include "RPS/Runtime/BodyAccess.h"

#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/Memory.h"

#include <array>
#include <cstddef>
#include <limits>

namespace RPS::Runtime::Physics
{
    namespace
    {
        using namespace Addresses::Layouts;

        struct alignas(16) NativeBody
        {
            Transform transform;
            std::uint32_t flags;
            std::uint32_t collisionFilterInfo;
            std::uintptr_t shape;
            std::array<std::byte, 0x10> shapeTail;
            std::uint32_t bodyId;
            std::uint32_t nextAttachedBodyId;
            std::uint32_t motionIndex;
            std::int32_t deactivationIslandId;
            std::uint16_t materialId;
            std::uint8_t motionPropertiesId;
            std::array<std::byte, 0x0B> propertyTail;
            std::uint8_t qualityId;
            std::array<std::byte, 0x09> qualityTail;
            std::uintptr_t userData;
        };
        static_assert(sizeof(NativeBody) == Havok::HknpBody_Stride);
        static_assert(offsetof(NativeBody, flags) == Havok::HknpBody_Flags);
        static_assert(offsetof(NativeBody, collisionFilterInfo) == Havok::HknpBody_CollisionFilterInfo);
        static_assert(offsetof(NativeBody, shape) == Havok::HknpBody_Shape);
        static_assert(offsetof(NativeBody, bodyId) == Havok::HknpBody_Id);
        static_assert(offsetof(NativeBody, nextAttachedBodyId) == Havok::HknpBody_NextAttachedBodyId);
        static_assert(offsetof(NativeBody, motionIndex) == Havok::HknpBody_MotionIndex);
        static_assert(offsetof(NativeBody, deactivationIslandId) == Havok::HknpBody_DeactivationIslandId);
        static_assert(offsetof(NativeBody, materialId) == Havok::HknpBody_MaterialId);
        static_assert(offsetof(NativeBody, motionPropertiesId) == Havok::HknpBody_MotionPropertiesId);
        static_assert(offsetof(NativeBody, qualityId) == Havok::HknpBody_QualityId);
        static_assert(offsetof(NativeBody, userData) == Havok::HknpBody_CollisionObject);

        struct alignas(16) NativeMotion
        {
            Vector4 position;
            Vector4 orientation;
            std::int16_t packedInverseInertia[4];
            std::uint32_t firstBodyId;
            std::uint32_t deactivationIndex;
            std::uint64_t internalState;
            std::uint16_t motionPropertiesId;
            std::uint16_t maxLinearVelocityPacked;
            std::uint16_t maxAngularVelocityPacked;
            std::uint8_t cellIndex;
            std::uint8_t deactivationState;
            Vector4 linearVelocity;
            Vector4 angularVelocity;
            Vector4 previousStepLinearVelocity;
            Vector4 previousStepAngularVelocity;
        };
        static_assert(sizeof(NativeMotion) == Havok::HknpMotion_Stride);
        static_assert(offsetof(NativeMotion, position) == Havok::HknpMotion_Position);
        static_assert(offsetof(NativeMotion, orientation) == Havok::HknpMotion_Orientation);
        static_assert(offsetof(NativeMotion, packedInverseInertia) == Havok::HknpMotion_PackedInverseInertia);
        static_assert(offsetof(NativeMotion, firstBodyId) == Havok::HknpMotion_FirstBodyId);
        static_assert(offsetof(NativeMotion, motionPropertiesId) == Havok::HknpMotion_PropertiesId);
        static_assert(offsetof(NativeMotion, maxLinearVelocityPacked) == Havok::HknpMotion_MaxLinearVelocityPacked);
        static_assert(offsetof(NativeMotion, maxAngularVelocityPacked) == Havok::HknpMotion_MaxAngularVelocityPacked);
        static_assert(offsetof(NativeMotion, deactivationState) == Havok::HknpMotion_DeactivationState);
        static_assert(offsetof(NativeMotion, linearVelocity) == Havok::HknpMotion_LinearVelocity);
        static_assert(offsetof(NativeMotion, angularVelocity) == Havok::HknpMotion_AngularVelocity);
        static_assert(offsetof(NativeMotion, previousStepLinearVelocity) == Havok::HknpMotion_PreviousLinearVelocity);
        static_assert(offsetof(NativeMotion, previousStepAngularVelocity) == Havok::HknpMotion_PreviousAngularVelocity);

        struct NativeCollisionObject
        {
            std::array<std::byte, Bethesda::CollisionObject_OwnerNode> prefix;
            std::uintptr_t ownerNode;
            std::array<std::byte, Bethesda::CollisionObject_PhysicsSystem -
                                      Bethesda::CollisionObject_OwnerNode - sizeof(std::uintptr_t)> ownerTail;
            std::uintptr_t physicsSystem;
            std::uint32_t bodyIndex;
            std::uint32_t tail;
        };
        static_assert(sizeof(NativeCollisionObject) == Bethesda::CollisionObjectSize);
        static_assert(offsetof(NativeCollisionObject, ownerNode) == Bethesda::CollisionObject_OwnerNode);
        static_assert(offsetof(NativeCollisionObject, physicsSystem) == Bethesda::CollisionObject_PhysicsSystem);
        static_assert(offsetof(NativeCollisionObject, bodyIndex) == Bethesda::CollisionObject_BodyIndex);

        struct NativePhysicsSystem
        {
            std::array<std::byte, Bethesda::PhysicsSystem_Instance> prefix;
            std::uintptr_t instance;
            std::array<std::byte, Bethesda::PhysicsSystemSize -
                                      Bethesda::PhysicsSystem_Instance - sizeof(std::uintptr_t)> tail;
        };
        static_assert(sizeof(NativePhysicsSystem) == Bethesda::PhysicsSystemSize);
        static_assert(offsetof(NativePhysicsSystem, instance) == Bethesda::PhysicsSystem_Instance);

        struct NativePhysicsInstance
        {
            std::array<std::byte, Bethesda::PhysicsSystemInstance_World> prefix;
            std::uintptr_t world;
            std::uintptr_t bodyIds;
            std::int32_t bodyCount;
            std::uint32_t tail;
        };
        static_assert(sizeof(NativePhysicsInstance) == Bethesda::PhysicsSystemInstance_MinimumReadableSize);
        static_assert(offsetof(NativePhysicsInstance, world) == Bethesda::PhysicsSystemInstance_World);
        static_assert(offsetof(NativePhysicsInstance, bodyIds) == Bethesda::PhysicsSystemInstance_BodyIds);
        static_assert(offsetof(NativePhysicsInstance, bodyCount) == Bethesda::PhysicsSystemInstance_BodyCount);

        [[nodiscard]] bool indexedAddress(
            const std::uintptr_t base,
            const std::uint32_t index,
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
    }

    BodySnapshot snapshotBodyDuringSafeEpoch(void* const hknpWorld, const BodyId bodyId) noexcept
    {
        BodySnapshot result{};
        if (!hknpWorld || !bodyId.valid() || bodyId.value > Havok::MaxReadableBodyIndex) {
            return result;
        }

        const auto world = reinterpret_cast<std::uintptr_t>(hknpWorld);
        if (!Memory::read(
                reinterpret_cast<const void*>(world + Havok::HknpWorld_BodyHighWaterMark),
                result.bodyHighWaterMark) ||
            result.bodyHighWaterMark > Havok::MaxReadableBodyIndex || bodyId.value > result.bodyHighWaterMark) {
            return result;
        }

        std::uintptr_t bodyArray{};
        std::uintptr_t bodyAddress{};
        NativeBody nativeBody{};
        if (!Memory::read(reinterpret_cast<const void*>(world + Havok::HknpWorld_BodyArray), bodyArray) ||
            !indexedAddress(bodyArray, bodyId.value, Havok::HknpBody_Stride, bodyAddress) ||
            !Memory::read(reinterpret_cast<const void*>(bodyAddress), nativeBody) || nativeBody.bodyId != bodyId.value ||
            nativeBody.motionIndex == InvalidBodyId) {
            return result;
        }

        result.body = {
            .transform = nativeBody.transform,
            .flags = nativeBody.flags,
            .collisionFilterInfo = nativeBody.collisionFilterInfo,
            .shapeAddress = nativeBody.shape,
            .bodyId = BodyId{ nativeBody.bodyId },
            .nextAttachedBodyId = nativeBody.nextAttachedBodyId,
            .motionIndex = nativeBody.motionIndex,
            .deactivationIslandId = nativeBody.deactivationIslandId,
            .materialId = nativeBody.materialId,
            .motionPropertiesId = nativeBody.motionPropertiesId,
            .qualityId = nativeBody.qualityId,
            .userDataAddress = nativeBody.userData,
        };
        result.valid = result.body.transform.finite();

        if (!result.valid || nativeBody.motionIndex == 0 || nativeBody.motionIndex >= Havok::MaxUsableMotionIndex) {
            return result;
        }

        std::uintptr_t motionArray{};
        std::uintptr_t motionAddress{};
        NativeMotion nativeMotion{};
        if (!Memory::read(reinterpret_cast<const void*>(world + Havok::HknpWorld_MotionArray), motionArray) ||
            !indexedAddress(motionArray, nativeBody.motionIndex, Havok::HknpMotion_Stride, motionAddress) ||
            !Memory::read(reinterpret_cast<const void*>(motionAddress), nativeMotion)) {
            return result;
        }

        result.motion = {
            .position = nativeMotion.position,
            .orientation = nativeMotion.orientation,
            .linearVelocity = nativeMotion.linearVelocity,
            .angularVelocity = nativeMotion.angularVelocity,
            .previousStepLinearVelocity = nativeMotion.previousStepLinearVelocity,
            .previousStepAngularVelocity = nativeMotion.previousStepAngularVelocity,
            .packedInverseInertia = {
                nativeMotion.packedInverseInertia[0],
                nativeMotion.packedInverseInertia[1],
                nativeMotion.packedInverseInertia[2],
                nativeMotion.packedInverseInertia[3],
            },
            .firstBodyId = BodyId{ nativeMotion.firstBodyId },
            .motionPropertiesId = nativeMotion.motionPropertiesId,
            .maxLinearVelocityPacked = nativeMotion.maxLinearVelocityPacked,
            .maxAngularVelocityPacked = nativeMotion.maxAngularVelocityPacked,
            .deactivationState = nativeMotion.deactivationState,
            .valid = nativeMotion.position.finite() && nativeMotion.orientation.finite() && nativeMotion.linearVelocity.finite() &&
                     nativeMotion.angularVelocity.finite() && nativeMotion.previousStepLinearVelocity.finite() &&
                     nativeMotion.previousStepAngularVelocity.finite(),
        };
        return result;
    }

    CollisionBodyResolveResult resolveCollisionObjectBody(
        void* const collisionObject,
        void* const expectedSceneOwner,
        void* const expectedHknpWorld) noexcept
    {
        using namespace Addresses::Layouts;

        CollisionBodyResolveResult result{};
        result.collisionObjectAddress = reinterpret_cast<std::uintptr_t>(collisionObject);
        if (!collisionObject) {
            return result;
        }
        result.stage = CollisionBodyResolveStage::CollisionObject;
        if (!Memory::rangeHasAccess(expectedSceneOwner, 1, Memory::Access::Read)) {
            result.status = CollisionBodyResolveStatus::MissingSceneOwner;
            return result;
        }
        if (!Memory::rangeHasAccess(expectedHknpWorld, 1, Memory::Access::Read)) {
            result.status = CollisionBodyResolveStatus::MissingExpectedWorld;
            return result;
        }

        NativeCollisionObject collision{};
        if (!Memory::read(collisionObject, collision)) {
            result.status = CollisionBodyResolveStatus::UnreadableCollisionObject;
            return result;
        }
        result.sceneOwnerAddress = collision.ownerNode;
        result.stage = CollisionBodyResolveStage::SceneOwner;
        if (collision.ownerNode != reinterpret_cast<std::uintptr_t>(expectedSceneOwner)) {
            result.status = CollisionBodyResolveStatus::SceneOwnerMismatch;
            return result;
        }

        result.physicsSystemAddress = collision.physicsSystem;
        if (result.physicsSystemAddress == 0) {
            result.status = CollisionBodyResolveStatus::MissingPhysicsSystem;
            return result;
        }
        result.stage = CollisionBodyResolveStage::PhysicsSystem;
        NativePhysicsSystem system{};
        if (!Memory::read(reinterpret_cast<const void*>(result.physicsSystemAddress), system)) {
            result.status = CollisionBodyResolveStatus::UnreadablePhysicsSystem;
            return result;
        }

        result.physicsInstanceAddress = system.instance;
        if (result.physicsInstanceAddress == 0) {
            result.status = CollisionBodyResolveStatus::MissingPhysicsInstance;
            return result;
        }
        result.stage = CollisionBodyResolveStage::PhysicsInstance;
        NativePhysicsInstance instance{};
        if (!Memory::read(reinterpret_cast<const void*>(result.physicsInstanceAddress), instance)) {
            result.status = CollisionBodyResolveStatus::UnreadablePhysicsInstance;
            return result;
        }

        result.hknpWorldAddress = instance.world;
        result.bodyIdsAddress = instance.bodyIds;
        result.bodyIndex = collision.bodyIndex;
        result.bodyCount = instance.bodyCount;
        if (instance.world != reinterpret_cast<std::uintptr_t>(expectedHknpWorld)) {
            result.status = CollisionBodyResolveStatus::WorldMismatch;
            return result;
        }
        result.stage = CollisionBodyResolveStage::CurrentWorld;
        if (instance.bodyCount <= 0 || instance.bodyCount > Bethesda::MaximumPhysicsSystemBodyCount) {
            result.status = CollisionBodyResolveStatus::InvalidBodyCount;
            return result;
        }
        if (instance.bodyIds == 0) {
            result.status = CollisionBodyResolveStatus::MissingBodyIds;
            return result;
        }
        result.stage = CollisionBodyResolveStage::BodyTable;
        if (collision.bodyIndex >= static_cast<std::uint32_t>(instance.bodyCount)) {
            result.status = CollisionBodyResolveStatus::BodyIndexOutOfRange;
            return result;
        }

        const auto bodyTableBytes = static_cast<std::size_t>(instance.bodyCount) * sizeof(std::uint32_t);
        if (!Memory::rangeHasAccess(
                reinterpret_cast<const void*>(instance.bodyIds),
                bodyTableBytes,
                Memory::Access::Read)) {
            result.status = CollisionBodyResolveStatus::UnreadableBodyTable;
            return result;
        }
        result.stage = CollisionBodyResolveStage::BodyIndex;
        std::uintptr_t bodyIdAddress{};
        if (!indexedAddress(instance.bodyIds, collision.bodyIndex, sizeof(std::uint32_t), bodyIdAddress) ||
            !Memory::read(reinterpret_cast<const void*>(bodyIdAddress), result.bodyId.value)) {
            result.status = CollisionBodyResolveStatus::UnreadableBodyId;
            return result;
        }
        if (!result.bodyId.valid() || result.bodyId.value > Havok::MaxReadableBodyIndex) {
            result.bodyId = {};
            result.status = CollisionBodyResolveStatus::InvalidBodyId;
            return result;
        }
        result.stage = CollisionBodyResolveStage::BodyId;

        NativeCollisionObject verifiedCollision{};
        NativePhysicsSystem verifiedSystem{};
        NativePhysicsInstance verifiedInstance{};
        std::uint32_t verifiedBodyId{ InvalidBodyId };
        if (!Memory::read(collisionObject, verifiedCollision) ||
            !Memory::read(reinterpret_cast<const void*>(result.physicsSystemAddress), verifiedSystem) ||
            !Memory::read(reinterpret_cast<const void*>(result.physicsInstanceAddress), verifiedInstance) ||
            !Memory::read(reinterpret_cast<const void*>(bodyIdAddress), verifiedBodyId) ||
            verifiedCollision.ownerNode != collision.ownerNode ||
            verifiedCollision.physicsSystem != collision.physicsSystem ||
            verifiedCollision.bodyIndex != collision.bodyIndex ||
            verifiedSystem.instance != system.instance || verifiedInstance.world != instance.world ||
            verifiedInstance.bodyIds != instance.bodyIds || verifiedInstance.bodyCount != instance.bodyCount ||
            verifiedBodyId != result.bodyId.value) {
            result.bodyId = {};
            result.status = CollisionBodyResolveStatus::GenerationChanged;
            return result;
        }

        result.stage = CollisionBodyResolveStage::Verified;
        result.status = CollisionBodyResolveStatus::Resolved;
        return result;
    }

    const char* toString(const CollisionBodyResolveStage stage) noexcept
    {
        switch (stage) {
        case CollisionBodyResolveStage::None:
            return "none";
        case CollisionBodyResolveStage::CollisionObject:
            return "collision-object";
        case CollisionBodyResolveStage::SceneOwner:
            return "scene-owner";
        case CollisionBodyResolveStage::PhysicsSystem:
            return "physics-system";
        case CollisionBodyResolveStage::PhysicsInstance:
            return "physics-instance";
        case CollisionBodyResolveStage::CurrentWorld:
            return "current-world";
        case CollisionBodyResolveStage::BodyTable:
            return "body-table";
        case CollisionBodyResolveStage::BodyIndex:
            return "body-index";
        case CollisionBodyResolveStage::BodyId:
            return "body-id";
        case CollisionBodyResolveStage::Verified:
            return "verified";
        default:
            return "unknown";
        }
    }

    const char* toString(const CollisionBodyResolveStatus status) noexcept
    {
        switch (status) {
        case CollisionBodyResolveStatus::Resolved:
            return "resolved";
        case CollisionBodyResolveStatus::MissingCollisionObject:
            return "missing-collision-object";
        case CollisionBodyResolveStatus::MissingSceneOwner:
            return "missing-scene-owner";
        case CollisionBodyResolveStatus::MissingExpectedWorld:
            return "missing-expected-world";
        case CollisionBodyResolveStatus::UnreadableCollisionObject:
            return "unreadable-collision-object";
        case CollisionBodyResolveStatus::SceneOwnerMismatch:
            return "scene-owner-mismatch";
        case CollisionBodyResolveStatus::MissingPhysicsSystem:
            return "missing-physics-system";
        case CollisionBodyResolveStatus::UnreadablePhysicsSystem:
            return "unreadable-physics-system";
        case CollisionBodyResolveStatus::MissingPhysicsInstance:
            return "missing-physics-instance";
        case CollisionBodyResolveStatus::UnreadablePhysicsInstance:
            return "unreadable-physics-instance";
        case CollisionBodyResolveStatus::WorldMismatch:
            return "world-mismatch";
        case CollisionBodyResolveStatus::InvalidBodyCount:
            return "invalid-body-count";
        case CollisionBodyResolveStatus::MissingBodyIds:
            return "missing-body-ids";
        case CollisionBodyResolveStatus::BodyIndexOutOfRange:
            return "body-index-out-of-range";
        case CollisionBodyResolveStatus::UnreadableBodyTable:
            return "unreadable-body-table";
        case CollisionBodyResolveStatus::UnreadableBodyId:
            return "unreadable-body-id";
        case CollisionBodyResolveStatus::InvalidBodyId:
            return "invalid-body-id";
        case CollisionBodyResolveStatus::GenerationChanged:
            return "generation-changed";
        default:
            return "unknown";
        }
    }
}
