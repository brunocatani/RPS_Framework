#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/BodyAccess.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string_view>

namespace
{
    template <class T, std::size_t Size>
    void write(std::array<std::byte, Size>& storage, const std::size_t offset, const T& value)
    {
        if (offset > storage.size() || sizeof(T) > storage.size() - offset) {
            std::abort();
        }
        std::memcpy(storage.data() + offset, &value, sizeof(T));
    }
}

int main()
{
    using namespace RPS;
    using namespace Runtime::Physics;
    namespace Havok = Addresses::Layouts::Havok;
    namespace Bethesda = Addresses::Layouts::Bethesda;

    alignas(16) std::array<std::byte, 0x700> world{};
    alignas(16) std::array<std::byte, Havok::HknpBody_Stride * 4> bodies{};
    alignas(16) std::array<std::byte, Havok::HknpMotion_Stride * 4> motions{};

    const auto bodyArray = reinterpret_cast<std::uintptr_t>(bodies.data());
    const auto motionArray = reinterpret_cast<std::uintptr_t>(motions.data());
    const std::uint32_t highWaterMark = 3;
    write(world, Havok::HknpWorld_BodyArray, bodyArray);
    write(world, Havok::HknpWorld_MotionArray, motionArray);
    write(world, Havok::HknpWorld_BodyHighWaterMark, highWaterMark);

    constexpr std::uint32_t bodyIndex = 2;
    constexpr std::uint32_t motionIndex = 1;
    const auto bodyOffset = bodyIndex * Havok::HknpBody_Stride;
    const Transform transform{};
    write(bodies, bodyOffset, transform);
    write(bodies, bodyOffset + Havok::HknpBody_Flags, std::uint32_t{ 7 });
    write(bodies, bodyOffset + Havok::HknpBody_CollisionFilterInfo, std::uint32_t{ 0x1234 });
    write(bodies, bodyOffset + Havok::HknpBody_Shape, std::uintptr_t{ 0x2000 });
    write(bodies, bodyOffset + Havok::HknpBody_Id, bodyIndex);
    write(bodies, bodyOffset + Havok::HknpBody_MotionIndex, motionIndex);
    write(bodies, bodyOffset + Havok::HknpBody_MaterialId, std::uint16_t{ 12 });
    write(bodies, bodyOffset + Havok::HknpBody_MotionPropertiesId, std::uint8_t{ 4 });
    write(bodies, bodyOffset + Havok::HknpBody_CollisionObject, std::uintptr_t{ 0x3000 });

    const auto motionOffset = motionIndex * Havok::HknpMotion_Stride;
    write(motions, motionOffset + Havok::HknpMotion_Position, Vector4{ 1.0f, 2.0f, 3.0f, 0.0f });
    write(motions, motionOffset + Havok::HknpMotion_Orientation, Vector4{ 0.0f, 0.0f, 0.0f, 1.0f });
    write(motions, motionOffset + Havok::HknpMotion_FirstBodyId, bodyIndex);
    write(motions, motionOffset + Havok::HknpMotion_PropertiesId, std::uint16_t{ 9 });
    write(motions, motionOffset + Havok::HknpMotion_LinearVelocity, Vector4{ 5.0f, 0.0f, 0.0f, 0.0f });

    const auto snapshot = snapshotBodyDuringSafeEpoch(world.data(), BodyId{ bodyIndex });
    if (!snapshot.valid || !snapshot.motion.valid || snapshot.body.bodyId.value != bodyIndex ||
        snapshot.body.motionIndex != motionIndex || snapshot.body.collisionFilterInfo != 0x1234 ||
        snapshot.body.shapeAddress != 0x2000 || snapshot.body.userDataAddress != 0x3000 ||
        snapshot.motion.firstBodyId.value != bodyIndex || snapshot.motion.position.y != 2.0f ||
        snapshot.motion.linearVelocity.x != 5.0f || snapshot.motion.motionPropertiesId != 9) {
        std::cerr << "complete body and motion snapshot failed\n";
        return 1;
    }

    write(bodies, bodyOffset + Havok::HknpBody_Id, std::uint32_t{ 1 });
    if (snapshotBodyDuringSafeEpoch(world.data(), BodyId{ bodyIndex }).valid) {
        std::cerr << "reused body slot did not fail closed\n";
        return 1;
    }
    write(bodies, bodyOffset + Havok::HknpBody_Id, bodyIndex);

    if (snapshotBodyDuringSafeEpoch(world.data(), BodyId{ 4 }).valid ||
        snapshotBodyDuringSafeEpoch(world.data(), BodyId{}).valid) {
        std::cerr << "body bounds did not fail closed\n";
        return 1;
    }

    constexpr std::uint32_t staticIndex = 3;
    const auto staticOffset = staticIndex * Havok::HknpBody_Stride;
    write(bodies, staticOffset, transform);
    write(bodies, staticOffset + Havok::HknpBody_Id, staticIndex);
    write(bodies, staticOffset + Havok::HknpBody_MotionIndex, std::uint32_t{ 0 });
    const auto staticSnapshot = snapshotBodyDuringSafeEpoch(world.data(), BodyId{ staticIndex });
    if (!staticSnapshot.valid || staticSnapshot.motion.valid) {
        std::cerr << "static body contract failed\n";
        return 1;
    }

    alignas(16) std::array<std::byte, Bethesda::CollisionObjectSize> collisionObject{};
    alignas(16) std::array<std::byte, Bethesda::PhysicsSystemSize> physicsSystem{};
    alignas(16) std::array<std::byte, Bethesda::PhysicsSystemInstance_MinimumReadableSize> physicsInstance{};
    std::array<std::uint32_t, 3> bodyIds{ 10, 11, 12 };
    int sceneOwner{};
    int otherSceneOwner{};
    int otherWorld{};
    write(collisionObject, Bethesda::CollisionObject_OwnerNode, reinterpret_cast<std::uintptr_t>(&sceneOwner));
    write(
        collisionObject,
        Bethesda::CollisionObject_PhysicsSystem,
        reinterpret_cast<std::uintptr_t>(physicsSystem.data()));
    write(collisionObject, Bethesda::CollisionObject_BodyIndex, std::uint32_t{ 1 });
    write(
        physicsSystem,
        Bethesda::PhysicsSystem_Instance,
        reinterpret_cast<std::uintptr_t>(physicsInstance.data()));
    write(
        physicsInstance,
        Bethesda::PhysicsSystemInstance_World,
        reinterpret_cast<std::uintptr_t>(world.data()));
    write(
        physicsInstance,
        Bethesda::PhysicsSystemInstance_BodyIds,
        reinterpret_cast<std::uintptr_t>(bodyIds.data()));
    write(physicsInstance, Bethesda::PhysicsSystemInstance_BodyCount, std::int32_t{ 3 });

    const auto resolved = resolveCollisionObjectBody(collisionObject.data(), &sceneOwner, world.data());
    if (!resolved || resolved.stage != CollisionBodyResolveStage::Verified || resolved.bodyId.value != 11 ||
        resolved.sceneOwnerAddress != reinterpret_cast<std::uintptr_t>(&sceneOwner) || resolved.bodyIndex != 1 ||
        resolved.bodyCount != 3 || toString(resolved.status) != std::string_view{ "resolved" }) {
        std::cerr << "collision object body resolution failed\n";
        return 1;
    }

    const auto wrongOwner = resolveCollisionObjectBody(collisionObject.data(), &otherSceneOwner, world.data());
    const auto wrongWorld = resolveCollisionObjectBody(collisionObject.data(), &sceneOwner, &otherWorld);
    if (wrongOwner.status != CollisionBodyResolveStatus::SceneOwnerMismatch ||
        wrongWorld.status != CollisionBodyResolveStatus::WorldMismatch ||
        resolveCollisionObjectBody(nullptr, &sceneOwner, world.data()).status !=
            CollisionBodyResolveStatus::MissingCollisionObject) {
        std::cerr << "collision object identity gates failed\n";
        return 1;
    }

    write(collisionObject, Bethesda::CollisionObject_BodyIndex, std::uint32_t{ 3 });
    if (resolveCollisionObjectBody(collisionObject.data(), &sceneOwner, world.data()).status !=
        CollisionBodyResolveStatus::BodyIndexOutOfRange) {
        std::cerr << "collision body index bounds failed\n";
        return 1;
    }
    write(collisionObject, Bethesda::CollisionObject_BodyIndex, std::uint32_t{ 1 });
    bodyIds[1] = InvalidBodyId;
    if (resolveCollisionObjectBody(collisionObject.data(), &sceneOwner, world.data()).status !=
        CollisionBodyResolveStatus::InvalidBodyId) {
        std::cerr << "collision body ID validation failed\n";
        return 1;
    }

    return 0;
}
