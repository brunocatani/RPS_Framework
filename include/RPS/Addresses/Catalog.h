#pragma once

#include "RPS/Addresses/RuntimeVersion.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace RPS::Addresses
{
    enum class SymbolKind : std::uint8_t
    {
        Function,
        Data,
        Vtable,
        Callsite,
        HookSite,
        PatchSite,
    };

    enum class Subsystem : std::uint8_t
    {
        Runtime,
        Memory,
        Physics,
        Collision,
        Constraint,
        Scene,
        Animation,
        Input,
        Ragdoll,
        Character,
        Rendering,
        Audio,
    };

    enum class Evidence : std::uint8_t
    {
        InGameProven,
    };

    enum class SourceProject : std::uint8_t
    {
        Rock,
        Paper,
        PaperToolkit,
        Scissors,
        RockAddon,
    };

    enum class Symbol : std::uint16_t
    {
#define RPS_SYMBOL(name, rva, kind, subsystem, source) name,
#include "RPS/Addresses/Fallout4Vr_1_2_72.inc"
#undef RPS_SYMBOL
        Count,
    };

    struct SymbolRecord
    {
        Symbol symbol{};
        std::string_view name{};
        std::uint32_t rva{};
        SymbolKind kind{};
        Subsystem subsystem{};
        SourceProject source{};
        Evidence evidence{ Evidence::InGameProven };
        RuntimeVersion runtime{ Fallout4Vr_1_2_72 };
    };

    [[nodiscard]] std::span<const SymbolRecord> catalog() noexcept;
    [[nodiscard]] const SymbolRecord& record(Symbol symbol) noexcept;
    [[nodiscard]] const SymbolRecord* find(std::string_view name) noexcept;
    [[nodiscard]] const SymbolRecord* findByRva(std::uint32_t rva) noexcept;
    [[nodiscard]] std::string_view toString(Symbol symbol) noexcept;
}
