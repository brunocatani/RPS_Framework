#include "RPS/Runtime/ActorState.h"

#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/Memory.h"
#include "RPS/Runtime/WorldAccess.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <array>
#include <limits>

namespace RPS::Runtime::Character
{
    namespace
    {
        using GetKnockStateFunction = std::uint32_t (*)(void*);
        using SetKnockStateFunction = bool (*)(void*, std::uint32_t);

        enum class NativeCallState : std::uint8_t
        {
            Completed,
            Unavailable,
            Faulted,
        };

        [[nodiscard]] bool addOffset(
            const std::uintptr_t base,
            const std::ptrdiff_t offset,
            std::uintptr_t& result) noexcept
        {
            if (base == 0 || offset < 0 ||
                static_cast<std::uintptr_t>(offset) > (std::numeric_limits<std::uintptr_t>::max)() - base) {
                return false;
            }
            result = base + static_cast<std::uintptr_t>(offset);
            return true;
        }

        [[nodiscard]] bool inspectExecutableVtable(
            const void* const object,
            std::uintptr_t& vtableAddress) noexcept
        {
            std::uintptr_t firstFunction{};
            return Memory::read(object, vtableAddress) && vtableAddress != 0 &&
                   Memory::read(reinterpret_cast<const void*>(vtableAddress), firstFunction) && firstFunction != 0 &&
                   Memory::rangeHasAccess(
                       reinterpret_cast<const void*>(firstFunction),
                       1,
                       Memory::Access::Execute);
        }

        [[nodiscard]] bool resolveActorStateVirtual(
            void* const actor,
            const std::ptrdiff_t slotOffset,
            std::uintptr_t& actorStateAddress,
            std::uintptr_t& functionAddress) noexcept
        {
            namespace Layout = Addresses::Layouts::ActorState;

            actorStateAddress = 0;
            functionAddress = 0;
            if (!Memory::rangeHasAccess(actor, Layout::Actor_MinimumReadableSize, Memory::Access::Read) ||
                !addOffset(reinterpret_cast<std::uintptr_t>(actor), Layout::Actor_StateSubobject, actorStateAddress)) {
                return false;
            }

            std::uintptr_t vtableAddress{};
            std::uintptr_t slotAddress{};
            return inspectExecutableVtable(reinterpret_cast<const void*>(actorStateAddress), vtableAddress) &&
                   addOffset(vtableAddress, slotOffset, slotAddress) &&
                   Memory::read(reinterpret_cast<const void*>(slotAddress), functionAddress) && functionAddress != 0 &&
                   Memory::rangeHasAccess(
                       reinterpret_cast<const void*>(functionAddress),
                       1,
                       Memory::Access::Execute);
        }

        [[nodiscard]] NativeCallState invokeGetKnockState(
            const GetKnockStateFunction function,
            void* const actorState,
            std::uint32_t& state) noexcept
        {
            state = 0;
            if (!function || !actorState) {
                return NativeCallState::Unavailable;
            }
#if defined(_MSC_VER)
            __try {
                state = function(actorState);
                return NativeCallState::Completed;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return NativeCallState::Faulted;
            }
#else
            state = function(actorState);
            return NativeCallState::Completed;
#endif
        }

        [[nodiscard]] NativeCallState invokeSetKnockState(
            const SetKnockStateFunction function,
            void* const actorState,
            const std::uint32_t state,
            bool& nativeReturn) noexcept
        {
            nativeReturn = false;
            if (!function || !actorState) {
                return NativeCallState::Unavailable;
            }
#if defined(_MSC_VER)
            __try {
                nativeReturn = function(actorState, state);
                return NativeCallState::Completed;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return NativeCallState::Faulted;
            }
#else
            nativeReturn = function(actorState, state);
            return NativeCallState::Completed;
#endif
        }
    }

    std::uint8_t decodeActorLifeState(const std::uint32_t flags) noexcept
    {
        namespace Layout = Addresses::Layouts::ActorState;
        return static_cast<std::uint8_t>((flags >> Layout::LifeStateShift) & Layout::LifeStateMask);
    }

    std::uint8_t decodeActorKnockCode(const std::uint32_t flags) noexcept
    {
        namespace Layout = Addresses::Layouts::ActorState;
        return static_cast<std::uint8_t>((flags >> Layout::KnockCodeShift) & Layout::KnockCodeMask);
    }

    std::uint32_t clearActorRagdollMovementFlag(const std::uint32_t flags) noexcept
    {
        return flags & ~Addresses::Layouts::ActorState::RagdollMovementFlagMask;
    }

