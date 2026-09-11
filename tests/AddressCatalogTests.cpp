#include "RPS/Addresses/Catalog.h"

#include <iostream>
#include <string_view>
#include <unordered_set>

int main()
{
    using namespace RPS::Addresses;

    const auto entries = catalog();
    if (entries.size() != static_cast<std::size_t>(Symbol::Count) || entries.size() < 150) {
        std::cerr << "catalog size contract failed\n";
        return 1;
    }

    std::unordered_set<std::string_view> names;
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const auto& entry = entries[index];
        if (static_cast<std::size_t>(entry.symbol) != index || entry.name.empty() || entry.rva == 0 ||
            entry.runtime != Fallout4Vr_1_2_72 || entry.evidence != Evidence::InGameProven || !names.insert(entry.name).second) {
            std::cerr << "invalid catalog record at index " << index << '\n';
            return 1;
        }
        if (find(entry.name) != &entry || toString(entry.symbol) != entry.name) {
            std::cerr << "catalog lookup contract failed for " << entry.name << '\n';
            return 1;
        }
    }

    const auto& velocity = record(Symbol::Physics_SetBodyVelocityDeferred);
    if (velocity.rva != 0x1DF56F0 || velocity.kind != SymbolKind::Function ||
        velocity.subsystem != Subsystem::Physics || velocity.source != SourceProject::Rock) {
        std::cerr << "known physics symbol contract failed\n";
        return 1;
    }
    if (find("not-a-symbol") != nullptr || findByRva(0) != nullptr) {
        std::cerr << "missing lookup contract failed\n";
        return 1;
    }

    return 0;
}
