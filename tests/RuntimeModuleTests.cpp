#include "RPS/Runtime/Memory.h"
#include "RPS/Runtime/RuntimeModule.h"

#include <array>
#include <cstddef>
#include <iostream>

int main()
{
    using namespace RPS::Runtime;

    std::array<std::byte, 16> bytes{};
    bytes[3] = std::byte{ 0x5A };
    std::byte copied{};
    if (!Memory::rangeHasAccess(bytes.data(), bytes.size(), Memory::Access::Read) ||
        !Memory::copyFrom(bytes.data() + 3, &copied, sizeof(copied)) || copied != std::byte{ 0x5A } ||
        Memory::rangeHasAccess(nullptr, 1, Memory::Access::Read) || Memory::rangeHasAccess(bytes.data(), 0, Memory::Access::Read)) {
        std::cerr << "memory guard contract failed\n";
        return 1;
    }

    const auto module = RuntimeModule::detect();
    if (module.status() == ModuleStatus::Ready || module.resolve(RPS::Addresses::Symbol::Physics_SetBodyVelocity) != 0) {
        std::cerr << "non-game executable must fail closed\n";
        return 1;
    }

    return 0;
}
