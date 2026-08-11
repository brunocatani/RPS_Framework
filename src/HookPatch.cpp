#include "RPS/Runtime/HookPatch.h"

#include "RPS/Runtime/Memory.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <type_traits>

namespace RPS::Runtime::Hooks
{
    namespace
    {
        struct ProtectedSegment
        {
            void* address{};
            std::size_t bytes{};
            DWORD originalProtection{};
        };

        class WritableRange
        {
        public:
            [[nodiscard]] SiteStatus acquire(void* const address, const std::size_t bytes) noexcept
            {
                if (!address || bytes == 0 || _count != 0) {
                    return SiteStatus::InvalidArgument;
                }

                const auto start = reinterpret_cast<std::uintptr_t>(address);
                if (start > (std::numeric_limits<std::uintptr_t>::max)() - bytes) {
                    return SiteStatus::InvalidArgument;
                }
                const auto end = start + bytes;
                auto cursor = start;

                while (cursor < end) {
                    if (_count == _segments.size()) {
                        return releaseAfterFailure(SiteStatus::ProtectionFailed);
                    }

                    MEMORY_BASIC_INFORMATION information{};
                    if (VirtualQuery(reinterpret_cast<const void*>(cursor), &information, sizeof(information)) !=
                            sizeof(information) ||
                        information.State != MEM_COMMIT ||
                        (information.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
                        return releaseAfterFailure(SiteStatus::ProtectionFailed);
                    }

                    const auto regionStart = reinterpret_cast<std::uintptr_t>(information.BaseAddress);
                    if (regionStart > (std::numeric_limits<std::uintptr_t>::max)() - information.RegionSize) {
                        return releaseAfterFailure(SiteStatus::ProtectionFailed);
                    }
                    const auto regionEnd = regionStart + information.RegionSize;
                    if (regionEnd <= cursor) {
                        return releaseAfterFailure(SiteStatus::ProtectionFailed);
                    }

                    const auto segmentEnd = (std::min)(end, regionEnd);
                    const auto segmentBytes = segmentEnd - cursor;
                    const auto baseProtection = information.Protect & 0xFFu;
                    const bool executable = baseProtection == PAGE_EXECUTE || baseProtection == PAGE_EXECUTE_READ ||
                                            baseProtection == PAGE_EXECUTE_READWRITE ||
                                            baseProtection == PAGE_EXECUTE_WRITECOPY;
                    const bool writable = baseProtection == PAGE_READWRITE || baseProtection == PAGE_WRITECOPY ||
                                          baseProtection == PAGE_EXECUTE_READWRITE ||
                                          baseProtection == PAGE_EXECUTE_WRITECOPY;

                    if (!writable) {
                        DWORD originalProtection{};
                        const auto desiredProtection = executable ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;
                        if (!VirtualProtect(
                                reinterpret_cast<void*>(cursor),
                                segmentBytes,
                                desiredProtection,
                                &originalProtection)) {
                            return releaseAfterFailure(SiteStatus::ProtectionFailed);
                        }
                        _segments[_count++] = ProtectedSegment{
                            reinterpret_cast<void*>(cursor), segmentBytes, originalProtection
                        };
                    }

                    cursor = segmentEnd;
                }
                return SiteStatus::Ready;
            }

            [[nodiscard]] SiteStatus release() noexcept
            {
                bool restored = true;
                while (_count != 0) {
                    const auto& segment = _segments[--_count];
                    DWORD ignored{};
                    restored = VirtualProtect(
                                   segment.address,
                                   segment.bytes,
                                   segment.originalProtection,
                                   &ignored) != FALSE &&
                               restored;
                }
                return restored ? SiteStatus::Ready : SiteStatus::ProtectionRestoreFailed;
            }

            ~WritableRange()
            {
                if (_count != 0) {
                    static_cast<void>(release());
                }
            }

        private:
            [[nodiscard]] SiteStatus releaseAfterFailure(const SiteStatus failure) noexcept
            {
                return release() == SiteStatus::Ready ? failure : SiteStatus::ProtectionRestoreFailed;
            }

            std::array<ProtectedSegment, 4> _segments{};
            std::size_t _count{};
        };

