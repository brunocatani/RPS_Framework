#include "RPS/Runtime/ScenePhysics.h"

#include "RPS/Runtime/Memory.h"
#include "RPS/Runtime/WorldAccess.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <cstdint>

namespace RPS::Runtime::Physics
{
    namespace
    {
        using SetMotionRecursiveFunction = std::uint8_t (*)(void*, std::uint32_t, bool, bool, bool);
        using EnableCollisionRecursiveFunction = std::uint8_t (*)(void*, bool, bool, bool);

        [[nodiscard]] bool readableSceneRoot(void* const sceneRoot) noexcept
        {
            std::uintptr_t vtable{};
            return Memory::read(sceneRoot, vtable) && vtable != 0 &&
                   Memory::rangeHasAccess(reinterpret_cast<const void*>(vtable), sizeof(std::uintptr_t), Memory::Access::Read);
        }

        [[nodiscard]] RecursiveCommandStatus executionContextStatus(const RuntimeModule& module) noexcept
        {
            if (!module) {
                return RecursiveCommandStatus::InvalidRuntime;
            }
            switch (currentThreadPhysicsStepState(module)) {
            case PhysicsStepState::Outside:
                return RecursiveCommandStatus::Accepted;
            case PhysicsStepState::Inside:
                return RecursiveCommandStatus::PhysicsStepActive;
            case PhysicsStepState::Unknown:
            default:
                return RecursiveCommandStatus::PhysicsStepStateUnavailable;
            }
        }

        template <class Function, class... Arguments>
        [[nodiscard]] RecursiveCommandResult invokeRecursiveCommand(
            const Function function,
            Arguments... arguments) noexcept
        {
            if (!function) {
                return { .status = RecursiveCommandStatus::NativeUnavailable };
            }

            std::uint8_t nativeResult{};
#if defined(_MSC_VER)
            __try {
                nativeResult = function(arguments...);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return { .status = RecursiveCommandStatus::NativeFault, .invoked = true };
            }
#else
            nativeResult = function(arguments...);
#endif
            return {
                .status = nativeResult != 0 ? RecursiveCommandStatus::Accepted : RecursiveCommandStatus::NativeRejected,
                .invoked = true,
            };
        }
    }

    std::string_view toString(const RecursiveCommandStatus status) noexcept
    {
        switch (status) {
        case RecursiveCommandStatus::Accepted:
            return "accepted";
        case RecursiveCommandStatus::InvalidRuntime:
            return "invalid-runtime";
        case RecursiveCommandStatus::InvalidSceneRoot:
            return "invalid-scene-root";
        case RecursiveCommandStatus::InvalidMotionPreset:
            return "invalid-motion-preset";
        case RecursiveCommandStatus::PhysicsStepActive:
            return "physics-step-active";
        case RecursiveCommandStatus::PhysicsStepStateUnavailable:
            return "physics-step-state-unavailable";
        case RecursiveCommandStatus::NativeUnavailable:
            return "native-unavailable";
        case RecursiveCommandStatus::NativeRejected:
            return "native-rejected";
        case RecursiveCommandStatus::NativeFault:
            return "native-fault";
        default:
            return "unknown";
        }
    }

    RecursiveCommandResult setMotionRecursive(
        const RuntimeModule& module,
        void* const sceneRoot,
        const RecursiveMotionRequest request) noexcept
    {
        const auto contextStatus = executionContextStatus(module);
        if (contextStatus != RecursiveCommandStatus::Accepted) {
            return { .status = contextStatus };
        }
        if (!validMotionPreset(request.preset)) {
            return { .status = RecursiveCommandStatus::InvalidMotionPreset };
        }
        if (!readableSceneRoot(sceneRoot)) {
            return { .status = RecursiveCommandStatus::InvalidSceneRoot };
        }

        return invokeRecursiveCommand(
            module.resolveFunction<SetMotionRecursiveFunction>(Addresses::Symbol::World_SetMotionRecursive),
            sceneRoot,
            static_cast<std::uint32_t>(request.preset),
            request.recursive,
            request.force,
            request.activate);
    }

    RecursiveCommandResult enableCollisionRecursive(
        const RuntimeModule& module,
        void* const sceneRoot,
        const RecursiveCollisionRequest request) noexcept
    {
        const auto contextStatus = executionContextStatus(module);
        if (contextStatus != RecursiveCommandStatus::Accepted) {
            return { .status = contextStatus };
        }
        if (!readableSceneRoot(sceneRoot)) {
            return { .status = RecursiveCommandStatus::InvalidSceneRoot };
        }

        return invokeRecursiveCommand(
            module.resolveFunction<EnableCollisionRecursiveFunction>(Addresses::Symbol::World_EnableCollisionRecursive),
            sceneRoot,
            request.enable,
            request.recursive,
            request.force);
    }
}
