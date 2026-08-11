#include "RPS/Runtime/CharacterController.h"

#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/Memory.h"
#include "RPS/Runtime/WorldAccess.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <limits>

namespace RPS::Runtime::Character
{
    namespace
    {
        using ActorGetCharacterControllerFunction = void* (*)(void*);
        using AddCharacterControllerFunction = bool (*)(void*, void*);

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

        [[nodiscard]] NativeCallState invokeGetController(
            const ActorGetCharacterControllerFunction function,
            void* const actor,
            void*& result) noexcept
        {
            result = nullptr;
            if (!function) {
                return NativeCallState::Unavailable;
            }
#if defined(_MSC_VER)
            __try {
                result = function(actor);
                return NativeCallState::Completed;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return NativeCallState::Faulted;
            }
#else
            result = function(actor);
            return NativeCallState::Completed;
#endif
        }

        [[nodiscard]] NativeCallState invokeAddController(
            const AddCharacterControllerFunction function,
            void* const world,
            void* const controller,
            bool& inserted) noexcept
        {
            inserted = false;
            if (!function) {
                return NativeCallState::Unavailable;
            }
#if defined(_MSC_VER)
            __try {
                inserted = function(world, controller);
                return NativeCallState::Completed;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return NativeCallState::Faulted;
            }
#else
            inserted = function(world, controller);
            return NativeCallState::Completed;
#endif
        }
    }

    CharacterControllerSnapshot inspectCharacterControllerPointer(void* const controller) noexcept
    {
        namespace Layout = Addresses::Layouts::CharacterController;

        CharacterControllerSnapshot result{};
        result.controllerAddress = reinterpret_cast<std::uintptr_t>(controller);
        if (!Memory::rangeHasAccess(
                controller,
                Layout::Controller_MinimumReadableSize,
                Memory::Access::Read)) {
            return result;
        }
        result.controllerResolved = true;
        result.vtableReadable = inspectExecutableVtable(controller, result.controllerVtableAddress);

        std::uintptr_t bodyPointerAddress{};
        if (!addOffset(result.controllerAddress, Layout::Controller_RigidBody, bodyPointerAddress) ||
            !Memory::read(reinterpret_cast<const void*>(bodyPointerAddress), result.rigidBodyAddress) ||
            !Memory::rangeHasAccess(
                reinterpret_cast<const void*>(result.rigidBodyAddress),
                Layout::RigidBody_MinimumReadableSize,
                Memory::Access::Read)) {
            result.rigidBodyAddress = 0;
            return result;
        }

        result.rigidBodyResolved = true;
        result.rigidStepGateReadable =
            inspectExecutableVtable(
                reinterpret_cast<const void*>(result.rigidBodyAddress),
                result.rigidBodyVtableAddress) &&
            Memory::read(
                reinterpret_cast<const void*>(result.rigidBodyAddress + Layout::RigidBody_StepGate),
                result.rigidStepGate);
        return result;
    }

    std::string_view toString(const CharacterControllerStatus status) noexcept
    {
        switch (status) {
        case CharacterControllerStatus::Resolved:
            return "resolved";
        case CharacterControllerStatus::Inserted:
            return "inserted";
        case CharacterControllerStatus::NoFreshInsertion:
            return "no-fresh-insertion";
        case CharacterControllerStatus::InvalidRuntime:
            return "invalid-runtime";
        case CharacterControllerStatus::InvalidActor:
            return "invalid-actor";
        case CharacterControllerStatus::InvalidWorld:
            return "invalid-world";
        case CharacterControllerStatus::PhysicsStepActive:
            return "physics-step-active";
        case CharacterControllerStatus::PhysicsStepStateUnavailable:
            return "physics-step-state-unavailable";
        case CharacterControllerStatus::NativeUnavailable:
            return "native-unavailable";
        case CharacterControllerStatus::NativeFault:
            return "native-fault";
        case CharacterControllerStatus::ControllerUnavailable:
            return "controller-unavailable";
        case CharacterControllerStatus::ControllerChanged:
            return "controller-changed";
        default:
            return "unknown";
        }
    }