        [[nodiscard]] SiteStatus writeProtectedBytes(
            void* const destination,
            const void* const source,
            const std::size_t bytes,
            const bool flushInstructions) noexcept
        {
            WritableRange writable{};
            const auto acquireStatus = writable.acquire(destination, bytes);
            if (acquireStatus != SiteStatus::Ready) {
                return acquireStatus;
            }

            const bool copied = Memory::copyTo(destination, source, bytes);
            const bool flushed = !flushInstructions ||
                                 FlushInstructionCache(GetCurrentProcess(), destination, bytes) != FALSE;
            const auto releaseStatus = writable.release();
            if (releaseStatus != SiteStatus::Ready) {
                return releaseStatus;
            }
            if (!copied) {
                return SiteStatus::WriteFailed;
            }
            if (!flushed) {
                return SiteStatus::InstructionCacheFlushFailed;
            }
            return SiteStatus::Ready;
        }

        [[nodiscard]] SiteStatus exchangeProtectedPointer(
            const std::uintptr_t slot,
            const std::uintptr_t replacement) noexcept
        {
            static_assert(sizeof(std::uintptr_t) == sizeof(void*));

            WritableRange writable{};
            const auto acquireStatus = writable.acquire(reinterpret_cast<void*>(slot), sizeof(std::uintptr_t));
            if (acquireStatus != SiteStatus::Ready) {
                return acquireStatus;
            }

            InterlockedExchangePointer(
                reinterpret_cast<void* volatile*>(slot),
                reinterpret_cast<void*>(replacement));
            return writable.release();
        }

        [[nodiscard]] bool isExecutable(const std::uintptr_t address) noexcept
        {
            return address != 0 && Memory::rangeHasAccess(
                                       reinterpret_cast<const void*>(address),
                                       1,
                                       Memory::Access::Execute);
        }

        template <class Patch>
        [[nodiscard]] bool duplicateSite(const std::span<const Patch> patches, const std::size_t index) noexcept
        {
            const auto site = [&]() noexcept {
                if constexpr (std::is_same_v<Patch, DirectCallPatch>) {
                    return patches[index].callsite;
                } else {
                    return patches[index].slot;
                }
            }();
            for (std::size_t prior = 0; prior < index; ++prior) {
                const auto priorSite = [&]() noexcept {
                    if constexpr (std::is_same_v<Patch, DirectCallPatch>) {
                        return patches[prior].callsite;
                    } else {
                        return patches[prior].slot;
                    }
                }();
                if (priorSite == site) {
                    return true;
                }
            }
            return false;
        }

