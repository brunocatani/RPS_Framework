#include "RPS/Addresses/Catalog.h"

#include <array>

namespace RPS::Addresses
{
    namespace
    {
        constexpr auto Entries = std::to_array<SymbolRecord>({
#define RPS_SYMBOL(name, rva, kind, subsystem, source) \
    SymbolRecord{ Symbol::name, #name, rva, SymbolKind::kind, Subsystem::subsystem, SourceProject::source },
#include "RPS/Addresses/Fallout4Vr_1_2_72.inc"
#undef RPS_SYMBOL
        });

        static_assert(Entries.size() == static_cast<std::size_t>(Symbol::Count));
    }

    std::span<const SymbolRecord> catalog() noexcept
    {
        return Entries;
    }

    const SymbolRecord& record(const Symbol symbol) noexcept
    {
        const auto index = static_cast<std::size_t>(symbol);
        if (index >= Entries.size()) {
            return Entries.front();
        }
        return Entries[index];
    }

    const SymbolRecord* find(const std::string_view name) noexcept
    {
        for (const auto& entry : Entries) {
            if (entry.name == name) {
                return &entry;
            }
        }
        return nullptr;
    }

    const SymbolRecord* findByRva(const std::uint32_t rva) noexcept
    {
        for (const auto& entry : Entries) {
            if (entry.rva == rva) {
                return &entry;
            }
        }
        return nullptr;
    }

    std::string_view toString(const Symbol symbol) noexcept
    {
        return record(symbol).name;
    }
}
