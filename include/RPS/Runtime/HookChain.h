#pragma once

// Hook chaining: patching code another plugin has already hooked.
//
// A hook that refuses to install because another plugin got to its call site,
// vtable slot or function entry first turns a mod feature off for every player
// who runs both. These helpers recognise another plugin's detour so the hook
// can call through it instead:
// - a call site whose E8 call no longer reaches the native function, but an
//   F4SE trampoline stub or a function inside another loaded DLL;
// - a vtable slot holding another DLL's function instead of the native one;
// - a function entry whose prologue was replaced by a jump into another DLL.
// A detour counts only when its jump chain ends in executable code of a loaded
// DLL that is neither the game nor the calling DLL itself. Anything else (a
// changed game binary, our own earlier patch, unreadable memory) stays
// unrecognised and the caller fails closed as before. Raw byte patches of
// instructions cannot chain; their owners need their own answer.
//
// Threads: load-time helpers. They query memory protection and module tables,
// so never call them per frame.
//
// Header-only: the module checks use the address of an inline function, which
// lives in each DLL that includes this header. Include it after CommonLibF4's
// headers (a PCH) or before <Windows.h> is otherwise needed: the RE headers do
// not compile after <Windows.h>.

#include <Windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string>

namespace RPS::Runtime::HookChain
{
    enum class Origin : std::uint8_t
    {
        Native,
        Foreign,
        Unrecognized,
    };

    struct Resolved
    {
        Origin origin{ Origin::Unrecognized };
        // What the hook calls for the behaviour it wraps: the native function,
        // or the first address of the other plugin's detour chain.
        std::uintptr_t callee{};
        // Foreign entries only: bytes of the other plugin's jump at the entry,
        // which a chained entry patch may overwrite (5, 6 or 14).
        std::size_t entryJumpBytes{};
        // Foreign only: file name of the DLL the chain ends in, for logs.
        std::string owner;

        [[nodiscard]] explicit operator bool() const noexcept { return origin != Origin::Unrecognized; }
        [[nodiscard]] bool foreign() const noexcept { return origin == Origin::Foreign; }
    };

    namespace detail
    {
        inline constexpr std::size_t kMaximumJumps = 4;

        [[nodiscard]] inline bool readable(const std::uintptr_t address, const std::size_t length) noexcept
        {
            MEMORY_BASIC_INFORMATION region{};
            if (!address || !length ||
                VirtualQuery(reinterpret_cast<const void*>(address), &region, sizeof(region)) != sizeof(region) ||
                region.State != MEM_COMMIT || (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
                return false;
            }
            const auto end = reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize;
            return address < end && length <= end - address;
        }

        [[nodiscard]] inline bool executable(const std::uintptr_t address) noexcept
        {
            MEMORY_BASIC_INFORMATION region{};
            if (!address ||
                VirtualQuery(reinterpret_cast<const void*>(address), &region, sizeof(region)) != sizeof(region) ||
                region.State != MEM_COMMIT || (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
                return false;
            }
            const DWORD protection = region.Protect & 0xFF;
            return protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ ||
                protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
        }

        [[nodiscard]] inline HMODULE moduleAt(const std::uintptr_t address) noexcept
        {
            HMODULE module = nullptr;
            if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCWSTR>(address), &module)) {
                return nullptr;
            }
            return module;
        }

        // The DLL that includes this header: each DLL has its own copy of this function.
        [[nodiscard]] inline HMODULE selfModule() noexcept
        {
            return moduleAt(reinterpret_cast<std::uintptr_t>(&selfModule));
        }

        struct Jump
        {
            std::uintptr_t destination{};
            std::size_t bytes{};
        };

        // E9 rel32, or FF 25 rel32 through a pointer (with rel32 = 0 the pointer
        // follows inline: F4SE's 14-byte stub). Anything else is not a jump.
        [[nodiscard]] inline std::optional<Jump> decodeJump(const std::uintptr_t address) noexcept
        {
            if (!readable(address, 6)) {
                return std::nullopt;
            }
            std::array<std::uint8_t, 6> bytes{};
            std::memcpy(bytes.data(), reinterpret_cast<const void*>(address), bytes.size());
            std::int32_t displacement{};
            if (bytes[0] == 0xE9) {
                std::memcpy(&displacement, bytes.data() + 1, sizeof(displacement));
                return Jump{ address + 5 + static_cast<std::intptr_t>(displacement), 5 };
            }
            if (bytes[0] != 0xFF || bytes[1] != 0x25) {
                return std::nullopt;
            }
            std::memcpy(&displacement, bytes.data() + 2, sizeof(displacement));
            const auto cell = address + 6 + static_cast<std::intptr_t>(displacement);
            std::uintptr_t destination{};
            if (!readable(cell, sizeof(destination))) {
                return std::nullopt;
            }
            std::memcpy(&destination, reinterpret_cast<const void*>(cell), sizeof(destination));
            return Jump{ destination, displacement == 0 ? std::size_t{ 14 } : std::size_t{ 6 } };
        }

