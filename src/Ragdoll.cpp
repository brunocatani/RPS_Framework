#include "RPS/Runtime/Ragdoll.h"

#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/Memory.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <intrin.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>

namespace RPS::Runtime::Physics
{
    namespace
    {
        namespace Layout = Addresses::Layouts::Ragdoll;

        using GetGraphManagerFunction = bool (*)(const void*, void**);
        using BooleanFunctor = std::uint64_t (*)(void*, bool*);

        struct SetPhysicsWorldState
        {
            void* world{};
            bool applied{};
            std::array<std::byte, 7> reserved{};
        };
        static_assert(sizeof(SetPhysicsWorldState) == Layout::SetPhysicsWorldStateSize);
        static_assert(offsetof(SetPhysicsWorldState, applied) == Layout::SetPhysicsWorldState_Applied);
        using SetPhysicsWorldFunctor = std::uint64_t (*)(void*, SetPhysicsWorldState*);

        struct GraphSyncState
        {
            bool applied{};
            bool value{};
        };

        struct InWorldCacheState
        {
            bool value{};
            bool applied{};
        };

        [[nodiscard]] bool validManager(const void* const manager) noexcept
        {
            return Memory::rangeHasAccess(manager, Layout::GraphManagerSize, Memory::Access::Read);
        }

        [[nodiscard]] bool releaseManagerReference(void* const manager) noexcept
        {
            if (!validManager(manager)) {
                return false;
            }
            auto* const referenceCount = reinterpret_cast<volatile long*>(
                reinterpret_cast<std::byte*>(manager) + Layout::GraphManager_ReferenceCount);
            if (!Memory::rangeHasAccess(
                    const_cast<const long*>(referenceCount), sizeof(long), Memory::Access::Write)) {
                return false;
            }

            auto current = InterlockedCompareExchange(referenceCount, 0, 0);
            while (current > 1) {
                const auto exchanged = InterlockedCompareExchange(referenceCount, current - 1, current);
                if (exchanged == current) {
                    return true;
                }
                current = exchanged;
            }

            // A valid actor-held manager cannot reach its terminal reference
            // here. Keep the unexpected last reference rather than destroying
            // an engine allocation through the consumer CRT.
            return false;
        }

        template <class Result, class Function, class... Arguments>
        [[nodiscard]] bool invoke(Result& result, const Function function, Arguments... arguments) noexcept
        {
            result = {};
            if (!function || !Memory::rangeHasAccess(
                    reinterpret_cast<const void*>(function), 1, Memory::Access::Execute)) {
                return false;
            }
#if defined(_MSC_VER)
            __try {
                result = function(arguments...);
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                result = {};
                return false;
            }
#else
            result = function(arguments...);
            return true;
#endif
        }

        [[nodiscard]] bool acquireSpinLock(void* const lock, const std::uint32_t threadId) noexcept
        {
            if (!lock || threadId == 0 || !Memory::rangeHasAccess(lock, 8, Memory::Access::Write)) {
                return false;
            }
            auto* const owner = reinterpret_cast<volatile long*>(lock);
            auto* const count = reinterpret_cast<volatile long*>(reinterpret_cast<std::byte*>(lock) + 4);

            for (std::uint32_t attempt = 0; attempt < Layout::GraphLockAttemptLimit; ++attempt) {
                const auto currentOwner = InterlockedCompareExchange(owner, 0, 0);
                const auto currentCount = InterlockedCompareExchange(count, 0, 0);
                if (currentOwner == static_cast<long>(threadId) && currentCount > 0 &&
                    currentCount < (std::numeric_limits<long>::max)()) {
                    if (InterlockedCompareExchange(count, currentCount + 1, currentCount) == currentCount) {
                        return true;
                    }
                } else if (currentCount == 0 && InterlockedCompareExchange(count, 1, 0) == 0) {
                    InterlockedExchange(owner, static_cast<long>(threadId));
                    return true;
                }
                YieldProcessor();
            }
            return false;
        }