    ActorStateSnapshot inspectActorStatePointer(void* const actor) noexcept
    {
        namespace Layout = Addresses::Layouts::ActorState;

        ActorStateSnapshot result{};
        result.actorAddress = reinterpret_cast<std::uintptr_t>(actor);
        if (!Memory::rangeHasAccess(actor, Layout::Actor_MinimumReadableSize, Memory::Access::Read)) {
            return result;
        }
        result.actorReadable = true;

        if (!addOffset(result.actorAddress, Layout::Actor_StateSubobject, result.actorStateAddress)) {
            result.actorReadable = false;
            return result;
        }
        result.actorStateVtableReadable = inspectExecutableVtable(
            reinterpret_cast<const void*>(result.actorStateAddress),
            result.actorStateVtableAddress);

        std::uintptr_t fieldAddress{};
        if (addOffset(result.actorAddress, Layout::Actor_LifeFlags, fieldAddress) &&
            Memory::read(reinterpret_cast<const void*>(fieldAddress), result.rawLifeFlags)) {
            result.lifeFlagsReadable = true;
            result.lifeState = decodeActorLifeState(result.rawLifeFlags);
        }
        if (addOffset(result.actorAddress, Layout::Actor_KnockFlags, fieldAddress) &&
            Memory::read(reinterpret_cast<const void*>(fieldAddress), result.rawKnockFlags)) {
            result.knockFlagsReadable = true;
            result.rawKnockCode = decodeActorKnockCode(result.rawKnockFlags);
        }
        if (addOffset(result.actorAddress, Layout::Actor_RagdollMovementFlags, fieldAddress) &&
            Memory::read(reinterpret_cast<const void*>(fieldAddress), result.rawMovementFlags)) {
            result.movementFlagsReadable = true;
            result.ragdollMovementBit =
                (result.rawMovementFlags & Layout::RagdollMovementFlagMask) != 0;
        }

        if (!addOffset(result.actorAddress, Layout::Actor_AIProcess, fieldAddress) ||
            !Memory::read(reinterpret_cast<const void*>(fieldAddress), result.processAddress) ||
            !Memory::rangeHasAccess(
                reinterpret_cast<const void*>(result.processAddress),
                Layout::AIProcess_MinimumReadableSize,
                Memory::Access::Read)) {
            result.processAddress = 0;
            return result;
        }
        result.processReadable = true;

        if (addOffset(result.processAddress, Layout::AIProcess_KnockData, fieldAddress) &&
            Memory::read(reinterpret_cast<const void*>(fieldAddress), result.knockDataAddress) &&
            Memory::rangeHasAccess(
                reinterpret_cast<const void*>(result.knockDataAddress),
                Layout::KnockData_MinimumReadableSize,
                Memory::Access::Read)) {
            result.knockDataReadable = true;
            std::uint8_t ragdollFlag{};
            if (Memory::read(
                    reinterpret_cast<const void*>(result.knockDataAddress + Layout::KnockData_RagdollFlag),
                    ragdollFlag)) {
                result.processRagdollFlagReadable = true;
                result.processRagdollFlag = ragdollFlag != 0;
            }
            result.knockRequestHandleReadable = Memory::read(
                reinterpret_cast<const void*>(result.knockDataAddress + Layout::KnockData_CurrentHandle),
                result.knockRequestHandle);
        } else {
            result.knockDataAddress = 0;
        }

        if (addOffset(result.processAddress, Layout::AIProcess_HighData, fieldAddress) &&
            Memory::read(reinterpret_cast<const void*>(fieldAddress), result.highProcessAddress) &&
            Memory::rangeHasAccess(
                reinterpret_cast<const void*>(result.highProcessAddress),
                Layout::HighProcess_MinimumReadableSize,
                Memory::Access::Read)) {
            result.highProcessReadable = true;
            std::array<std::uint8_t, 2> flags{};
            result.processFlagsReadable = Memory::read(
                reinterpret_cast<const void*>(result.highProcessAddress + Layout::HighProcess_FullRagdollFlagA),
                flags);
            if (result.processFlagsReadable) {
                result.processFlagA = flags[0] != 0;
                result.processFlagB = flags[1] != 0;
            }
        } else {
            result.highProcessAddress = 0;
        }
        return result;
    }