        [[nodiscard]] inline std::string fileName(const HMODULE module)
        {
            std::array<char, MAX_PATH> path{};
            const auto length = GetModuleFileNameA(module, path.data(), static_cast<DWORD>(path.size()));
            std::string name(path.data(), length);
            const auto slash = name.find_last_of("\\/");
            return slash == std::string::npos ? name : name.substr(slash + 1);
        }

        // Where `address` leads after the jumps in front of it, and the DLL that
        // code belongs to, when that is another plugin's DLL.
        [[nodiscard]] inline std::optional<std::string> foreignOwner(std::uintptr_t address) noexcept
        {
            try {
                const auto game = GetModuleHandleW(nullptr);
                const auto self = selfModule();
                for (std::size_t hop = 0; hop <= kMaximumJumps; ++hop) {
                    if (!executable(address)) {
                        return std::nullopt;
                    }
                    const auto owner = moduleAt(address);
                    if (owner == game || owner == self) {
                        return std::nullopt;
                    }
                    if (owner) {
                        return fileName(owner);
                    }
                    // Unowned executable memory: a trampoline stub, which must jump on.
                    const auto jump = decodeJump(address);
                    if (!jump) {
                        return std::nullopt;
                    }
                    address = jump->destination;
                }
            } catch (...) {
            }
            return std::nullopt;
        }

        [[nodiscard]] inline std::optional<std::uintptr_t> directCallTarget(const std::uintptr_t callsite) noexcept
        {
            if (!readable(callsite, 5) || *reinterpret_cast<const std::uint8_t*>(callsite) != 0xE8) {
                return std::nullopt;
            }
            std::int32_t displacement{};
            std::memcpy(&displacement, reinterpret_cast<const void*>(callsite + 1), sizeof(displacement));
            return callsite + 5 + static_cast<std::intptr_t>(displacement);
        }

        [[nodiscard]] inline Resolved foreignAt(const std::uintptr_t callee)
        {
            auto owner = foreignOwner(callee);
            if (!owner) {
                return {};
            }
            return { .origin = Origin::Foreign, .callee = callee, .owner = std::move(*owner) };
        }
    }

    // A direct call (E8) we redirect. Native when it still calls
    // `nativeTarget`; Foreign when it calls into another plugin's detour.
    [[nodiscard]] inline Resolved resolveCall(const std::uintptr_t callsite, const std::uintptr_t nativeTarget) noexcept
    {
        try {
            const auto target = detail::directCallTarget(callsite);
            if (!target) {
                return {};
            }
            if (*target == nativeTarget) {
                return { .origin = Origin::Native, .callee = nativeTarget };
            }
            return detail::foreignAt(*target);
        } catch (...) {
            return {};
        }
    }

    // A vtable slot we swap. Native when it holds `nativeTarget`; Foreign when it
    // holds another plugin's function.
    [[nodiscard]] inline Resolved resolveSlot(const std::uintptr_t slot, const std::uintptr_t nativeTarget) noexcept
    {
        try {
            std::uintptr_t value{};
            if (!detail::readable(slot, sizeof(value))) {
                return {};
            }
            std::memcpy(&value, reinterpret_cast<const void*>(slot), sizeof(value));
            if (value == nativeTarget) {
                return { .origin = Origin::Native, .callee = nativeTarget };
            }
            return detail::foreignAt(value);
        } catch (...) {
            return {};
        }
    }

    // A function entry we detour, or only call. Native when the prologue
    // matches; Foreign when another plugin's jump sits at the entry. For a
    // foreign entry, `callee` is that jump's destination: a chained detour calls
    // it as the original and may overwrite only `entryJumpBytes` bytes. A
    // function we only call stays callable at its entry either way.
    [[nodiscard]] inline Resolved resolveEntry(const std::uintptr_t entry,
        const std::span<const std::uint8_t> nativePrologue) noexcept
    {
        try {
            if (nativePrologue.empty() || !detail::readable(entry, nativePrologue.size())) {
                return {};
            }
            if (std::memcmp(reinterpret_cast<const void*>(entry), nativePrologue.data(), nativePrologue.size()) == 0) {
                return { .origin = Origin::Native, .callee = entry };
            }
            const auto jump = detail::decodeJump(entry);
            if (!jump) {
                return {};
            }
            auto resolved = detail::foreignAt(jump->destination);
            resolved.entryJumpBytes = resolved ? jump->bytes : 0;
            return resolved;
        } catch (...) {
            return {};
        }
    }

    // True when the jumps at `from` (an entry or stub, at most a few) lead to
    // `destination`: checks that a patch, chained or not, still reaches a hook.
    [[nodiscard]] inline bool reaches(std::uintptr_t from, const std::uintptr_t destination) noexcept
    {
        for (std::size_t hop = 0; hop <= detail::kMaximumJumps; ++hop) {
            if (from == destination) {
                return true;
            }
            const auto jump = detail::decodeJump(from);
            if (!jump) {
                return false;
            }
            from = jump->destination;
        }
        return from == destination;
    }

    [[nodiscard]] inline const char* originName(const Origin origin) noexcept
    {
        switch (origin) {
        case Origin::Native:
            return "native";
        case Origin::Foreign:
            return "chained";
        default:
            return "unrecognized";
        }
    }
}
