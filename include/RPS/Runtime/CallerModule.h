#pragma once

// Names the mod behind a provider call, for the provider's log.
//
// A provider passes the return address of its discovery export, which lies in
// the calling mod's DLL. Both functions may run on any thread. The seen-caller
// table lives in this header, so each DLL that includes it has its own.

#include <Windows.h>

#include <array>
#include <mutex>
#include <string>
#include <string_view>

namespace RPS::Runtime::CallerModule
{
    namespace detail
    {
        inline constexpr std::size_t kMaxCallers = 32;

        struct State
        {
            std::mutex mutex;
            std::array<const void*, kMaxCallers> callers{};  // guarded by mutex
        };

        inline State& state() noexcept
        {
            static State instance;
            return instance;
        }
    }

    // File name (no folder) of the loaded DLL containing address; empty when
    // none.
    inline std::wstring moduleFileNameAt(const void* address) noexcept
    {
        HMODULE module = nullptr;
        if (!address ||
            !::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                static_cast<LPCWSTR>(address), &module) ||
            !module)
            return {};
        std::array<wchar_t, MAX_PATH> path{};
        const auto length = ::GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0 || length >= path.size())
            return {};
        try {
            const std::wstring_view full{ path.data(), length };
            const auto slash = full.find_last_of(L"\\/");
            return std::wstring{ slash == std::wstring_view::npos ? full : full.substr(slash + 1) };
        } catch (...) {
            return {};
        }
    }

    // moduleFileNameAt for a log line: characters outside ASCII become '?', and
    // an unknown module is named as such.
    inline std::string moduleNameForLog(const void* address) noexcept
    {
        try {
            const auto name = moduleFileNameAt(address);
            if (name.empty())
                return "an unidentified module";
            std::string result;
            result.reserve(name.size());
            for (const wchar_t c : name)
                result.push_back(c > 0 && c < 0x80 ? static_cast<char>(c) : '?');
            return result;
        } catch (...) {
            return {};
        }
    }

    // True the first time a call site is seen. A refused mod usually retries
    // every frame; this keeps the module lookup and the log line to once.
    inline bool firstFromCaller(const void* address) noexcept
    {
        auto& s = detail::state();
        const std::scoped_lock lock{ s.mutex };
        for (auto& seen : s.callers) {
            if (seen == address)
                return false;
            if (!seen) {
                seen = address;
                return true;
            }
        }
        return false;  // table full: those callers are already logged
    }
}
