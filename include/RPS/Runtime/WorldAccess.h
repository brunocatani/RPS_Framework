#pragma once

#include "RPS/Runtime/RuntimeModule.h"

#include <cstdint>

namespace RPS::Runtime::Physics
{
    enum class PhysicsStepState : std::uint8_t
    {
        Unknown,
        Outside,
        Inside,
    };

    enum class ReadAccessMode : std::uint8_t
    {
        None,
        PhysicsStepOwned,
        ReadLockHeld,
    };

    enum class WriteAccessMode : std::uint8_t
    {
        None,
        PhysicsStepOwned,
        WriteMarked,
    };

    /*
     * This thread's block of the executable's implicit (static) TLS, which
     * holds Bethesda per-thread state such as the physics-step flag and the
     * allocator context. The loader keeps it in the thread's TLS array
     * (gs:[0x58]) at the executable's _tls_index. It is not a TlsAlloc slot:
     * TlsGetValue with that index reads an unrelated slot. 0 when unavailable.
     */
    [[nodiscard]] std::uintptr_t currentThreadExecutableTlsBlock(const RuntimeModule& module) noexcept;
    [[nodiscard]] PhysicsStepState currentThreadPhysicsStepState(const RuntimeModule& module) noexcept;
    [[nodiscard]] bool currentThreadInsidePhysicsStep(const RuntimeModule& module) noexcept;

    class WorldReadGuard
    {
    public:
        WorldReadGuard(const RuntimeModule& module, void* hknpWorld) noexcept;
        ~WorldReadGuard() noexcept;

        WorldReadGuard(const WorldReadGuard&) = delete;
        WorldReadGuard& operator=(const WorldReadGuard&) = delete;
        WorldReadGuard(WorldReadGuard&&) = delete;
        WorldReadGuard& operator=(WorldReadGuard&&) = delete;

        [[nodiscard]] bool active() const noexcept { return _mode != ReadAccessMode::None; }
        [[nodiscard]] bool owns(const void* world) const noexcept { return active() && _world == world; }
        [[nodiscard]] ReadAccessMode mode() const noexcept { return _mode; }

    private:
        const RuntimeModule* _module{};
        void* _world{};
        void* _lock{};
        ReadAccessMode _mode{ ReadAccessMode::None };
    };

    class WorldWriteGuard
    {
    public:
        WorldWriteGuard(const RuntimeModule& module, void* hknpWorld) noexcept;
        ~WorldWriteGuard() noexcept;

        WorldWriteGuard(const WorldWriteGuard&) = delete;
        WorldWriteGuard& operator=(const WorldWriteGuard&) = delete;
        WorldWriteGuard(WorldWriteGuard&&) = delete;
        WorldWriteGuard& operator=(WorldWriteGuard&&) = delete;

        [[nodiscard]] bool active() const noexcept { return _mode != WriteAccessMode::None; }
        [[nodiscard]] bool owns(const void* world) const noexcept { return active() && _world == world; }
        [[nodiscard]] WriteAccessMode mode() const noexcept { return _mode; }

    private:
        const RuntimeModule* _module{};
        void* _world{};
        WriteAccessMode _mode{ WriteAccessMode::None };
    };
}
