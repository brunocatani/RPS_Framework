#pragma once

#include "RPS/Runtime/RuntimeModule.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace RPS::Runtime::Hooks
{
    inline constexpr std::size_t DirectCallSize = 5;
    inline constexpr std::size_t MaximumPatchGroupSize = 256;
    inline constexpr std::size_t NoFailedIndex = (std::numeric_limits<std::size_t>::max)();

    enum class SiteStatus : std::uint8_t
    {
        Ready,
        InvalidArgument,
        InvalidRuntime,
        InvalidPattern,
        WrongSymbolKind,
        Unresolved,
        Unreadable,
        Misaligned,
        NotExecutable,
        NotDirectCall,
        TargetOverflow,
        TargetNotExecutable,
        TargetMismatch,
        PatternMismatch,
        ProtectionFailed,
        WriteFailed,
        ProtectionRestoreFailed,
        InstructionCacheFlushFailed,
        VerificationFailed,
    };

    struct SiteInspection
    {
        SiteStatus status{ SiteStatus::InvalidArgument };
        std::uintptr_t address{};
        std::uintptr_t target{};

        [[nodiscard]] explicit operator bool() const noexcept { return status == SiteStatus::Ready; }
    };

    struct DirectCallPatch
    {
        std::uintptr_t callsite{};
        std::uintptr_t expectedTarget{};
        std::uintptr_t replacementTarget{};
    };

    struct VtableSlotPatch
    {
        std::uintptr_t slot{};
        std::uintptr_t expectedTarget{};
        std::uintptr_t replacementTarget{};
    };

    enum class TransactionStatus : std::uint8_t
    {
        Applied,
        EmptyGroup,
        GroupTooLarge,
        CaptureSizeMismatch,
        DuplicateSite,
        PreflightFailed,
        CommitFailedRestored,
        RollbackFailed,
    };

    struct PatchTransactionResult
    {
        TransactionStatus status{ TransactionStatus::EmptyGroup };
        std::size_t failedIndex{ NoFailedIndex };
        std::size_t changedCount{};
        SiteStatus siteStatus{ SiteStatus::InvalidArgument };
        SiteStatus rollbackStatus{ SiteStatus::Ready };

        [[nodiscard]] explicit operator bool() const noexcept { return status == TransactionStatus::Applied; }
    };

    // The caller owns thread quiescence for the complete inspection and patch
    // interval. Direct E8 rewrites are not instruction-stream atomic.
    [[nodiscard]] SiteInspection inspectDirectCall(
        std::uintptr_t callsite,
        std::uintptr_t expectedTarget = 0) noexcept;
    [[nodiscard]] SiteStatus encodeDirectCall(
        std::uintptr_t callsite,
        std::uintptr_t target,
        std::array<std::byte, DirectCallSize>& encoded) noexcept;
    [[nodiscard]] SiteStatus replaceDirectCallTarget(
        std::uintptr_t callsite,
        std::uintptr_t expectedTarget,
        std::uintptr_t replacementTarget) noexcept;
    [[nodiscard]] PatchTransactionResult patchDirectCallsTransactional(
        std::span<const DirectCallPatch> patches,
        std::span<std::uintptr_t> capturedTargets) noexcept;

    // Vtable slots must be naturally aligned. The pointer exchange itself is
    // atomic, but the caller still owns group synchronization, object lifetime,
    // and detour shutdown.
    [[nodiscard]] SiteInspection inspectVtableSlot(
        std::uintptr_t slot,
        std::uintptr_t expectedTarget = 0) noexcept;
    [[nodiscard]] SiteStatus replaceVtableSlotTarget(
        std::uintptr_t slot,
        std::uintptr_t expectedTarget,
        std::uintptr_t replacementTarget) noexcept;
    [[nodiscard]] PatchTransactionResult patchVtableSlotsTransactional(
        std::span<const VtableSlotPatch> patches,
        std::span<std::uintptr_t> capturedTargets) noexcept;

    // This is a read-only entry/patch-site identity gate. It does not allocate
    // a trampoline or infer how many instructions a detour may overwrite.
    [[nodiscard]] SiteInspection inspectExecutableSite(
        const RuntimeModule& module,
        Addresses::Symbol symbol,
        BytePattern pattern) noexcept;
}
