#pragma once

#include "RPS/Runtime/RuntimeModule.h"

#include <cstdint>
#include <string_view>

namespace RPS::Runtime::Character
{
    struct CharacterControllerSnapshot
    {
        bool controllerResolved{};
        bool vtableReadable{};
        bool rigidBodyResolved{};
        bool rigidStepGateReadable{};
        std::uintptr_t actorAddress{};
        std::uintptr_t controllerAddress{};
        std::uintptr_t controllerVtableAddress{};
        std::uintptr_t rigidBodyAddress{};
        std::uintptr_t rigidBodyVtableAddress{};
        std::uint8_t rigidStepGate{};

        [[nodiscard]] bool rigidBodyComplete() const noexcept
        {
            return controllerResolved && vtableReadable && rigidBodyResolved && rigidStepGateReadable;
        }
    };

    /** Inspects a borrowed GetCharacterController result without retaining it. */
    [[nodiscard]] CharacterControllerSnapshot inspectCharacterControllerPointer(void* controller) noexcept;

    enum class CharacterControllerStatus : std::uint8_t
    {
        Resolved,
        Inserted,
        NoFreshInsertion,
        InvalidRuntime,
        InvalidActor,
        InvalidWorld,
        PhysicsStepActive,
        PhysicsStepStateUnavailable,
        NativeUnavailable,
        NativeFault,
        ControllerUnavailable,
        ControllerChanged,
    };

    struct CharacterControllerResolveResult
    {
        CharacterControllerStatus status{ CharacterControllerStatus::InvalidRuntime };
        CharacterControllerSnapshot snapshot{};
        bool invoked{};

        [[nodiscard]] bool resolved() const noexcept { return status == CharacterControllerStatus::Resolved; }
        [[nodiscard]] explicit operator bool() const noexcept { return resolved(); }
    };

    struct CharacterControllerAddResult
    {
        CharacterControllerStatus status{ CharacterControllerStatus::InvalidRuntime };
        CharacterControllerSnapshot before{};
        CharacterControllerSnapshot after{};
        std::uintptr_t bhkWorldAddress{};
        std::uintptr_t managerAddress{};
        std::uintptr_t managerVtableAddress{};
        bool invoked{};
        bool inserted{};

        [[nodiscard]] bool completed() const noexcept
        {
            return status == CharacterControllerStatus::Inserted ||
                   status == CharacterControllerStatus::NoFreshInsertion;
        }
        [[nodiscard]] explicit operator bool() const noexcept { return completed(); }
    };

    [[nodiscard]] std::string_view toString(CharacterControllerStatus status) noexcept;

    /**
     * Resolves an actor's current bhkCharacterController and exposes the
     * engine's locked, deduplicating AddCharacterController wrapper. The actor,
     * world, controller, and rigid body are borrowed only for each call.
     * addToWorld must run on the owning game/frame thread outside physics; the
     * native locks bhkWorld internally, so no hknp WorldWriteGuard is accepted.
     * A false native return means "already present or rejected", never proof
     * that the controller is absent.
     */
    class CharacterControllerApi
    {
    public:
        CharacterControllerApi(const RuntimeModule& module, void* actor) noexcept : _module(&module), _actor(actor) {}

        [[nodiscard]] CharacterControllerResolveResult resolve() const noexcept;
        [[nodiscard]] CharacterControllerAddResult addToWorld(void* bhkWorld) const noexcept;

    private:
        [[nodiscard]] CharacterControllerStatus executionContextStatus() const noexcept;

        const RuntimeModule* _module{};
        void* _actor{};
    };
}
