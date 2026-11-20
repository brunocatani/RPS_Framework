// Conversions of an option's value that a mod runs once its file holds an
// older form: a string that became a table, or retired keys folded into the
// option that replaced them. A mod lists them in a frozen table; at startup
// Store runs each on the player's TOML file of the option's group, and on the
// values migrated from old INI files, and writes what it returns.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace RPS::Settings
{
    enum class ValueType : std::uint32_t;

    // The old settings a conversion reads: the keys of the TOML file it runs
    // on, or the values migrated from old INI files, including those a
    // LegacyKind::ConversionInput row carried to a key that is no option.
    class ConversionSource
    {
    public:
        // The canonical value at table.key read as type, or nullopt when the
        // source has none there or it does not fit the type.
        [[nodiscard]] virtual std::optional<std::string> value(std::string_view table, std::string_view key,
            ValueType type) const = 0;

    protected:
        ~ConversionSource() = default;
    };

    // One option's conversion. convert returns the option's new value (any
    // form normalizeValue accepts), or nullopt to leave the file as it is. It
    // must return nullopt, or the value the file already holds, once the file
    // is in the new form, so that it acts once.
    struct ValueConversion
    {
        std::string_view table;
        std::string_view key;
        std::optional<std::string> (*convert)(const ConversionSource& source) = nullptr;
    };
}