    std::string_view toString(const ActorStateStatus status) noexcept
    {
        switch (status) {
        case ActorStateStatus::Inspected:
            return "inspected";
        case ActorStateStatus::KnockStateResolved:
            return "knock-state-resolved";
        case ActorStateStatus::KnockStateCleared:
            return "knock-state-cleared";
        case ActorStateStatus::AlreadyClear:
            return "already-clear";
        case ActorStateStatus::ActorMovementBitCleared:
            return "actor-movement-bit-cleared";
        case ActorStateStatus::ProcessRagdollFlagsCleared:
            return "process-ragdoll-flags-cleared";
        case ActorStateStatus::InvalidRuntime:
            return "invalid-runtime";
        case ActorStateStatus::InvalidActor:
            return "invalid-actor";
        case ActorStateStatus::PhysicsStepActive:
            return "physics-step-active";
        case ActorStateStatus::PhysicsStepStateUnavailable:
            return "physics-step-state-unavailable";
        case ActorStateStatus::StateUnavailable:
            return "state-unavailable";
        case ActorStateStatus::VtableUnavailable:
            return "vtable-unavailable";
        case ActorStateStatus::NativeFault:
            return "native-fault";
        case ActorStateStatus::NativeRejected:
            return "native-rejected";
        case ActorStateStatus::InvalidKnockState:
            return "invalid-knock-state";
        case ActorStateStatus::GenerationChanged:
            return "generation-changed";
        case ActorStateStatus::WriteFailed:
            return "write-failed";
        case ActorStateStatus::VerificationFailed:
            return "verification-failed";
        default:
            return "unknown";
        }
    }

    bool ActorStateApi::executionContextValid(ActorStateStatus& status) const noexcept
    {
        if (!_module || !*_module) {
            status = ActorStateStatus::InvalidRuntime;
            return false;
        }
        if (!_actor) {
            status = ActorStateStatus::InvalidActor;
            return false;
        }
        switch (Physics::currentThreadPhysicsStepState(*_module)) {
        case Physics::PhysicsStepState::Inside:
            status = ActorStateStatus::PhysicsStepActive;
            return false;
        case Physics::PhysicsStepState::Unknown:
            status = ActorStateStatus::PhysicsStepStateUnavailable;
            return false;
        case Physics::PhysicsStepState::Outside:
            return true;
        }
        status = ActorStateStatus::PhysicsStepStateUnavailable;
        return false;
    }

    ActorStateInspectResult ActorStateApi::inspect() const noexcept
    {
        ActorStateInspectResult result{};
        if (!executionContextValid(result.status)) {
            return result;
        }
        result.snapshot = inspectActorStatePointer(_actor);
        result.status = result.snapshot.actorReadable ? ActorStateStatus::Inspected : ActorStateStatus::InvalidActor;
        return result;
    }

    KnockStateReadResult ActorStateApi::readKnockState() const noexcept
    {
        namespace Layout = Addresses::Layouts::ActorState;

        KnockStateReadResult result{};
        if (!executionContextValid(result.status)) {
            return result;
        }
        result.snapshot = inspectActorStatePointer(_actor);
        if (!result.snapshot.actorReadable) {
            result.status = ActorStateStatus::InvalidActor;
            return result;
        }

        std::uintptr_t actorStateAddress{};
        if (!resolveActorStateVirtual(
                _actor,
                Layout::ActorState_GetKnockStateVtableSlot,
                actorStateAddress,
                result.getterAddress)) {
            result.status = ActorStateStatus::VtableUnavailable;
            return result;
        }

        const auto callState = invokeGetKnockState(
            reinterpret_cast<GetKnockStateFunction>(result.getterAddress),
            reinterpret_cast<void*>(actorStateAddress),
            result.state);
        if (callState == NativeCallState::Unavailable) {
            result.status = ActorStateStatus::VtableUnavailable;
            return result;
        }
        result.invoked = true;
        if (callState == NativeCallState::Faulted) {
            result.status = ActorStateStatus::NativeFault;
            return result;
        }
        result.status = result.state <= Layout::MaximumKnockState ?
            ActorStateStatus::KnockStateResolved :
            ActorStateStatus::InvalidKnockState;
        return result;
    }

    KnockStateClearResult ActorStateApi::clearKnockState() const noexcept
    {
        namespace Layout = Addresses::Layouts::ActorState;

        KnockStateClearResult result{};
        result.before = readKnockState();
        result.status = result.before.status;
        if (!result.before) {
            return result;
        }
        if (result.before.state == Layout::NormalKnockState) {
            result.status = ActorStateStatus::AlreadyClear;
            result.after = result.before;
            return result;
        }

        ActorStateStatus contextStatus{};
        if (!executionContextValid(contextStatus)) {
            result.status = contextStatus;
            return result;
        }

        std::uintptr_t actorStateAddress{};
        if (!resolveActorStateVirtual(
                _actor,
                Layout::ActorState_SetKnockStateVtableSlot,
                actorStateAddress,
                result.setterAddress)) {
            result.status = ActorStateStatus::VtableUnavailable;
            return result;
        }
        const auto callState = invokeSetKnockState(
            reinterpret_cast<SetKnockStateFunction>(result.setterAddress),
            reinterpret_cast<void*>(actorStateAddress),
            Layout::NormalKnockState,
            result.nativeReturn);
        if (callState == NativeCallState::Unavailable) {
            result.status = ActorStateStatus::VtableUnavailable;
            return result;
        }
        result.invoked = true;
        if (callState == NativeCallState::Faulted) {
            result.status = ActorStateStatus::NativeFault;
            return result;
        }

        result.after = readKnockState();
        if (result.after && result.after.state == Layout::NormalKnockState) {
            result.status = ActorStateStatus::KnockStateCleared;
        } else {
            result.status = result.nativeReturn ?
                ActorStateStatus::VerificationFailed :
                ActorStateStatus::NativeRejected;
        }
        return result;
    }