        template <class Patch, class Inspect, class ValidateReplacement, class Replace>
        [[nodiscard]] PatchTransactionResult patchTransactional(
            const std::span<const Patch> patches,
            const std::span<std::uintptr_t> capturedTargets,
            Inspect&& inspect,
            ValidateReplacement&& validateReplacement,
            Replace&& replace) noexcept
        {
            if (patches.empty()) {
                return { TransactionStatus::EmptyGroup };
            }
            if (patches.size() > MaximumPatchGroupSize) {
                return { TransactionStatus::GroupTooLarge };
            }
            if (capturedTargets.size() != patches.size()) {
                return { TransactionStatus::CaptureSizeMismatch };
            }

            for (std::size_t index = 0; index < patches.size(); ++index) {
                if (duplicateSite(patches, index)) {
                    return {
                        TransactionStatus::DuplicateSite,
                        index,
                        0,
                        SiteStatus::InvalidArgument,
                    };
                }

                const auto& patch = patches[index];
                if (patch.expectedTarget == 0 || patch.replacementTarget == 0 ||
                    patch.expectedTarget == patch.replacementTarget) {
                    return {
                        TransactionStatus::PreflightFailed,
                        index,
                        0,
                        SiteStatus::InvalidArgument,
                    };
                }
                const auto inspection = inspect(patch, patch.expectedTarget);
                if (!inspection) {
                    return {
                        TransactionStatus::PreflightFailed,
                        index,
                        0,
                        inspection.status,
                    };
                }
                if (!isExecutable(patch.replacementTarget)) {
                    return {
                        TransactionStatus::PreflightFailed,
                        index,
                        0,
                        SiteStatus::TargetNotExecutable,
                    };
                }
                const auto replacementStatus = validateReplacement(patch);
                if (replacementStatus != SiteStatus::Ready) {
                    return {
                        TransactionStatus::PreflightFailed,
                        index,
                        0,
                        replacementStatus,
                    };
                }
                capturedTargets[index] = inspection.target;
            }

            for (std::size_t index = 0; index < patches.size(); ++index) {
                const auto& patch = patches[index];
                const auto commitStatus = replace(patch, capturedTargets[index], patch.replacementTarget);
                if (commitStatus == SiteStatus::Ready) {
                    continue;
                }

                SiteStatus rollbackStatus = SiteStatus::Ready;
                for (std::size_t rollback = index + 1; rollback != 0; --rollback) {
                    const auto rollbackIndex = rollback - 1;
                    const auto& rollbackPatch = patches[rollbackIndex];
                    const auto originalTarget = capturedTargets[rollbackIndex];
                    const auto originalInspection = inspect(rollbackPatch, originalTarget);
                    if (originalInspection) {
                        continue;
                    }

                    const auto replacementInspection = inspect(rollbackPatch, rollbackPatch.replacementTarget);
                    if (!replacementInspection) {
                        if (rollbackStatus == SiteStatus::Ready) {
                            rollbackStatus = replacementInspection.status;
                        }
                        continue;
                    }
                    const auto restored = replace(
                        rollbackPatch,
                        rollbackPatch.replacementTarget,
                        originalTarget);
                    if (restored != SiteStatus::Ready && rollbackStatus == SiteStatus::Ready) {
                        rollbackStatus = restored;
                    }
                }

                std::size_t changedCount = 0;
                for (std::size_t changed = 0; changed <= index; ++changed) {
                    if (!inspect(patches[changed], capturedTargets[changed])) {
                        ++changedCount;
                    }
                }
                if (changedCount != 0 && rollbackStatus == SiteStatus::Ready) {
                    rollbackStatus = SiteStatus::VerificationFailed;
                }
                return {
                    rollbackStatus == SiteStatus::Ready && changedCount == 0 ?
                        TransactionStatus::CommitFailedRestored :
                        TransactionStatus::RollbackFailed,
                    index,
                    changedCount,
                    commitStatus,
                    rollbackStatus,
                };
            }

            return {
                TransactionStatus::Applied,
                NoFailedIndex,
                patches.size(),
                SiteStatus::Ready,
                SiteStatus::Ready,
            };
        }
    }

    SiteInspection inspectDirectCall(
        const std::uintptr_t callsite,
        const std::uintptr_t expectedTarget) noexcept
    {
        if (callsite == 0) {
            return { SiteStatus::InvalidArgument, callsite };
        }
        if (!Memory::rangeHasAccess(
                reinterpret_cast<const void*>(callsite),
                DirectCallSize,
                Memory::Access::Execute)) {
            return { SiteStatus::NotExecutable, callsite };
        }

        std::array<std::byte, DirectCallSize> instruction{};
        if (!Memory::copyFrom(
                reinterpret_cast<const void*>(callsite),
                instruction.data(),
                instruction.size())) {
            return { SiteStatus::Unreadable, callsite };
        }
        if (instruction[0] != std::byte{ 0xE8 }) {
            return { SiteStatus::NotDirectCall, callsite };
        }
        if (callsite > (std::numeric_limits<std::uintptr_t>::max)() - DirectCallSize) {
            return { SiteStatus::TargetOverflow, callsite };
        }

        std::int32_t displacement{};
        std::memcpy(&displacement, instruction.data() + 1, sizeof(displacement));
        const auto nextInstruction = callsite + DirectCallSize;
        std::uintptr_t target{};
        if (displacement >= 0) {
            const auto distance = static_cast<std::uintptr_t>(displacement);
            if (nextInstruction > (std::numeric_limits<std::uintptr_t>::max)() - distance) {
                return { SiteStatus::TargetOverflow, callsite };
            }
            target = nextInstruction + distance;
        } else {
            const auto distance = static_cast<std::uintptr_t>(-(static_cast<std::int64_t>(displacement)));
            if (nextInstruction < distance) {
                return { SiteStatus::TargetOverflow, callsite };
            }
            target = nextInstruction - distance;
        }

        if (expectedTarget != 0 && target != expectedTarget) {
            return { SiteStatus::TargetMismatch, callsite, target };
        }
        if (!isExecutable(target)) {
            return { SiteStatus::TargetNotExecutable, callsite, target };
        }
        return { SiteStatus::Ready, callsite, target };
    }

