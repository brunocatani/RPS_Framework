#pragma once

#include "RPS/Runtime/RuntimeModule.h"

#include <cstddef>

namespace RPS::Runtime
{
    class HavokAllocator
    {
    public:
        explicit HavokAllocator(RuntimeModule module) noexcept : _module(module) {}

        [[nodiscard]] void* allocate(std::size_t bytes) const noexcept;
        [[nodiscard]] bool deallocate(void* allocation, std::size_t bytes) const noexcept;
        [[nodiscard]] bool reserveArray(void* arrayHeader, int elementBytes) const noexcept;

    private:
        [[nodiscard]] void* currentThreadAllocator() const noexcept;

        RuntimeModule _module{};
    };
}