    ActorStateMutationResult ActorStateApi::clearMovementRagdollBit() const noexcept
    {
        namespace Layout = Addresses::Layouts::ActorState;

        ActorStateMutationResult result{};
        const auto inspected = inspect();
        result.status = inspected.status;
        result.before = inspected.snapshot;
        if (!inspected) {
            return result;
        }
        if (!result.before.movementFlagsReadable) {
            result.status = ActorStateStatus::StateUnavailable;
            return result;
        }
        if (!result.before.ragdollMovementBit) {
            result.status = ActorStateStatus::AlreadyClear;
            result.after = result.before;
            return result;
        }

        const auto preflight = inspect();
        if (!preflight) {
            result.status = preflight.status;
            return result;
        }
        if (preflight.snapshot.actorAddress != result.before.actorAddress ||
            !preflight.snapshot.movementFlagsReadable ||
            preflight.snapshot.rawMovementFlags != result.before.rawMovementFlags) {
            result.after = preflight.snapshot;
            result.status = ActorStateStatus::GenerationChanged;
            return result;
        }

        const auto address = reinterpret_cast<void*>(
            result.before.actorAddress + Layout::Actor_RagdollMovementFlags);
        const auto cleared = clearActorRagdollMovementFlag(result.before.rawMovementFlags);
        if (!Memory::rangeHasAccess(address, sizeof(cleared), Memory::Access::Write) ||
            !Memory::write(address, cleared)) {
            result.status = ActorStateStatus::WriteFailed;
            return result;
        }
        result.changed = true;

        const auto after = inspect();
        result.after = after.snapshot;
        result.status = after && result.after.actorAddress == result.before.actorAddress &&
                                result.after.movementFlagsReadable &&
                                result.after.rawMovementFlags == cleared ?
            ActorStateStatus::ActorMovementBitCleared :
            ActorStateStatus::VerificationFailed;
        return result;
    }

    ActorStateMutationResult ActorStateApi::clearProcessRagdollFlags() const noexcept
    {
        namespace Layout = Addresses::Layouts::ActorState;

        ActorStateMutationResult result{};
        const auto inspected = inspect();
        result.status = inspected.status;
        result.before = inspected.snapshot;
        if (!inspected) {
            return result;
        }
        if (!result.before.processFlagsReadable) {
            result.status = ActorStateStatus::StateUnavailable;
            return result;
        }
        if (!result.before.processFlagA && !result.before.processFlagB) {
            result.status = ActorStateStatus::AlreadyClear;
            result.after = result.before;
            return result;
        }

        const auto preflight = inspect();
        if (!preflight) {
            result.status = preflight.status;
            return result;
        }
        if (preflight.snapshot.processAddress != result.before.processAddress ||
            preflight.snapshot.highProcessAddress != result.before.highProcessAddress ||
            !preflight.snapshot.processFlagsReadable) {
            result.after = preflight.snapshot;
            result.status = ActorStateStatus::GenerationChanged;
            return result;
        }
        if (!preflight.snapshot.processFlagA && !preflight.snapshot.processFlagB) {
            result.after = preflight.snapshot;
            result.status = ActorStateStatus::AlreadyClear;
            return result;
        }

        const std::array<std::uint8_t, 2> clearFlags{};
        auto* const address = reinterpret_cast<void*>(
            result.before.highProcessAddress + Layout::HighProcess_FullRagdollFlagA);
        if (!Memory::rangeHasAccess(address, clearFlags.size(), Memory::Access::Write) ||
            !Memory::write(address, clearFlags)) {
            result.status = ActorStateStatus::WriteFailed;
            return result;
        }
        result.changed = true;

        const auto after = inspect();
        result.after = after.snapshot;
        if (!after || result.after.processAddress != result.before.processAddress ||
            result.after.highProcessAddress != result.before.highProcessAddress) {
            result.status = ActorStateStatus::GenerationChanged;
        } else if (result.after.processFlagsReadable && !result.after.processFlagA && !result.after.processFlagB) {
            result.status = ActorStateStatus::ProcessRagdollFlagsCleared;
        } else {
            result.status = ActorStateStatus::VerificationFailed;
        }
        return result;
    }
}
