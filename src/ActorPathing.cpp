#include "RPS/Runtime/ActorPathing.h"

#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/Memory.h"
#include "RPS/Runtime/WorldAccess.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <cmath>

namespace RPS::Runtime::Character
{
    namespace
    {
        namespace Layout = Addresses::Layouts::Pathing;

        using QueryBooleanFunction = bool (*)(void*);
        using QueryPackageLoopFunction = std::uint32_t (*)(void*);
        using QueryCurrentRequestFunction = void (*)(void*, void**);
        using QueryVectorFunction = bool (*)(void*, float*);
        using QueryAngleFunction = float (*)(void*);

        template <class Function>
        [[nodiscard]] Function checkedFunction(
            const RuntimeModule& module,
            const Addresses::Symbol symbol) noexcept
        {
            const auto function = module.resolveFunction<Function>(symbol);
            return Memory::rangeHasAccess(
                       reinterpret_cast<const void*>(function),
                       1,
                       Memory::Access::Execute) ?
                function :
                nullptr;
        }

        [[nodiscard]] bool invokeBoolean(
            const QueryBooleanFunction function,
            void* const actor,
            bool& output) noexcept
        {
#if defined(_MSC_VER)
            __try {
                output = function(actor);
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                output = false;
                return false;
            }
#else
            output = function(actor);
            return true;
#endif
        }

        [[nodiscard]] bool invokeCurrentRequest(
            const QueryCurrentRequestFunction function,
            void* const actor,
            void*& output) noexcept
        {
            output = nullptr;
#if defined(_MSC_VER)
            __try {
                function(actor, &output);
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                output = nullptr;
                return false;
            }
#else
            function(actor, &output);
            return true;
#endif
        }
    }

    std::string_view toString(const PathingQueryStatus status) noexcept
    {
        switch (status) {
        case PathingQueryStatus::Completed: return "completed";
        case PathingQueryStatus::InvalidRuntime: return "invalid-runtime";
        case PathingQueryStatus::InvalidActor: return "invalid-actor";
        case PathingQueryStatus::PhysicsStepActive: return "physics-step-active";
        case PathingQueryStatus::PhysicsStepStateUnavailable: return "physics-step-state-unavailable";
        case PathingQueryStatus::FunctionUnavailable: return "function-unavailable";
        case PathingQueryStatus::NativeFault: return "native-fault";
        case PathingQueryStatus::NonFiniteResult: return "non-finite-result";
        case PathingQueryStatus::InvalidRetainedRequest: return "invalid-retained-request";
        default: return "unknown";
        }
    }

    std::string_view toString(const PathingStateStage stage) noexcept
    {
        switch (stage) {
        case PathingStateStage::NotStarted: return "not-started";
        case PathingStateStage::IsPathing: return "is-pathing";
        case PathingStateStage::QueryPathingState: return "query-pathing-state";
        case PathingStateStage::NativePackageLoopGuard: return "native-package-loop-guard";
        case PathingStateStage::CanSubmitGoal: return "can-submit-goal";
        case PathingStateStage::Completed: return "completed";
        default: return "unknown";
        }
    }

    PathingQueryStatus ActorPathingApi::executionStatus() const noexcept
    {
        if (!_module) {
            return PathingQueryStatus::InvalidRuntime;
        }
        if (!Memory::rangeHasAccess(
                _actor,
                Layout::Actor_MinimumReadableSize,
                Memory::Access::Read)) {
            return PathingQueryStatus::InvalidActor;
        }
        switch (Physics::currentThreadPhysicsStepState(_module)) {
        case Physics::PhysicsStepState::Inside:
            return PathingQueryStatus::PhysicsStepActive;
        case Physics::PhysicsStepState::Unknown:
            return PathingQueryStatus::PhysicsStepStateUnavailable;
        case Physics::PhysicsStepState::Outside:
            return PathingQueryStatus::Completed;
        }
        return PathingQueryStatus::PhysicsStepStateUnavailable;
    }

