// TOML settings files for a mod: one player file and an optional developer
// file under Documents\My Games\Fallout4VR\Mods_Config\<Mod>.
//
// The mod declares every option once (table path, key, category, help, file,
// type and default). Startup and every write organize each file the same way:
// every recognized key once, grouped by table in declaration order under its
// category heading, preceded by its help as "##" lines. Single-"#" lines above
// a key are the player's notes and stay with it. The player file gains every
// missing option at its default. Entries the mod does not read from a file
// (retired options, options of the other file, foreign keys) become "#"
// comment lines in a trailing block and never apply. A recognized value of
// the wrong type is kept as written, reported with its line, and the option
// keeps its last value. A file that does not parse is never rewritten and its
// options keep the last values read from it. A Table option is one inline
// table whose keys the mod reads itself; the store keeps the player's text.
//
// The developer file is never created by loading: it holds only options that
// differ from their defaults and is removed when none remain.
//
// When the player file does not exist at startup, the mod's old INI files are
// migrated once (LegacyIni.h) and deleted after the TOML files read back.
// Keys a later version renamed are carried to their new option at startup
// (RenamedKey.h), and values in an older form are converted (Conversion.h).
//
// Not thread-safe: the owning mod serializes load and writes.
#pragma once

#include "RPS/Settings/Conversion.h"
#include "RPS/Settings/LegacyIni.h"
#include "RPS/Settings/RenamedKey.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace RPS::Settings
{
    enum class ValueType : std::uint32_t
    {
        Boolean,
        Integer,
        Float,
        String,
        Float3,    // Three numbers, written "[x, y, z]".
        Transform, // An inline table "{ pos = [x, y, z], rot = [heading, roll, attitude], scale = s }", parts optional.
        Table,     // Any inline table, kept as written; the mod reads its keys.
    };

    // A Transform value as F4VR-CommonFramework reads it: each part is
    // optional, and a missing part means no move, no rotation or a scale of 1.
    struct Transform
    {
        std::optional<std::array<float, 3>> position;
        std::optional<std::array<float, 3>> rotation; // [heading, roll, attitude] in degrees.
        std::optional<float> scale;
    };

    struct TableEntry;

    // One value of a Table option, as parseTable reads it. std::monostate is
    // a value of another kind (an array, a date or a time).
    struct TableValue
    {
        std::variant<std::monostate, bool, std::int64_t, double, std::string, std::vector<TableEntry>> value;
    };

    struct TableEntry
    {
        std::string key;
        TableValue value;
    };

    enum class Group : std::uint32_t
    {
        Player,
        Developer,
    };

    // One option as the mod declares it. defaultValue is canonical text.
    struct Definition
    {
        std::string table; // TOML table path, for example "ControllerInput.Left".
        std::string key;
        std::string category;
        std::string description; // Help, one "\n"-separated line per "##" line.
        Group group = Group::Player;
        ValueType type = ValueType::String;
        std::string defaultValue;
    };

    struct Setting
    {
        std::string table;
        std::string key;
        std::string defaultValue;
        std::string value;
        std::string category;
        std::string description;
        Group group = Group::Player;
        ValueType type = ValueType::String;
        bool specified = false; // The file holds this option.
        std::size_t displayOrder = 0;
    };

    struct Change
    {
        std::string_view table, key, value;
    };

    struct Files
    {
        std::filesystem::path directory;
        std::string modName;           // Names the mod in the files and messages, for example "ROCK".
        std::string playerFileName;    // For example "ROCK.toml".
        std::string developerFileName; // Empty when the mod has no developer file.
        std::string playerHeader;      // "##" lines written at the top of each file.
        std::string developerHeader;
    };

    // Canonical values of the options the files specify, keyed by settingPath().
    using LoadedValues = std::map<std::string, std::string, std::less<>>;

    [[nodiscard]] std::string settingPath(std::string_view table, std::string_view key);

    // Canonical text of a value: true/false, a decimal integer, a float that
    // always carries a '.' or exponent, raw text, "[x, y, z]", a transform
    // table with its parts in pos, rot, scale order, or an inline table as
    // written (its key order and spacing kept, a trailing comment dropped).
    // Accepts the looser forms an API caller may send (1/0, yes/no, on/off,
    // "x, y, z", any key order). Returns nullopt for a value the type cannot
    // hold.
    [[nodiscard]] std::optional<std::string> normalizeValue(ValueType type, std::string_view value);
    [[nodiscard]] std::optional<std::array<float, 3>> parseFloat3(std::string_view value);
    [[nodiscard]] std::optional<Transform> parseTransform(std::string_view value);
    [[nodiscard]] std::string formatTransform(const Transform& value);
    // The keys of a Table value, sorted by key, or nullopt for text that is
    // not one inline table on one line.
    [[nodiscard]] std::optional<std::vector<TableEntry>> parseTable(std::string_view value);

    class Store
    {
    public:
        // Throws std::invalid_argument when a definition is duplicated, has a
        // default that is not canonical, names a developer option without a
        // developer file, a legacy key names a missing option or the wrong type,
        // or a renamed key names a missing option, the wrong type, or an
        // old key that is itself an option, or a conversion names a missing
        // option or has no function, or a ConversionInput row names an option.
        Store(Files files, std::vector<Definition> definitions, LegacyIni legacy = {},
            std::span<const RenamedKey> renamed = {}, std::span<const ValueConversion> conversions = {});

        // startup: organize the files (migrate old INIs, carry renamed keys
        // and run conversions). Otherwise only read.
        void load(bool startup);
        // Writes the file only: the loaded values, settings() and revision()
        // change at the next load(), so the owner applies a write the same way
        // as an external edit.
        [[nodiscard]] bool setValue(Group group, std::string_view table, std::string_view key, std::string_view value);
        [[nodiscard]] bool setValues(Group group, std::span<const Change> changes);
        [[nodiscard]] LoadedValues loadedValues() const;

        [[nodiscard]] const std::vector<Setting>& settings() const noexcept { return _settings; }
        [[nodiscard]] const Setting* find(std::string_view table, std::string_view key) const noexcept;

        // Why the last setValue/setValues failed.
        [[nodiscard]] const std::string& error() const noexcept { return _error; }
        // Problems found by the last load, each naming its file and line.
        [[nodiscard]] const std::vector<std::string>& messages() const noexcept { return _messages; }
        // What the last startup load did besides reading (INI migration and
        // deletion, renamed keys carried over, values converted).
        [[nodiscard]] const std::vector<std::string>& notices() const noexcept { return _notices; }

        [[nodiscard]] const std::filesystem::path& directory() const noexcept { return _files.directory; }
        [[nodiscard]] std::filesystem::path path(Group group) const;
        [[nodiscard]] const Files& files() const noexcept { return _files; }
        // Advances whenever a load changes a value or whether it is specified.
        [[nodiscard]] std::uint64_t revision() const noexcept { return _revision; }

        [[nodiscard]] static bool isDefault(const Setting& setting, std::string_view value);

    private:
        void migrateLegacyFiles();
        bool removeDeveloperFile();
        bool publish(Group group, std::string_view contents, bool replace);
        [[nodiscard]] bool hasDeveloperFile() const noexcept { return !_files.developerFileName.empty(); }

        Files _files;
        LegacyIni _legacy;
        std::span<const RenamedKey> _renamed;
        std::span<const ValueConversion> _conversions;
        std::vector<Setting> _settings;
        std::string _error;
        std::vector<std::string> _messages;
        std::vector<std::string> _notices;
        std::uint64_t _revision = 0;
    };
}
