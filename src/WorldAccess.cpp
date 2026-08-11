#include "RPS/Runtime/WorldAccess.h"

#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/Memory.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace RPS::Runtime::Physics
{
    namespace
    {
        using WorldAccessFunction = void (*)(void*);

        [[nodiscard]] bool invokeWorldAccess(const WorldAccessFunction function, void* const value) noexcept
        {
            if (!function || !value) {
                return false;
            }
#if defined(_MSC_VER)
            __try {
                function(value);
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
#else
            function(value);
            return true;
#endif
        }
    }

    bool currentThreadInsidePhysicsStep(const RuntimeModule& module) noexcept
    {
        if (!module) {
            return false;
        }

        std::uint32_t tlsIndex{};
        const auto indexAddress = module.resolve(Addresses::Symbol::Memory_BethesdaTlsIndex);
        if (indexAddress == 0 || !Memory::read(reinterpret_cast<const void*>(indexAddress), tlsIndex)) {
            return false;
        }

        const auto tlsBase = reinterpret_cast<std::uintptr_t>(TlsGetValue(tlsIndex));
        std::uint8_t flag{};
        return tlsBase != 0 &&
               Memory::read(
                   reinterpret_cast<const void*>(tlsBase + Addresses::Layouts::Havok::ExeTls_InPhysicsStepFlag), flag) &&
               flag != 0;
    }

    WorldReadGuard::WorldReadGuard(const RuntimeModule& module, void* const hknpWorld) noexcept :
        _module(&module), _world(hknpWorld)
    {
        if (!module || !hknpWorld) {
            return;
        }
        if (currentThreadInsidePhysicsStep(module)) {
            _mode = ReadAccessMode::PhysicsStepOwned;
            return;
        }

        _lock = reinterpret_cast<void*>(
            reinterpret_cast<std::uintptr_t>(hknpWorld) + Addresses::Layouts::Havok::HknpWorld_AccessLock);
        const auto lock = module.resolveFunction<WorldAccessFunction>(Addresses::Symbol::World_LockForRead);
        if (invokeWorldAccess(lock, _lock)) {
            _mode = ReadAccessMode::ReadLockHeld;
        }
    }

    WorldReadGuard::~WorldReadGuard() noexcept
    {
        if (_mode == ReadAccessMode::ReadLockHeld && _module && *_module) {
            const auto unlock = _module->resolveFunction<WorldAccessFunction>(Addresses::Symbol::World_UnlockForRead);
            (void)invokeWorldAccess(unlock, _lock);
        }
        _mode = ReadAccessMode::None;
    }

    WorldWriteGuard::WorldWriteGuard(const RuntimeModule& module, void* const hknpWorld) noexcept :
        _module(&module), _world(hknpWorld)
    {
        if (!module || !hknpWorld) {
            return;
        }
        const auto mark = module.resolveFunction<WorldAccessFunction>(Addresses::Symbol::World_MarkForWrite);
        _marked = invokeWorldAccess(mark, hknpWorld);
    }

    WorldWriteGuard::~WorldWriteGuard() noexcept
    {
        if (_marked && _module && *_module) {
            const auto unmark = _module->resolveFunction<WorldAccessFunction>(Addresses::Symbol::World_UnmarkForWrite);
            (void)invokeWorldAccess(unmark, _world);
        }
        _marked = false;
    }
}
