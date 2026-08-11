#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/Memory.h"
#include "RPS/Runtime/Ragdoll.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <utility>

namespace
{
    namespace Layout = RPS::Addresses::Layouts::Ragdoll;

    struct FakeActor
    {
        void** vtable{};
        void* manager{};
    };

    bool getGraphManager(const void* actorValue, void** output)
    {
        const auto* actor = static_cast<const FakeActor*>(actorValue);
        if (!actor || !output || !actor->manager) {
            return false;
        }
        auto* const count = reinterpret_cast<volatile long*>(
            static_cast<std::byte*>(actor->manager) + Layout::GraphManager_ReferenceCount);
        InterlockedIncrement(count);
        *output = actor->manager;
        return true;
    }

    template <class T>
    void write(std::byte* base, const std::ptrdiff_t offset, const T& value)
    {
        std::memcpy(base + offset, &value, sizeof(value));
    }
}

int main()
{
    using namespace RPS::Runtime;
    using namespace RPS::Runtime::Physics;

    alignas(16) std::array<std::byte, Layout::GraphManagerSize> manager{};
    long referenceCount = 1;
    write(manager.data(), Layout::GraphManager_ReferenceCount, referenceCount);

    alignas(16) std::array<std::byte, 544> graph{};
    alignas(16) std::array<std::byte, 128> driver{};
    alignas(16) std::array<std::byte, 64> ragdollInterface{};
    alignas(16) std::array<std::byte, 64> worldReference{};
    alignas(16) std::array<std::byte, 128> ragdoll{};
    std::array<std::uint32_t, 3> bodyIds{ 3, 5, 8 };

    void* driverPointer = driver.data();
    void* interfacePointer = ragdollInterface.data();
    void* worldReferencePointer = worldReference.data();
    void* hknpWorld = manager.data();
    void* ragdollPointer = ragdoll.data();
    write(graph.data(), Layout::Graph_RagdollDriver, driverPointer);
    write(driver.data(), Layout::Driver_RagdollInterface, interfacePointer);
    write(ragdollInterface.data(), Layout::Interface_WorldReference, worldReferencePointer);
    write(ragdollInterface.data(), Layout::Interface_Ragdoll, ragdollPointer);
    write(worldReference.data(), Layout::WorldReference_World, hknpWorld);
    const auto* bodyData = bodyIds.data();
    const std::uint32_t bodyCount = static_cast<std::uint32_t>(bodyIds.size());
    const std::uint32_t bodyCapacity = bodyCount;
    write(ragdoll.data(), Layout::Ragdoll_BodyIds, bodyData);
    write(ragdoll.data(), Layout::Ragdoll_BodyIdCount, bodyCount);
    write(ragdoll.data(), Layout::Ragdoll_BodyIdCapacityAndFlags, bodyCapacity);

    const std::uint32_t graphFlags = Layout::GraphArrayInlineFlag | 1u;
    const std::uint32_t graphCount = 1;
    void* graphPointer = graph.data();
    write(manager.data(), Layout::GraphManager_GraphArrayFlags, graphFlags);
    write(manager.data(), Layout::GraphManager_GraphArrayStorage, graphPointer);
    write(manager.data(), Layout::GraphManager_GraphArrayCount, graphCount);

    std::array<void*, 5> actorVtable{};
    actorVtable[4] = reinterpret_cast<void*>(getGraphManager);
    FakeActor actor{ actorVtable.data(), manager.data() };
    {
        auto lease = acquireActorGraphManager(&actor);
        if (!lease) {
            std::cerr << "graph manager acquisition failed\n";
            return 1;
        }
        long retained{};
        std::memcpy(&retained, manager.data() + Layout::GraphManager_ReferenceCount, sizeof(retained));
        if (retained != 2) {
            std::cerr << "graph manager reference was not retained\n";
            return 1;
        }

        GraphManagerLock lock{ lease };
        std::array<RagdollPointers, 1> pointers{};
        const auto collection = collectRagdolls(lease, lock, pointers);
        if (!lock.active() || !collection || collection.graphCount != 1 || collection.writtenCount != 1 ||
            collection.completeCount != 1 || pointers[0].hknpWorld != hknpWorld) {
            std::cerr << "ragdoll pointer collection failed\n";
            return 1;
        }

        std::array<BodyId, 3> copied{};
        const auto bodyResult = copyRagdollBodyIds(lease, lock, pointers[0], copied);
        if (!bodyResult || bodyResult.nativeCount != 3 || bodyResult.validCount != 3 ||
            copied[0].value != 3 || copied[1].value != 5 || copied[2].value != 8) {
            std::cerr << "ragdoll body id copy failed\n";
            return 1;
        }

        auto foreignPointers = pointers[0];
        foreignPointers.ownerManager = &actor;
        if (copyRagdollBodyIds(lease, lock, foreignPointers, copied).error != RagdollError::InvalidRagdoll) {
            std::cerr << "foreign ragdoll pointers were accepted\n";
            return 1;
        }
    }

    std::memcpy(&referenceCount, manager.data() + Layout::GraphManager_ReferenceCount, sizeof(referenceCount));
    std::uint32_t lockOwner{};
    std::uint32_t lockCount{};
    std::memcpy(&lockOwner, manager.data() + Layout::GraphManager_UpdateLock, sizeof(lockOwner));
    std::memcpy(&lockCount, manager.data() + Layout::GraphManager_UpdateLock + 4, sizeof(lockCount));
    if (referenceCount != 1 || lockOwner != 0 || lockCount != 0) {
        std::cerr << "ragdoll manager RAII cleanup failed\n";
        return 1;
    }

    const auto module = RuntimeModule::detect();
    InterlockedIncrement(reinterpret_cast<volatile long*>(
        manager.data() + Layout::GraphManager_ReferenceCount));
    {
        auto apiLease = GraphManagerLease::adoptRetained(manager.data());
        RagdollManagerApi api{ module, std::move(apiLease), hknpWorld };
        WorldWriteGuard writeGuard{ module, hknpWorld };
        if (module || apiLease || !api.managerLease() || api.hasRagdoll().error != RagdollError::InvalidRuntime ||
            api.setGraphSync(true).error != RagdollError::InvalidRuntime ||
            api.activate(writeGuard, manager.data()).error != RagdollError::InvalidRuntime ||
            api.remove(writeGuard).error != RagdollError::InvalidRuntime) {
            std::cerr << "invalid runtime ragdoll API did not fail closed\n";
            return 1;
        }
    }
    std::memcpy(&referenceCount, manager.data() + Layout::GraphManager_ReferenceCount, sizeof(referenceCount));
    if (referenceCount != 1) {
        std::cerr << "ragdoll API did not release its manager lease\n";
        return 1;
    }

    return 0;
}