    CharacterControllerStatus CharacterControllerApi::executionContextStatus() const noexcept
    {
        if (!_module || !*_module) {
            return CharacterControllerStatus::InvalidRuntime;
        }
        if (!_actor) {
            return CharacterControllerStatus::InvalidActor;
        }
        switch (Physics::currentThreadPhysicsStepState(*_module)) {
        case Physics::PhysicsStepState::Inside:
            return CharacterControllerStatus::PhysicsStepActive;
        case Physics::PhysicsStepState::Unknown:
            return CharacterControllerStatus::PhysicsStepStateUnavailable;
        case Physics::PhysicsStepState::Outside:
            return CharacterControllerStatus::Resolved;
        }
        return CharacterControllerStatus::PhysicsStepStateUnavailable;
    }

    CharacterControllerResolveResult CharacterControllerApi::resolve() const noexcept
    {
        CharacterControllerResolveResult result{};
        result.status = executionContextStatus();
        if (result.status != CharacterControllerStatus::Resolved) {
            return result;
        }

        void* controller{};
        const auto callState = invokeGetController(
            _module->resolveFunction<ActorGetCharacterControllerFunction>(Addresses::Symbol::Character_GetController),
            _actor,
            controller);
        if (callState == NativeCallState::Unavailable) {
            result.status = CharacterControllerStatus::NativeUnavailable;
            return result;
        }
        result.invoked = true;
        if (callState == NativeCallState::Faulted) {
            result.status = CharacterControllerStatus::NativeFault;
            return result;
        }

        result.snapshot = inspectCharacterControllerPointer(controller);
        result.snapshot.actorAddress = reinterpret_cast<std::uintptr_t>(_actor);
        result.status = result.snapshot.controllerResolved ?
            CharacterControllerStatus::Resolved :
            CharacterControllerStatus::ControllerUnavailable;
        return result;
    }

    CharacterControllerAddResult CharacterControllerApi::addToWorld(void* const bhkWorld) const noexcept
    {
        namespace Layout = Addresses::Layouts::CharacterController;

        CharacterControllerAddResult result{};
        result.bhkWorldAddress = reinterpret_cast<std::uintptr_t>(bhkWorld);
        const auto resolved = resolve();
        result.status = resolved.status;
        result.before = resolved.snapshot;
        if (!resolved) {
            return result;
        }

        if (!Memory::rangeHasAccess(
                bhkWorld,
                Layout::BhkWorld_MinimumReadableSize,
                Memory::Access::Read) ||
            !addOffset(result.bhkWorldAddress, Layout::BhkWorld_RigidBodyManager, result.managerAddress) ||
            !Memory::rangeHasAccess(
                reinterpret_cast<const void*>(result.managerAddress),
                Layout::RigidBodyManager_MinimumReadableSize,
                Memory::Access::Read) ||
            !inspectExecutableVtable(
                reinterpret_cast<const void*>(result.managerAddress),
                result.managerVtableAddress)) {
            result.status = CharacterControllerStatus::InvalidWorld;
            return result;
        }

        const auto callState = invokeAddController(
            _module->resolveFunction<AddCharacterControllerFunction>(Addresses::Symbol::Character_AddController),
            bhkWorld,
            reinterpret_cast<void*>(result.before.controllerAddress),
            result.inserted);
        if (callState == NativeCallState::Unavailable) {
            result.status = CharacterControllerStatus::NativeUnavailable;
            return result;
        }
        result.invoked = true;
        if (callState == NativeCallState::Faulted) {
            result.status = CharacterControllerStatus::NativeFault;
            return result;
        }

        const auto after = resolve();
        result.after = after.snapshot;
        if (!after || result.after.controllerAddress != result.before.controllerAddress) {
            result.status = CharacterControllerStatus::ControllerChanged;
            return result;
        }
        result.status = result.inserted ?
            CharacterControllerStatus::Inserted :
            CharacterControllerStatus::NoFreshInsertion;
        return result;
    }
}
