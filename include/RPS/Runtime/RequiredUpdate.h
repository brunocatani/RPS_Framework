#pragma once

// Required-update stop.
//
// A mod that finds another mod it cannot work with (a consumer built for an
// older provider API, or a provider version this mod does not support) stops
// the game on purpose: a Windows error box names every mod to update, and the
// game closes when the box is closed. A silent refusal would leave the player
// with features that quietly do nothing, so a version mismatch never keeps
// running.
//
// Threads and lifetime:
// - report(), moduleFileNameAt() and firstFromCaller() may run on any thread
//   (provider discovery runs on any thread).
// - watchMainMenu() runs once from the F4SE GameLoaded or GameDataReady
//   message, which F4SEVR sends from the game-data thread before the main menu
//   opens. Its sink runs on the thread that sends menu events and stays
//   registered for the process lifetime, like the DLL that owns it.
// - Reports made before the main menu first opens are shown when it opens. By
//   then every plugin has handled those messages, where mods connect to their
//   providers, so one box names every mismatch this DLL found. A report made
//   later, or after the main menu was already open, is shown at once.
// - The state lives in this header, so each DLL that includes it has its own.
//   The first DLL to show its box ends the process when the box is closed.
//
// Include it after CommonLibF4's headers (a PCH) or before <Windows.h>: the
// RE headers do not compile after <Windows.h>.

#include <RE/Fallout.h>

#include <Windows.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace RPS::Runtime::RequiredUpdate
{
    // Flushes the calling mod's log before the process ends.
    using FlushLog = void (*)() noexcept;

    namespace detail
    {
        inline constexpr std::size_t kMaxReports = 16;
        inline constexpr std::size_t kMaxCallers = 32;
        // On top of every window, as other startup requirement boxes on this game.
        inline constexpr UINT kErrorBoxFlags = MB_OK | MB_ICONERROR | MB_SYSTEMMODAL | MB_SETFOREGROUND;
        inline constexpr std::wstring_view kClosing = L"The game closes when this message is closed.";

        struct State
        {
            std::mutex mutex;
            std::vector<std::wstring> reports;                 // guarded by mutex
            std::wstring title;                                // guarded by mutex
            FlushLog flushLog{ nullptr };                      // guarded by mutex
            std::array<const void*, kMaxCallers> callers{};    // guarded by mutex
            std::atomic<bool> mainMenuSeen{ false };
            std::atomic<bool> stopping{ false };
        };

        inline State& state() noexcept
        {
            static State instance;
            return instance;
        }

        // Shows every report and ends the process. Returns only when nothing
        // was reported or another thread of this DLL is already showing it.
        inline void stopIfReported() noexcept
        {
            auto& s = state();
            std::wstring text;
            std::wstring title;
            FlushLog flushLog{ nullptr };
            try {
                const std::scoped_lock lock{ s.mutex };
                if (s.reports.empty())
                    return;
                for (const auto& report : s.reports) {
                    text += report;
                    text += L"\n\n";
                }
                text += kClosing;
                title = s.title.empty() ? std::wstring{ L"Update required" } : s.title;
                flushLog = s.flushLog;
            } catch (...) {
                // Out of memory: end the process anyway; the mod logged the reason.
                text.clear();
            }
            if (s.stopping.exchange(true))
                return;
            if (flushLog)
                flushLog();
            if (!text.empty())
                ::MessageBoxW(nullptr, text.c_str(), title.c_str(), kErrorBoxFlags);
            ::TerminateProcess(::GetCurrentProcess(), 1);
        }

        class MainMenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
        {
        public:
            RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent& event,
                RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
            {
                const char* name = event.menuName.c_str();
                if (event.opening && name && std::string_view{ name } == "MainMenu") {
                    state().mainMenuSeen.store(true);
                    stopIfReported();
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        };

        inline MainMenuSink& sink() noexcept
        {
            static MainMenuSink instance;
            return instance;
        }
    }

    // File name (no folder) of the loaded DLL containing address; empty when
    // none. Providers pass the return address of their discovery export, which
    // lies in the calling mod.
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

    // True the first time a call site is seen. An outdated mod usually retries
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
        return false;  // table full: those mods are already reported
    }

    // Any thread. Adds one paragraph naming a mod and what to update; repeats
    // of the same paragraph are ignored. Log the reason before calling.
    inline void report(std::wstring paragraph) noexcept
    {
        auto& s = detail::state();
        try {
            const std::scoped_lock lock{ s.mutex };
            for (const auto& existing : s.reports)
                if (existing == paragraph)
                    return;
            if (s.reports.size() >= detail::kMaxReports)
                return;
            s.reports.push_back(std::move(paragraph));
        } catch (...) {
            return;
        }
        if (s.mainMenuSeen.load())
            detail::stopIfReported();
    }

    // Once, from the F4SE GameLoaded or GameDataReady message. title is the
    // error box caption.
    // Returns false when the UI is unavailable; reports then wait, and the mod
    // should log that the stop cannot be shown.
    inline bool watchMainMenu(std::wstring title, FlushLog flushLog = nullptr) noexcept
    {
        auto& s = detail::state();
        try {
            const std::scoped_lock lock{ s.mutex };
            s.title = std::move(title);
            s.flushLog = flushLog;
        } catch (...) {
            return false;
        }
        auto* ui = RE::UI::GetSingleton();
        if (!ui)
            return false;
        static bool registered = false;
        if (registered)
            return true;
        ui->RegisterSink(&detail::sink());
        registered = true;
        // Registered after the main menu opened: its open event is gone, so
        // pending and later reports are shown at once.
        if (ui->GetMenuOpen(RE::BSFixedString{ "MainMenu" })) {
            s.mainMenuSeen.store(true);
            detail::stopIfReported();
        }
        return true;
    }
}