        void releaseSpinLock(void* const lock, const std::uint32_t threadId) noexcept
        {
            if (!lock || threadId == 0) {
                return;
            }
            auto* const owner = reinterpret_cast<volatile long*>(lock);
            auto* const count = reinterpret_cast<volatile long*>(reinterpret_cast<std::byte*>(lock) + 4);
            if (InterlockedCompareExchange(owner, 0, 0) != static_cast<long>(threadId)) {
                return;
            }
            const auto currentCount = InterlockedCompareExchange(count, 0, 0);
            if (currentCount == 1) {
                InterlockedExchange(owner, 0);
                static_cast<void>(InterlockedCompareExchange(count, 0, 1));
            } else if (currentCount > 1) {
                static_cast<void>(InterlockedDecrement(count));
            }
        }

        [[nodiscard]] void* readPointer(const void* const base, const std::ptrdiff_t offset) noexcept
        {
            void* result{};
            if (!base || !Memory::read(reinterpret_cast<const std::byte*>(base) + offset, result) ||
                !Memory::rangeHasAccess(result, sizeof(void*), Memory::Access::Read)) {
                return nullptr;
            }
            return result;
        }

        [[nodiscard]] RagdollPointers inspectGraph(
            void* const ownerManager,
            const std::uint32_t index,
            void* const graph) noexcept
        {
            RagdollPointers result{};
            result.ownerManager = ownerManager;
            result.graphIndex = index;
            result.graph = graph;
            result.driver = readPointer(graph, Layout::Graph_RagdollDriver);
            result.ragdollInterface = readPointer(result.driver, Layout::Driver_RagdollInterface);
            result.worldReference = readPointer(result.ragdollInterface, Layout::Interface_WorldReference);
            result.hknpWorld = readPointer(result.worldReference, Layout::WorldReference_World);
            result.ragdoll = readPointer(result.ragdollInterface, Layout::Interface_Ragdoll);
            return result;
        }

        [[nodiscard]] GraphOperationResult operationFailure(const RagdollError error) noexcept
        {
            return { error, false, false };
        }
    }

    GraphManagerLease::~GraphManagerLease() noexcept
    {
        reset();
    }

    GraphManagerLease::GraphManagerLease(GraphManagerLease&& other) noexcept :
        _manager(other.release())
    {}

    GraphManagerLease& GraphManagerLease::operator=(GraphManagerLease&& other) noexcept
    {
        if (this != &other) {
            reset();
            _manager = other.release();
        }
        return *this;
    }

    GraphManagerLease GraphManagerLease::adoptRetained(void* const graphManager) noexcept
    {
        return validManager(graphManager) ? GraphManagerLease{ graphManager } : GraphManagerLease{};
    }

    void* GraphManagerLease::release() noexcept
    {
        void* const result = _manager;
        _manager = nullptr;
        return result;
    }

    void GraphManagerLease::reset() noexcept
    {
        if (_manager) {
            static_cast<void>(releaseManagerReference(_manager));
            _manager = nullptr;
        }
    }

    GraphManagerLease acquireActorGraphManager(void* const actor) noexcept
    {
        if (!Memory::rangeHasAccess(actor, sizeof(void*), Memory::Access::Read)) {
            return {};
        }
        void* vtable{};
        if (!Memory::read(actor, vtable) || !vtable) {
            return {};
        }
        GetGraphManagerFunction function{};
        if (!Memory::read(
                reinterpret_cast<const std::byte*>(vtable) + Layout::Actor_GetGraphManagerVtable,
                function) ||
            !Memory::rangeHasAccess(reinterpret_cast<const void*>(function), 1, Memory::Access::Execute)) {
            return {};
        }

        void* manager{};
        bool returned{};
        if (!invoke(returned, function, actor, &manager) || !returned || !validManager(manager)) {
            if (manager) {
                static_cast<void>(releaseManagerReference(manager));
            }
            return {};
        }
        return GraphManagerLease::adoptRetained(manager);
    }

