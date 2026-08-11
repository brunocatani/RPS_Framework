#pragma once

#include "RPS/Runtime/RuntimeModule.h"

#include <cstdint>

namespace RPS::Runtime::Physics
{
    enum class ReadAccessMode : std::uint8_t
    {
        None,
        PhysicsStepOwned,
        ReadLockHeld,
    };

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

        [[nodiscard]] bool active() const noexcept { return _marked; }
        [[nodiscard]] bool owns(const void* world) const noexcept { return _marked && _world == world; }

    private:
        const RuntimeModule* _module{};
        void* _world{};
        bool _marked{};
    };
}
