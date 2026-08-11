#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/BodyGravity.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>

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
    using namespace Runtime;
    using namespace Physics;
    namespace Havok = Addresses::Layouts::Havok;

    alignas(16) std::array<std::byte, 0x700> world{};
    alignas(16) std::array<std::byte, Havok::HknpBody_Stride * 3> bodies{};
    alignas(16) std::array<std::byte, Havok::HknpMotion_Stride * 3> motions{};
    alignas(16) std::array<std::byte, 0x40> library{};
    alignas(16) std::array<std::byte, Havok::MotionProperties_Stride * 16> properties{};

    const auto bodyArray = reinterpret_cast<std::uintptr_t>(bodies.data());
    const auto motionArray = reinterpret_cast<std::uintptr_t>(motions.data());
    const auto libraryAddress = reinterpret_cast<std::uintptr_t>(library.data());
    const auto propertiesAddress = reinterpret_cast<std::uintptr_t>(properties.data());
    write(world, Havok::HknpWorld_BodyArray, bodyArray);
    write(world, Havok::HknpWorld_MotionArray, motionArray);
    write(world, Havok::HknpWorld_BodyHighWaterMark, std::uint32_t{ 2 });
    write(world, Havok::HknpWorld_MotionPropertiesLibrary, libraryAddress);
    write(library, Havok::MotionPropertiesLibrary_Data, propertiesAddress);

    constexpr std::uint32_t bodyIndex = 2;
    constexpr std::uint32_t motionIndex = 1;
    constexpr std::uint16_t propertiesId = 9;
    const auto bodyOffset = bodyIndex * Havok::HknpBody_Stride;
    const auto motionOffset = motionIndex * Havok::HknpMotion_Stride;
    write(bodies, bodyOffset + Havok::HknpBody_Id, bodyIndex);
    write(bodies, bodyOffset + Havok::HknpBody_MotionIndex, motionIndex);
    write(motions, motionOffset + Havok::HknpMotion_PropertiesId, propertiesId);
    write(properties,
        propertiesId * Havok::MotionProperties_Stride + Havok::MotionProperties_GravityFactor,
        0.35f);

    const auto snapshot = snapshotBodyGravityDuringSafeEpoch(world.data(), BodyId{ bodyIndex });
    if (!snapshot.valid || snapshot.bodyId.value != bodyIndex || snapshot.motionIndex != motionIndex ||
        snapshot.motionPropertiesId != propertiesId || snapshot.gravityFactor != 0.35f) {
        std::cerr << "gravity snapshot failed\n";
        return 1;
    }

    write(properties,
        propertiesId * Havok::MotionProperties_Stride + Havok::MotionProperties_GravityFactor,
        (std::numeric_limits<float>::quiet_NaN)());
    if (snapshotBodyGravityDuringSafeEpoch(world.data(), BodyId{ bodyIndex }).valid ||
        snapshotBodyGravityDuringSafeEpoch(world.data(), BodyId{}).valid || !validGravityFactor(0.0f) ||
        !validGravityFactor(-1.0f) || validGravityFactor((std::numeric_limits<float>::infinity)())) {
        std::cerr << "gravity validation did not fail closed\n";
        return 1;
    }

    const auto module = RuntimeModule::detect();
    BodyGravityApi api{ module, world.data() };
    const auto writeResult = api.setBodyFactor(BodyId{ bodyIndex }, 1.0f);
    if (module || writeResult.status != BodyGravityWriteStatus::InvalidRuntime || writeResult.invoked || writeResult ||
        toString(BodyGravityWriteStatus::PhysicsStepActive) != "physics-step-active") {
        std::cerr << "gravity write did not fail closed without FO4VR\n";
        return 1;
    }

    return 0;
}
