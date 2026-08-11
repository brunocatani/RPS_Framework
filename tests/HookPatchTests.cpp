#include "RPS/Addresses/Catalog.h"
#include "RPS/Runtime/HookPatch.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>

namespace
{
    class Allocation
    {
    public:
        Allocation(const std::size_t bytes, const DWORD protection) noexcept :
            _memory(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, protection)),
            _bytes(bytes)
        {}

        ~Allocation()
        {
            if (_memory) {
                VirtualFree(_memory, 0, MEM_RELEASE);
            }
        }

        Allocation(const Allocation&) = delete;
        Allocation& operator=(const Allocation&) = delete;

        [[nodiscard]] explicit operator bool() const noexcept { return _memory != nullptr; }
        [[nodiscard]] std::byte* data() const noexcept { return static_cast<std::byte*>(_memory); }
        [[nodiscard]] std::size_t size() const noexcept { return _bytes; }

        [[nodiscard]] bool protect(const DWORD protection) const noexcept
        {
            DWORD previous{};
            return VirtualProtect(_memory, _bytes, protection, &previous) != FALSE;
        }

        [[nodiscard]] DWORD protection() const noexcept
        {
            MEMORY_BASIC_INFORMATION information{};
            return VirtualQuery(_memory, &information, sizeof(information)) == sizeof(information) ?
                       information.Protect :
                       0;
        }

    private:
        void* _memory{};
        std::size_t _bytes{};
    };

    [[nodiscard]] bool initializeCall(
        std::byte* const callsite,
        const std::uintptr_t target) noexcept
    {
        using namespace RPS::Runtime::Hooks;
        std::array<std::byte, DirectCallSize> instruction{};
        if (encodeDirectCall(reinterpret_cast<std::uintptr_t>(callsite), target, instruction) != SiteStatus::Ready) {
            return false;
        }
        std::memcpy(callsite, instruction.data(), instruction.size());
        return true;
    }
}

