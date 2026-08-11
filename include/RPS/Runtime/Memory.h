#pragma once

#include <cstddef>
#include <type_traits>

namespace RPS::Runtime::Memory
{
    enum class Access
    {
        Read,
        Write,
        Execute,
    };

    [[nodiscard]] bool rangeHasAccess(const void* address, std::size_t bytes, Access access) noexcept;
    [[nodiscard]] bool copyFrom(const void* source, void* destination, std::size_t bytes) noexcept;
    [[nodiscard]] bool copyTo(void* destination, const void* source, std::size_t bytes) noexcept;

    template <class T>
        requires std::is_trivially_copyable_v<T>
    [[nodiscard]] bool read(const void* source, T& destination) noexcept
    {
        return copyFrom(source, &destination, sizeof(T));
    }

    template <class T>
        requires std::is_trivially_copyable_v<T>
    [[nodiscard]] bool write(void* destination, const T& source) noexcept
    {
        return copyTo(destination, &source, sizeof(T));
    }
}
