#include "RPS/Runtime/Constraint.h"

#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/BodyAccess.h"
#include "RPS/Runtime/Memory.h"
#include "RPS/Runtime/NativeReference.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>

namespace RPS::Runtime::Physics
{
    namespace
    {
        struct ConstraintCreationInfo
        {
            void* constraintData{};
            std::uint32_t bodyA{};
            std::uint32_t bodyB{};
            std::uint8_t flags{};
            std::array<std::byte, 7> reserved{};
        };
        static_assert(sizeof(ConstraintCreationInfo) == Addresses::Layouts::Constraint::CreationInfoSize);

        struct PositionMotorData
        {
            void* vtable{};
            std::uint32_t referenceWord{};
            std::uint32_t reserved0{};
            std::uint8_t type{};
            std::array<std::byte, 7> reserved1{};
            float minimumForce{};
            float maximumForce{};
            float tau{};
            float damping{};
            float proportionalRecoveryVelocity{};
            float constantRecoveryVelocity{};
        };
        static_assert(sizeof(PositionMotorData) == Addresses::Layouts::Constraint::PositionMotorSize);

        using ConstraintDataConstructor = void* (*)(void*);
        using BallAndSocketSetPivots = void (*)(void*, const Vector4&, const Vector4&);
        using ConstraintSetInWorldSpace = void (*)(
            void*,
            const Transform&,
            const Transform&,
            const Vector4&,
            const Vector4&);
        using CreateConstraint = std::uint32_t* (*)(void*, std::uint32_t*, ConstraintCreationInfo*);
        using DestroyConstraints = void (*)(void*, std::uint32_t*, std::int32_t);

        template <class Function>
        [[nodiscard]] Function checkedFunction(const RuntimeModule& module, const Addresses::Symbol symbol) noexcept
        {
            const auto function = module.resolveFunction<Function>(symbol);
            return function && Memory::rangeHasAccess(
                                   reinterpret_cast<const void*>(function), 1, Memory::Access::Execute) ?
                       function :
                       nullptr;
        }

