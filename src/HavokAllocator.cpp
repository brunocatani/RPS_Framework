#include "RPS/Runtime/HavokAllocator.h"

#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/Memory.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <cstdint>

namespace RPS::Runtime
{
    void* HavokAllocator::currentThreadAllocator() const noexcept
    {
        if (!_module) {
            return nullptr;
        }
        std::uint32_t tlsIndex{};
        const auto tlsIndexAddress = _module.resolve(Addresses::Symbol::Memory_HavokTlsAllocKey);
        if (tlsIndexAddress == 0 || !Memory::read(reinterpret_cast<const void*>(tlsIndexAddress), tlsIndex)) {
            return nullptr;
        }

        const auto tlsBlock = reinterpret_cast<std::uintptr_t>(TlsGetValue(tlsIndex));
        void* allocator{};
        if (tlsBlock == 0 ||
            !Memory::read(
                reinterpret_cast<const void*>(tlsBlock + Addresses::Layouts::Havok::HkThreadMemory_Allocator), allocator)) {
            return nullptr;
        }
        return allocator;
    }

    void* HavokAllocator::allocate(const std::size_t bytes) const noexcept
    {
        void* const allocator = bytes != 0 ? currentThreadAllocator() : nullptr;
        void** vtable{};
        void* entry{};
        if (!allocator || !Memory::read(allocator, vtable) || !vtable || !Memory::read(vtable + 1, entry) ||
            !Memory::rangeHasAccess(entry, 1, Memory::Access::Execute)) {
            return nullptr;
        }

        const auto allocateFunction = reinterpret_cast<void* (*)(void*, std::size_t)>(entry);
#if defined(_MSC_VER)
        __try {
            return allocateFunction(allocator, bytes);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return nullptr;
        }
#else
        return allocateFunction(allocator, bytes);
#endif
    }

    bool HavokAllocator::deallocate(void* const allocation, const std::size_t bytes) const noexcept
    {
        void* const allocator = allocation && bytes != 0 ? currentThreadAllocator() : nullptr;
        void** vtable{};
        void* entry{};
        if (!allocator || !Memory::read(allocator, vtable) || !vtable || !Memory::read(vtable + 2, entry) ||
            !Memory::rangeHasAccess(entry, 1, Memory::Access::Execute)) {
            return false;
        }

        const auto freeFunction = reinterpret_cast<void (*)(void*, void*, std::size_t)>(entry);
#if defined(_MSC_VER)
        __try {
            freeFunction(allocator, allocation, bytes);
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
#else
        freeFunction(allocator, allocation, bytes);
        return true;
#endif
    }

    bool HavokAllocator::reserveArray(void* const arrayHeader, const int elementBytes) const noexcept
    {
        if (!_module || !arrayHeader || elementBytes <= 0) {
            return false;
        }
        using Function = void (*)(void*, void*, int);
        const auto function = _module.resolveFunction<Function>(Addresses::Symbol::Memory_HkArrayReserveMore);
        const auto allocatorGlobal = _module.resolve(Addresses::Symbol::Memory_HkArrayAllocatorGlobal);
        if (!function || allocatorGlobal == 0 || !Memory::rangeHasAccess(
                reinterpret_cast<const void*>(function), 1, Memory::Access::Execute)) {
            return false;
        }
#if defined(_MSC_VER)
        __try {
            function(reinterpret_cast<void*>(allocatorGlobal), arrayHeader, elementBytes);
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
#else
        function(reinterpret_cast<void*>(allocatorGlobal), arrayHeader, elementBytes);
        return true;
#endif
    }
}
