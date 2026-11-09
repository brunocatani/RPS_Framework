#include "RPS/Settings/Store.h"

#include <toml++/toml.hpp>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <climits>
#include <cmath>
#include <format>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace RPS::Settings
{
    namespace
    {
        std::string_view trim(std::string_view text) noexcept
        {
            const auto first = text.find_first_not_of(" \t");
            if (first == std::string_view::npos)
                return {};
            return text.substr(first, text.find_last_not_of(" \t") - first + 1);
        }

        std::string formatFloat(float value)
        {
            std::array<char, 64> text{};
            const auto [end, error] =
                std::to_chars(text.data(), text.data() + text.size(), value == 0.0f ? 0.0f : value);
            if (error != std::errc{})
                return "0.0";
            std::string result(text.data(), end);
            if (result.find_first_of(".eE") == std::string::npos)
                result += ".0";
            return result;
        }

        std::optional<float> parseFloat(std::string_view text)
        {
            text = trim(text);
            float number{};
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), number);
            if (text.empty() || error != std::errc{} || end != text.data() + text.size() || !std::isfinite(number))
                return std::nullopt;
            return number;
        }

        std::string formatFloat3(const std::array<float, 3>& value)
        {
            return std::format("[{}, {}, {}]", formatFloat(value[0]), formatFloat(value[1]), formatFloat(value[2]));
        }

        std::string quoteString(std::string_view text)
        {
            std::string result = "\"";
            for (const char c : text) {
                switch (c) {
                case '"':
                    result += "\\\"";
                    break;
                case '\\':
                    result += "\\\\";
                    break;
                case '\t':
                    result += "\\t";
                    break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20 || c == 0x7F)
                        result += std::format("\\u{:04X}", static_cast<unsigned char>(c));
                    else
                        result += c;
                }
            }
            return result + "\"";
        }

        // TOML text of a canonical value.
        std::string tomlText(ValueType type, std::string_view canonical)
        {
            return type == ValueType::String ? quoteString(canonical) : std::string(canonical);
        }

        std::optional<float> numberOf(const toml::node& node)
        {
            double number{};
            if (const auto* floating = node.as_floating_point())
                number = floating->get();
            else if (const auto* integer = node.as_integer())
                number = static_cast<double>(integer->get());
            else
                return std::nullopt;
            const auto narrowed = static_cast<float>(number);
            if (!std::isfinite(narrowed))
                return std::nullopt;
            return narrowed;
        }

        std::optional<std::string> canonicalFromNode(ValueType type, const toml::node& node)
        {
            switch (type) {
            case ValueType::Boolean:
                if (const auto* value = node.as_boolean())
                    return value->get() ? "true" : "false";
                return std::nullopt;
            case ValueType::Integer:
                if (const auto* value = node.as_integer()) {
                    const auto number = value->get();
                    if (number < INT_MIN || number > INT_MAX)
                        return std::nullopt;
                    return std::to_string(number);
                }
                return std::nullopt;
            case ValueType::Float:
                if (const auto number = numberOf(node))
                    return formatFloat(*number);
                return std::nullopt;
            case ValueType::String:
                if (const auto* value = node.as_string()) {
                    if (value->get().find_first_of("\r\n") != std::string::npos)
                        return std::nullopt;
                    return value->get();
                }
                return std::nullopt;
            case ValueType::Float3:
                if (const auto* array = node.as_array(); array && array->size() == 3) {
                    std::array<float, 3> components{};
                    for (std::size_t index = 0; index < 3; ++index) {
                        const auto component = numberOf(*array->get(index));
                        if (!component)
                            return std::nullopt;
                        components[index] = *component;
                    }
                    return formatFloat3(components);
                }
                return std::nullopt;
            }
            return std::nullopt;
        }

        std::string_view typeDescription(ValueType type) noexcept
        {
            switch (type) {
            case ValueType::Boolean:
                return "true or false";
            case ValueType::Integer:
                return "a whole number";
            case ValueType::Float:
                return "a number";
            case ValueType::String:
                return "one line of text in quotes";
            case ValueType::Float3:
                return "a list of three numbers such as [0.0, 0.0, 0.0]";
            }
            return "a valid value";
        }

        // The node as TOML text on one line, for values the mod keeps but cannot use.
        std::string rawText(const toml::node& node)
        {
            std::ostringstream stream;
            stream << toml::toml_formatter{ node, toml::format_flags::none };
            auto text = stream.str();
            std::replace(text.begin(), text.end(), '\n', ' ');
            return text;
        }

        // Splits a TOML key or table name on dots outside quotes and removes the quotes.
        std::string joinedKeyPath(std::string_view prefix, std::string_view text)
        {
            std::string result(prefix);
            std::string part;
            char quote = 0;
            const auto flush = [&]() {
                const auto trimmed = trim(part);
                if (!result.empty())
                    result += '.';
                result += trimmed;
                part.clear();
            };
            for (const char c : text) {
                if (quote) {
                    if (c == quote)
                        quote = 0;
                    else
                        part += c;
                } else if (c == '"' || c == '\'') {
                    quote = c;
                } else if (c == '.') {
                    flush();
                } else {
                    part += c;
                }
            }
            flush();
            return result;
        }

        std::size_t findOutsideQuotes(std::string_view text, char wanted) noexcept
        {
            char quote = 0;
            for (std::size_t index = 0; index < text.size(); ++index) {
                const char c = text[index];
                if (quote) {
                    if (c == '\\' && quote == '"')
                        ++index;
                    else if (c == quote)
                        quote = 0;
                } else if (c == '"' || c == '\'') {
                    quote = c;
                } else if (c == wanted) {
                    return index;
                }
            }
            return std::string_view::npos;
        }

        std::size_t occurrences(std::string_view text, std::string_view token) noexcept
        {
            std::size_t count = 0;
            for (auto at = text.find(token); at != std::string_view::npos; at = text.find(token, at + token.size()))
                ++count;
            return count;
        }

        std::string unusedHeading(std::string_view modName)
        {
            return std::format("## === Not read by this version of {} ===", modName);
        }

        // Comments toml++ does not keep: the player's notes (single '#'), each
        // attached to the key below it, and the lines of the unused block.
        // Lines starting with "##" belong to the mod and are regenerated.
        struct Comments
        {
            std::map<std::string, std::vector<std::string>, std::less<>> notes;
            std::vector<std::string> unused;
        };

        Comments scanComments(std::string_view text, std::string_view heading)
        {
            Comments result;
            std::string table;
            std::vector<std::string> pending;
            std::string_view multiLineDelimiter;
            bool inUnused = false;
            while (!text.empty()) {
                const auto end = text.find('\n');
                auto line = text.substr(0, end);
                text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
                if (!line.empty() && line.back() == '\r')
                    line.remove_suffix(1);
                const auto trimmed = trim(line);
                if (!multiLineDelimiter.empty()) {
                    if (occurrences(trimmed, multiLineDelimiter) % 2 == 1)
                        multiLineDelimiter = {};
                    continue;
                }
                if (trimmed.empty())
                    continue;
                if (trimmed.front() == '#') {
                    if (trimmed.starts_with("##")) {
                        inUnused |= trimmed == heading;
                        continue;
                    }
                    (inUnused ? result.unused : pending).emplace_back(trimmed);
                    continue;
                }
                if (trimmed.front() == '[') {
                    const bool arrayOfTables = trimmed.starts_with("[[");
                    const auto open = arrayOfTables ? 2u : 1u;
                    const auto close = trimmed.find(arrayOfTables ? "]]" : "]", open);
                    if (close != std::string_view::npos)
                        table = joinedKeyPath({}, trimmed.substr(open, close - open));
                    continue;
                }
                const auto equals = findOutsideQuotes(trimmed, '=');
                if (equals == std::string_view::npos)
                    continue; // Continuation of a multi-line array or inline table.
                const auto path = joinedKeyPath(table, trimmed.substr(0, equals));
                if (!pending.empty()) {
                    auto& notes = result.notes[path];
                    notes.insert(notes.end(), pending.begin(), pending.end());
                    pending.clear();
                }
                const auto value = trim(trimmed.substr(equals + 1));
                for (const std::string_view delimiter : { std::string_view("\"\"\""), std::string_view("'''") }) {
                    if (value.starts_with(delimiter) && occurrences(value, delimiter) % 2 == 1)
                        multiLineDelimiter = delimiter;
                }
            }
            // Notes no key follows stay visible in the unused block.
            result.unused.insert(result.unused.end(), pending.begin(), pending.end());
            return result;
        }

        enum class FileState
        {
            Missing,
            Parsed,
            Failed,
        };

        struct Document
        {
            FileState state = FileState::Missing;
            std::string text;
            toml::table table;
            Comments comments;
            std::string problem;
        };

        Document readDocument(const std::filesystem::path& file, std::string_view modName)
        {
            Document document;
            const auto name = file.filename().string();
            std::error_code ec;
            const bool exists = std::filesystem::exists(file, ec);
            if (ec) {
                document.state = FileState::Failed;
                document.problem = std::format("Cannot inspect {}: {}", file.string(), ec.message());
                return document;
            }
            if (!exists)
                return document;
            {
                std::ifstream input(file, std::ios::binary);
                if (input)
                    document.text.assign(std::istreambuf_iterator<char>(input), {});
                if (!input || input.bad()) {
                    document.state = FileState::Failed;
                    document.problem = std::format("Cannot read {}", file.string());
                    return document;
                }
            }
            try {
                document.table = toml::parse(document.text, name);
            } catch (const toml::parse_error& error) {
                document.state = FileState::Failed;
                document.problem = std::format(
                    "{} line {}, column {}: {}. {} keeps the last values it read from this file and does not "
                    "rewrite it until the error is fixed.",
                    name, error.source().begin.line, error.source().begin.column, error.description(), modName);
                return document;
            }
            document.comments = scanComments(document.text, unusedHeading(modName));
            document.state = FileState::Parsed;
            return document;
        }

        template <class Visitor>
        void forEachLeaf(const toml::table& table, const std::string& prefix, Visitor&& visit)
        {
            for (const auto& [key, node] : table) {
                auto path = prefix.empty() ? std::string(key.str()) : std::format("{}.{}", prefix, key.str());
                if (const auto* child = node.as_table())
                    forEachLeaf(*child, path, visit);
                else
                    visit(path, node);
            }
        }

        using Overrides = std::map<std::string, std::optional<std::string>, std::less<>>;

        // Writes every recognized entry of the group once, grouped by table in
        // declaration order with the help above it. The player file gains
        // every missing option at its default; the developer file holds only
        // the entries it already has or an override adds. Overrides map a
        // setting path to its new canonical value, or to nullopt to drop it.
        std::string organize(const Files& files, Group group, std::span<const Setting> settings,
            const Document* document, const Overrides& overrides, bool& hasEntries)
        {
            struct Entry
            {
                const Setting* setting;
                std::string path;
                std::string value;
            };
            std::vector<Entry> entries;
            std::set<std::string, std::less<>> recognized;
            for (const auto& setting : settings) {
                if (setting.group != group)
                    continue;
                auto path = settingPath(setting.table, setting.key);
                recognized.insert(path);
                std::string value;
                const toml::node* node = document ? document->table.at_path(path).node() : nullptr;
                if (const auto found = overrides.find(path); found != overrides.end()) {
                    if (!found->second)
                        continue;
                    value = tomlText(setting.type, *found->second);
                } else if (node) {
                    if (const auto canonical = canonicalFromNode(setting.type, *node))
                        value = tomlText(setting.type, *canonical);
                    else if (!node->is_table())
                        value = rawText(*node); // Kept as the player wrote it; load reported it.
                    else
                        value = tomlText(setting.type, setting.value);
                } else if (group == Group::Player) {
                    value = tomlText(setting.type, setting.defaultValue);
                } else {
                    continue;
                }
                entries.push_back({ &setting, std::move(path), std::move(value) });
            }
            hasEntries = !entries.empty();

            std::vector<std::string_view> tables;
            for (const auto& entry : entries) {
                if (std::find(tables.begin(), tables.end(), entry.setting->table) == tables.end())
                    tables.push_back(entry.setting->table);
            }
            std::string text(group == Group::Player ? files.playerHeader : files.developerHeader);
            std::string_view previousCategory;
            for (const auto table : tables) {
                bool firstInTable = true;
                for (const auto& entry : entries) {
                    if (entry.setting->table != table)
                        continue;
                    if (firstInTable) {
                        if (entry.setting->category != previousCategory)
                            text += std::format("\n## === {} ===\n", entry.setting->category);
                        else
                            text += '\n';
                        previousCategory = entry.setting->category;
                        text += std::format("[{}]\n", table);
                        firstInTable = false;
                    } else {
                        text += '\n';
                    }
                    std::string_view description = entry.setting->description;
                    while (!description.empty()) {
                        const auto end = description.find('\n');
                        text += std::format("## {}\n", description.substr(0, end));
                        description =
                            end == std::string_view::npos ? std::string_view{} : description.substr(end + 1);
                    }
                    if (document) {
                        if (const auto notes = document->comments.notes.find(entry.path);
                            notes != document->comments.notes.end()) {
                            for (const auto& note : notes->second)
                                text += note + '\n';
                        }
                    }
                    text += std::format("{} = {}\n", entry.setting->key, entry.value);
                }
            }

            // Every other entry is one the mod does not read from this file: a
            // retired option, an option owned by the other file, or a foreign
            // key. Keep it as a comment so the value stays visible but can
            // never apply.
            std::vector<std::string> unused;
            const auto addUnused = [&](std::string line) {
                if (std::find(unused.begin(), unused.end(), line) == unused.end())
                    unused.push_back(std::move(line));
            };
            if (document) {
                for (const auto& line : document->comments.unused)
                    addUnused(line);
                std::vector<std::pair<std::uint32_t, std::string>> leaves;
                forEachLeaf(document->table, {}, [&](const std::string& path, const toml::node& node) {
                    if (recognized.contains(path))
                        return;
                    std::string lines;
                    if (const auto notes = document->comments.notes.find(path);
                        notes != document->comments.notes.end()) {
                        for (const auto& note : notes->second)
                            lines += note + '\n';
                    }
                    lines += std::format("# {} = {}", path, rawText(node));
                    leaves.emplace_back(node.source().begin.line, std::move(lines));
                });
                std::stable_sort(leaves.begin(), leaves.end(),
                    [](const auto& a, const auto& b) { return a.first < b.first; });
                for (auto& [line, lines] : leaves)
                    addUnused(std::move(lines));
            }
            if (!unused.empty()) {
                text += std::format("\n{}\n## {} ignores the lines below. They keep your old values for reference; "
                                    "delete them whenever you like.\n",
                    unusedHeading(files.modName), files.modName);
                for (const auto& line : unused)
                    text += line + '\n';
            }
            return text;
        }

        // ---- Legacy INI translation ----

        using Values = std::map<std::string, std::string, std::less<>>;

        struct IniEntry
        {
            std::string section;
            std::string key;
            std::string value;
        };

        // key = value lines with the section each sits in; comments (; or #)
        // and blank lines are skipped. The value is everything after the first
        // '=', so string options holding '=' or ';' keep their text.
        void readIni(std::string_view text, std::vector<IniEntry>& entries)
        {
            if (text.starts_with("\xEF\xBB\xBF"))
                text.remove_prefix(3);
            std::string section;
            while (!text.empty()) {
                const auto end = text.find('\n');
                auto line = text.substr(0, end);
                text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
                if (!line.empty() && line.back() == '\r')
                    line.remove_suffix(1);
                line = trim(line);
                if (line.empty() || line.front() == ';' || line.front() == '#')
                    continue;
                if (line.front() == '[') {
                    const auto close = line.find(']');
                    section = trim(line.substr(1, close == std::string_view::npos ? std::string_view::npos : close - 1));
                    continue;
                }
                const auto equals = line.find('=');
                if (equals == std::string_view::npos)
                    continue;
                const auto key = trim(line.substr(0, equals));
                if (!key.empty())
                    entries.push_back({ section, std::string(key), std::string(trim(line.substr(equals + 1))) });
            }
        }

        bool equalNoCase(std::string_view left, std::string_view right)
        {
            return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(), [](char a, char b) {
                return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
            });
        }

        // The row for an INI entry: one naming its section first, else one for any section.
        const LegacyKey* legacyRow(std::span<const LegacyKey> keys, const IniEntry& entry)
        {
            const LegacyKey* anySection = nullptr;
            for (const auto& row : keys) {
                if (row.oldKey != entry.key)
                    continue;
                if (row.oldSection.empty()) {
                    if (!anySection)
                        anySection = &row;
                } else if (equalNoCase(row.oldSection, entry.section)) {
                    return &row;
                }
            }
            return anySection;
        }

        bool isOldDefault(const LegacyKey& row, const auto& equal)
        {
            return (row.oldDefault && equal(*row.oldDefault)) || (row.otherOldDefault && equal(*row.otherOldDefault));
        }

        struct Translation
        {
            Values player;
            Values developer;
            std::vector<std::string> retired;
            std::vector<std::string> invalid;
        };

        const Setting* findSetting(std::span<const Setting> settings, std::string_view table, std::string_view key)
        {
            const auto found = std::find_if(settings.begin(), settings.end(),
                [&](const Setting& setting) { return setting.table == table && setting.key == key; });
            return found == settings.end() ? nullptr : &*found;
        }

        // Keeps only values the player changed: a value equal to the option's
        // old default is left out, so the current default applies.
        Translation translate(std::span<const std::string> iniTexts, std::span<const LegacyKey> keys,
            std::span<const Setting> settings)
        {
            std::vector<IniEntry> entries;
            for (const auto& text : iniTexts)
                readIni(text, entries);

            Translation result;
            // Each row's value in first-seen order; a later entry for a row replaces it.
            std::vector<std::pair<const LegacyKey*, std::string>> rowValues;
            for (auto& entry : entries) {
                const auto* row = legacyRow(keys, entry);
                if (!row) {
                    if (std::find(result.retired.begin(), result.retired.end(), entry.key) == result.retired.end())
                        result.retired.push_back(entry.key);
                    continue;
                }
                const auto known = std::find_if(rowValues.begin(), rowValues.end(),
                    [&](const auto& pair) { return pair.first == row; });
                if (known == rowValues.end())
                    rowValues.emplace_back(row, std::move(entry.value));
                else
                    known->second = std::move(entry.value);
            }
            const auto assign = [&](const Setting& setting, std::string value) {
                auto& target = setting.group == Group::Developer ? result.developer : result.player;
                const auto path = settingPath(setting.table, setting.key);
                // The developer file holds only options that differ from their defaults.
                if (setting.group == Group::Developer && value == setting.defaultValue)
                    target.erase(path);
                else
                    target.insert_or_assign(path, std::move(value));
            };

            struct List
            {
                const Setting* setting = nullptr;
                std::array<std::optional<float>, 3> components{};
            };
            std::map<std::string, List, std::less<>> lists;

            for (const auto& [row, text] : rowValues) {
                const auto& legacy = *row;
                const auto oldKey = legacy.oldKey;
                const auto* setting = findSetting(settings, legacy.table, legacy.key);
                if (!setting) {
                    result.retired.emplace_back(oldKey);
                    continue;
                }
                const auto invalid = [&]() { result.invalid.push_back(std::format("{} = {}", oldKey, text)); };
                switch (legacy.kind) {
                case LegacyKind::Value: {
                    const auto canonical = normalizeValue(setting->type, text);
                    if (!canonical) {
                        invalid();
                        break;
                    }
                    if (!isOldDefault(legacy, [&](std::string_view old) { return normalizeValue(setting->type, old) == canonical; }))
                        assign(*setting, *canonical);
                    break;
                }
                case LegacyKind::BooleanToString: {
                    const auto canonical = normalizeValue(ValueType::Boolean, text);
                    if (!canonical) {
                        invalid();
                        break;
                    }
                    if (!isOldDefault(legacy, [&](std::string_view old) { return normalizeValue(ValueType::Boolean, old) == canonical; }))
                        assign(*setting, std::string(*canonical == "true" ? legacy.trueText : legacy.falseText));
                    break;
                }
                case LegacyKind::Float3Component: {
                    const auto number = parseFloat(text);
                    if (!number) {
                        invalid();
                        break;
                    }
                    if (isOldDefault(legacy, [&](std::string_view old) { return parseFloat(old) == number; }))
                        break;
                    auto& list = lists[settingPath(setting->table, setting->key)];
                    list.setting = setting;
                    list.components[legacy.component] = *number;
                    break;
                }
                }
            }

            // A list starts from the current default; only the components the
            // player changed replace it.
            for (const auto& [path, list] : lists) {
                auto merged = parseFloat3(list.setting->defaultValue).value_or(std::array<float, 3>{});
                for (std::size_t index = 0; index < 3; ++index) {
                    if (list.components[index])
                        merged[index] = *list.components[index];
                }
                assign(*list.setting, formatFloat3(merged));
            }
            return result;
        }
    }

    std::string settingPath(std::string_view table, std::string_view key)
    {
        return table.empty() ? std::string(key) : std::format("{}.{}", table, key);
    }

    std::optional<std::array<float, 3>> parseFloat3(std::string_view value)
    {
        value = trim(value);
        if (value.size() >= 2 && value.front() == '[' && value.back() == ']')
            value = value.substr(1, value.size() - 2);
        std::array<float, 3> result{};
        for (std::size_t index = 0; index < 3; ++index) {
            const auto comma = value.find(',');
            if ((comma == std::string_view::npos) != (index == 2))
                return std::nullopt;
            const auto component = parseFloat(value.substr(0, comma));
            if (!component)
                return std::nullopt;
            result[index] = *component;
            value = comma == std::string_view::npos ? std::string_view{} : value.substr(comma + 1);
        }
        return result;
    }

    std::optional<std::string> normalizeValue(ValueType type, std::string_view value)
    {
        switch (type) {
        case ValueType::String:
            if (value.find_first_of("\r\n") != std::string_view::npos)
                return std::nullopt;
            return std::string(value);
        case ValueType::Boolean: {
            std::string lower(trim(value));
            for (auto& c : lower)
                if (c >= 'A' && c <= 'Z')
                    c += 'a' - 'A';
            if (lower == "true" || lower == "1" || lower == "yes" || lower == "on")
                return "true";
            if (lower == "false" || lower == "0" || lower == "no" || lower == "off")
                return "false";
            return std::nullopt;
        }
        case ValueType::Integer: {
            value = trim(value);
            int number{};
            const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), number);
            if (value.empty() || error != std::errc{} || end != value.data() + value.size())
                return std::nullopt;
            return std::to_string(number);
        }
        case ValueType::Float:
            if (const auto number = parseFloat(value))
                return formatFloat(*number);
            return std::nullopt;
        case ValueType::Float3:
            if (const auto vector = parseFloat3(value))
                return formatFloat3(*vector);
            return std::nullopt;
        }
        return std::nullopt;
    }

    Store::Store(Files files, std::vector<Definition> definitions, LegacyIni legacy) :
        _files(std::move(files)), _legacy(std::move(legacy))
    {
        if (_files.directory.empty() || _files.playerFileName.empty())
            throw std::invalid_argument("Settings files need a directory and a player file name");
        for (std::size_t index = 0; index < definitions.size(); ++index) {
            auto& definition = definitions[index];
            const auto path = settingPath(definition.table, definition.key);
            if (normalizeValue(definition.type, definition.defaultValue) != definition.defaultValue)
                throw std::invalid_argument(std::format("Default of {} is not canonical", path));
            if (definition.group == Group::Developer && !hasDeveloperFile())
                throw std::invalid_argument(std::format("{} is a developer option but there is no developer file", path));
            if (find(definition.table, definition.key))
                throw std::invalid_argument(std::format("{} is declared twice", path));
            _settings.push_back(Setting{
                .table = std::move(definition.table),
                .key = std::move(definition.key),
                .defaultValue = definition.defaultValue,
                .value = definition.defaultValue,
                .category = std::move(definition.category),
                .description = std::move(definition.description),
                .group = definition.group,
                .type = definition.type,
                .displayOrder = index,
            });
        }
        for (const auto& row : _legacy.keys) {
            const auto* setting = find(row.table, row.key);
            if (!setting)
                throw std::invalid_argument(
                    std::format("INI key {} maps to unknown option {}.{}", row.oldKey, row.table, row.key));
            const bool typeMatches = row.kind == LegacyKind::Float3Component ? setting->type == ValueType::Float3
                : row.kind == LegacyKind::BooleanToString ? setting->type == ValueType::String
                                                          : setting->type != ValueType::Float3;
            if (!typeMatches || row.component > 2)
                throw std::invalid_argument(
                    std::format("INI key {} does not fit the type of {}.{}", row.oldKey, row.table, row.key));
        }
    }

    const Setting* Store::find(std::string_view table, std::string_view key) const noexcept
    {
        return findSetting(_settings, table, key);
    }

    std::filesystem::path Store::path(Group group) const
    {
        return _files.directory / (group == Group::Developer ? _files.developerFileName : _files.playerFileName);
    }

    bool Store::removeDeveloperFile()
    {
        const auto destination = path(Group::Developer);
        std::error_code ec;
        std::filesystem::remove(destination, ec);
        if (ec)
            _error = std::format("Cannot remove empty {}: {}", destination.string(), ec.message());
        return !ec;
    }

    bool Store::publish(Group group, std::string_view contents, bool replace)
    {
        const auto destination = path(group);
        std::error_code ec;
        std::filesystem::create_directories(_files.directory, ec);
        if (ec) {
            _error = std::format("Cannot create {}: {}", _files.directory.string(), ec.message());
            return false;
        }
        auto temporary = destination;
        temporary += L".creating";
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        output.close();
        if (!output) {
            _error = std::format("Cannot write {}", temporary.string());
            std::filesystem::remove(temporary, ec);
            return false;
        }
        const DWORD flags = MOVEFILE_WRITE_THROUGH | (replace ? MOVEFILE_REPLACE_EXISTING : 0);
        if (!MoveFileExW(temporary.c_str(), destination.c_str(), flags)) {
            const auto error = GetLastError();
            std::filesystem::remove(temporary, ec);
            if (!replace && (error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS))
                return true;
            _error = std::format("Cannot publish {}: Windows error {}", destination.string(), error);
            return false;
        }
        return true;
    }

    void Store::migrateLegacyFiles()
    {
        std::vector<std::filesystem::path> present;
        for (const auto& file : _legacy.files) {
            std::error_code ec;
            if (std::filesystem::exists(file, ec)) {
                present.push_back(file);
            } else if (ec) {
                _messages.push_back(std::format("Cannot inspect {}: {}", file.string(), ec.message()));
                return;
            }
        }
        if (present.empty())
            return;

        std::error_code ec;
        const bool playerExists = std::filesystem::exists(path(Group::Player), ec);
        if (ec) {
            _messages.push_back(std::format("Cannot inspect {}: {}", path(Group::Player).string(), ec.message()));
            return;
        }
        if (playerExists) {
            // The TOML files already hold this player's settings.
            _notices.push_back(std::format("{} exists, so the old INI files are not read", _files.playerFileName));
        } else {
            std::vector<std::string> texts;
            for (const auto& file : present) {
                std::ifstream input(file, std::ios::binary);
                std::string text(std::istreambuf_iterator<char>(input), {});
                if (!input && !input.eof()) {
                    _messages.push_back(
                        std::format("Cannot read {}; it is kept and migrated at the next start", file.string()));
                    return;
                }
                texts.push_back(std::move(text));
            }
            const auto translation = translate(texts, _legacy.keys, _settings);
            const auto overridesOf = [](const Values& values) {
                Overrides overrides;
                for (const auto& [entryPath, value] : values)
                    overrides.emplace(entryPath, value);
                return overrides;
            };

            // The developer file first: the player file is written last, so it
            // exists only once everything is carried over, and an interrupted
            // migration runs again at the next start.
            bool developerWritten = false;
            if (!translation.developer.empty() && !std::filesystem::exists(path(Group::Developer), ec)) {
                bool hasEntries = false;
                const auto text =
                    organize(_files, Group::Developer, _settings, nullptr, overridesOf(translation.developer), hasEntries);
                if (hasEntries) {
                    if (!publish(Group::Developer, text, false)) {
                        _messages.push_back(std::exchange(_error, {}) + "; the old INI files are kept");
                        return;
                    }
                    developerWritten = true;
                }
            }
            bool hasEntries = false;
            const auto text =
                organize(_files, Group::Player, _settings, nullptr, overridesOf(translation.player), hasEntries);
            if (!publish(Group::Player, text, false)) {
                _messages.push_back(std::exchange(_error, {}) + "; the old INI files are kept");
                return;
            }
            // Delete the INIs only once the new files read back.
            for (const auto group : { Group::Player, Group::Developer }) {
                if (group == Group::Developer && !developerWritten)
                    continue;
                if (readDocument(path(group), _files.modName).state != FileState::Parsed) {
                    _messages.push_back(std::format(
                        "{} could not be read back after migration; the old INI files are kept",
                        path(group).filename().string()));
                    return;
                }
            }

            _notices.push_back(std::format("Carried {} changed settings from the old INI files into {}{}",
                translation.player.size() + (developerWritten ? translation.developer.size() : 0),
                _files.playerFileName,
                developerWritten ? std::format(" and {}", _files.developerFileName) : std::string{}));
            if (!translation.retired.empty()) {
                std::string names;
                for (const auto& name : translation.retired)
                    names += (names.empty() ? "" : ", ") + name;
                _notices.push_back(std::format("{} old settings {} no longer has were not carried over: {}",
                    translation.retired.size(), _files.modName, names));
            }
            for (const auto& line : translation.invalid)
                _messages.push_back(std::format("Old INI setting not carried over, its value does not fit: {}", line));
        }

        for (const auto& file : present) {
            std::filesystem::remove(file, ec);
            if (ec)
                _messages.push_back(std::format(
                    "Cannot delete {}: {}; {} tries again at the next start", file.string(), ec.message(), _files.modName));
            else
                _notices.push_back(std::format("Deleted {}", file.string()));
        }
    }

    void Store::load(bool startup)
    {
        _error.clear();
        _messages.clear();
        _notices.clear();
        if (startup)
            migrateLegacyFiles();
        auto updated = _settings;
        bool changed = _revision == 0;
        for (const auto group : { Group::Player, Group::Developer }) {
            if (group == Group::Developer && !hasDeveloperFile())
                continue;
            const auto file = path(group);
            const auto document = readDocument(file, _files.modName);
            if (document.state == FileState::Failed) {
                _messages.push_back(document.problem);
                continue;
            }
            std::vector<Setting*> missing;
            for (auto& setting : updated) {
                if (setting.group != group)
                    continue;
                std::string next = setting.defaultValue;
                bool specified = false;
                if (const auto node = document.table.at_path(settingPath(setting.table, setting.key))) {
                    const auto canonical = canonicalFromNode(setting.type, *node.node());
                    if (!canonical) {
                        _messages.push_back(std::format("{} line {}: {}.{} must be {}; keeping {}",
                            file.filename().string(), node.node()->source().begin.line, setting.table, setting.key,
                            typeDescription(setting.type), setting.value));
                        continue;
                    }
                    next = *canonical;
                    specified = true;
                } else {
                    missing.push_back(&setting);
                }
                changed |= setting.value != next || setting.specified != specified;
                setting.value = std::move(next);
                setting.specified = specified;
            }
            if (!startup || (document.state == FileState::Missing && group == Group::Developer))
                continue;
            // Organizing writes each recognized key once with the value it
            // holds, so the rewrite cannot change what the loader reads. An
            // unwritable file can still be read; report and keep going.
            bool hasEntries = false;
            const auto* source = document.state == FileState::Parsed ? &document : nullptr;
            const auto text = organize(_files, group, updated, source, {}, hasEntries);
            bool written = true;
            if (group == Group::Developer && !hasEntries)
                written = removeDeveloperFile();
            else if (text != document.text)
                written = publish(group, text, document.state == FileState::Parsed);
            if (!written) {
                _messages.push_back(std::exchange(_error, {}));
            } else if (group == Group::Player) {
                // The organized player file now holds these at their defaults,
                // so the next reload reads them as set without a revision change.
                for (auto* setting : missing)
                    setting->specified = true;
            }
        }
        _settings = std::move(updated);
        if (changed)
            ++_revision;
    }

    bool Store::isDefault(const Setting& setting, std::string_view value)
    {
        const auto normalized = normalizeValue(setting.type, value);
        return normalized && *normalized == setting.defaultValue;
    }

    bool Store::setValue(Group group, std::string_view table, std::string_view key, std::string_view value)
    {
        const Change change{ table, key, value };
        return setValues(group, std::span(&change, 1));
    }

    bool Store::setValues(Group group, std::span<const Change> changes)
    {
        _error.clear();
        if (group == Group::Developer && !hasDeveloperFile()) {
            _error = std::format("{} has no developer settings file", _files.modName);
            return false;
        }
        // Read the current file before each edit, preserving external edits.
        const auto document = readDocument(path(group), _files.modName);
        if (document.state == FileState::Failed) {
            _error = document.problem;
            return false;
        }
        Overrides overrides;
        for (const auto& change : changes) {
            const auto found = std::find_if(_settings.begin(), _settings.end(), [&](const Setting& setting) {
                return setting.group == group && setting.table == change.table && setting.key == change.key;
            });
            if (found == _settings.end()) {
                _error = "Unknown setting or incorrect settings file";
                return false;
            }
            auto normalized = normalizeValue(found->type, change.value);
            if (!normalized) {
                _error = std::format("Invalid setting value: {} must be {}", settingPath(found->table, found->key),
                    typeDescription(found->type));
                return false;
            }
            auto& override = overrides[settingPath(found->table, found->key)];
            if (group == Group::Developer && *normalized == found->defaultValue)
                override.reset();
            else
                override = std::move(normalized);
        }
        bool hasEntries = false;
        const auto text = organize(_files, group, _settings, document.state == FileState::Parsed ? &document : nullptr,
            overrides, hasEntries);
        if (group == Group::Developer && !hasEntries)
            return removeDeveloperFile();
        return publish(group, text, true);
    }

    LoadedValues Store::loadedValues() const
    {
        LoadedValues values;
        for (const auto& setting : _settings) {
            if (setting.specified)
                values.emplace(settingPath(setting.table, setting.key), setting.value);
        }
        return values;
    }
}
