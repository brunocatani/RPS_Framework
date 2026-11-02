#include "RPS/Runtime/Audio.h"

#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/Memory.h"
#include "RPS/Runtime/WorldAccess.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <cmath>

namespace RPS::Runtime::Audio
{
    namespace
    {
        namespace Layout = Addresses::Layouts::Audio;

        using ListenerDistanceFunction = float (*)(void*, Scene::Point3*);
        using PlayFollowingFunction = void (*)(NativeSoundHandle*, void*, float, void*);
        using SetVolumeFunction = bool (*)(NativeSoundHandle*, float);
        using FadeFunction = bool (*)(NativeSoundHandle*, std::uint16_t);

        template <class Function>
        [[nodiscard]] Function checkedFunction(
            const RuntimeModule& module,
            const Addresses::Symbol symbol,
            std::uintptr_t& address) noexcept
        {
            // Resolved inside the verified executable image; every call runs
            // under SEH, so no page-protection query is needed.
            address = module.resolve(symbol);
            return address != 0 ? reinterpret_cast<Function>(address) : nullptr;
        }

        // One SEH-guarded read proves the object is mapped; a page-protection
        // query would cost a kernel call on every sound start.
        [[nodiscard]] bool readableObject(const void* const object) noexcept
        {
            std::uintptr_t firstWord{};
            return object && Memory::read(object, firstWord);
        }

        [[nodiscard]] bool invokeBoolean(
            const FadeFunction function,
            NativeSoundHandle& handle,
            const std::uint16_t milliseconds,
            bool& accepted) noexcept
        {
#if defined(_MSC_VER)
            __try {
                accepted = function(&handle, milliseconds);
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                accepted = false;
                return false;
            }
#else
            accepted = function(&handle, milliseconds);
            return true;
#endif
        }
    }

    NativeSoundHandle::NativeSoundHandle() noexcept
    {
        invalidate();
    }

    NativeSoundHandle::NativeSoundHandle(NativeSoundHandle&& other) noexcept :
        _soundId(other._soundId),
        _assumeSuccess(other._assumeSuccess),
        _state(other._state),
        _reserved(other._reserved)
    {
        other.invalidate();
    }

    bool NativeSoundHandle::active() const noexcept
    {
        return _soundId != Layout::InvalidSoundId;
    }

    void NativeSoundHandle::invalidate() noexcept
    {
        _soundId = Layout::InvalidSoundId;
        _assumeSuccess = false;
        _state = 0;
        _reserved = 0;
    }

    std::string_view toString(const AudioStatus status) noexcept
    {
        switch (status) {
        case AudioStatus::Completed: return "completed";
        case AudioStatus::InvalidRuntime: return "invalid-runtime";
        case AudioStatus::PhysicsStepActive: return "physics-step-active";
        case AudioStatus::PhysicsStepStateUnavailable: return "physics-step-state-unavailable";
        case AudioStatus::InvalidManager: return "invalid-manager";
        case AudioStatus::InvalidDescriptor: return "invalid-descriptor";
        case AudioStatus::InvalidSceneObject: return "invalid-scene-object";
        case AudioStatus::NonFinitePosition: return "non-finite-position";
        case AudioStatus::InvalidDistance: return "invalid-distance";
        case AudioStatus::InvalidVolume: return "invalid-volume";
        case AudioStatus::InvalidHandle: return "invalid-handle";
        case AudioStatus::HandleAlreadyActive: return "handle-already-active";
        case AudioStatus::FunctionUnavailable: return "function-unavailable";
        case AudioStatus::NativeRejected: return "native-rejected";
        case AudioStatus::NativeFault: return "native-fault";
        default: return "unknown";
        }
    }

    AudioStatus AudioApi::executionStatus() const noexcept
    {
        if (!_module) {
            return AudioStatus::InvalidRuntime;
        }
        switch (Physics::currentThreadPhysicsStepState(_module)) {
        case Physics::PhysicsStepState::Inside:
            return AudioStatus::PhysicsStepActive;
        case Physics::PhysicsStepState::Unknown:
            return AudioStatus::PhysicsStepStateUnavailable;
        case Physics::PhysicsStepState::Outside:
            return AudioStatus::Completed;
        }
        return AudioStatus::PhysicsStepStateUnavailable;
    }

