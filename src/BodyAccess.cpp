#include "RPS/Runtime/BodyAccess.h"

#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/Memory.h"

#include <array>
#include <cstddef>
#include <cstring>
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
}
