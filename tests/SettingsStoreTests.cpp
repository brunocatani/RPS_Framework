// RPS::Settings::Store against real files: organizing keeps what a file means,
// faults keep the last values, the developer file holds only overrides,
// transforms stay one inline value, old INI files migrate once and are
// deleted, and renamed keys carry their values once.
#include "RPS/Settings/Store.h"

#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace
{
    using namespace RPS::Settings;
    namespace fs = std::filesystem;

    void require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    std::string bytes(const fs::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        require(input.good(), "cannot read a test file");
        return { std::istreambuf_iterator<char>(input), {} };
    }

    void write(const fs::path& path, std::string_view text)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << text;
        require(output.good(), "cannot write a test file");
    }

    bool hasLine(const std::vector<std::string>& lines, std::string_view text)
    {
        for (const auto& line : lines) {
            if (line.find(text) != std::string::npos)
                return true;
        }
        return false;
    }

    std::vector<Definition> definitions()
    {
        return {
            { "Logging", "LogLevel", "01. Logging", "Log level\n0 to 6.", Group::Player, ValueType::Integer, "2" },
            { "Weapons", "Gunstock", "02. Weapons", "Gunstock Mode", Group::Player, ValueType::Boolean, "false" },
            { "Weapons", "Fuse", "02. Weapons", "Fuse seconds", Group::Player, ValueType::Float, "4.0" },
            { "Hand.Left", "AimOffset", "03. Hands", "Aim offset", Group::Player, ValueType::Float3, "[0.0, 0.0, 0.0]" },
            { "Hand", "DefaultHand", "03. Hands", "right or left", Group::Player, ValueType::String, "right" },
            { "Hand.Left", "Grip", "03. Hands", "Grip transform", Group::Player, ValueType::Transform,
                "{ pos = [0.0, 0.0, 0.0] }" },
            { "Debug", "Pattern", "04. Debug", "Log pattern", Group::Developer, ValueType::String, "%v" },
            { "Debug", "Proxy", "04. Debug", "Proxy transform", Group::Developer, ValueType::Transform, "{ scale = 1.0 }" },
            { "Debug", "Offset", "04. Debug", "Proxy offset", Group::Developer, ValueType::Float3, "[0.5, -1.0, 0.0]" },
            { "Debug", "MaxBodies", "04. Debug", "Body limit", Group::Developer, ValueType::Integer, "195" },
        };
    }

    Files files(const fs::path& directory)
    {
        return { directory, "TestMod", "TestMod.toml", "TestMod_Developer.toml", "## TestMod.toml\n",
            "## TestMod_Developer.toml\n" };
    }

    constexpr LegacyKey kLegacy[] = {
        { "iLogLevel", "Logging", "LogLevel", LegacyKind::Value, 0, "2", "2" },
        { "bGunstockMode", "Weapons", "Gunstock", LegacyKind::Value, 0, {}, "false" },
        { "fFuseSeconds", "Weapons", "Fuse", LegacyKind::Value, 0, "4.0", "4" },
        { "fAimOffsetX", "Hand.Left", "AimOffset", LegacyKind::Float3Component, 0, "0.0", "0" },
        { "fAimOffsetY", "Hand.Left", "AimOffset", LegacyKind::Float3Component, 1, "0.0", "0" },
        { "fAimOffsetZ", "Hand.Left", "AimOffset", LegacyKind::Float3Component, 2, "0.0", "0" },
        { "bLeftHanded", "Hand", "DefaultHand", LegacyKind::BooleanToString, 0, "false", {}, "left", "right" },
        { "fOffsetY", "Debug", "Offset", LegacyKind::Float3Component, 1, "-1.0", {} },
        { "iMaxBodies", "Debug", "MaxBodies", LegacyKind::Value, 0, "171", "163" },
        { "fGripZ", "Hand.Left", "Grip", LegacyKind::Float3Component, 2, "0.0", {} },
        { "fProxyX", "Debug", "Proxy", LegacyKind::Float3Component, 0, "0.0", {} },
    };

    void valueChecks()
    {
        require(normalizeValue(ValueType::Float3, "[1, 2, 3]") == "[1.0, 2.0, 3.0]", "float3 brackets");
        require(normalizeValue(ValueType::Float3, " 0.5,-1 , 0 ") == "[0.5, -1.0, 0.0]", "float3 without brackets");
        require(!normalizeValue(ValueType::Float3, "[1, 2]") && !normalizeValue(ValueType::Float3, "[1, nan, 3]"),
            "malformed float3 accepted");
        require(normalizeValue(ValueType::Float, "4") == "4.0" && normalizeValue(ValueType::Float, "-0") == "0.0",
            "float canonical form");
        require(!normalizeValue(ValueType::Float, "inf") && !normalizeValue(ValueType::Integer, "2.5"),
            "non-finite float or fractional integer accepted");
        require(normalizeValue(ValueType::Float, "+15") == "15.0" && normalizeValue(ValueType::Integer, "+3") == "3" &&
                    !normalizeValue(ValueType::Float, "+-1") && !normalizeValue(ValueType::Integer, "++3"),
            "a leading plus");
        require(normalizeValue(ValueType::Boolean, "On") == "true" && !normalizeValue(ValueType::Boolean, "maybe"),
            "boolean forms");
        require(!normalizeValue(ValueType::String, "two\nlines"), "multi-line string accepted");
        require(normalizeValue(ValueType::Transform, "{scale=2, pos=[1, 2, 3]}") == "{ pos = [1.0, 2.0, 3.0], scale = 2.0 }",
            "transform canonical form");
        require(normalizeValue(ValueType::Transform, " {} ") == "{}", "empty transform");
        require(!normalizeValue(ValueType::Transform, "{ pos = [1, 2] }") &&
                    !normalizeValue(ValueType::Transform, "{ size = 1 }") &&
                    !normalizeValue(ValueType::Transform, "[1, 2, 3]") &&
                    !normalizeValue(ValueType::Transform, "{ scale = \"big\" }") &&
                    !normalizeValue(ValueType::Transform, "{ pos = [1, 2, 3] }\nother = 1"),
            "malformed transform accepted");
        const auto transform = parseTransform("{ rot = [0, 90, -5.5] }");
        require(transform && !transform->position && !transform->scale && transform->rotation &&
                    formatTransform(*transform) == "{ rot = [0.0, 90.0, -5.5] }",
            "transform parts did not round-trip");
    }

    // A transform is one value whether written inline or as a table section,
    // is written back inline, and keeps a malformed table as the player wrote it.
    void transformChecks(const fs::path& directory)
    {
        fs::create_directories(directory);
        Store store(files(directory), definitions());
        write(store.path(Group::Player), "[Hand.Left]\nGrip = { scale = 1, pos = [1, 2, 3] }\n");
        store.load(true);
        require(store.messages().empty(), "a valid transform reported problems");
        require(store.find("Hand.Left", "Grip")->value == "{ pos = [1.0, 2.0, 3.0], scale = 1.0 }",
            "an inline transform did not apply");
        auto organized = bytes(store.path(Group::Player));
        require(organized.find("Grip = { pos = [1.0, 2.0, 3.0], scale = 1.0 }\n") != std::string::npos,
            "a transform was not written inline");
        require(organized.find("Not read by this version") == std::string::npos, "a transform's parts became unread");

        write(store.path(Group::Player), "[Hand.Left.Grip]\npos = [4, 5, 6]\n");
        store.load(true);
        require(store.find("Hand.Left", "Grip")->value == "{ pos = [4.0, 5.0, 6.0] }", "a transform section did not apply");
        organized = bytes(store.path(Group::Player));
        require(organized.find("Grip = { pos = [4.0, 5.0, 6.0] }\n") != std::string::npos &&
                    organized.find("[Hand.Left.Grip]") == std::string::npos &&
                    organized.find("Not read by this version") == std::string::npos,
            "a transform section was not rewritten inline");

        write(store.path(Group::Player), "[Hand.Left]\nGrip = { pos = [1, 2] }\n[Debug]\nProxy = { scale = 3 }\n");
        store.load(true);
        require(store.find("Hand.Left", "Grip")->value == "{ pos = [4.0, 5.0, 6.0] }",
            "a malformed transform replaced the last value");
        require(hasLine(store.messages(), "Hand.Left.Grip must be a table"), "a malformed transform was not reported");
        organized = bytes(store.path(Group::Player));
        require(organized.find("Grip = { pos = [ 1, 2 ] }\n") != std::string::npos &&
                    organized.find("# Hand.Left.Grip") == std::string::npos,
            "a malformed transform was not kept as written");
        require(organized.find("# Debug.Proxy = { scale = 3 }") != std::string::npos,
            "a transform of the other file was not kept whole as a comment");
        store.load(true);
        require(bytes(store.path(Group::Player)) == organized, "a second start changed a kept transform");

        const auto& grip = *store.find("Hand.Left", "Grip");
        require(Store::isDefault(grip, "{}") && Store::isDefault(grip, "{ scale = 1, rot = [0, 0, 0] }") &&
                    !Store::isDefault(grip, "{ pos = [0, 0, 1] }") && !Store::isDefault(grip, "[0, 0, 0]"),
            "a transform's missing parts did not equal no change");
        require(store.setValue(Group::Player, "Hand.Left", "Grip", "{ pos = [1, 0, 0] }"), "a transform write failed");
        require(bytes(store.path(Group::Player)).find("Grip = { pos = [1.0, 0.0, 0.0] }\n") != std::string::npos,
            "a transform write was not canonical");
        require(store.setValue(Group::Developer, "Debug", "Proxy", "{ scale = 2 }"), "a developer transform write failed");
        require(store.setValue(Group::Developer, "Debug", "Proxy", "{}"), "a developer transform reset failed");
        require(!fs::exists(store.path(Group::Developer)), "a transform equal to its default kept the developer file");
    }

    // Startup organizes a hand-edited file without changing what it means.
    void organizeChecks(const fs::path& directory)
    {
        fs::create_directories(directory);
        Store store(files(directory), definitions());
        write(store.path(Group::Player),
            "## stale help\n[Logging]\n# my note\nLogLevel = 3\nRetired = 7\n[Weapons]\nGunstock = true\n"
            "[Debug]\nPattern = \"x\"\n# a note no key follows\n");
        store.load(true);
        require(store.messages().empty(), "a valid file reported problems");
        const auto organized = bytes(store.path(Group::Player));
        require(organized.starts_with("## TestMod.toml\n"), "the header was not written");
        require(organized.find("# my note\nLogLevel = 3\n") != std::string::npos, "a player note moved");
        require(organized.find("stale help") == std::string::npos, "help was not regenerated");
        require(organized.find("## === Not read by this version of TestMod ===") != std::string::npos &&
                organized.find("# Logging.Retired = 7") != std::string::npos &&
                organized.find("# Debug.Pattern = \"x\"") != std::string::npos &&
                organized.find("# a note no key follows") != std::string::npos,
            "unread entries were not kept as comments");
        require(organized.find("[Hand.Left]\n") != std::string::npos, "missing options were not added");
        require(store.find("Logging", "LogLevel")->value == "3" && store.find("Weapons", "Gunstock")->value == "true",
            "values did not apply");
        require(store.find("Debug", "Pattern")->value == "%v", "a developer option applied from the player file");
        require(!fs::exists(store.path(Group::Developer)), "loading created the developer file");
        const auto revision = store.revision();
        store.load(true);
        require(bytes(store.path(Group::Player)) == organized && store.revision() == revision,
            "a second start changed the file or the revision");
        require(store.setValue(Group::Player, "Logging", "LogLevel", "1"), "a write failed");
        require(bytes(store.path(Group::Player)).find("# my note\nLogLevel = 1\n") != std::string::npos,
            "a write lost the player note");
    }

    // A file that does not parse is never rewritten and keeps its last values.
    void faultChecks(const fs::path& directory)
    {
        fs::create_directories(directory);
        Store store(files(directory), definitions());
        write(store.path(Group::Player), "[Logging]\nLogLevel = 4\n[Weapons]\nFuse = 5\n");
        store.load(true);
        require(store.find("Weapons", "Fuse")->value == "5.0", "a whole number was not accepted for a decimal");
        const std::string broken = "[Logging]\nLogLevel = 1\n[Weapons\n";
        write(store.path(Group::Player), broken);
        store.load(true);
        require(bytes(store.path(Group::Player)) == broken, "a broken file was rewritten");
        require(store.find("Logging", "LogLevel")->value == "4", "a broken file did not keep the last values");
        require(hasLine(store.messages(), "TestMod.toml line 3"), "the syntax error was not reported");
        require(!store.setValue(Group::Player, "Logging", "LogLevel", "2"), "a write replaced a broken file");
        write(store.path(Group::Player), "[Logging]\nLogLevel = \"high\"\n");
        store.load(true);
        require(store.find("Logging", "LogLevel")->value == "4", "a wrong-type value replaced the last value");
        require(hasLine(store.messages(), "Logging.LogLevel must be a whole number"), "the wrong type was not reported");
    }

    void developerChecks(const fs::path& directory)
    {
        fs::create_directories(directory);
        Store store(files(directory), definitions());
        store.load(true);
        require(store.setValue(Group::Developer, "Debug", "Offset", "[1, 2.5, -3]"), "a float3 write failed");
        require(bytes(store.path(Group::Developer)).find("Offset = [1.0, 2.5, -3.0]") != std::string::npos,
            "a float3 was not written as a list");
        require(!store.setValue(Group::Developer, "Logging", "LogLevel", "2"), "a write to the wrong file was accepted");
        require(store.setValue(Group::Developer, "Debug", "Offset", "[0.5, -1, 0]"), "a developer reset failed");
        require(!fs::exists(store.path(Group::Developer)), "resetting every developer option kept the file");
    }

    // A player upgrading from INI keeps the settings they changed; old
    // defaults, retired keys and values that do not fit stay behind; the INIs
    // are deleted once the TOML files exist.
    void migrationChecks(const fs::path& directory)
    {
        fs::create_directories(directory);
        const auto playerIni = directory / "TestMod.ini";
        const auto developerIni = directory / "TestMod_Developer.ini";
        write(playerIni,
            "\xEF\xBB\xBF; TestMod.ini\r\n[General]\r\niLogLevel = 2\r\nbGunstockMode = true\r\n"
            "fFuseSeconds = soon\r\nbLeftHanded = true\r\nfAimOffsetX = 0.0\r\nfAimOffsetY = 2.5\r\n"
            "bRetiredOption = true\r\nfGripZ = 4\r\n[Moved]\r\nfOffsetY = 12\r\n");
        write(developerIni, "[Debug]\r\niMaxBodies = 100\r\nfProxyX = 3\r\n");
        Store store(files(directory), definitions(), { { playerIni, developerIni }, kLegacy });
        store.load(true);
        require(!fs::exists(playerIni) && !fs::exists(developerIni), "migrated INIs were not deleted");
        require(store.find("Weapons", "Gunstock")->value == "true", "a changed option was not carried");
        require(store.find("Logging", "LogLevel")->value == "2", "an old default was carried");
        require(store.find("Hand", "DefaultHand")->value == "left", "a boolean-to-string key was not carried");
        require(store.find("Hand.Left", "AimOffset")->value == "[0.0, 2.5, 0.0]", "a triplet was not carried");
        require(store.find("Hand.Left", "Grip")->value == "{ pos = [0.0, 0.0, 4.0] }" &&
                    store.find("Debug", "Proxy")->value == "{ pos = [3.0, 0.0, 0.0], scale = 1.0 }",
            "a component was not carried into a transform's pos over its default");
        require(store.find("Debug", "Offset")->value == "[0.5, 12.0, 0.0]" &&
                    store.find("Debug", "MaxBodies")->value == "100",
            "developer options were not carried");
        require(bytes(store.path(Group::Developer)).find("Offset = [0.5, 12.0, 0.0]") != std::string::npos,
            "an option of the developer file was not written there");
        require(hasLine(store.messages(), "does not fit: fFuseSeconds = soon"), "a bad value was not reported");
        require(hasLine(store.notices(), "bRetiredOption"), "a retired key was not named");

        // With the player file present the TOML files win and a leftover INI goes.
        write(playerIni, "[General]\r\niLogLevel = 0\r\n");
        const auto organized = bytes(store.path(Group::Player));
        store.load(true);
        require(!fs::exists(playerIni), "a leftover INI was not deleted");
        require(bytes(store.path(Group::Player)) == organized, "a leftover INI changed the player file");
    }

    constexpr RenamedKey kRenamed[] = {
        { "Hand.Left", "GripOffset", "Hand.Left", "Grip", RenameKind::Float3ToTransformPosition },
        { "Weapons", "FuseSeconds", "Weapons", "Fuse", RenameKind::Value },
        { "Debug", "OldPattern", "Debug", "Pattern", RenameKind::Value },
    };

    // A renamed key carries its value to the new option once, at startup,
    // and then stays only as a comment; a new key the file has always wins.
    void renameChecks(const fs::path& directory)
    {
        fs::create_directories(directory);
        Store store(files(directory), definitions(), {}, kRenamed);
        write(store.path(Group::Player), "[Hand.Left]\nGripOffset = [1, 2, 3]\n[Weapons]\nFuseSeconds = 6\n");
        write(store.path(Group::Developer), "[Debug]\nOldPattern = \"%x\"\n");
        store.load(true);
        require(store.messages().empty(), "carrying renamed keys reported problems");
        require(store.find("Hand.Left", "Grip")->value == "{ pos = [1.0, 2.0, 3.0] }", "a float3 was not carried into a transform");
        require(store.find("Weapons", "Fuse")->value == "6.0", "a renamed value was not carried");
        require(store.find("Debug", "Pattern")->value == "%x", "a renamed developer value was not carried");
        require(hasLine(store.notices(), "Carried Hand.Left.GripOffset into Hand.Left.Grip"), "a carried key was not named");
        const auto organized = bytes(store.path(Group::Player));
        require(organized.find("Grip = { pos = [1.0, 2.0, 3.0] }\n") != std::string::npos &&
                    organized.find("# Hand.Left.GripOffset = [ 1, 2, 3 ]") != std::string::npos,
            "the carried value was not written with the old key as a comment");
        require(bytes(store.path(Group::Developer)).find("Pattern = \"%x\"\n") != std::string::npos,
            "a carried developer value was not written to the developer file");
        const auto revision = store.revision();
        store.load(true);
        require(bytes(store.path(Group::Player)) == organized && store.revision() == revision && store.notices().empty(),
            "a renamed key was carried twice");

        write(store.path(Group::Player), "[Hand.Left]\nGripOffset = [1, 2, 3]\nGrip = { pos = [9, 9, 9] }\n");
        store.load(true);
        require(store.find("Hand.Left", "Grip")->value == "{ pos = [9.0, 9.0, 9.0] }", "an old key replaced the new key");

        write(store.path(Group::Player), "[Hand.Left]\nGripOffset = \"left\"\n");
        store.load(true);
        require(store.find("Hand.Left", "Grip")->value == "{ pos = [0.0, 0.0, 0.0] }", "a value that does not fit was carried");
        require(hasLine(store.messages(), "Hand.Left.GripOffset is not carried into Hand.Left.Grip"),
            "a value that does not fit was not reported");
    }

    // An INI that used one key in two sections: rows naming the section take
    // the entry from that section, whichever comes last in the file.
    void sectionChecks(const fs::path& directory)
    {
        fs::create_directories(directory);
        constexpr LegacyKey sectioned[] = {
            { "bEnabled", "Weapons", "Gunstock", LegacyKind::Value, 0, "false", {}, {}, {}, "Weapons" },
            { "bEnabled", "Hand", "DefaultHand", LegacyKind::BooleanToString, 0, "false", {}, "left", "right", "Hands" },
        };
        const auto ini = directory / "TestMod.ini";
        write(ini, "[Weapons]\r\nbEnabled = false\r\n[hands]\r\nbEnabled = true\r\n[Other]\r\nbEnabled = true\r\n");
        Store store(files(directory), definitions(), { { ini }, sectioned });
        store.load(true);
        require(store.find("Weapons", "Gunstock")->value == "false", "a later section overwrote a section-named key");
        require(store.find("Hand", "DefaultHand")->value == "left", "a section-named key was not carried");
        require(hasLine(store.notices(), "bEnabled"), "a key in a section no row names was not reported");
    }

    void declarationChecks(const fs::path& directory)
    {
        auto duplicated = definitions();
        duplicated.push_back(duplicated.front());
        bool refused = false;
        try {
            Store store(files(directory), duplicated);
        } catch (const std::invalid_argument&) {
            refused = true;
        }
        require(refused, "a duplicated option was accepted");
        constexpr LegacyKey wrongType[] = { { "fGunstock", "Weapons", "Gunstock", LegacyKind::Float3Component, 0 } };
        refused = false;
        try {
            Store store(files(directory), definitions(), { {}, wrongType });
        } catch (const std::invalid_argument&) {
            refused = true;
        }
        require(refused, "a legacy key of the wrong type was accepted");
        constexpr RenamedKey badRenames[][1] = {
            { { "Old", "Key", "Missing", "Option" } },
            { { "Old", "Key", "Weapons", "Fuse", RenameKind::Float3ToTransformPosition } },
            { { "Weapons", "Gunstock", "Weapons", "Fuse" } },
        };
        for (const auto& rename : badRenames) {
            refused = false;
            try {
                Store store(files(directory), definitions(), {}, rename);
            } catch (const std::invalid_argument&) {
                refused = true;
            }
            require(refused, "a renamed key to a missing option, of the wrong type or still an option was accepted");
        }
    }
}

int main()
{
    const fs::path root = fs::path(RPS_SETTINGS_TEST_DIR);
    try {
        std::error_code ec;
        fs::remove_all(root, ec);
        valueChecks();
        organizeChecks(root / "organize");
        transformChecks(root / "transform");
        faultChecks(root / "faults");
        developerChecks(root / "developer");
        migrationChecks(root / "migration");
        sectionChecks(root / "sections");
        renameChecks(root / "renames");
        declarationChecks(root / "declaration");
        fs::remove_all(root, ec);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
