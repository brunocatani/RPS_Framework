#pragma once

#include "RPS/Runtime/BodyAccess.h"
#include "RPS/Runtime/WorldAccess.h"

#include <cstdint>

namespace RPS::Runtime::Physics
{
    class Api
    {
    public:
        Api(const RuntimeModule& module, void* hknpWorld) noexcept : _module(&module), _world(hknpWorld) {}

        [[nodiscard]] BodySnapshot snapshot(const WorldReadGuard& guard, BodyId bodyId) const noexcept;
        [[nodiscard]] bool setCollisionFilterInfo(
            const WorldWriteGuard& guard,
            BodyId bodyId,
            std::uint32_t filterInfo,
            std::uint32_t rebuildMode = 1) const noexcept;
        [[nodiscard]] bool setVelocityDeferred(
            const WorldWriteGuard& guard,
            BodyId bodyId,
            const Vector4& linearVelocity,
            const Vector4& angularVelocity) const noexcept;
        [[nodiscard]] bool setTransformDeferred(
            const WorldWriteGuard& guard,
            BodyId bodyId,
            const Transform& transform,
            int mode = 0) const noexcept;
        [[nodiscard]] bool setKeyframed(const WorldWriteGuard& guard, BodyId bodyId) const noexcept;
        [[nodiscard]] bool activate(const WorldWriteGuard& guard, BodyId bodyId) const noexcept;
        [[nodiscard]] bool enableFlags(
            const WorldWriteGuard& guard,
            BodyId bodyId,
            std::uint32_t flags,
            std::uint32_t mode = 0) const noexcept;
        [[nodiscard]] bool disableFlags(
            const WorldWriteGuard& guard,
            BodyId bodyId,
            std::uint32_t flags,
            std::uint32_t mode = 0) const noexcept;
        [[nodiscard]] bool rebuildMotionMassProperties(
            const WorldWriteGuard& guard,
            std::uint32_t motionIndex,
            int rebuildMode = 1) const noexcept;
        [[nodiscard]] bool applyImpulseAt(
            const WorldWriteGuard& guard,
            BodyId bodyId,
            const Vector4& impulse,
            const Vector4& worldPoint) const noexcept;
        [[nodiscard]] bool computeHardKeyFrame(
            const WorldWriteGuard& guard,
            BodyId bodyId,
            const Vector4& targetPosition,
            const Vector4& targetRotation,
            float deltaSeconds,
            Vector4& outLinearVelocity,
            Vector4& outAngularVelocity) const noexcept;

    private:
        [[nodiscard]] bool canMutate(const WorldWriteGuard& guard, BodyId bodyId) const noexcept;

        const RuntimeModule* _module{};
        void* _world{};
    };
}