    PathingStateResult ActorPathingApi::queryState() const noexcept
    {
        PathingStateResult result{};
        result.status = executionStatus();
        if (result.status != PathingQueryStatus::Completed) {
            return result;
        }
        const auto isPathing = checkedFunction<QueryBooleanFunction>(
            _module,
            Addresses::Symbol::Character_ActorIsPathing);
        const auto queryPathingState = checkedFunction<QueryBooleanFunction>(
            _module,
            Addresses::Symbol::Character_ActorQueryPathingState);
        const auto packageLoopGuard = checkedFunction<QueryPackageLoopFunction>(
            _module,
            Addresses::Symbol::Character_ActorNativePackageLoopGuard);
        const auto canSubmitGoal = checkedFunction<QueryBooleanFunction>(
            _module,
            Addresses::Symbol::Character_ActorCanSubmitPathingGoal);
        if (!isPathing || !queryPathingState || !packageLoopGuard || !canSubmitGoal) {
            result.status = PathingQueryStatus::FunctionUnavailable;
            return result;
        }

        result.stage = PathingStateStage::IsPathing;
        if (!invokeBoolean(isPathing, _actor, result.isPathing)) {
            result.status = PathingQueryStatus::NativeFault;
            return result;
        }
        result.stage = PathingStateStage::QueryPathingState;
        if (!invokeBoolean(queryPathingState, _actor, result.pathingStateActive)) {
            result.status = PathingQueryStatus::NativeFault;
            return result;
        }
        result.stage = PathingStateStage::NativePackageLoopGuard;
#if defined(_MSC_VER)
        __try {
            result.packageLoopGuard = packageLoopGuard(_actor);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            result.status = PathingQueryStatus::NativeFault;
            return result;
        }
#else
        result.packageLoopGuard = packageLoopGuard(_actor);
#endif
        result.packageLoopAllowed =
            (result.packageLoopGuard & Layout::PackageLoopAllowedMask) != 0;
        result.stage = PathingStateStage::CanSubmitGoal;
        if (!invokeBoolean(canSubmitGoal, _actor, result.canSubmitGoal)) {
            result.status = PathingQueryStatus::NativeFault;
            return result;
        }
        result.stage = PathingStateStage::Completed;
        return result;
    }

    DirectMovementResult ActorPathingApi::queryDirectMovement() const noexcept
    {
        DirectMovementResult result{};
        result.status = executionStatus();
        if (result.status != PathingQueryStatus::Completed) {
            return result;
        }
        const auto queryState = checkedFunction<QueryVectorFunction>(
            _module,
            Addresses::Symbol::Character_ActorQueryDirectMovementState);
        const auto queryOffset = checkedFunction<QueryVectorFunction>(
            _module,
            Addresses::Symbol::Character_ActorQueryDirectMovementTargetOffset);
        const auto queryAngle = checkedFunction<QueryAngleFunction>(
            _module,
            Addresses::Symbol::Character_ActorQueryDirectMovementTargetAngle);
        if (!queryState || !queryOffset || !queryAngle) {
            result.status = PathingQueryStatus::FunctionUnavailable;
            return result;
        }
#if defined(_MSC_VER)
        __try {
            result.stateAvailable = queryState(_actor, result.state.data());
            result.targetOffsetAvailable = queryOffset(_actor, result.targetOffset.data());
            result.targetAngle = queryAngle(_actor);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            result = {};
            result.status = PathingQueryStatus::NativeFault;
            return result;
        }
#else
        result.stateAvailable = queryState(_actor, result.state.data());
        result.targetOffsetAvailable = queryOffset(_actor, result.targetOffset.data());
        result.targetAngle = queryAngle(_actor);
#endif
        const auto finiteVector = [](const std::array<float, Layout::DirectMovementVectorCount>& vector) {
            return std::all_of(vector.begin(), vector.end(), [](const float value) {
                return std::isfinite(value);
            });
        };
        if ((result.stateAvailable && !finiteVector(result.state)) ||
            (result.targetOffsetAvailable && !finiteVector(result.targetOffset)) ||
            !std::isfinite(result.targetAngle)) {
            result.status = PathingQueryStatus::NonFiniteResult;
        }
        return result;
    }

    CurrentPathRequestResult ActorPathingApi::queryCurrentRequest() const noexcept
    {
        CurrentPathRequestResult result{};
        result.status = executionStatus();
        if (result.status != PathingQueryStatus::Completed) {
            return result;
        }
        const auto function = checkedFunction<QueryCurrentRequestFunction>(
            _module,
            Addresses::Symbol::Character_ActorQueryCurrentPathRequest);
        if (!function) {
            result.status = PathingQueryStatus::FunctionUnavailable;
            return result;
        }
        void* request{};
        if (!invokeCurrentRequest(function, _actor, request)) {
            result.status = PathingQueryStatus::NativeFault;
            return result;
        }
        result.requestAddress = reinterpret_cast<std::uintptr_t>(request);
        result.present = request != nullptr;
        if (!result.present) {
            return result;
        }
        result.reference = inspectNativeIntrusiveReference(request);
        result.request = NativeIntrusivePtr::adoptRetained(request);
        if (!result.reference.valid || result.reference.referenceCount <= 0 || !result.request) {
            result.status = PathingQueryStatus::InvalidRetainedRequest;
        }
        return result;
    }
}
