#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/CharacterController.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace
{
    void fakeVirtualFunction() {}

    template <class T, std::size_t Size>
    void write(std::array<std::byte, Size>& storage, const std::size_t offset, const T& value)
    {
        if (offset > storage.size() || sizeof(T) > storage.size() - offset) {
            std::abort();
        }
        std::memcpy(storage.data() + offset, &value, sizeof(T));
    }
}

int main()
{
    using namespace RPS;
    using namespace Runtime;
    using namespace Character;
    namespace Layout = Addresses::Layouts::CharacterController;

    alignas(16) std::array<std::byte, Layout::Controller_MinimumReadableSize> controller{};
    alignas(16) std::array<std::byte, Layout::RigidBody_MinimumReadableSize> body{};
    std::uintptr_t controllerVtable[1]{ reinterpret_cast<std::uintptr_t>(&fakeVirtualFunction) };
    std::uintptr_t bodyVtable[1]{ reinterpret_cast<std::uintptr_t>(&fakeVirtualFunction) };
    const auto bodyAddress = reinterpret_cast<std::uintptr_t>(body.data());
    write(controller, 0, reinterpret_cast<std::uintptr_t>(controllerVtable));
    write(controller, Layout::Controller_RigidBody, bodyAddress);
    write(body, 0, reinterpret_cast<std::uintptr_t>(bodyVtable));
    write(body, Layout::RigidBody_StepGate, std::uint8_t{ 1 });

    const auto snapshot = inspectCharacterControllerPointer(controller.data());
    if (!snapshot.rigidBodyComplete() || snapshot.controllerAddress != reinterpret_cast<std::uintptr_t>(controller.data()) ||
        snapshot.controllerVtableAddress != reinterpret_cast<std::uintptr_t>(controllerVtable) ||
        snapshot.rigidBodyAddress != bodyAddress ||
        snapshot.rigidBodyVtableAddress != reinterpret_cast<std::uintptr_t>(bodyVtable) ||
        snapshot.rigidStepGate != 1) {
        std::cerr << "character-controller rigid-body snapshot failed\n";
        return 1;
    }

    write(controller, Layout::Controller_RigidBody, std::uintptr_t{});
    const auto proxyLike = inspectCharacterControllerPointer(controller.data());
    if (!proxyLike.controllerResolved || !proxyLike.vtableReadable || proxyLike.rigidBodyResolved ||
        inspectCharacterControllerPointer(nullptr).controllerResolved) {
        std::cerr << "character-controller optional rigid body did not fail closed\n";
        return 1;
    }

    const auto module = RuntimeModule::detect();
    int actor{};
    int world{};
    CharacterControllerApi api{ module, &actor };
    const auto resolve = api.resolve();
    const auto add = api.addToWorld(&world);
    if (module || resolve.status != CharacterControllerStatus::InvalidRuntime || resolve.invoked || resolve ||
        add.status != CharacterControllerStatus::InvalidRuntime || add.invoked || add ||
        toString(CharacterControllerStatus::NoFreshInsertion) != "no-fresh-insertion") {
        std::cerr << "character-controller API did not fail closed without FO4VR\n";
        return 1;
    }

    return 0;
}
