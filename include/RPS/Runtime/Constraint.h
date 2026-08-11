#pragma once

#include "RPS/Runtime/HavokAllocator.h"
#include "RPS/Runtime/PhysicsTypes.h"
#include "RPS/Runtime/WorldAccess.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

namespace RPS::Runtime::Physics
{
    using ConstraintId = BodyId;
    inline constexpr ConstraintId InvalidConstraintId{};
    inline constexpr std::size_t ConstraintRetirementCapacity = 512;

    enum class ConstraintError : std::uint8_t
    {
        None,
        InvalidRuntime,
        WrongThread,
        UnsafeWorldAccess,
        InvalidParameters,
        BodyUnavailable,
        AllocationFailed,
        NativeFailure,
    };

    struct ConstraintCreateResult
    {
        ConstraintId id{};
        ConstraintError error{ ConstraintError::NativeFailure };

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return error == ConstraintError::None && id.valid();
        }
    };

    struct BallAndSocketConstraintInfo
    {
        Vector4 localPivotA{};
        Vector4 localPivotB{};
        BodyId bodyA{};
        BodyId bodyB{};
        std::array<std::byte, 8> reserved{};
    };

    struct LimitedHingeConstraintInfo
    {
        Transform bodyAWorld{};
        Transform bodyBWorld{};
        Vector4 worldPivot{};
        Vector4 worldAxis{};
        float minimumRelativeAngle{};
        float maximumRelativeAngle{};
        BodyId bodyA{};
        BodyId bodyB{};
    };

    struct PrismaticConstraintInfo
    {
        Transform bodyAWorld{};
        Transform bodyBWorld{};
        Vector4 worldPivot{};
        Vector4 worldAxis{};
        float minimumRelativeDistanceHavok{};
        float maximumRelativeDistanceHavok{};
        BodyId bodyA{};
        BodyId bodyB{};
    };

    class ConstraintApi
    {
    public:
        ConstraintApi(RuntimeModule module, void* hknpWorld) noexcept : _module(module), _world(hknpWorld) {}

        [[nodiscard]] ConstraintCreateResult createBallAndSocket(
            const WorldWriteGuard& guard,
            const BallAndSocketConstraintInfo& info) const noexcept;
        [[nodiscard]] ConstraintCreateResult createLimitedHinge(
            const WorldWriteGuard& guard,
            const LimitedHingeConstraintInfo& info) const noexcept;
        [[nodiscard]] ConstraintCreateResult createPrismatic(
            const WorldWriteGuard& guard,
            const PrismaticConstraintInfo& info) const noexcept;
        [[nodiscard]] bool destroy(const WorldWriteGuard& guard, ConstraintId& id) const noexcept;

    private:
        [[nodiscard]] ConstraintError validateBodies(
            const WorldWriteGuard& guard,
            BodyId bodyA,
            BodyId bodyB) const noexcept;
        [[nodiscard]] ConstraintCreateResult create(
            const WorldWriteGuard& guard,
            void* constraintData,
            BodyId bodyA,
            BodyId bodyB) const noexcept;

        RuntimeModule _module{};
        void* _world{};
    };

    namespace Detail
    {
        class ConstraintState;
    }

    class OwnedConstraint
    {
    public:
        OwnedConstraint() = default;
        ~OwnedConstraint() noexcept;

        OwnedConstraint(const OwnedConstraint&) = delete;
        OwnedConstraint& operator=(const OwnedConstraint&) = delete;
        OwnedConstraint(OwnedConstraint&& other) noexcept;
        OwnedConstraint& operator=(OwnedConstraint&& other) noexcept;

        [[nodiscard]] ConstraintId id() const noexcept { return _id; }
        [[nodiscard]] bool valid() const noexcept { return _state && _id.valid(); }
        [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

    private:
        friend class ConstraintService;
        friend class Detail::ConstraintState;

        OwnedConstraint(std::shared_ptr<Detail::ConstraintState> state, ConstraintId id) noexcept :
            _state(std::move(state)), _id(id)
        {}

        void retireOrQueue() noexcept;
        void clearWithoutDestroy() noexcept;

        std::shared_ptr<Detail::ConstraintState> _state{};
        ConstraintId _id{};
    };

    struct OwnedConstraintCreateResult
    {
        OwnedConstraint constraint{};
        ConstraintError error{ ConstraintError::NativeFailure };

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return error == ConstraintError::None && static_cast<bool>(constraint);
        }
    };

    class ConstraintService
    {
    public:
        /*
         * The constructing thread owns native constraint creation/removal.
         * Destruction on any other thread enters a fixed pending queue. Service
         * that queue on the owner thread under a WorldWriteGuard. If the queue
         * fills, the live world-owned constraint is intentionally leaked rather
         * than destroyed in an unsafe epoch.
         */
        ConstraintService() = default;
        ConstraintService(RuntimeModule module, void* hknpWorld) noexcept;

        ConstraintService(const ConstraintService&) = delete;
        ConstraintService& operator=(const ConstraintService&) = delete;
        ConstraintService(ConstraintService&&) noexcept = default;
        ConstraintService& operator=(ConstraintService&&) noexcept = default;

        [[nodiscard]] bool ready() const noexcept;
        [[nodiscard]] explicit operator bool() const noexcept { return ready(); }
        [[nodiscard]] OwnedConstraintCreateResult createBallAndSocket(
            const WorldWriteGuard& guard,
            const BallAndSocketConstraintInfo& info) noexcept;
        [[nodiscard]] OwnedConstraintCreateResult createLimitedHinge(
            const WorldWriteGuard& guard,
            const LimitedHingeConstraintInfo& info) noexcept;
        [[nodiscard]] OwnedConstraintCreateResult createPrismatic(
            const WorldWriteGuard& guard,
            const PrismaticConstraintInfo& info) noexcept;
        [[nodiscard]] bool retire(const WorldWriteGuard& guard, OwnedConstraint& constraint) noexcept;
        [[nodiscard]] std::size_t servicePendingRetirements(const WorldWriteGuard& guard) noexcept;
        [[nodiscard]] std::size_t shutdownAfterWorldLoss() noexcept;
        [[nodiscard]] std::size_t retirementCount() const noexcept;

    private:
        std::shared_ptr<Detail::ConstraintState> _state{};
    };

    struct PositionMotorTuning
    {
        float minimumForce{};
        float maximumForce{};
        float tau{ 0.03f };
        float damping{ 0.8f };
        float proportionalRecoveryVelocity{ 2.0f };
        float constantRecoveryVelocity{ 1.0f };
    };

    [[nodiscard]] bool validPositionMotorTuning(const PositionMotorTuning& tuning) noexcept;

    class PositionMotor
    {
    public:
        PositionMotor() = default;
        ~PositionMotor() noexcept;

        PositionMotor(const PositionMotor&) = delete;
        PositionMotor& operator=(const PositionMotor&) = delete;
        PositionMotor(PositionMotor&& other) noexcept;
        PositionMotor& operator=(PositionMotor&& other) noexcept;

        [[nodiscard]] static PositionMotor create(RuntimeModule module, const PositionMotorTuning& tuning) noexcept;

        [[nodiscard]] void* get() const noexcept { return _motor; }
        [[nodiscard]] explicit operator bool() const noexcept { return _motor != nullptr; }

        // Only detach or destroy a motor while no native constraint can read it.
        [[nodiscard]] void* release() noexcept;
        [[nodiscard]] bool resetDetached() noexcept;

    private:
        PositionMotor(RuntimeModule module, void* motor) noexcept : _module(module), _motor(motor) {}

        RuntimeModule _module{};
        void* _motor{};
    };
}
