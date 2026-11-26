// SavedForm: the co-save encoding of a form reference (every length checked,
// never a byte past the record) and its save/load mapping through a load order
// described by the FormDirectory seam: light and regular masks, a load order
// change, a plugin turned light or regular, a missing plugin, the origin check,
// injected records and runtime-created 0xFF forms.

#include "RPS/Runtime/SavedForm.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace
{
    namespace SF = RPS::Runtime::SavedForms;

    int g_failures = 0;

    void check(const bool condition, const char* what)
    {
        if (!condition) {
            std::cerr << "FAILED: " << what << '\n';
            ++g_failures;
        }
    }

    // A compiled load order as data: the seam's answers, no engine objects.
    class LoadOrder final : public SF::FormDirectory
    {
    public:
        struct File
        {
            std::string name;
            bool light{};
            std::uint8_t compileIndex{};
            std::uint16_t smallIndex{};
        };

        void regular(std::string name, const std::uint8_t index) { _files.push_back({ std::move(name), false, index, 0 }); }
        void light(std::string name, const std::uint16_t index) { _files.push_back({ std::move(name), true, 0xFE, index }); }
        void live(const std::uint32_t formID, std::string firstFile) { _origins.emplace_back(formID, std::move(firstFile)); }

        [[nodiscard]] std::optional<SF::LoadedFile> regularAt(const std::uint8_t index) const override
        {
            for (const auto& file : _files) {
                if (!file.light && file.compileIndex == index) {
                    return describe(file);
                }
            }
            return std::nullopt;
        }

        [[nodiscard]] std::optional<SF::LoadedFile> lightAt(const std::uint16_t index) const override
        {
            for (const auto& file : _files) {
                if (file.light && file.smallIndex == index) {
                    return describe(file);
                }
            }
            return std::nullopt;
        }

        [[nodiscard]] std::optional<SF::LoadedFile> named(const std::string_view name) const override
        {
            for (const auto& file : _files) {
                if (SF::sameFileName(file.name, name)) {
                    return describe(file);
                }
            }
            return std::nullopt;
        }

        [[nodiscard]] std::string_view origin(const std::uint32_t formID) const override
        {
            for (const auto& [id, file] : _origins) {
                if (id == formID) {
                    return file;
                }
            }
            return {};
        }

    private:
        [[nodiscard]] static SF::LoadedFile describe(const File& file)
        {
            return { file.name, file.light, file.compileIndex, file.smallIndex };
        }

        std::vector<File> _files;
        std::vector<std::pair<std::uint32_t, std::string>> _origins;
    };

    [[nodiscard]] std::vector<std::uint8_t> encode(const SF::SavedForm& form)
    {
        std::vector<std::uint8_t> bytes;
        const bool written = SF::write(form, [&](const void* data, const std::uint32_t size) {
            const auto* begin = static_cast<const std::uint8_t*>(data);
            bytes.insert(bytes.end(), begin, begin + size);
            return true;
        });
        check(written, "write accepts a valid form");
        check(bytes.size() == SF::encodedSize(form), "encodedSize matches the written bytes");
        return bytes;
    }

    // Reads from `bytes`, with `available` bytes really present in the stream
    // and `remaining` the record length the caller believes in. Fails the test
    // if the reader ever asks past `remaining`.
    [[nodiscard]] std::optional<SF::SavedForm> decode(const std::vector<std::uint8_t>& bytes, std::uint32_t remaining,
        const std::size_t available, std::uint32_t* left = nullptr)
    {
        const std::uint32_t record = remaining;
        std::size_t offset = 0;
        bool overrun = false;
        auto result = SF::read(
            [&](void* data, const std::uint32_t size) -> std::uint32_t {
                if (offset + size > record) {
                    overrun = true;
                }
                const auto have = offset < available ? available - offset : 0;
                const auto copy = static_cast<std::uint32_t>(size < have ? size : have);
                std::memcpy(data, bytes.data() + offset, copy);
                offset += copy;
                return copy;
            },
            remaining);
        check(!overrun, "read never asks past the record");
        check(offset <= record, "read never consumes past the record");
        if (left) {
            *left = remaining;
        }
        return result;
    }

    [[nodiscard]] bool same(const SF::SavedForm& a, const SF::SavedForm& b)
    {
        return a.kind == b.kind && a.id == b.id && a.file == b.file;
    }

    void roundTrips()
    {
        const SF::SavedForm forms[]{
            {},
            { .kind = SF::Kind::Created, .id = 0xFF000801 },
            { .kind = SF::Kind::Plugin, .id = 0x00ABCDEF, .file = "Weapons.esp" },
            { .kind = SF::Kind::Plugin, .id = 0xABC, .file = "Light.esl" },
            { .kind = SF::Kind::InjectedPlugin, .id = 0x000014, .file = "Fallout4.esm" },
            { .kind = SF::Kind::Plugin, .id = 1, .file = std::string(SF::kMaxFileNameBytes, 'n') },
        };
        for (const auto& form : forms) {
            const auto bytes = encode(form);
            std::uint32_t left = 99;
            const auto back = decode(bytes, static_cast<std::uint32_t>(bytes.size()), bytes.size(), &left);
            check(back && same(*back, form), "round trip keeps every field");
            check(left == 0, "round trip consumes exactly the entry");
        }

        // Two entries back to back read one after the other within one record.
        auto bytes = encode(forms[2]);
        const auto second = encode(forms[1]);
        bytes.insert(bytes.end(), second.begin(), second.end());
        std::uint32_t remaining = static_cast<std::uint32_t>(bytes.size());
        std::size_t offset = 0;
        const auto reader = [&](void* data, const std::uint32_t size) -> std::uint32_t {
            std::memcpy(data, bytes.data() + offset, size);
            offset += size;
            return size;
        };
        const auto first = SF::read(reader, remaining);
        const auto next = SF::read(reader, remaining);
        check(first && same(*first, forms[2]) && next && same(*next, forms[1]) && remaining == 0,
            "consecutive entries read in sequence");
    }

    void truncations()
    {
        const SF::SavedForm forms[]{
            { .kind = SF::Kind::Created, .id = 0xFF000801 },
            { .kind = SF::Kind::Plugin, .id = 0x00ABCDEF, .file = "Weapons.esp" },
        };
        for (const auto& form : forms) {
            const auto bytes = encode(form);
            for (std::size_t cut = 0; cut < bytes.size(); ++cut) {
                // The record is shorter than the entry.
                check(!decode(bytes, static_cast<std::uint32_t>(cut), cut), "a record cut short is refused");
                // The record claims enough bytes but the stream ends early.
                check(!decode(bytes, static_cast<std::uint32_t>(bytes.size()), cut), "a stream cut short is refused");
            }
        }
    }

    void corruptions()
    {
        const auto plugin = encode({ .kind = SF::Kind::Plugin, .id = 0x00ABCDEF, .file = "Weapons.esp" });
        const auto refused = [](std::vector<std::uint8_t> bytes, const char* what) {
            check(!decode(bytes, static_cast<std::uint32_t>(bytes.size()), bytes.size()), what);
        };

        auto bytes = plugin;
        bytes[0] = 4;
        refused(bytes, "an unknown kind is refused");

        bytes = plugin;
        bytes[5] = 0;
        bytes[6] = 0;
        refused(bytes, "an empty file name is refused");

        bytes = plugin;
        const std::uint16_t tooLong = SF::kMaxFileNameBytes + 1;
        std::memcpy(bytes.data() + 5, &tooLong, sizeof(tooLong));
        bytes.resize(7 + tooLong, 'x');
        refused(bytes, "a file name over the cap is refused");

        bytes = plugin;
        const std::uint16_t pastRecord = 200;
        std::memcpy(bytes.data() + 5, &pastRecord, sizeof(pastRecord));
        refused(bytes, "a file name longer than the record is refused before reading it");

        bytes = plugin;
        bytes[7 + 3] = 0;
        refused(bytes, "a file name holding a NUL is refused");

        bytes = plugin;
        const std::uint32_t zero = 0;
        std::memcpy(bytes.data() + 1, &zero, sizeof(zero));
        refused(bytes, "a plugin local ID of 0 is refused");

        bytes = plugin;
        const std::uint32_t wide = 0x01000000;
        std::memcpy(bytes.data() + 1, &wide, sizeof(wide));
        refused(bytes, "a local ID past 24 bits is refused");

        auto created = encode({ .kind = SF::Kind::Created, .id = 0xFF000801 });
        created[4] = 0x05;
        refused(created, "a created ID without the 0xFF slot is refused");

        check(!SF::write({ .kind = SF::Kind::Plugin, .id = 0x1234 }, [](const void*, std::uint32_t) { return true; }),
            "write refuses a plugin form without a file name");
    }

    void savesByOwningFile()
    {
        LoadOrder order;
        order.regular("Fallout4.esm", 0x00);
        order.regular("Weapons.esp", 0x05);
        order.light("Light.esl", 0x012);

        const auto regular = SF::save(0x05ABCDEF, order);
        check(regular && regular->kind == SF::Kind::Plugin && regular->id == 0xABCDEF && regular->file == "Weapons.esp",
            "a regular ID keeps a 24-bit local ID under its slot's file");

        const auto light = SF::save(0xFE012ABC, order);
        check(light && light->kind == SF::Kind::Plugin && light->id == 0xABC && light->file == "Light.esl",
            "a light ID keeps a 12-bit local ID under its small index's file");

        // An override in a later plugin keeps the master's ID and is saved under the master.
        order.live(0x00000014, "Fallout4.esm");
        const auto master = SF::save(0x00000014, order);
        check(master && master->kind == SF::Kind::Plugin && master->file == "Fallout4.esm" && master->id == 0x14,
            "an overridden form is saved under the file that defined it");

        check(SF::save(0, order) && SF::save(0, order)->kind == SF::Kind::None, "ID 0 saves as None");
        check(!SF::save(0x07000801, order), "an ID whose slot holds no loaded file cannot be saved");
        check(!SF::save(0xFE099801, order), "a light ID whose small index holds no file cannot be saved");

        // VR without an ESL loader: a regular file can sit at slot 0xFE.
        LoadOrder loaderless;
        loaderless.regular("Late.esp", 0xFE);
        const auto late = SF::save(0xFE123456, loaderless);
        check(late && late->id == 0x123456 && late->file == "Late.esp", "a regular file at slot 0xFE keeps 24 bits");
    }

    void resolvesByName()
    {
        LoadOrder before;
        before.regular("Fallout4.esm", 0x00);
        before.regular("Weapons.esp", 0x05);
        before.light("Light.esl", 0x012);
        const auto weapon = *SF::save(0x05ABCDEF, before);
        const auto lightForm = *SF::save(0xFE012ABC, before);

        LoadOrder after;
        after.regular("Fallout4.esm", 0x00);
        after.light("Other.esl", 0x000);
        after.light("light.ESL", 0x020);
        after.regular("Weapons.esp", 0x09);
        check(SF::resolve(weapon, after).formID == 0x09ABCDEF, "a regular plugin that moved resolves at its new slot");
        check(SF::resolve(lightForm, after).formID == 0xFE020ABC,
            "a light plugin that moved resolves at its new small index (names compare case-insensitively)");

        // A light plugin turned regular, a regular plugin turned light.
        LoadOrder flipped;
        flipped.regular("Light.esl", 0x07);
        flipped.light("Weapons.esp", 0x003);
        check(SF::resolve(lightForm, flipped).formID == 0x07000ABC, "a plugin no longer light resolves at its slot");
        const auto small = *SF::save(0x05000ABC, before);
        check(SF::resolve(small, flipped).formID == 0xFE003ABC, "a plugin turned light resolves a 12-bit local ID");
        check(SF::resolve(weapon, flipped).status == SF::Status::LocalOutOfRange,
            "a light plugin cannot hold a local ID past 12 bits");

        // An uninstalled plugin: unresolved, with its name and local ID for the log line.
        LoadOrder removed;
        removed.regular("Fallout4.esm", 0x00);
        const auto missing = SF::resolve(weapon, removed);
        check(!missing && missing.status == SF::Status::MissingFile && weapon.file == "Weapons.esp" && weapon.id == 0xABCDEF,
            "a missing plugin leaves the reference unresolved");
    }

    void originCheck()
    {
        LoadOrder order;
        order.regular("Fallout4.esm", 0x00);
        order.regular("Weapons.esp", 0x05);
        const SF::SavedForm form{ .kind = SF::Kind::Plugin, .id = 0x000801, .file = "Weapons.esp" };

        check(SF::resolve(form, order).formID == 0x05000801, "a form not loaded now resolves by its composed ID");

        order.live(0x05000801, "WEAPONS.ESP");
        check(SF::resolve(form, order).formID == 0x05000801, "a live form from the named plugin resolves");

        LoadOrder wrong;
        wrong.regular("Weapons.esp", 0x05);
        wrong.live(0x05000801, "Armor.esp");
        check(SF::resolve(form, wrong).status == SF::Status::OriginMismatch,
            "a live form from another plugin at the composed ID is refused");

        // A record Patch.esp defined inside Fallout4.esm's ID space.
        LoadOrder injected;
        injected.regular("Fallout4.esm", 0x00);
        injected.regular("Patch.esp", 0x03);
        injected.live(0x00F00001, "Patch.esp");
        const auto saved = SF::save(0x00F00001, injected);
        check(saved && saved->kind == SF::Kind::InjectedPlugin && saved->file == "Fallout4.esm" && saved->id == 0xF00001,
            "an injected record is saved under the slot's owner and marked injected");
        check(saved && SF::resolve(*saved, injected).formID == 0x00F00001, "an injected record resolves");
    }

    void createdForms()
    {
        LoadOrder empty;
        const auto saved = SF::save(0xFF000801, empty);
        check(saved && saved->kind == SF::Kind::Created && saved->id == 0xFF000801 && saved->file.empty(),
            "a created form saves its raw ID");
        const auto resolved = SF::resolve(*saved, empty);
        check(resolved && resolved.formID == 0xFF000801, "a created form resolves to the same ID");
        check(SF::resolve({}, empty).status == SF::Status::None, "None resolves to None");
        check(SF::resolve({ .kind = SF::Kind::Created, .id = 0x05000801 }, empty).status == SF::Status::Invalid,
            "a created form outside slot 0xFF is invalid");
    }
}

int main()
{
    roundTrips();
    truncations();
    corruptions();
    savesByOwningFile();
    resolvesByName();
    originCheck();
    createdForms();
    if (g_failures == 0) {
        std::cout << "SavedForm tests passed\n";
    }
    return g_failures == 0 ? 0 : 1;
}