        [[nodiscard]] bool invokeConstructor(const ConstraintDataConstructor function, void* const allocation) noexcept
        {
#if defined(_MSC_VER)
            __try {
                (void)function(allocation);
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
#else
            (void)function(allocation);
            return true;
#endif
        }

        template <class Function, class... Arguments>
        [[nodiscard]] bool invokeVoid(const Function function, Arguments&&... arguments) noexcept
        {
#if defined(_MSC_VER)
            __try {
                function(std::forward<Arguments>(arguments)...);
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
#else
            function(std::forward<Arguments>(arguments)...);
            return true;
#endif
        }

        [[nodiscard]] bool invokeCreate(
            const CreateConstraint function,
            void* const world,
            std::uint32_t& id,
            ConstraintCreationInfo& info) noexcept
        {
#if defined(_MSC_VER)
            __try {
                (void)function(world, &id, &info);
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
#else
            (void)function(world, &id, &info);
            return true;
#endif
        }

        [[nodiscard]] void* constructConstraintData(
            const RuntimeModule& module,
            const Addresses::Symbol constructorSymbol,
            const std::size_t bytes) noexcept
        {
            const auto constructor = checkedFunction<ConstraintDataConstructor>(module, constructorSymbol);
            HavokAllocator allocator{ module };
            void* const allocation = constructor ? allocator.allocate(bytes) : nullptr;
            if (!allocation) {
                return nullptr;
            }
            if (!invokeConstructor(constructor, allocation)) {
                (void)allocator.deallocate(allocation, bytes);
                return nullptr;
            }
            return allocation;
        }

        [[nodiscard]] bool validAxis(const Vector4& axis) noexcept
        {
            if (!axis.finite()) {
                return false;
            }
            const float lengthSquared = axis.x * axis.x + axis.y * axis.y + axis.z * axis.z;
            return std::isfinite(lengthSquared) && lengthSquared > 0.000001f;
        }
    }

    namespace Detail
    {
        class ConstraintState
        {
        public:
            ConstraintState(const RuntimeModule module, void* const world) noexcept :
                _module(module), _world(world), _ownerThreadId(GetCurrentThreadId())
            {}

            [[nodiscard]] bool ready() const noexcept
            {
                std::scoped_lock lock(_mutex);
                return static_cast<bool>(_module) && _world != nullptr;
            }

            [[nodiscard]] OwnedConstraintCreateResult createBallAndSocket(
                const WorldWriteGuard& guard,
                const BallAndSocketConstraintInfo& info,
                const std::shared_ptr<ConstraintState>& self) noexcept
            {
                if (!onOwnerThread()) {
                    return { {}, ConstraintError::WrongThread };
                }
                void* const world = currentWorld();
                if (!_module || !world) {
                    return { {}, ConstraintError::InvalidRuntime };
                }
                return own(ConstraintApi{ _module, world }.createBallAndSocket(guard, info), self);
            }

            [[nodiscard]] OwnedConstraintCreateResult createLimitedHinge(
                const WorldWriteGuard& guard,
                const LimitedHingeConstraintInfo& info,
                const std::shared_ptr<ConstraintState>& self) noexcept
            {
                if (!onOwnerThread()) {
                    return { {}, ConstraintError::WrongThread };
                }
                void* const world = currentWorld();
                if (!_module || !world) {
                    return { {}, ConstraintError::InvalidRuntime };
                }
                return own(ConstraintApi{ _module, world }.createLimitedHinge(guard, info), self);
            }

            [[nodiscard]] OwnedConstraintCreateResult createPrismatic(
                const WorldWriteGuard& guard,
                const PrismaticConstraintInfo& info,
                const std::shared_ptr<ConstraintState>& self) noexcept
            {
                if (!onOwnerThread()) {
                    return { {}, ConstraintError::WrongThread };
                }
                void* const world = currentWorld();
                if (!_module || !world) {
                    return { {}, ConstraintError::InvalidRuntime };
                }
                return own(ConstraintApi{ _module, world }.createPrismatic(guard, info), self);
            }

            [[nodiscard]] bool retire(const WorldWriteGuard& guard, OwnedConstraint& constraint) noexcept
            {
                if (!onOwnerThread() || !constraint.valid() || constraint._state.get() != this) {
                    return false;
                }
                void* const world = currentWorld();
                if (!world) {
                    constraint.clearWithoutDestroy();
                    return true;
                }
                auto id = constraint._id;
                if (!ConstraintApi{ _module, world }.destroy(guard, id)) {
                    return false;
                }
                constraint.clearWithoutDestroy();
                return true;
            }

            void queueOrLeak(OwnedConstraint& constraint) noexcept
            {
                if (!constraint.valid() || constraint._state.get() != this) {
                    return;
                }
                {
                    std::scoped_lock lock(_mutex);
                    if (_world && _retirementCount < _retirements.size()) {
                        _retirements[_retirementCount++] = constraint._id;
                    }
                }
                // Queue exhaustion intentionally forgets the ID and leaves the world-owned constraint alive.
                constraint.clearWithoutDestroy();
            }

            [[nodiscard]] std::size_t servicePendingRetirements(const WorldWriteGuard& guard) noexcept
            {
                if (!onOwnerThread()) {
                    return 0;
                }
                void* const world = currentWorld();
                if (!world || !guard.owns(world)) {
                    return 0;
                }

                std::array<ConstraintId, ConstraintRetirementCapacity> pending{};
                std::size_t pendingCount{};
                {
                    std::scoped_lock lock(_mutex);
                    pendingCount = _retirementCount;
                    for (std::size_t index = 0; index < pendingCount; ++index) {
                        pending[index] = _retirements[index];
                        _retirements[index] = {};
                    }
                    _retirementCount = 0;
                }

                ConstraintApi api{ _module, world };
                std::size_t destroyed{};
                std::array<ConstraintId, ConstraintRetirementCapacity> failed{};
                std::size_t failedCount{};
                for (std::size_t index = 0; index < pendingCount; ++index) {
                    auto id = pending[index];
                    if (api.destroy(guard, id)) {
                        ++destroyed;
                    } else {
                        failed[failedCount++] = pending[index];
                    }
                }

                if (failedCount != 0) {
                    std::scoped_lock lock(_mutex);
                    for (std::size_t index = 0;
                         index < failedCount && _retirementCount < _retirements.size();
                         ++index) {
                        _retirements[_retirementCount++] = failed[index];
                    }
                }
                return destroyed;
            }

            [[nodiscard]] std::size_t shutdownAfterWorldLoss() noexcept
            {
                if (!onOwnerThread()) {
                    return 0;
                }
                std::scoped_lock lock(_mutex);
                const auto discarded = _retirementCount;
                for (std::size_t index = 0; index < _retirementCount; ++index) {
                    _retirements[index] = {};
                }
                _retirementCount = 0;
                _world = nullptr;
                return discarded;
            }

            [[nodiscard]] std::size_t retirementCount() const noexcept
            {
                std::scoped_lock lock(_mutex);
                return _retirementCount;
            }

        private:
            [[nodiscard]] bool onOwnerThread() const noexcept { return GetCurrentThreadId() == _ownerThreadId; }

            [[nodiscard]] void* currentWorld() const noexcept
            {
                std::scoped_lock lock(_mutex);
                return _world;
            }

            [[nodiscard]] static OwnedConstraintCreateResult own(
                const ConstraintCreateResult result,
                const std::shared_ptr<ConstraintState>& self) noexcept
            {
                if (!result) {
                    return { {}, result.error };
                }
                return { OwnedConstraint{ self, result.id }, ConstraintError::None };
            }

            RuntimeModule _module{};
            void* _world{};
            DWORD _ownerThreadId{};
            mutable std::mutex _mutex{};
            std::array<ConstraintId, ConstraintRetirementCapacity> _retirements{};
            std::size_t _retirementCount{};
        };
    }

    ConstraintError ConstraintApi::validateBodies(
        const WorldWriteGuard& guard,
        const BodyId bodyA,
        const BodyId bodyB) const noexcept
    {
        if (!_module || !_world) {
            return ConstraintError::InvalidRuntime;
        }
        if (!guard.owns(_world)) {
            return ConstraintError::UnsafeWorldAccess;
        }
        if (!bodyA.valid() || !bodyB.valid() || bodyA == bodyB) {
            return ConstraintError::InvalidParameters;
        }
        if (!snapshotBodyDuringSafeEpoch(_world, bodyA).valid || !snapshotBodyDuringSafeEpoch(_world, bodyB).valid) {
            return ConstraintError::BodyUnavailable;
        }
        return ConstraintError::None;
    }

    ConstraintCreateResult ConstraintApi::create(
        const WorldWriteGuard& guard,
        void* const constraintData,
        const BodyId bodyA,
        const BodyId bodyB) const noexcept
    {
        const auto bodyError = validateBodies(guard, bodyA, bodyB);
        if (bodyError != ConstraintError::None) {
            if (constraintData) {
                (void)releaseHavokReference(constraintData);
            }
            return { {}, bodyError };
        }
        if (!constraintData) {
            return { {}, ConstraintError::AllocationFailed };
        }

        const auto function = checkedFunction<CreateConstraint>(_module, Addresses::Symbol::Constraint_Create);
        ConstraintCreationInfo info{ constraintData, bodyA.value, bodyB.value };
        std::uint32_t id = InvalidConstraintId.value;
        const bool invoked = function && invokeCreate(function, _world, id, info);

        // A successful world insertion owns its own reference; this always drops the factory reference.
        (void)releaseHavokReference(constraintData);
        if (!invoked || id == InvalidConstraintId.value) {
            return { {}, ConstraintError::NativeFailure };
        }
        return { ConstraintId{ id }, ConstraintError::None };
    }

    ConstraintCreateResult ConstraintApi::createBallAndSocket(
        const WorldWriteGuard& guard,
        const BallAndSocketConstraintInfo& info) const noexcept
    {
        const auto bodyError = validateBodies(guard, info.bodyA, info.bodyB);
        if (bodyError != ConstraintError::None) {
            return { {}, bodyError };
        }
        if (!info.localPivotA.finite() || !info.localPivotB.finite()) {
            return { {}, ConstraintError::InvalidParameters };
        }

        void* const data = constructConstraintData(
            _module,
            Addresses::Symbol::Constraint_BallAndSocketCtor,
            Addresses::Layouts::Constraint::BallAndSocketDataSize);
        const auto setPivots = checkedFunction<BallAndSocketSetPivots>(
            _module, Addresses::Symbol::Constraint_BallAndSocketSetPivots);
        if (!data) {
            return { {}, ConstraintError::AllocationFailed };
        }
        if (!setPivots || !invokeVoid(setPivots, data, info.localPivotA, info.localPivotB)) {
            (void)releaseHavokReference(data);
            return { {}, ConstraintError::NativeFailure };
        }
        return create(guard, data, info.bodyA, info.bodyB);
    }

    ConstraintCreateResult ConstraintApi::createLimitedHinge(
        const WorldWriteGuard& guard,
        const LimitedHingeConstraintInfo& info) const noexcept
    {
        const auto bodyError = validateBodies(guard, info.bodyA, info.bodyB);
        if (bodyError != ConstraintError::None) {
            return { {}, bodyError };
        }
        if (!info.bodyAWorld.finite() || !info.bodyBWorld.finite() || !info.worldPivot.finite() ||
            !validAxis(info.worldAxis) || !std::isfinite(info.minimumRelativeAngle) ||
            !std::isfinite(info.maximumRelativeAngle) || info.minimumRelativeAngle > info.maximumRelativeAngle) {
            return { {}, ConstraintError::InvalidParameters };
        }

        void* const data = constructConstraintData(
            _module,
            Addresses::Symbol::Constraint_LimitedHingeCtor,
            Addresses::Layouts::Constraint::LimitedHingeDataSize);
        const auto setWorld = checkedFunction<ConstraintSetInWorldSpace>(
            _module, Addresses::Symbol::Constraint_LimitedHingeSetInWorldSpace);
        if (!data) {
            return { {}, ConstraintError::AllocationFailed };
        }
        const bool configured = setWorld &&
                                invokeVoid(
                                    setWorld,
                                    data,
                                    info.bodyAWorld,
                                    info.bodyBWorld,
                                    info.worldPivot,
                                    info.worldAxis) &&
                                Memory::write(
                                    reinterpret_cast<std::byte*>(data) +
                                        Addresses::Layouts::Constraint::LimitedHinge_LimitEnabled,
                                    std::uint8_t{ 1 }) &&
                                Memory::write(
                                    reinterpret_cast<std::byte*>(data) +
                                        Addresses::Layouts::Constraint::LimitedHinge_MinimumAngle,
                                    info.minimumRelativeAngle) &&
                                Memory::write(
                                    reinterpret_cast<std::byte*>(data) +
                                        Addresses::Layouts::Constraint::LimitedHinge_MaximumAngle,
                                    info.maximumRelativeAngle);
        if (!configured) {
            (void)releaseHavokReference(data);
            return { {}, ConstraintError::NativeFailure };
        }
        return create(guard, data, info.bodyA, info.bodyB);
    }

    ConstraintCreateResult ConstraintApi::createPrismatic(
        const WorldWriteGuard& guard,
        const PrismaticConstraintInfo& info) const noexcept
    {
        const auto bodyError = validateBodies(guard, info.bodyA, info.bodyB);
        if (bodyError != ConstraintError::None) {
            return { {}, bodyError };
        }
        if (!info.bodyAWorld.finite() || !info.bodyBWorld.finite() || !info.worldPivot.finite() ||
            !validAxis(info.worldAxis) || !std::isfinite(info.minimumRelativeDistanceHavok) ||
            !std::isfinite(info.maximumRelativeDistanceHavok) ||
            info.minimumRelativeDistanceHavok > info.maximumRelativeDistanceHavok) {
            return { {}, ConstraintError::InvalidParameters };
        }

        void* const data = constructConstraintData(
            _module,
            Addresses::Symbol::Constraint_PrismaticCtor,
            Addresses::Layouts::Constraint::PrismaticDataSize);
        const auto setWorld = checkedFunction<ConstraintSetInWorldSpace>(
            _module, Addresses::Symbol::Constraint_PrismaticSetInWorldSpace);
        if (!data) {
            return { {}, ConstraintError::AllocationFailed };
        }
        const bool configured = setWorld &&
                                invokeVoid(
                                    setWorld,
                                    data,
                                    info.bodyAWorld,
                                    info.bodyBWorld,
                                    info.worldPivot,
                                    info.worldAxis) &&
                                Memory::write(
                                    reinterpret_cast<std::byte*>(data) +
                                        Addresses::Layouts::Constraint::Prismatic_LimitEnabled,
                                    std::uint8_t{ 1 }) &&
                                Memory::write(
                                    reinterpret_cast<std::byte*>(data) +
                                        Addresses::Layouts::Constraint::Prismatic_MinimumDistance,
                                    info.minimumRelativeDistanceHavok) &&
                                Memory::write(
                                    reinterpret_cast<std::byte*>(data) +
                                        Addresses::Layouts::Constraint::Prismatic_MaximumDistance,
                                    info.maximumRelativeDistanceHavok);
        if (!configured) {
            (void)releaseHavokReference(data);
            return { {}, ConstraintError::NativeFailure };
        }
        return create(guard, data, info.bodyA, info.bodyB);
    }

    bool ConstraintApi::destroy(const WorldWriteGuard& guard, ConstraintId& id) const noexcept
    {
        if (!_module || !_world || !guard.owns(_world) || !id.valid()) {
            return false;
        }
        const auto function = checkedFunction<DestroyConstraints>(_module, Addresses::Symbol::Constraint_Destroy);
        auto rawId = id.value;
        if (!function || !invokeVoid(function, _world, &rawId, std::int32_t{ 1 })) {
            return false;
        }
        id = InvalidConstraintId;
        return true;
    }

    OwnedConstraint::~OwnedConstraint() noexcept
    {
        retireOrQueue();
    }

    OwnedConstraint::OwnedConstraint(OwnedConstraint&& other) noexcept :
        _state(std::move(other._state)), _id(std::exchange(other._id, ConstraintId{}))
    {}

    OwnedConstraint& OwnedConstraint::operator=(OwnedConstraint&& other) noexcept
    {
        if (this != std::addressof(other)) {
            retireOrQueue();
            _state = std::move(other._state);
            _id = std::exchange(other._id, ConstraintId{});
        }
        return *this;
    }

    void OwnedConstraint::retireOrQueue() noexcept
    {
        if (valid()) {
            auto state = _state;
            state->queueOrLeak(*this);
        }
    }

    void OwnedConstraint::clearWithoutDestroy() noexcept
    {
        _state.reset();
        _id = {};
    }

    ConstraintService::ConstraintService(const RuntimeModule module, void* const hknpWorld) noexcept
    {
        try {
            _state = std::make_shared<Detail::ConstraintState>(module, hknpWorld);
        } catch (...) {
            _state.reset();
        }
    }

    bool ConstraintService::ready() const noexcept
    {
        return _state && _state->ready();
    }

    OwnedConstraintCreateResult ConstraintService::createBallAndSocket(
        const WorldWriteGuard& guard,
        const BallAndSocketConstraintInfo& info) noexcept
    {
        return _state ?
                   _state->createBallAndSocket(guard, info, _state) :
                   OwnedConstraintCreateResult{ {}, ConstraintError::InvalidRuntime };
    }

    OwnedConstraintCreateResult ConstraintService::createLimitedHinge(
        const WorldWriteGuard& guard,
        const LimitedHingeConstraintInfo& info) noexcept
    {
        return _state ?
                   _state->createLimitedHinge(guard, info, _state) :
                   OwnedConstraintCreateResult{ {}, ConstraintError::InvalidRuntime };
    }

    OwnedConstraintCreateResult ConstraintService::createPrismatic(
        const WorldWriteGuard& guard,
        const PrismaticConstraintInfo& info) noexcept
    {
        return _state ?
                   _state->createPrismatic(guard, info, _state) :
                   OwnedConstraintCreateResult{ {}, ConstraintError::InvalidRuntime };
    }

    bool ConstraintService::retire(const WorldWriteGuard& guard, OwnedConstraint& constraint) noexcept
    {
        return _state && _state->retire(guard, constraint);
    }

    std::size_t ConstraintService::servicePendingRetirements(const WorldWriteGuard& guard) noexcept
    {
        return _state ? _state->servicePendingRetirements(guard) : 0;
    }

    std::size_t ConstraintService::shutdownAfterWorldLoss() noexcept
    {
        return _state ? _state->shutdownAfterWorldLoss() : 0;
    }

    std::size_t ConstraintService::retirementCount() const noexcept
    {
        return _state ? _state->retirementCount() : 0;
    }

    bool validPositionMotorTuning(const PositionMotorTuning& tuning) noexcept
    {
        return std::isfinite(tuning.minimumForce) && std::isfinite(tuning.maximumForce) &&
               tuning.minimumForce <= tuning.maximumForce && std::isfinite(tuning.tau) && tuning.tau >= 0.0f &&
               std::isfinite(tuning.damping) && tuning.damping >= 0.0f &&
               std::isfinite(tuning.proportionalRecoveryVelocity) && tuning.proportionalRecoveryVelocity >= 0.0f &&
               std::isfinite(tuning.constantRecoveryVelocity) && tuning.constantRecoveryVelocity >= 0.0f;
    }

    PositionMotor::~PositionMotor() noexcept
    {
        (void)resetDetached();
    }

    PositionMotor::PositionMotor(PositionMotor&& other) noexcept :
        _module(other._module), _motor(std::exchange(other._motor, nullptr))
    {}

    PositionMotor& PositionMotor::operator=(PositionMotor&& other) noexcept
    {
        if (this != &other) {
            if (!resetDetached()) {
                return *this;
            }
            _module = other._module;
            _motor = std::exchange(other._motor, nullptr);
        }
        return *this;
    }

    PositionMotor PositionMotor::create(const RuntimeModule module, const PositionMotorTuning& tuning) noexcept
    {
        if (!module || !validPositionMotorTuning(tuning)) {
            return {};
        }
        const auto vtable = module.resolve(Addresses::Symbol::Constraint_PositionMotorVtable);
        if (vtable == 0 || !Memory::rangeHasAccess(
                               reinterpret_cast<const void*>(vtable), sizeof(void*), Memory::Access::Read)) {
            return {};
        }

        HavokAllocator allocator{ module };
        void* const allocation = allocator.allocate(Addresses::Layouts::Constraint::PositionMotorSize);
        if (!allocation) {
            return {};
        }
        const PositionMotorData data{
            reinterpret_cast<void*>(vtable),
            Addresses::Layouts::Constraint::PositionMotorInitialReferenceWord,
            0,
            Addresses::Layouts::Constraint::PositionMotorType,
            {},
            tuning.minimumForce,
            tuning.maximumForce,
            tuning.tau,
            tuning.damping,
            tuning.proportionalRecoveryVelocity,
            tuning.constantRecoveryVelocity,
        };
        if (!Memory::copyTo(allocation, &data, sizeof(data))) {
            (void)allocator.deallocate(allocation, Addresses::Layouts::Constraint::PositionMotorSize);
            return {};
        }
        return PositionMotor{ module, allocation };
    }

    void* PositionMotor::release() noexcept
    {
        return std::exchange(_motor, nullptr);
    }

    bool PositionMotor::resetDetached() noexcept
    {
        if (!_motor) {
            return true;
        }
        HavokAllocator allocator{ _module };
        if (!allocator.deallocate(_motor, Addresses::Layouts::Constraint::PositionMotorSize)) {
            return false;
        }
        _motor = nullptr;
        return true;
    }
}