    StartFollowingResult AudioApi::playFollowingDescriptor(
        NativeSoundHandle& handle,
        void* const descriptor,
        void* const sceneObject,
        const Scene::Point3& worldPosition) const noexcept
    {
        StartFollowingResult result{};
        result.status = executionStatus();
        if (result.status != AudioStatus::Completed) {
            return result;
        }
        if (handle.active()) {
            result.status = AudioStatus::HandleAlreadyActive;
            return result;
        }
        handle.invalidate();
        if (!readableObject(descriptor)) {
            result.status = AudioStatus::InvalidDescriptor;
            return result;
        }
        if (!readableObject(sceneObject)) {
            result.status = AudioStatus::InvalidSceneObject;
            return result;
        }
        if (!worldPosition.finite()) {
            result.status = AudioStatus::NonFinitePosition;
            return result;
        }
        const auto managerGlobal = _module.resolve(Addresses::Symbol::Audio_ManagerSingleton);
        void* manager{};
        if (!Memory::read(reinterpret_cast<const void*>(managerGlobal), manager) ||
            !readableObject(manager)) {
            result.status = AudioStatus::InvalidManager;
            return result;
        }
        result.managerAddress = reinterpret_cast<std::uintptr_t>(manager);
        const auto distance = checkedFunction<ListenerDistanceFunction>(
            _module,
            Addresses::Symbol::Audio_GetListenerDistanceSquared,
            result.distanceFunctionAddress);
        const auto play = checkedFunction<PlayFollowingFunction>(
            _module,
            Addresses::Symbol::Audio_PlayFollowingDescriptor,
            result.playFunctionAddress);
        if (!distance || !play) {
            result.status = AudioStatus::FunctionUnavailable;
            return result;
        }
        auto position = worldPosition;
#if defined(_MSC_VER)
        __try {
            result.distanceSquared = distance(manager, &position);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            result.status = AudioStatus::NativeFault;
            return result;
        }
#else
        result.distanceSquared = distance(manager, &position);
#endif
        if (!std::isfinite(result.distanceSquared) || result.distanceSquared < 0.0f) {
            result.status = AudioStatus::InvalidDistance;
            return result;
        }
        result.listenerDistance = std::sqrt(result.distanceSquared);
        if (!std::isfinite(result.listenerDistance)) {
            result.status = AudioStatus::InvalidDistance;
            return result;
        }
#if defined(_MSC_VER)
        __try {
            result.playInvoked = true;
            play(&handle, descriptor, result.listenerDistance, sceneObject);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            result.status = AudioStatus::NativeFault;
            return result;
        }
#else
        result.playInvoked = true;
        play(&handle, descriptor, result.listenerDistance, sceneObject);
#endif
        result.status = handle.active() ? AudioStatus::Completed : AudioStatus::NativeRejected;
        return result;
    }

    AudioResult AudioApi::setVolume(NativeSoundHandle& handle, const float volume) const noexcept
    {
        AudioResult result{};
        result.status = executionStatus();
        result.soundIdBefore = handle.soundId();
        result.soundIdAfter = handle.soundId();
        if (result.status != AudioStatus::Completed) {
            return result;
        }
        if (!handle.active()) {
            result.status = AudioStatus::InvalidHandle;
            return result;
        }
        if (!std::isfinite(volume) || volume < 0.0f || volume > 1.0f) {
            result.status = AudioStatus::InvalidVolume;
            return result;
        }
        const auto function = checkedFunction<SetVolumeFunction>(
            _module,
            Addresses::Symbol::Audio_SetSoundVolume,
            result.functionAddress);
        if (!function) {
            result.status = AudioStatus::FunctionUnavailable;
            return result;
        }
#if defined(_MSC_VER)
        __try {
            result.invoked = true;
            result.accepted = function(&handle, volume);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            result.status = AudioStatus::NativeFault;
            result.soundIdAfter = handle.soundId();
            return result;
        }
#else
        result.invoked = true;
        result.accepted = function(&handle, volume);
#endif
        result.soundIdAfter = handle.soundId();
        result.status = result.accepted ? AudioStatus::Completed : AudioStatus::NativeRejected;
        return result;
    }

    AudioResult AudioApi::fadeInPlay(
        NativeSoundHandle& handle,
        const std::uint16_t milliseconds) const noexcept
    {
        AudioResult result{};
        result.status = executionStatus();
        result.soundIdBefore = handle.soundId();
        result.soundIdAfter = handle.soundId();
        if (result.status != AudioStatus::Completed) {
            return result;
        }
        if (!handle.active()) {
            result.status = AudioStatus::InvalidHandle;
            return result;
        }
        const auto function = checkedFunction<FadeFunction>(
            _module,
            Addresses::Symbol::Audio_FadeInPlay,
            result.functionAddress);
        if (!function) {
            result.status = AudioStatus::FunctionUnavailable;
            return result;
        }
        result.invoked = true;
        if (!invokeBoolean(function, handle, milliseconds, result.accepted)) {
            result.status = AudioStatus::NativeFault;
        } else {
            result.status = result.accepted ? AudioStatus::Completed : AudioStatus::NativeRejected;
        }
        result.soundIdAfter = handle.soundId();
        return result;
    }

    AudioResult AudioApi::fadeOutAndRelease(
        NativeSoundHandle& handle,
        const std::uint16_t milliseconds) const noexcept
    {
        AudioResult result{};
        result.status = executionStatus();
        result.soundIdBefore = handle.soundId();
        result.soundIdAfter = handle.soundId();
        if (result.status != AudioStatus::Completed) {
            return result;
        }
        if (!handle.active()) {
            result.status = AudioStatus::InvalidHandle;
            return result;
        }
        const auto function = checkedFunction<FadeFunction>(
            _module,
            Addresses::Symbol::Audio_FadeOutAndRelease,
            result.functionAddress);
        if (!function) {
            result.status = AudioStatus::FunctionUnavailable;
            return result;
        }
        result.invoked = true;
        const auto completed = invokeBoolean(function, handle, milliseconds, result.accepted);
        handle.invalidate();
        result.soundIdAfter = handle.soundId();
        if (!completed) {
            result.status = AudioStatus::NativeFault;
        } else {
            result.status = result.accepted ? AudioStatus::Completed : AudioStatus::NativeRejected;
        }
        return result;
    }
}