    GraphManagerLock::GraphManagerLock(const GraphManagerLease& lease) noexcept :
        _manager(lease.get()),
        _lock(_manager ? reinterpret_cast<std::byte*>(_manager) + Layout::GraphManager_UpdateLock : nullptr),
        _threadId(GetCurrentThreadId())
    {
        _active = validManager(_manager) && acquireSpinLock(_lock, _threadId);
    }

    GraphManagerLock::~GraphManagerLock() noexcept
    {
        if (_active) {
            releaseSpinLock(_lock, _threadId);
        }
        _active = false;
    }

    RagdollCollectionResult collectRagdolls(
        const GraphManagerLease& lease,
        const GraphManagerLock& lock,
        const std::span<RagdollPointers> output) noexcept
    {
        RagdollCollectionResult result{};
        void* const manager = lease.get();
        if (!validManager(manager)) {
            result.error = RagdollError::InvalidManager;
            return result;
        }
        if (!lock.owns(manager)) {
            result.error = RagdollError::ManagerLockUnavailable;
            return result;
        }

        const auto* const base = reinterpret_cast<const std::byte*>(manager);
        std::uint32_t flags{};
        std::uint32_t count{};
        if (!Memory::read(base + Layout::GraphManager_GraphArrayFlags, flags) ||
            !Memory::read(base + Layout::GraphManager_GraphArrayCount, count)) {
            result.error = RagdollError::InvalidGraphArray;
            return result;
        }
        const auto capacity = flags & Layout::GraphArrayCapacityMask;
        if (count > capacity || count > Layout::MaximumGraphCount) {
            result.error = RagdollError::InvalidGraphArray;
            return result;
        }
        result.graphCount = count;
        if (count == 0) {
            result.error = RagdollError::None;
            return result;
        }
        if (output.empty()) {
            result.error = RagdollError::OutputUnavailable;
            return result;
        }

        const std::byte* storage = base + Layout::GraphManager_GraphArrayStorage;
        if ((flags & Layout::GraphArrayInlineFlag) == 0) {
            if (!Memory::read(base + Layout::GraphManager_GraphArrayStorage, storage) || !storage) {
                result.error = RagdollError::InvalidGraphArray;
                return result;
            }
        }
        if (!Memory::rangeHasAccess(storage, static_cast<std::size_t>(count) * sizeof(void*), Memory::Access::Read)) {
            result.error = RagdollError::InvalidGraphArray;
            return result;
        }

        result.writtenCount = (std::min)(output.size(), static_cast<std::size_t>(count));
        result.truncated = result.writtenCount != count;
        for (std::size_t index = 0; index < result.writtenCount; ++index) {
            void* graph{};
            if (Memory::read(storage + index * sizeof(void*), graph) && graph) {
                output[index] = inspectGraph(manager, static_cast<std::uint32_t>(index), graph);
                result.completeCount += output[index].complete() ? 1 : 0;
            } else {
                output[index] = {};
                output[index].ownerManager = manager;
                output[index].graphIndex = static_cast<std::uint32_t>(index);
            }
        }
        result.error = RagdollError::None;
        return result;
    }

