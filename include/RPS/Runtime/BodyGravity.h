#pragma once

#include "RPS/Runtime/BodyAccess.h"
#include "RPS/Runtime/WorldAccess.h"

#include <cstdint>
#include <string_view>

namespace RPS::Runtime::Physics
{
    struct BodyGravitySnapshot
    {
        bool valid{};
        BodyId bodyId{};
        std::uint32_t motionIndex{};
        std::uint16_t motionPropertiesId{};
        float gravityFactor{};
    };

    /** Caller must already own a valid read/write epoch for hknpWorld. */
    [[nodiscard]] BodyGravitySnapshot snapshotBodyGravityDuringSafeEpoch(
        void* hknpWorld,
        BodyId bodyId) noexcept;

    [[nodiscard]] bool validGravityFactor(float gravityFactor) noexcept;

    enum class BodyGravityWriteStatus : std::uint8_t
    {
        Applied,
        InvalidRuntime,
        InvalidWorld,
        InvalidBody,
        InvalidGravityFactor,
        PhysicsStepActive,
        PhysicsStepStateUnavailable,
        ReadAccessUnavailable,
        BodyUnavailable,
        NativeUnavailable,
        NativeFault,
        VerificationFailed,
    };

    struct BodyGravityWriteResult
    {
        BodyGravityWriteStatus status{ BodyGravityWriteStatus::InvalidRuntime };
        BodyGravitySnapshot before{};
        BodyGravitySnapshot after{};
        float requestedGravityFactor{};
        bool invoked{};

        [[nodiscard]] bool applied() const noexcept { return status == BodyGravityWriteStatus::Applied; }
        [[nodiscard]] explicit operator bool() const noexcept { return applied(); }
    };

    [[nodiscard]] std::string_view toString(BodyGravityWriteStatus status) noexcept;

    class BodyGravityApi
    {
    public:
        BodyGravityApi(const RuntimeModule& module, void* hknpWorld) noexcept : _module(&module), _world(hknpWorld) {}

        [[nodiscard]] BodyGravitySnapshot read(const WorldReadGuard& guard, BodyId bodyId) const noexcept;

        /**
         * BGSWaterCollisionManager::SetWaterGravityFactor acquires the world
         * write lock internally. Do not hold WorldWriteGuard around this call.
         * This wrapper rejects physics-step and unreadable TLS contexts, then
         * verifies the result under separate read-lock epochs.
         */
        [[nodiscard]] BodyGravityWriteResult setBodyFactor(BodyId bodyId, float gravityFactor) const noexcept;

    private:
        const RuntimeModule* _module{};
        void* _world{};
    };
}