int main()
{
    using namespace RPS::Runtime;
    using namespace RPS::Runtime::Hooks;

    Allocation code{ 4096, PAGE_EXECUTE_READWRITE };
    Allocation slots{ 4096, PAGE_READWRITE };
    if (!code || !slots) {
        std::cerr << "virtual allocation failed\n";
        return 1;
    }

    auto* const callsiteA = code.data() + 16;
    auto* const callsiteB = code.data() + 32;
    auto* const invalidCallsite = code.data() + 48;
    const auto original = reinterpret_cast<std::uintptr_t>(code.data() + 512);
    const auto replacement = reinterpret_cast<std::uintptr_t>(code.data() + 768);
    if (!initializeCall(callsiteA, original) || !initializeCall(callsiteB, original)) {
        std::cerr << "direct call initialization failed\n";
        return 1;
    }
    invalidCallsite[0] = std::byte{ 0x90 };

    auto* const vtable = reinterpret_cast<std::uintptr_t*>(slots.data());
    vtable[0] = original;
    vtable[1] = original;
    if (!code.protect(PAGE_EXECUTE_READ) || !slots.protect(PAGE_READONLY)) {
        std::cerr << "read-only protection setup failed\n";
        return 1;
    }

    const auto callsiteAddressA = reinterpret_cast<std::uintptr_t>(callsiteA);
    const auto callsiteAddressB = reinterpret_cast<std::uintptr_t>(callsiteB);
    if (!inspectDirectCall(callsiteAddressA, original) ||
        inspectDirectCall(callsiteAddressA, replacement).status != SiteStatus::TargetMismatch ||
        inspectDirectCall(reinterpret_cast<std::uintptr_t>(invalidCallsite)).status != SiteStatus::NotDirectCall) {
        std::cerr << "direct call inspection contract failed\n";
        return 1;
    }

    std::array<std::byte, DirectCallSize> unreachable{};
    if (encodeDirectCall(
            callsiteAddressA,
            (std::numeric_limits<std::uintptr_t>::max)(),
            unreachable) != SiteStatus::TargetOverflow) {
        std::cerr << "out-of-range direct call was accepted\n";
        return 1;
    }

    const std::array directPatches{
        DirectCallPatch{ callsiteAddressA, original, replacement },
        DirectCallPatch{ callsiteAddressB, original, replacement },
    };
    std::array<std::uintptr_t, directPatches.size()> directCaptured{};
    const auto directResult = patchDirectCallsTransactional(directPatches, directCaptured);
    if (!directResult || directResult.changedCount != directPatches.size() ||
        directCaptured[0] != original || directCaptured[1] != original ||
        !inspectDirectCall(callsiteAddressA, replacement) ||
        !inspectDirectCall(callsiteAddressB, replacement) ||
        (code.protection() & 0xFFu) != PAGE_EXECUTE_READ) {
        std::cerr << "transactional direct call patch failed\n";
        return 1;
    }

    const std::array restoreDirect{
        DirectCallPatch{ callsiteAddressA, replacement, directCaptured[0] },
        DirectCallPatch{ callsiteAddressB, replacement, directCaptured[1] },
    };
    std::array<std::uintptr_t, restoreDirect.size()> restoreCaptured{};
    if (!patchDirectCallsTransactional(restoreDirect, restoreCaptured) ||
        !inspectDirectCall(callsiteAddressA, original) ||
        !inspectDirectCall(callsiteAddressB, original)) {
        std::cerr << "transactional direct call restore failed\n";
        return 1;
    }

    const std::array mismatch{
        DirectCallPatch{ callsiteAddressA, replacement, original },
    };
    std::array<std::uintptr_t, mismatch.size()> mismatchCaptured{};
    const auto mismatchResult = patchDirectCallsTransactional(mismatch, mismatchCaptured);
    if (mismatchResult.status != TransactionStatus::PreflightFailed ||
        mismatchResult.siteStatus != SiteStatus::TargetMismatch ||
        !inspectDirectCall(callsiteAddressA, original)) {
        std::cerr << "failed preflight mutated a callsite\n";
        return 1;
    }

    const std::array duplicate{
        DirectCallPatch{ callsiteAddressA, original, replacement },
        DirectCallPatch{ callsiteAddressA, original, replacement },
    };
    std::array<std::uintptr_t, duplicate.size()> duplicateCaptured{};
    if (patchDirectCallsTransactional(duplicate, duplicateCaptured).status != TransactionStatus::DuplicateSite) {
        std::cerr << "duplicate callsite group was accepted\n";
        return 1;
    }

    const auto slotA = reinterpret_cast<std::uintptr_t>(&vtable[0]);
    const auto slotB = reinterpret_cast<std::uintptr_t>(&vtable[1]);
    if (!inspectVtableSlot(slotA, original) ||
        inspectVtableSlot(slotA + 1, original).status != SiteStatus::Misaligned) {
        std::cerr << "vtable inspection contract failed\n";
        return 1;
    }

    const std::array vtablePatches{
        VtableSlotPatch{ slotA, original, replacement },
        VtableSlotPatch{ slotB, original, replacement },
    };
    std::array<std::uintptr_t, vtablePatches.size()> vtableCaptured{};
    if (!patchVtableSlotsTransactional(vtablePatches, vtableCaptured) ||
        !inspectVtableSlot(slotA, replacement) ||
        !inspectVtableSlot(slotB, replacement) ||
        (slots.protection() & 0xFFu) != PAGE_READONLY) {
        std::cerr << "transactional vtable patch failed\n";
        return 1;
    }

    const std::array restoreVtable{
        VtableSlotPatch{ slotA, replacement, original },
        VtableSlotPatch{ slotB, replacement, original },
    };
    std::array<std::uintptr_t, restoreVtable.size()> restoreVtableCaptured{};
    if (!patchVtableSlotsTransactional(restoreVtable, restoreVtableCaptured) ||
        !inspectVtableSlot(slotA, original) ||
        !inspectVtableSlot(slotB, original)) {
        std::cerr << "transactional vtable restore failed\n";
        return 1;
    }

    const std::array<std::byte, 1> patternBytes{ std::byte{ 0x90 } };
    const auto invalidModule = RuntimeModule::detect();
    if (inspectExecutableSite(
            invalidModule,
            RPS::Addresses::Symbol::Physics_SetBodyVelocity,
            BytePattern{ patternBytes }).status != SiteStatus::InvalidRuntime) {
        std::cerr << "invalid runtime executable-site gate did not fail closed\n";
        return 1;
    }

    return 0;
}