    RagdollBodyIdsResult copyRagdollBodyIds(
        const GraphManagerLease& lease,
        const GraphManagerLock& lock,
        const RagdollPointers& pointers,
        const std::span<BodyId> output) noexcept
    {
        RagdollBodyIdsResult result{};
        if (!lease || !lock.owns(lease.get())) {
            result.error = RagdollError::ManagerLockUnavailable;
            return result;
        }
        if (pointers.ownerManager != lease.get() || !pointers.ragdoll) {
            result.error = RagdollError::InvalidRagdoll;
            return result;
        }

        const auto* const base = reinterpret_cast<const std::byte*>(pointers.ragdoll);
        const std::uint32_t* data{};
        std::uint32_t count{};
        std::uint32_t capacityAndFlags{};
        if (!Memory::read(base + Layout::Ragdoll_BodyIds, data) ||
            !Memory::read(base + Layout::Ragdoll_BodyIdCount, count) ||
            !Memory::read(base + Layout::Ragdoll_BodyIdCapacityAndFlags, capacityAndFlags)) {
            result.error = RagdollError::InvalidBodyArray;
            return result;
        }
        const auto capacity = capacityAndFlags & Layout::ArrayCapacityMask;
        if (!data || count == 0 || count > capacity || count > Layout::MaximumBodyCount ||
            !Memory::rangeHasAccess(data, static_cast<std::size_t>(count) * sizeof(std::uint32_t), Memory::Access::Read)) {
            result.error = RagdollError::InvalidBodyArray;
            return result;
        }
        result.nativeCount = count;
        if (output.empty()) {
            result.error = RagdollError::OutputUnavailable;
            return result;
        }

        result.writtenCount = (std::min)(output.size(), static_cast<std::size_t>(count));
        result.truncated = result.writtenCount != count;
        for (std::size_t index = 0; index < result.writtenCount; ++index) {
            std::uint32_t value{};
            if (!Memory::read(data + index, value)) {
                result.error = RagdollError::InvalidBodyArray;
                result.writtenCount = index;
                return result;
            }
            output[index] = BodyId{ value };
            result.validCount += output[index].valid() ? 1 : 0;
        }
        result.error = RagdollError::None;
        return result;
    }

    GraphOperationResult RagdollManagerApi::invokeBoolean(const Addresses::Symbol symbol) const noexcept
    {
        if (!_module) {
            return operationFailure(RagdollError::InvalidRuntime);
        }
        void* const manager = _managerLease.get();
        if (!validManager(manager)) {
            return operationFailure(RagdollError::InvalidManager);
        }
        const auto function = _module.resolveFunction<BooleanFunctor>(symbol);
        if (!function) {
            return operationFailure(RagdollError::FunctionUnavailable);
        }
        bool applied{};
        std::uint64_t ignored{};
        if (!invoke(ignored, function, manager, &applied)) {
            return operationFailure(RagdollError::NativeCallFailed);
        }
        return { applied ? RagdollError::None : RagdollError::NativeOperationRejected, true, applied };
    }

    GraphOperationResult RagdollManagerApi::hasRagdoll() const noexcept
    {
        return invokeBoolean(Addresses::Symbol::Ragdoll_HasRagdoll);
    }

    GraphOperationResult RagdollManagerApi::setPhysicsWorld(void* const bhkWorld) const noexcept
    {
        if (!_module) {
            return operationFailure(RagdollError::InvalidRuntime);
        }
        void* const manager = _managerLease.get();
        if (!validManager(manager)) {
            return operationFailure(RagdollError::InvalidManager);
        }
        if (!bhkWorld || !Memory::rangeHasAccess(bhkWorld, sizeof(void*), Memory::Access::Read)) {
            return operationFailure(RagdollError::InvalidWorld);
        }
        const auto function = _module.resolveFunction<SetPhysicsWorldFunctor>(Addresses::Symbol::Ragdoll_SetPhysicsWorld);
        if (!function) {
            return operationFailure(RagdollError::FunctionUnavailable);
        }
        SetPhysicsWorldState state{ bhkWorld };
        std::uint64_t ignored{};
        if (!invoke(ignored, function, manager, &state)) {
            return operationFailure(RagdollError::NativeCallFailed);
        }
        return { state.applied ? RagdollError::None : RagdollError::NativeOperationRejected, true, state.applied };
    }

    GraphOperationResult RagdollManagerApi::invokeGraphSync(const bool value) const noexcept
    {
        void* const manager = _managerLease.get();
        if (!_module || !validManager(manager)) {
            return operationFailure(_module ? RagdollError::InvalidManager : RagdollError::InvalidRuntime);
        }
        const auto function = _module.resolveFunction<BooleanFunctor>(Addresses::Symbol::Ragdoll_SetGraphSync);
        if (!function) {
            return operationFailure(RagdollError::FunctionUnavailable);
        }
        GraphSyncState state{ false, value };
        std::uint64_t ignored{};
        if (!invoke(ignored, function, manager, &state.applied)) {
            return operationFailure(RagdollError::NativeCallFailed);
        }
        return { state.applied ? RagdollError::None : RagdollError::NativeOperationRejected, true, state.applied };
    }

