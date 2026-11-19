// The TOML keys an earlier version of a mod wrote, and the option each
// became. A mod lists them in a frozen table; at startup Store carries a
// value from the old key to the new one when the file has the old key and not
// the new one, and the old key then stays only as a comment.
#pragma once

#include <cstdint>
#include <string_view>

namespace RPS::Settings
{
    enum class RenameKind : std::uint8_t
    {
        Value,                     // The same value, which must fit the new option's type.
        Float3ToTransformPosition, // A Float3 "[x, y, z]" that became a Transform's pos.
    };

    // An old key and the option it became. The old key is read from the file
    // of the new option's group.
    struct RenamedKey
    {
        std::string_view oldTable;
        std::string_view oldKey;
        std::string_view table;
        std::string_view key;
        RenameKind kind = RenameKind::Value;
    };
}
