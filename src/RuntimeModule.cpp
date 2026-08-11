#include "RPS/Runtime/RuntimeModule.h"

#include "RPS/Runtime/Memory.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <winver.h>

#include <algorithm>
#include <array>
#include <cwchar>
#include <vector>

namespace RPS::Runtime
{
    namespace
    {
        [[nodiscard]] const wchar_t* fileName(const wchar_t* path) noexcept
        {
            if (!path) {
                return L"";
            }
            const auto* slash = std::wcsrchr(path, L'\\');
            const auto* alternate = std::wcsrchr(path, L'/');
            const auto* separator = slash > alternate ? slash : alternate;
            return separator ? separator + 1 : path;
        }

        [[nodiscard]] Addresses::RuntimeVersion readFileVersion(const wchar_t* path) noexcept
        {
            DWORD ignored = 0;
            const auto bytes = GetFileVersionInfoSizeW(path, &ignored);
            if (bytes == 0) {
                return {};
            }

            std::vector<std::byte> storage(bytes);
            if (!GetFileVersionInfoW(path, 0, bytes, storage.data())) {
                return {};
            }

            VS_FIXEDFILEINFO* information = nullptr;
            UINT informationBytes = 0;
            if (!VerQueryValueW(storage.data(), L"\\", reinterpret_cast<void**>(&information), &informationBytes) ||
                !information || informationBytes < sizeof(VS_FIXEDFILEINFO) || information->dwSignature != VS_FFI_SIGNATURE) {
                return {};
            }

            return {
                static_cast<std::uint16_t>(HIWORD(information->dwFileVersionMS)),
                static_cast<std::uint16_t>(LOWORD(information->dwFileVersionMS)),
                static_cast<std::uint16_t>(HIWORD(information->dwFileVersionLS)),
                static_cast<std::uint16_t>(LOWORD(information->dwFileVersionLS)),
            };
        }
    }

    RuntimeModule RuntimeModule::detect() noexcept
    {
        RuntimeModule result{};
        const auto module = GetModuleHandleW(nullptr);
        if (!module) {
            return result;
        }

        std::array<wchar_t, 32768> path{};
        const auto length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0 || length >= path.size() || _wcsicmp(fileName(path.data()), L"Fallout4VR.exe") != 0) {
            result._status = ModuleStatus::WrongExecutable;
            return result;
        }

        const auto base = reinterpret_cast<std::uintptr_t>(module);
        IMAGE_DOS_HEADER dos{};
        if (!Memory::read(reinterpret_cast<const void*>(base), dos) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0) {
            result._status = ModuleStatus::InvalidPeImage;
            return result;
        }

        IMAGE_NT_HEADERS64 headers{};
        if (!Memory::read(reinterpret_cast<const void*>(base + static_cast<std::uintptr_t>(dos.e_lfanew)), headers) ||
            headers.Signature != IMAGE_NT_SIGNATURE || headers.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
            headers.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC || headers.OptionalHeader.SizeOfImage == 0) {
            result._status = ModuleStatus::InvalidPeImage;
            return result;
        }

        result._version = readFileVersion(path.data());
        if (result._version != Addresses::Fallout4Vr_1_2_72) {
            result._status = ModuleStatus::UnsupportedRuntime;
            return result;
        }

        result._base = base;
        result._imageSize = headers.OptionalHeader.SizeOfImage;
        result._status = ModuleStatus::Ready;
        return result;
    }

    std::uintptr_t RuntimeModule::resolve(const Addresses::Symbol symbol) const noexcept
    {
        if (!*this) {
            return 0;
        }
        const auto rva = static_cast<std::size_t>(Addresses::record(symbol).rva);
        if (rva >= _imageSize) {
            return 0;
        }
        return _base + rva;
    }

    bool RuntimeModule::matches(const Addresses::Symbol symbol, const BytePattern pattern) const noexcept
    {
        if (pattern.bytes.empty() || (!pattern.mask.empty() && pattern.mask.size() != pattern.bytes.size())) {
            return false;
        }

        const auto address = resolve(symbol);
        if (address == 0 || pattern.bytes.size() > _imageSize ||
            address - _base > _imageSize - pattern.bytes.size()) {
            return false;
        }

        std::vector<std::byte> actual(pattern.bytes.size());
        if (!Memory::copyFrom(reinterpret_cast<const void*>(address), actual.data(), actual.size())) {
            return false;
        }

        for (std::size_t index = 0; index < actual.size(); ++index) {
            const auto mask = pattern.mask.empty() ? std::byte{ 0xFF } : pattern.mask[index];
            if ((actual[index] & mask) != (pattern.bytes[index] & mask)) {
                return false;
            }
        }
        return true;
    }
}
