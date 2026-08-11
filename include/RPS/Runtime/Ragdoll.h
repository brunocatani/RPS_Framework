#pragma once

#include "RPS/Runtime/PhysicsTypes.h"
#include "RPS/Runtime/RuntimeModule.h"
#include "RPS/Runtime/WorldAccess.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace RPS::Runtime::Physics
{
    class GraphManagerLease
    {
    public:
        GraphManagerLease() = default;
        ~GraphManagerLease() noexcept;

        GraphManagerLease(const GraphManagerLease&) = delete;
        GraphManagerLease& operator=(const GraphManagerLease&) = delete;
        GraphManagerLease(GraphManagerLease&& other) noexcept;
        GraphManagerLease& operator=(GraphManagerLease&& other) noexcept;

        // The pointer must already carry one intrusive reference owned by the
        // caller. The lease consumes that reference without incrementing it.
        [[nodiscard]] static GraphManagerLease adoptRetained(void* graphManager) noexcept;
        [[nodiscard]] void* get() const noexcept { return _manager; }
        [[nodiscard]] explicit operator bool() const noexcept { return _manager != nullptr; }
        [[nodiscard]] void* release() noexcept;
        void reset() noexcept;

    private:
        explicit GraphManagerLease(void* graphManager) noexcept : _manager(graphManager) {}

        void* _manager{};
    };

    [[nodiscard]] GraphManagerLease acquireActorGraphManager(void* actor) noexcept;

    class GraphManagerLock
    {
    public:
        explicit GraphManagerLock(const GraphManagerLease& lease) noexcept;
        ~GraphManagerLock() noexcept;

        GraphManagerLock(const GraphManagerLock&) = delete;
        GraphManagerLock& operator=(const GraphManagerLock&) = delete;
        GraphManagerLock(GraphManagerLock&&) = delete;
        GraphManagerLock& operator=(GraphManagerLock&&) = delete;

        [[nodiscard]] bool active() const noexcept { return _active; }
        [[nodiscard]] bool owns(const void* manager) const noexcept { return _active && _manager == manager; }

    private:
        void* _manager{};
        void* _lock{};
        std::uint32_t _threadId{};
        bool _active{};
    };

    struct RagdollPointers
    {
        void* ownerManager{};
        std::uint32_t graphIndex{};
        void* graph{};
        void* driver{};
        void* ragdollInterface{};
        void* worldReference{};
        void* hknpWorld{};
        void* ragdoll{};

        [[nodiscard]] bool complete() const noexcept
        {
            return ownerManager && graph && driver && ragdollInterface && ragdoll;
        }
    };

    enum class RagdollError : std::uint8_t
    {
        None,
        InvalidRuntime,
        InvalidActor,
        InvalidManager,
        ManagerUnavailable,
        ManagerLockUnavailable,
        InvalidGraphArray,
        InvalidRagdoll,
        InvalidBodyArray,
        OutputUnavailable,
        InvalidWorld,
        InvalidWriteGuard,
        FunctionUnavailable,
        NativeCallFailed,
        NativeOperationRejected,
    };

    struct RagdollCollectionResult
    {
        RagdollError error{ RagdollError::InvalidManager };
        std::size_t graphCount{};
        std::size_t writtenCount{};
        std::size_t completeCount{};
        bool truncated{};

        [[nodiscard]] explicit operator bool() const noexcept { return error == RagdollError::None; }
    };

    struct RagdollBodyIdsResult
    {
        RagdollError error{ RagdollError::InvalidRagdoll };
        std::size_t nativeCount{};
        std::size_t writtenCount{};
        std::size_t validCount{};
        bool truncated{};

        [[nodiscard]] explicit operator bool() const noexcept { return error == RagdollError::None; }
    };

    // Returned engine pointers are borrowed and remain valid only while the
    // matching manager lease and lock are alive on the calling thread.
    [[nodiscard]] RagdollCollectionResult collectRagdolls(
        const GraphManagerLease& lease,
        const GraphManagerLock& lock,
        std::span<RagdollPointers> output) noexcept;
    [[nodiscard]] RagdollBodyIdsResult copyRagdollBodyIds(
        const GraphManagerLease& lease,
        const GraphManagerLock& lock,
        const RagdollPointers& pointers,
        std::span<BodyId> output) noexcept;

    struct GraphOperationResult
    {
        RagdollError error{ RagdollError::InvalidManager };
        bool invoked{};
        bool applied{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return error == RagdollError::None && invoked && applied;
        }
    };

    enum class RagdollActivationStage : std::uint8_t
    {
        NotStarted,
        HasRagdoll,
        SetPhysicsWorld,
        RemoveAttachments,
        AddToWorld,
        UpdateConstraints,
        Completed,
    };

    struct RagdollActivationResult
    {
        RagdollError error{ RagdollError::InvalidManager };
        RagdollActivationStage stage{ RagdollActivationStage::NotStarted };
        GraphOperationResult hasRagdoll{};
        GraphOperationResult setPhysicsWorld{};
        GraphOperationResult removeAttachments{};
        GraphOperationResult addToWorld{};
        GraphOperationResult updateConstraints{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return error == RagdollError::None && stage == RagdollActivationStage::Completed;
        }
    };

    struct RagdollRemovalResult
    {
        RagdollError error{ RagdollError::InvalidManager };
        GraphOperationResult removeFromWorld{};
        GraphOperationResult updateConstraints{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return error == RagdollError::None && static_cast<bool>(removeFromWorld);
        }
    };

    class RagdollManagerApi
    {
    public:
        RagdollManagerApi(RuntimeModule module, GraphManagerLease&& managerLease, void* hknpWorld) noexcept :
            _module(module), _managerLease(static_cast<GraphManagerLease&&>(managerLease)), _hknpWorld(hknpWorld)
        {}

        RagdollManagerApi(const RagdollManagerApi&) = delete;
        RagdollManagerApi& operator=(const RagdollManagerApi&) = delete;
        RagdollManagerApi(RagdollManagerApi&&) noexcept = default;
        RagdollManagerApi& operator=(RagdollManagerApi&&) noexcept = default;

        [[nodiscard]] const GraphManagerLease& managerLease() const noexcept { return _managerLease; }

        [[nodiscard]] GraphOperationResult hasRagdoll() const noexcept;
        [[nodiscard]] GraphOperationResult setGraphSync(bool value) const noexcept;
        [[nodiscard]] GraphOperationResult setInWorldCache(bool value) const noexcept;
        [[nodiscard]] RagdollActivationResult activate(
            const WorldWriteGuard& guard,
            void* bhkWorld) const noexcept;
        [[nodiscard]] RagdollRemovalResult remove(const WorldWriteGuard& guard) const noexcept;

    private:
        [[nodiscard]] GraphOperationResult invokeBoolean(Addresses::Symbol symbol) const noexcept;
        [[nodiscard]] GraphOperationResult setPhysicsWorld(void* bhkWorld) const noexcept;
        [[nodiscard]] GraphOperationResult invokeGraphSync(bool value) const noexcept;
        [[nodiscard]] GraphOperationResult invokeInWorldCache(bool value) const noexcept;

        RuntimeModule _module{};
        GraphManagerLease _managerLease{};
        void* _hknpWorld{};
    };
}