    GraphOperationResult RagdollManagerApi::invokeInWorldCache(const bool value) const noexcept
    {
        void* const manager = _managerLease.get();
        if (!_module || !validManager(manager)) {
            return operationFailure(_module ? RagdollError::InvalidManager : RagdollError::InvalidRuntime);
        }
        const auto function = _module.resolveFunction<BooleanFunctor>(Addresses::Symbol::Ragdoll_SetInWorldCache);
        if (!function) {
            return operationFailure(RagdollError::FunctionUnavailable);
        }
        InWorldCacheState state{ value, false };
        std::uint64_t ignored{};
        if (!invoke(ignored, function, manager, &state.value)) {
            return operationFailure(RagdollError::NativeCallFailed);
        }
        return { state.applied ? RagdollError::None : RagdollError::NativeOperationRejected, true, state.applied };
    }

    GraphOperationResult RagdollManagerApi::setGraphSync(const bool value) const noexcept
    {
        return invokeGraphSync(value);
    }

    GraphOperationResult RagdollManagerApi::setInWorldCache(const bool value) const noexcept
    {
        return invokeInWorldCache(value);
    }

    RagdollActivationResult RagdollManagerApi::activate(
        const WorldWriteGuard& guard,
        void* const bhkWorld) const noexcept
    {
        RagdollActivationResult result{};
        if (!_module) {
            result.error = RagdollError::InvalidRuntime;
            return result;
        }
        if (!validManager(_managerLease.get())) {
            result.error = RagdollError::InvalidManager;
            return result;
        }
        if (!_hknpWorld) {
            result.error = RagdollError::InvalidWorld;
            return result;
        }
        if (!guard.owns(_hknpWorld)) {
            result.error = RagdollError::InvalidWriteGuard;
            return result;
        }

        result.stage = RagdollActivationStage::HasRagdoll;
        result.hasRagdoll = hasRagdoll();
        if (!result.hasRagdoll) {
            result.error = result.hasRagdoll.error;
            return result;
        }
        result.stage = RagdollActivationStage::SetPhysicsWorld;
        result.setPhysicsWorld = setPhysicsWorld(bhkWorld);
        if (!result.setPhysicsWorld) {
            result.error = result.setPhysicsWorld.error;
            return result;
        }
        result.stage = RagdollActivationStage::RemoveAttachments;
        result.removeAttachments = invokeBoolean(Addresses::Symbol::Ragdoll_RemoveAttachments);
        if (!result.removeAttachments) {
            result.error = result.removeAttachments.error;
            return result;
        }
        result.stage = RagdollActivationStage::AddToWorld;
        result.addToWorld = invokeBoolean(Addresses::Symbol::Ragdoll_AddToWorld);
        if (!result.addToWorld) {
            result.error = result.addToWorld.error;
            return result;
        }
        result.stage = RagdollActivationStage::UpdateConstraints;
        result.updateConstraints = invokeBoolean(Addresses::Symbol::Ragdoll_UpdateConstraints);
        result.stage = RagdollActivationStage::Completed;
        result.error = RagdollError::None;
        return result;
    }

    RagdollRemovalResult RagdollManagerApi::remove(const WorldWriteGuard& guard) const noexcept
    {
        RagdollRemovalResult result{};
        if (!_module) {
            result.error = RagdollError::InvalidRuntime;
            return result;
        }
        if (!validManager(_managerLease.get())) {
            result.error = RagdollError::InvalidManager;
            return result;
        }
        if (!_hknpWorld) {
            result.error = RagdollError::InvalidWorld;
            return result;
        }
        if (!guard.owns(_hknpWorld)) {
            result.error = RagdollError::InvalidWriteGuard;
            return result;
        }
        result.removeFromWorld = invokeBoolean(Addresses::Symbol::Ragdoll_RemoveFromWorld);
        result.updateConstraints = invokeBoolean(Addresses::Symbol::Ragdoll_UpdateConstraints);
        result.error = result.removeFromWorld.error;
        return result;
    }
}
