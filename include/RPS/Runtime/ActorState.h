#pragma once

#include "RPS/Runtime/RuntimeModule.h"

#include <cstdint>
#include <string_view>

namespace RPS::Runtime::Character
{
    struct ActorStateSnapshot
    {
        bool actorReadable{};
        bool actorStateVtableReadable{};
        bool lifeFlagsReadable{};
        bool knockFlagsReadable{};
        bool movementFlagsReadable{};
        bool processReadable{};
        bool knockDataReadable{};
        bool processRagdollFlagReadable{};
        bool knockRequestHandleReadable{};
        bool highProcessReadable{};
        bool processFlagsReadable{};
        bool ragdollMovementBit{};
        bool processRagdollFlag{};
        bool processFlagA{};
        bool processFlagB{};
        std::uint8_t lifeState{};
        std::uint8_t rawKnockCode{};
        std::uint32_t rawLifeFlags{};
        std::uint32_t rawKnockFlags{};
        std::uint32_t rawMovementFlags{};
        std::uint32_t knockRequestHandle{};
        std::uintptr_t actorAddress{};
        std::uintptr_t actorStateAddress{};
        std::uintptr_t actorStateVtableAddress{};
        std::uintptr_t processAddress{};
        std::uintptr_t knockDataAddress{};
        std::uintptr_t highProcessAddress{};

        [[nodiscard]] bool movementAuthorityReadable() const noexcept
        {
            return movementFlagsReadable && processFlagsReadable;
        }
    };

    [[nodiscard]] std::uint8_t decodeActorLifeState(std::uint32_t flags) noexcept;
    [[nodiscard]] std::uint8_t decodeActorKnockCode(std::uint32_t flags) noexcept;
    [[nodiscard]] std::uint32_t clearActorRagdollMovementFlag(std::uint32_t flags) noexcept;

    /**
     * Copies the proven FO4VR actor-state fields from a borrowed actor. The
     * caller owns thread/lifetime stability; no engine pointers in the result
     * are retained and no native function is invoked.
     */
    [[nodiscard]] ActorStateSnapshot inspectActorStatePointer(void* actor) noexcept;

    enum class ActorStateStatus : std::uint8_t
    {
        Inspected,
        KnockStateResolved,
        KnockStateCleared,
        AlreadyClear,
        ActorMovementBitCleared,
        ProcessRagdollFlagsCleared,
        InvalidRuntime,
        InvalidActor,
        PhysicsStepActive,
        PhysicsStepStateUnavailable,
        StateUnavailable,
        VtableUnavailable,
        NativeFault,
        NativeRejected,
        InvalidKnockState,
        GenerationChanged,
        WriteFailed,
        VerificationFailed,
    };

    struct ActorStateInspectResult
    {
        ActorStateStatus status{ ActorStateStatus::InvalidRuntime };
        ActorStateSnapshot snapshot{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return status == ActorStateStatus::Inspected;
        }
    };

    struct KnockStateReadResult
    {
        ActorStateStatus status{ ActorStateStatus::InvalidRuntime };
        ActorStateSnapshot snapshot{};
        std::uintptr_t getterAddress{};
        bool invoked{};
        std::uint32_t state{};

        [[nodiscard]] bool resolved() const noexcept
        {
            return status == ActorStateStatus::KnockStateResolved;
        }
        [[nodiscard]] explicit operator bool() const noexcept { return resolved(); }
    };

    struct KnockStateClearResult
    {
        ActorStateStatus status{ ActorStateStatus::InvalidRuntime };
        KnockStateReadResult before{};
        KnockStateReadResult after{};
        std::uintptr_t setterAddress{};
        bool invoked{};
        bool nativeReturn{};

        [[nodiscard]] bool completed() const noexcept
        {
            return status == ActorStateStatus::KnockStateCleared ||
                   status == ActorStateStatus::AlreadyClear;
        }
        [[nodiscard]] explicit operator bool() const noexcept { return completed(); }
    };

    struct ActorStateMutationResult
    {
        ActorStateStatus status{ ActorStateStatus::InvalidRuntime };
        ActorStateSnapshot before{};
        ActorStateSnapshot after{};
        bool changed{};

        [[nodiscard]] bool completed() const noexcept
        {
            return status == ActorStateStatus::ActorMovementBitCleared ||
                   status == ActorStateStatus::ProcessRagdollFlagsCleared ||
                   status == ActorStateStatus::AlreadyClear;
        }
        [[nodiscard]] explicit operator bool() const noexcept { return completed(); }
    };

    [[nodiscard]] std::string_view toString(ActorStateStatus status) noexcept;

    /**
     * Corrected FO4VR actor-state access. Native calls and direct authority-bit
     * writes require the owning game/frame thread outside physics. The narrow
     * bit-clearing operations do not implement active-ragdoll ownership policy;
     * consumers decide when they own those fields.
     */
    class ActorStateApi
    {
    public:
        ActorStateApi(const RuntimeModule& module, void* actor) noexcept : _module(&module), _actor(actor) {}

        [[nodiscard]] ActorStateInspectResult inspect() const noexcept;
        [[nodiscard]] KnockStateReadResult readKnockState() const noexcept;
        [[nodiscard]] KnockStateClearResult clearKnockState() const noexcept;
        [[nodiscard]] ActorStateMutationResult clearMovementRagdollBit() const noexcept;
        [[nodiscard]] ActorStateMutationResult clearProcessRagdollFlags() const noexcept;

    private:
        [[nodiscard]] bool executionContextValid(ActorStateStatus& status) const noexcept;

        const RuntimeModule* _module{};
        void* _actor{};
    };
}
