#include "RPS/Runtime/Memory.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>

namespace RPS::Runtime::Memory
{
    namespace
    {
        [[nodiscard]] bool readable(const DWORD protection) noexcept
        {
            const auto base = protection & 0xFFu;
            return base == PAGE_READONLY || base == PAGE_READWRITE || base == PAGE_WRITECOPY ||
                   base == PAGE_EXECUTE_READ || base == PAGE_EXECUTE_READWRITE || base == PAGE_EXECUTE_WRITECOPY;
        }

        [[nodiscard]] bool writable(const DWORD protection) noexcept
        {
            const auto base = protection & 0xFFu;
            return base == PAGE_READWRITE || base == PAGE_WRITECOPY || base == PAGE_EXECUTE_READWRITE ||
                   base == PAGE_EXECUTE_WRITECOPY;
        }

        [[nodiscard]] bool executable(const DWORD protection) noexcept
        {
            const auto base = protection & 0xFFu;
            return base == PAGE_EXECUTE || base == PAGE_EXECUTE_READ || base == PAGE_EXECUTE_READWRITE ||
                   base == PAGE_EXECUTE_WRITECOPY;
        }

        [[nodiscard]] bool protectionAllows(const DWORD protection, const Access access) noexcept
        {
            if ((protection & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
                return false;
            }
            switch (access) {
            case Access::Read:
                return readable(protection);
            case Access::Write:
                return writable(protection);
            case Access::Execute:
                return executable(protection);
            default:
                return false;
            }
        }
    }

    bool rangeHasAccess(const void* address, const std::size_t bytes, const Access access) noexcept
    {
        if (!address || bytes == 0) {
            return false;
        }

        const auto start = reinterpret_cast<std::uintptr_t>(address);
        if (start > (std::numeric_limits<std::uintptr_t>::max)() - bytes) {
            return false;
        }
        const auto end = start + bytes;
        auto cursor = start;

        while (cursor < end) {
            MEMORY_BASIC_INFORMATION information{};
            if (VirtualQuery(reinterpret_cast<const void*>(cursor), &information, sizeof(information)) != sizeof(information) ||
                information.State != MEM_COMMIT || !protectionAllows(information.Protect, access)) {
                return false;
            }

            const auto regionStart = reinterpret_cast<std::uintptr_t>(information.BaseAddress);
            if (regionStart > (std::numeric_limits<std::uintptr_t>::max)() - information.RegionSize) {
                return false;
            }
            const auto regionEnd = regionStart + information.RegionSize;
            if (regionEnd <= cursor) {
                return false;
            }
            cursor = std::min(end, regionEnd);
        }
        return true;
    }

    bool copyFrom(const void* source, void* destination, const std::size_t bytes) noexcept
    {
        if (!destination || !rangeHasAccess(source, bytes, Access::Read)) {
            return false;
        }
#if defined(_MSC_VER)
        __try {
            std::memcpy(destination, source, bytes);
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
#else
        std::memcpy(destination, source, bytes);
        return true;
#endif
    }

    bool copyTo(void* destination, const void* source, const std::size_t bytes) noexcept
    {
        if (!source || !rangeHasAccess(destination, bytes, Access::Write)) {
            return false;
        }
#if defined(_MSC_VER)
        __try {
            std::memcpy(destination, source, bytes);
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
#else
        std::memcpy(destination, source, bytes);
        return true;
#endif
    }
}
