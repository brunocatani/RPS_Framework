// The INI keys a mod shipped before its TOML settings files, and what each
// became. A mod lists them in a frozen table; Store carries a player's changed
// values into the TOML files once and then deletes the INIs.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace RPS::Settings
{
    enum class LegacyKind : std::uint8_t
    {
        Value,           // Same value, new table and key.
        Float3Component, // One of the X/Y/Z or pitch/yaw/roll keys of a list (component 0..2).
        BooleanToString, // A boolean that became one of two strings: trueText or falseText.
    };

    // An old INI key, the option it became, and its default in up to two
    // shipped versions, as the old INI wrote them. A value equal to an old
    // default was never changed by the player and is not carried over, so the
    // current default applies.
    struct LegacyKey
    {
        std::string_view oldKey;
        std::string_view table;
        std::string_view key;
        LegacyKind kind = LegacyKind::Value;
        std::uint8_t component = 0;
        std::optional<std::string_view> oldDefault;
        std::optional<std::string_view> otherOldDefault;
        std::string_view trueText;
        std::string_view falseText;
    };

    struct LegacyIni
    {
        // Read in order (player file, then developer file); a later key wins,
        // as in the INI loaders. Section headers were labels only and are ignored.
        std::vector<std::filesystem::path> files;
        std::span<const LegacyKey> keys;
    };
}
