#pragma once

#include "RPS/Addresses/Catalog.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace RPS::Runtime
{
    enum class ModuleStatus : std::uint8_t
    {
        Ready,
        ModuleUnavailable,
        WrongExecutable,
        InvalidPeImage,
        UnsupportedRuntime,
    };

    struct BytePattern
    {
        std::span<const std::byte> bytes{};
        std::span<const std::byte> mask{};
    };

    class RuntimeModule
    {
    public:
        [[nodiscard]] static RuntimeModule detect() noexcept;

        [[nodiscard]] ModuleStatus status() const noexcept { return _status; }
        [[nodiscard]] explicit operator bool() const noexcept { return _status == ModuleStatus::Ready; }
        [[nodiscard]] Addresses::RuntimeVersion version() const noexcept { return _version; }
        [[nodiscard]] std::uintptr_t base() const noexcept { return _base; }
        [[nodiscard]] std::size_t imageSize() const noexcept { return _imageSize; }

        [[nodiscard]] std::uintptr_t resolve(Addresses::Symbol symbol) const noexcept;
        [[nodiscard]] bool matches(Addresses::Symbol symbol, BytePattern pattern) const noexcept;

        template <class Function>
        [[nodiscard]] Function resolveFunction(Addresses::Symbol symbol) const noexcept
        {
            const auto& entry = Addresses::record(symbol);
            if (entry.kind != Addresses::SymbolKind::Function) {
                return nullptr;
            }
            return reinterpret_cast<Function>(resolve(symbol));
        }

    private:
        ModuleStatus _status{ ModuleStatus::ModuleUnavailable };
        Addresses::RuntimeVersion _version{};
        std::uintptr_t _base{};
        std::size_t _imageSize{};
    };
}
