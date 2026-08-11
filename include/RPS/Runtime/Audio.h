#pragma once

#include "RPS/Runtime/RuntimeModule.h"
#include "RPS/Runtime/Scene.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace RPS::Runtime::Audio
{
    class NativeSoundHandle
    {
    public:
        NativeSoundHandle() noexcept;
        NativeSoundHandle(const NativeSoundHandle&) = delete;
        NativeSoundHandle& operator=(const NativeSoundHandle&) = delete;
        NativeSoundHandle(NativeSoundHandle&& other) noexcept;
        NativeSoundHandle& operator=(NativeSoundHandle&&) = delete;

        [[nodiscard]] bool active() const noexcept;
        [[nodiscard]] std::uint32_t soundId() const noexcept { return _soundId; }
        [[nodiscard]] bool assumesSuccess() const noexcept { return _assumeSuccess; }
        [[nodiscard]] std::int8_t state() const noexcept { return _state; }

    private:
        friend class AudioApi;

        void invalidate() noexcept;

        std::uint32_t _soundId{};
        bool _assumeSuccess{};
        std::int8_t _state{};
        std::uint16_t _reserved{};
    };
    static_assert(sizeof(NativeSoundHandle) == 0x08);
    static_assert(std::is_standard_layout_v<NativeSoundHandle>);

    enum class AudioStatus : std::uint8_t
    {
        Completed,
        InvalidRuntime,
        PhysicsStepActive,
        PhysicsStepStateUnavailable,
        InvalidManager,
        InvalidDescriptor,
        InvalidSceneObject,
        NonFinitePosition,
        InvalidDistance,
        InvalidVolume,
        InvalidHandle,
        HandleAlreadyActive,
        FunctionUnavailable,
        NativeRejected,
        NativeFault,
    };

    struct AudioResult
    {
        AudioStatus status{ AudioStatus::InvalidRuntime };
        std::uint32_t soundIdBefore{};
        std::uint32_t soundIdAfter{};
        std::uintptr_t functionAddress{};
        bool invoked{};
        bool accepted{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return status == AudioStatus::Completed;
        }
    };

    struct StartFollowingResult
    {
        AudioStatus status{ AudioStatus::InvalidRuntime };
        std::uintptr_t managerAddress{};
        std::uintptr_t distanceFunctionAddress{};
        std::uintptr_t playFunctionAddress{};
        float distanceSquared{};
        float listenerDistance{};
        bool playInvoked{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return status == AudioStatus::Completed;
        }
    };

    [[nodiscard]] std::string_view toString(AudioStatus status) noexcept;

    /**
     * Low-level FO4VR audio boundary used by ROCK Interactive Objects. A valid
     * NativeSoundHandle owns an engine playback registration; the consumer must
     * call fadeOutAndRelease on the owning game thread before discarding or
     * overwriting it. The framework never silently replaces an active handle.
     */
    class AudioApi
    {
    public:
        explicit AudioApi(RuntimeModule module) noexcept : _module(module) {}

        [[nodiscard]] StartFollowingResult playFollowingDescriptor(
            NativeSoundHandle& handle,
            void* descriptor,
            void* sceneObject,
            const Scene::Point3& worldPosition) const noexcept;
        [[nodiscard]] AudioResult setVolume(NativeSoundHandle& handle, float volume) const noexcept;
        [[nodiscard]] AudioResult fadeInPlay(
            NativeSoundHandle& handle,
            std::uint16_t milliseconds) const noexcept;
        [[nodiscard]] AudioResult fadeOutAndRelease(
            NativeSoundHandle& handle,
            std::uint16_t milliseconds) const noexcept;

    private:
        [[nodiscard]] AudioStatus executionStatus() const noexcept;

        RuntimeModule _module{};
    };
}