    SiteStatus encodeDirectCall(
        const std::uintptr_t callsite,
        const std::uintptr_t target,
        std::array<std::byte, DirectCallSize>& encoded) noexcept
    {
        if (callsite == 0 || target == 0 ||
            callsite > (std::numeric_limits<std::uintptr_t>::max)() - DirectCallSize) {
            return SiteStatus::InvalidArgument;
        }

        const auto nextInstruction = callsite + DirectCallSize;
        std::int32_t displacement{};
        if (target >= nextInstruction) {
            const auto distance = target - nextInstruction;
            if (distance > static_cast<std::uintptr_t>((std::numeric_limits<std::int32_t>::max)())) {
                return SiteStatus::TargetOverflow;
            }
            displacement = static_cast<std::int32_t>(distance);
        } else {
            const auto distance = nextInstruction - target;
            constexpr auto minimumDistance = static_cast<std::uint64_t>(
                -(static_cast<std::int64_t>((std::numeric_limits<std::int32_t>::min)())));
            if (distance > minimumDistance) {
                return SiteStatus::TargetOverflow;
            }
            displacement = distance == minimumDistance ?
                               (std::numeric_limits<std::int32_t>::min)() :
                               -static_cast<std::int32_t>(distance);
        }

        encoded.fill(std::byte{});
        encoded[0] = std::byte{ 0xE8 };
        std::memcpy(encoded.data() + 1, &displacement, sizeof(displacement));
        return SiteStatus::Ready;
    }

    SiteStatus replaceDirectCallTarget(
        const std::uintptr_t callsite,
        const std::uintptr_t expectedTarget,
        const std::uintptr_t replacementTarget) noexcept
    {
        const auto inspection = inspectDirectCall(callsite, expectedTarget);
        if (!inspection) {
            return inspection.status;
        }
        if (!isExecutable(replacementTarget)) {
            return SiteStatus::TargetNotExecutable;
        }

        std::array<std::byte, DirectCallSize> encoded{};
        const auto encodeStatus = encodeDirectCall(callsite, replacementTarget, encoded);
        if (encodeStatus != SiteStatus::Ready) {
            return encodeStatus;
        }
        const auto writeStatus = writeProtectedBytes(
            reinterpret_cast<void*>(callsite),
            encoded.data(),
            encoded.size(),
            true);
        const auto verified = inspectDirectCall(callsite, replacementTarget);
        if (writeStatus == SiteStatus::Ready && !verified) {
            return SiteStatus::VerificationFailed;
        }
        if (writeStatus != SiteStatus::Ready && !verified &&
            !inspectDirectCall(callsite, expectedTarget)) {
            return SiteStatus::VerificationFailed;
        }
        return writeStatus;
    }

    PatchTransactionResult patchDirectCallsTransactional(
        const std::span<const DirectCallPatch> patches,
        const std::span<std::uintptr_t> capturedTargets) noexcept
    {
        const auto inspect = [](const DirectCallPatch& patch, const std::uintptr_t target) noexcept {
            return inspectDirectCall(patch.callsite, target);
        };
        const auto validateReplacement = [](const DirectCallPatch& patch) noexcept {
            std::array<std::byte, DirectCallSize> encoded{};
            return encodeDirectCall(patch.callsite, patch.replacementTarget, encoded);
        };
        const auto replace = [](const DirectCallPatch& patch, const std::uintptr_t expected, const std::uintptr_t target) noexcept {
            return replaceDirectCallTarget(patch.callsite, expected, target);
        };
        return patchTransactional(patches, capturedTargets, inspect, validateReplacement, replace);
    }

