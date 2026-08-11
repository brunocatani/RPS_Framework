#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/ActorState.h"

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
    namespace Layout = Addresses::Layouts::ActorState;

    constexpr std::uint32_t lifeFlags = 7u << Layout::LifeStateShift;
    constexpr std::uint32_t knockFlags = 3u << Layout::KnockCodeShift;
    constexpr std::uint32_t movementFlags = 0xA500u | Layout::RagdollMovementFlagMask;
    if (decodeActorLifeState(lifeFlags) != 7 || decodeActorKnockCode(knockFlags) != 3 ||
        clearActorRagdollMovementFlag(movementFlags) !=
            (movementFlags & ~Layout::RagdollMovementFlagMask)) {
        std::cerr << "actor-state bit decoding failed\n";
        return 1;
    }

    alignas(16) std::array<std::byte, Layout::Actor_MinimumReadableSize> actor{};
    alignas(16) std::array<std::byte, Layout::AIProcess_MinimumReadableSize> process{};
    alignas(16) std::array<std::byte, Layout::KnockData_MinimumReadableSize> knockData{};
    alignas(16) std::array<std::byte, Layout::HighProcess_MinimumReadableSize> highProcess{};
    std::array<std::uintptr_t, 1> actorStateVtable{
        reinterpret_cast<std::uintptr_t>(&fakeVirtualFunction)
    };

    write(actor, Layout::Actor_StateSubobject, reinterpret_cast<std::uintptr_t>(actorStateVtable.data()));
    write(actor, Layout::Actor_LifeFlags, lifeFlags);
    write(actor, Layout::Actor_KnockFlags, knockFlags);
    write(actor, Layout::Actor_RagdollMovementFlags, movementFlags);
    write(actor, Layout::Actor_AIProcess, reinterpret_cast<std::uintptr_t>(process.data()));
    write(process, Layout::AIProcess_KnockData, reinterpret_cast<std::uintptr_t>(knockData.data()));
    write(process, Layout::AIProcess_HighData, reinterpret_cast<std::uintptr_t>(highProcess.data()));
    write(knockData, Layout::KnockData_CurrentHandle, std::uint32_t{ 0x12345678u });
    write(knockData, Layout::KnockData_RagdollFlag, std::uint8_t{ 1 });
    write(highProcess, Layout::HighProcess_FullRagdollFlagA, std::uint8_t{ 1 });
    write(highProcess, Layout::HighProcess_FullRagdollFlagB, std::uint8_t{ 0 });

    const auto snapshot = inspectActorStatePointer(actor.data());
    if (!snapshot.actorReadable || !snapshot.actorStateVtableReadable || !snapshot.lifeFlagsReadable ||
        snapshot.lifeState != 7 || !snapshot.knockFlagsReadable || snapshot.rawKnockCode != 3 ||
        !snapshot.ragdollMovementBit || !snapshot.processReadable || !snapshot.knockDataReadable ||
        !snapshot.processRagdollFlagReadable || !snapshot.processRagdollFlag ||
        !snapshot.knockRequestHandleReadable || snapshot.knockRequestHandle != 0x12345678u ||
        !snapshot.highProcessReadable || !snapshot.processFlagsReadable || !snapshot.processFlagA ||
        snapshot.processFlagB) {
        std::cerr << "actor-state copied snapshot failed\n";
        return 1;
    }

    write(actor, Layout::Actor_AIProcess, std::uintptr_t{});
    const auto noProcess = inspectActorStatePointer(actor.data());
    if (!noProcess.actorReadable || noProcess.processReadable || noProcess.knockDataReadable ||
        noProcess.highProcessReadable || inspectActorStatePointer(nullptr).actorReadable) {
        std::cerr << "actor-state optional process chain did not fail closed\n";
        return 1;
    }

    const auto module = RuntimeModule::detect();
    ActorStateApi api{ module, actor.data() };
    const auto inspected = api.inspect();
    const auto knock = api.readKnockState();
    const auto clearKnock = api.clearKnockState();
    const auto clearMovement = api.clearMovementRagdollBit();
    const auto clearProcess = api.clearProcessRagdollFlags();
    if (module || inspected.status != ActorStateStatus::InvalidRuntime || inspected ||
        knock.status != ActorStateStatus::InvalidRuntime || knock.invoked || knock ||
        clearKnock.status != ActorStateStatus::InvalidRuntime || clearKnock.invoked || clearKnock ||
        clearMovement.status != ActorStateStatus::InvalidRuntime || clearMovement.changed || clearMovement ||
        clearProcess.status != ActorStateStatus::InvalidRuntime || clearProcess.changed || clearProcess ||
        toString(ActorStateStatus::GenerationChanged) != "generation-changed") {
        std::cerr << "actor-state API did not fail closed without FO4VR\n";
        return 1;
    }

    return 0;
}
