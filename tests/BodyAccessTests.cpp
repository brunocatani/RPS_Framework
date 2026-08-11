#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/BodyAccess.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>

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

    return 0;
}