    SiteInspection inspectVtableSlot(
        const std::uintptr_t slot,
        const std::uintptr_t expectedTarget) noexcept
    {
        if (slot == 0) {
            return { SiteStatus::InvalidArgument, slot };
        }
        if (slot % alignof(std::uintptr_t) != 0) {
            return { SiteStatus::Misaligned, slot };
        }

        std::uintptr_t target{};
        if (!Memory::read(reinterpret_cast<const void*>(slot), target)) {
            return { SiteStatus::Unreadable, slot };
        }
        if (expectedTarget != 0 && target != expectedTarget) {
            return { SiteStatus::TargetMismatch, slot, target };
        }
        if (!isExecutable(target)) {
            return { SiteStatus::TargetNotExecutable, slot, target };
        }
        return { SiteStatus::Ready, slot, target };
    }

    SiteStatus replaceVtableSlotTarget(
        const std::uintptr_t slot,
        const std::uintptr_t expectedTarget,
        const std::uintptr_t replacementTarget) noexcept
    {
        const auto inspection = inspectVtableSlot(slot, expectedTarget);
        if (!inspection) {
            return inspection.status;
        }
        if (!isExecutable(replacementTarget)) {
            return SiteStatus::TargetNotExecutable;
        }

        const auto writeStatus = exchangeProtectedPointer(slot, replacementTarget);
        const auto verified = inspectVtableSlot(slot, replacementTarget);
        if (writeStatus == SiteStatus::Ready && !verified) {
            return SiteStatus::VerificationFailed;
        }
        if (writeStatus != SiteStatus::Ready && !verified &&
            !inspectVtableSlot(slot, expectedTarget)) {
            return SiteStatus::VerificationFailed;
        }
        return writeStatus;
    }

    PatchTransactionResult patchVtableSlotsTransactional(
        const std::span<const VtableSlotPatch> patches,
        const std::span<std::uintptr_t> capturedTargets) noexcept
    {
        const auto inspect = [](const VtableSlotPatch& patch, const std::uintptr_t target) noexcept {
            return inspectVtableSlot(patch.slot, target);
        };
        const auto validateReplacement = [](const VtableSlotPatch&) noexcept {
            return SiteStatus::Ready;
        };
        const auto replace = [](const VtableSlotPatch& patch, const std::uintptr_t expected, const std::uintptr_t target) noexcept {
            return replaceVtableSlotTarget(patch.slot, expected, target);
        };
        return patchTransactional(patches, capturedTargets, inspect, validateReplacement, replace);
    }

    SiteInspection inspectExecutableSite(
        const RuntimeModule& module,
        const Addresses::Symbol symbol,
        const BytePattern pattern) noexcept
    {
        if (!module) {
            return { SiteStatus::InvalidRuntime };
        }
        if (pattern.bytes.empty() || (!pattern.mask.empty() && pattern.mask.size() != pattern.bytes.size())) {
            return { SiteStatus::InvalidPattern };
        }

        const auto kind = Addresses::record(symbol).kind;
        if (kind != Addresses::SymbolKind::Function && kind != Addresses::SymbolKind::Callsite &&
            kind != Addresses::SymbolKind::HookSite && kind != Addresses::SymbolKind::PatchSite) {
            return { SiteStatus::WrongSymbolKind };
        }

        const auto address = module.resolve(symbol);
        if (address == 0) {
            return { SiteStatus::Unresolved };
        }
        if (!isExecutable(address)) {
            return { SiteStatus::NotExecutable, address };
        }
        if (!module.matches(symbol, pattern)) {
            return { SiteStatus::PatternMismatch, address };
        }
        return { SiteStatus::Ready, address, address };
    }
}
