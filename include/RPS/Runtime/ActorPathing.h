#pragma once

#include "RPS/Runtime/NativeReference.h"
#include "RPS/Runtime/RuntimeModule.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace RPS::Runtime::Character
{
    enum class PathingQueryStatus : std::uint8_t
    {
        Completed,
        InvalidRuntime,
        InvalidActor,
        PhysicsStepActive,
        PhysicsStepStateUnavailable,
        FunctionUnavailable,
        NativeFault,
        NonFiniteResult,
        InvalidRetainedRequest,
    };

    enum class PathingStateStage : std::uint8_t
    {
        NotStarted,
        IsPathing,
        QueryPathingState,
        NativePackageLoopGuard,
        CanSubmitGoal,
        Completed,
    };

    struct PathingStateResult
    {
        PathingQueryStatus status{ PathingQueryStatus::InvalidRuntime };
        PathingStateStage stage{ PathingStateStage::NotStarted };
        bool isPathing{};
        bool pathingStateActive{};
        bool packageLoopAllowed{};
        bool canSubmitGoal{};
        std::uint32_t packageLoopGuard{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return status == PathingQueryStatus::Completed && stage == PathingStateStage::Completed;
        }
    };

    struct DirectMovementResult
    {
        PathingQueryStatus status{ PathingQueryStatus::InvalidRuntime };
        std::array<float, 3> state{};
        std::array<float, 3> targetOffset{};
        float targetAngle{};
        bool stateAvailable{};
        bool targetOffsetAvailable{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return status == PathingQueryStatus::Completed;
        }
    };

    struct CurrentPathRequestResult
    {
        PathingQueryStatus status{ PathingQueryStatus::InvalidRuntime };
        std::uintptr_t requestAddress{};
        NativeIntrusiveReferenceSnapshot reference{};
        NativeIntrusivePtr request{};
        bool present{};

        CurrentPathRequestResult() noexcept = default;
        CurrentPathRequestResult(const CurrentPathRequestResult&) = delete;
        CurrentPathRequestResult& operator=(const CurrentPathRequestResult&) = delete;
        CurrentPathRequestResult(CurrentPathRequestResult&&) noexcept = default;
        CurrentPathRequestResult& operator=(CurrentPathRequestResult&&) noexcept = default;

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return status == PathingQueryStatus::Completed;
        }
    };

    [[nodiscard]] std::string_view toString(PathingQueryStatus status) noexcept;
    [[nodiscard]] std::string_view toString(PathingStateStage stage) noexcept;

    /**
     * Bounded actor pathing queries proven by SCISSORS. Calls must run on the
     * owning game thread outside physics. queryCurrentRequest returns the one
     * retained native reference produced by the engine; keep and destroy that
     * result on the engine-owning thread or explicitly transfer its ownership.
     */
    class ActorPathingApi
    {
    public:
        ActorPathingApi(RuntimeModule module, void* actor) noexcept : _module(module), _actor(actor) {}

        [[nodiscard]] PathingStateResult queryState() const noexcept;
        [[nodiscard]] DirectMovementResult queryDirectMovement() const noexcept;
        [[nodiscard]] CurrentPathRequestResult queryCurrentRequest() const noexcept;

    private:
        [[nodiscard]] PathingQueryStatus executionStatus() const noexcept;

        RuntimeModule _module{};
        void* _actor{};
    };
}
