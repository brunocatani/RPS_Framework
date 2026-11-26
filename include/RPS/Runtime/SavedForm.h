#pragma once

// Form references stored in F4SE co-saves, independent of the load order.
//
// A raw form ID carries its file's load-order slot in its top bits. F4SE's
// ResolveFormID remaps regular slots through the plugin list it keeps in the
// co-save, but F4SEVR 0.6.21 records no light plugins, so every 0xFE ID saved
// by a mod is dropped on load. A SavedForm names the file instead:
// - Plugin: the file that owns the ID's slot (a light file for 0xFE IDs whose
//   small index names a compiled light file, otherwise the regular file at the
//   top byte) and the local ID, masked to 12 bits for a light owner and 24 bits
//   for a regular one, decided when saving. An override in a later plugin keeps
//   its master's ID, so it is saved under the master that defined it.
// - InjectedPlugin: as Plugin, for a record another file defined inside the
//   owner's ID space (its live form's first file is not the slot's owner).
// - Created: a 0xFF form made at runtime. It has no plugin and belongs to the
//   save itself: the engine keeps 0xFF IDs unchanged when it loads a save's form
//   table and F4SE passes them through, so the raw ID is stored.
// - None: no form (ID 0).
//
// Loading looks the file up by name, so a load order change, a regular plugin
// turned light or a light plugin turned regular all resolve. A light file
// cannot hold a local ID past 12 bits; a live form at the composed ID must
// first come from the named file (InjectedPlugin: any live form). A form that
// is not loaded now (a reference in an unloaded cell) resolves by its composed
// ID, as F4SE's remap does. A missing file or a failed check leaves the
// reference unresolved with its file name and local ID for the caller's log.
//
// Encoding, little-endian, fixed-width:
//   u8 kind (0 None, 1 Plugin, 2 Created, 3 InjectedPlugin)
//   Created:               u32 raw ID (top byte 0xFF)
//   Plugin/InjectedPlugin: u32 local ID (1 to 24 bits), u16 file name length
//                          (1 to kMaxFileNameBytes), the name's bytes as the
//                          engine holds them (no NUL, not transcoded)
// The name cap is TESFile's filename field, MAX_PATH bytes with its NUL.
// Reading checks every length against the record's remaining bytes before it
// reads and fails closed on a short or malformed entry, so it never reads past
// the record.
//
// Threads and lifetime: F4SE runs the save, load and revert callbacks on the
// game's main thread, after the load order is compiled; use these there. No
// globals; a SavedForm owns its file name, the only allocation. A FormDirectory
// is a non-owning view, made for one callback.
//
// Header-only and engine-free: the engine side is EngineFormDirectory, a
// template the consumer instantiates with CommonLibF4's RE::TESDataHandler and
// RE::TESForm, so it uses CommonLib's light-aware file lookups.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace RPS::Runtime::SavedForms
{
    inline constexpr std::size_t kMaxFileNameBytes = 259;
    inline constexpr std::uint32_t kRegularLocalMask = (1u << 24) - 1u;
    inline constexpr std::uint32_t kLightLocalMask = (1u << 12) - 1u;
    inline constexpr std::uint8_t kLightSlot = 0xFE;
    inline constexpr std::uint8_t kCreatedSlot = 0xFF;

    enum class Kind : std::uint8_t
    {
        None = 0,
        Plugin = 1,
        Created = 2,
        InjectedPlugin = 3,
    };

    struct SavedForm
    {
        Kind kind{ Kind::None };
        // Plugin kinds: the local ID; Created: the raw 0xFF ID.
        std::uint32_t id{};
        // Plugin kinds only.
        std::string file;
    };

    // A file of the compiled load order, as the engine side reports it.
    struct LoadedFile
    {
        std::string_view name;
        // CommonLib's IsCompiledLightFile: slot 0xFE together with the light flag.
        bool light{};
        std::uint8_t compileIndex{};
        std::uint16_t smallIndex{};
    };

    // The engine seam. Each answer is about the current, compiled load order.
    class FormDirectory
    {
    public:
        virtual ~FormDirectory() = default;
        // The loaded regular file at a load-order slot.
        [[nodiscard]] virtual std::optional<LoadedFile> regularAt(std::uint8_t compileIndex) const = 0;
        // The compiled light file at a small index.
        [[nodiscard]] virtual std::optional<LoadedFile> lightAt(std::uint16_t smallIndex) const = 0;
        // The loaded file of that name, regular or light (case-insensitive).
        [[nodiscard]] virtual std::optional<LoadedFile> named(std::string_view name) const = 0;
        // The first file of the live form with this ID (TESForm::GetFile(0));
        // empty when no form with this ID is loaded or it has no file.
        [[nodiscard]] virtual std::string_view origin(std::uint32_t formID) const = 0;
    };

    enum class Status : std::uint8_t
    {
        None,            // the saved reference was None
        Resolved,
        Invalid,         // the SavedForm breaks the encoding's rules (never from read())
        MissingFile,     // the named file is not in the load order
        LocalOutOfRange, // the file is light now and the local ID needs more than 12 bits
        OriginMismatch,  // the live form at the composed ID comes from another file
    };

    struct Resolution
    {
        Status status{ Status::None };
        std::uint32_t formID{};

        [[nodiscard]] explicit operator bool() const noexcept { return status == Status::Resolved; }
    };

    [[nodiscard]] inline const char* statusName(const Status status) noexcept
    {
        switch (status) {
        case Status::None:
            return "none";
        case Status::Resolved:
            return "resolved";
        case Status::Invalid:
            return "invalid";
        case Status::MissingFile:
            return "plugin not loaded";
        case Status::LocalOutOfRange:
            return "local ID too large for a light plugin";
        case Status::OriginMismatch:
            return "form comes from another plugin";
        }
        return "unknown";
    }

    // ASCII case-insensitive, as the engine and CommonLib compare file names.
    [[nodiscard]] inline bool sameFileName(const std::string_view a, const std::string_view b) noexcept
    {
        if (a.size() != b.size()) {
            return false;
        }
        for (std::size_t i = 0; i < a.size(); ++i) {
            auto x = static_cast<unsigned char>(a[i]);
            auto y = static_cast<unsigned char>(b[i]);
            if (x >= 'A' && x <= 'Z') {
                x = static_cast<unsigned char>(x - 'A' + 'a');
            }
            if (y >= 'A' && y <= 'Z') {
                y = static_cast<unsigned char>(y - 'A' + 'a');
            }
            if (x != y) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] inline bool validFileName(const std::string_view name) noexcept
    {
        return !name.empty() && name.size() <= kMaxFileNameBytes && name.find('\0') == std::string_view::npos;
    }

    // What read() accepts and write() writes.
    [[nodiscard]] inline bool valid(const SavedForm& form) noexcept
    {
        switch (form.kind) {
        case Kind::None:
            return form.id == 0 && form.file.empty();
        case Kind::Created:
            return (form.id >> 24) == kCreatedSlot && form.file.empty();
        case Kind::Plugin:
        case Kind::InjectedPlugin:
            return form.id != 0 && form.id <= kRegularLocalMask && validFileName(form.file);
        }
        return false;
    }

    [[nodiscard]] inline std::uint32_t encodedSize(const SavedForm& form) noexcept
    {
        switch (form.kind) {
        case Kind::None:
            return 1;
        case Kind::Created:
            return 1 + 4;
        case Kind::Plugin:
        case Kind::InjectedPlugin:
            return 1 + 4 + 2 + static_cast<std::uint32_t>(form.file.size());
        }
        return 0;
    }

    // The file that owns the ID's load-order slot, and whether it is light.
    [[nodiscard]] inline std::optional<LoadedFile> owner(const std::uint32_t formID, const FormDirectory& directory)
    {
        const auto slot = static_cast<std::uint8_t>(formID >> 24);
        if (slot == kLightSlot) {
            if (auto light = directory.lightAt(static_cast<std::uint16_t>((formID >> 12) & kLightLocalMask));
                light && light->light) {
                return light;
            }
        }
        // A regular file can sit at slot 0xFE in VR's 255-slot table without an ESL loader.
        if (auto regular = directory.regularAt(slot); regular && !regular->light) {
            return regular;
        }
        return std::nullopt;
    }

    // At save. nullopt when the ID's slot names no loaded file (nothing to
    // save it under); the caller stores None and logs.
    [[nodiscard]] inline std::optional<SavedForm> save(const std::uint32_t formID, const FormDirectory& directory)
    {
        if (formID == 0) {
            return SavedForm{};
        }
        if ((formID >> 24) == kCreatedSlot) {
            return SavedForm{ .kind = Kind::Created, .id = formID };
        }
        const auto file = owner(formID, directory);
        if (!file || !validFileName(file->name)) {
            return std::nullopt;
        }
        const auto local = formID & (file->light ? kLightLocalMask : kRegularLocalMask);
        if (local == 0) {
            return std::nullopt;
        }
        const auto origin = directory.origin(formID);
        const bool injected = !origin.empty() && !sameFileName(origin, file->name);
        return SavedForm{ .kind = injected ? Kind::InjectedPlugin : Kind::Plugin, .id = local, .file = std::string(file->name) };
    }

    // At load.
    [[nodiscard]] inline Resolution resolve(const SavedForm& form, const FormDirectory& directory)
    {
        if (!valid(form)) {
            return { Status::Invalid, 0 };
        }
        switch (form.kind) {
        case Kind::None:
            return { Status::None, 0 };
        case Kind::Created:
            return { Status::Resolved, form.id };
        case Kind::Plugin:
        case Kind::InjectedPlugin:
            break;
        }
        const auto file = directory.named(form.file);
        if (!file || !sameFileName(file->name, form.file)) {
            return { Status::MissingFile, 0 };
        }
        std::uint32_t formID{};
        if (file->light) {
            if (file->smallIndex > kLightLocalMask) {
                return { Status::MissingFile, 0 };
            }
            if (form.id > kLightLocalMask) {
                return { Status::LocalOutOfRange, 0 };
            }
            formID = (std::uint32_t{ kLightSlot } << 24) | (std::uint32_t{ file->smallIndex } << 12) | form.id;
        } else {
            if (file->compileIndex == kCreatedSlot) {
                return { Status::MissingFile, 0 };
            }
            formID = (std::uint32_t{ file->compileIndex } << 24) | form.id;
        }
        if (form.kind == Kind::Plugin) {
            const auto origin = directory.origin(formID);
            if (!origin.empty() && !sameFileName(origin, form.file)) {
                return { Status::OriginMismatch, 0 };
            }
        }
        return { Status::Resolved, formID };
    }

    // write(const void* data, std::uint32_t bytes) -> bool, true when every
    // byte was written (F4SE's WriteRecordData). Writes nothing for an invalid form.
    template <class Write>
    [[nodiscard]] bool write(const SavedForm& form, Write&& writeBytes)
    {
        if (!valid(form)) {
            return false;
        }
        const auto kind = static_cast<std::uint8_t>(form.kind);
        if (!writeBytes(&kind, std::uint32_t{ sizeof(kind) })) {
            return false;
        }
        if (form.kind == Kind::None) {
            return true;
        }
        if (!writeBytes(&form.id, std::uint32_t{ sizeof(form.id) })) {
            return false;
        }
        if (form.kind == Kind::Created) {
            return true;
        }
        const auto length = static_cast<std::uint16_t>(form.file.size());
        return writeBytes(&length, std::uint32_t{ sizeof(length) }) &&
               writeBytes(form.file.data(), static_cast<std::uint32_t>(length));
    }

    namespace detail
    {
        template <class Read>
        [[nodiscard]] bool take(Read& readBytes, std::uint32_t& remaining, void* data, const std::uint32_t bytes)
        {
            if (bytes > remaining) {
                return false;
            }
            const std::uint32_t got = readBytes(data, bytes);
            remaining -= got <= bytes ? got : bytes;
            return got == bytes;
        }
    }

    // read(void* data, std::uint32_t bytes) -> std::uint32_t bytes read (F4SE's
    // ReadRecordData). `remaining` is what is left of the record; it shrinks
    // by what was read. nullopt: the entry is short or malformed, and the rest
    // of the record cannot be trusted.
    template <class Read>
    [[nodiscard]] std::optional<SavedForm> read(Read&& readBytes, std::uint32_t& remaining)
    {
        std::uint8_t kind{};
        if (!detail::take(readBytes, remaining, &kind, sizeof(kind))) {
            return std::nullopt;
        }
        SavedForm form{ .kind = static_cast<Kind>(kind) };
        switch (form.kind) {
        case Kind::None:
            return form;
        case Kind::Created:
        case Kind::Plugin:
        case Kind::InjectedPlugin:
            break;
        default:
            return std::nullopt;
        }
        if (!detail::take(readBytes, remaining, &form.id, sizeof(form.id))) {
            return std::nullopt;
        }
        if (form.kind != Kind::Created) {
            std::uint16_t length{};
            if (!detail::take(readBytes, remaining, &length, sizeof(length)) || length == 0 ||
                length > kMaxFileNameBytes || length > remaining) {
                return std::nullopt;
            }
            form.file.resize(length);
            if (!detail::take(readBytes, remaining, form.file.data(), length)) {
                return std::nullopt;
            }
        }
        if (!valid(form)) {
            return std::nullopt;
        }
        return form;
    }

    // The engine side over CommonLibF4:
    //   EngineFormDirectory<RE::TESDataHandler, RE::TESForm> forms{ *RE::TESDataHandler::GetSingleton() };
    // Uses TESDataHandler's loaded-file lookups (light-aware in our CommonLibF4VR
    // fork under FalloutVRESL, Daytripper4 or no loader), IsCompiledLightFile,
    // and TESForm::GetFormByID / GetFile(0).
    template <class DataHandler, class Form>
    class EngineFormDirectory final : public FormDirectory
    {
    public:
        explicit EngineFormDirectory(DataHandler& handler) noexcept :
            _handler(handler)
        {}

        [[nodiscard]] std::optional<LoadedFile> regularAt(const std::uint8_t compileIndex) const override
        {
            return describe(_handler.LookupLoadedModByIndex(compileIndex));
        }

        [[nodiscard]] std::optional<LoadedFile> lightAt(const std::uint16_t smallIndex) const override
        {
            return describe(_handler.LookupLoadedLightModByIndex(smallIndex));
        }

        [[nodiscard]] std::optional<LoadedFile> named(const std::string_view name) const override
        {
            return describe(_handler.LookupLoadedFile(name).first);
        }

        [[nodiscard]] std::string_view origin(const std::uint32_t formID) const override
        {
            const auto* form = Form::GetFormByID(formID);
            const auto* file = form ? form->GetFile(0) : nullptr;
            return file ? file->GetFilename() : std::string_view{};
        }

    private:
        template <class File>
        [[nodiscard]] static std::optional<LoadedFile> describe(const File* file)
        {
            if (!file) {
                return std::nullopt;
            }
            return LoadedFile{ file->GetFilename(), DataHandler::IsCompiledLightFile(file), file->GetCompileIndex(),
                file->GetSmallFileCompileIndex() };
        }

        DataHandler& _handler;
    };
}
