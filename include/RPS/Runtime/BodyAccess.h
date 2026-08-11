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

    [[nodiscard]] BodySnapshot snapshotBodyDuringSafeEpoch(void* hknpWorld, BodyId bodyId) noexcept;
}
