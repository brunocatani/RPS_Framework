// HookChain on real executable memory: F4SE-style stubs and entry jumps that
// lead into a loaded DLL (kernel32) count as another plugin's detour; code of
// this executable (standing in for the game and for the calling DLL) never does.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "RPS/Runtime/HookChain.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>

namespace
{
    namespace Chain = RPS::Runtime::HookChain;

    int g_failures = 0;

    void check(const bool condition, const char* what)
    {
        if (!condition) {
            std::cerr << "FAILED: " << what << '\n';
            ++g_failures;
        }
    }

    class Code
    {
    public:
        Code() noexcept :
            _memory(static_cast<std::uint8_t*>(
                VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE)))
        {}
        ~Code()
        {
            if (_memory) {
                VirtualFree(_memory, 0, MEM_RELEASE);
            }
        }
        Code(const Code&) = delete;
        Code& operator=(const Code&) = delete;

        [[nodiscard]] std::uintptr_t at(const std::size_t offset) const noexcept
        {
            return reinterpret_cast<std::uintptr_t>(_memory + offset);
        }

        // jmp [rip+0] followed by the absolute destination: F4SE's 14-byte stub.
        void absoluteJump(const std::size_t offset, const std::uintptr_t destination) noexcept
        {
            const std::array<std::uint8_t, 6> jump{ 0xFF, 0x25, 0, 0, 0, 0 };
            std::memcpy(_memory + offset, jump.data(), jump.size());
            std::memcpy(_memory + offset + 6, &destination, sizeof(destination));
        }

        void relativeTransfer(const std::size_t offset, const std::uint8_t opcode, const std::uintptr_t destination) noexcept
        {
            const auto displacement = static_cast<std::int32_t>(
                static_cast<std::intptr_t>(destination) - static_cast<std::intptr_t>(at(offset) + 5));
            _memory[offset] = opcode;
            std::memcpy(_memory + offset + 1, &displacement, sizeof(displacement));
        }

        void bytes(const std::size_t offset, const std::uint8_t* data, const std::size_t size) noexcept
        {
            std::memcpy(_memory + offset, data, size);
        }

    private:
        std::uint8_t* _memory;
    };

    int nativeFunction(const int value)
    {
        return value + 1;
    }
}

int main()
{
    Code code;
    const auto foreign = reinterpret_cast<std::uintptr_t>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetTickCount"));
    const auto ours = reinterpret_cast<std::uintptr_t>(&nativeFunction);
    check(foreign != 0, "kernel32 export found");

    // Stubs: 0 -> kernel32, 32 -> this executable, 64 -> stub 0 (two hops).
    code.absoluteJump(0, foreign);
    code.absoluteJump(32, ours);
    code.relativeTransfer(64, 0xE9, code.at(0));

    // A call site calling the native target (an address in the same buffer, so
    // the rel32 always reaches), another plugin's stub, our own stub.
    const auto nativeTarget = code.at(200);
    code.relativeTransfer(128, 0xE8, nativeTarget);
    code.relativeTransfer(144, 0xE8, code.at(0));
    code.relativeTransfer(160, 0xE8, code.at(32));
    const auto native = Chain::resolveCall(code.at(128), nativeTarget);
    check(native.origin == Chain::Origin::Native && native.callee == nativeTarget, "native call target");
    const auto chained = Chain::resolveCall(code.at(144), ours);
    check(chained.foreign() && chained.callee == code.at(0), "call into another plugin's stub chains");
    check(chained.owner.find("KERNEL32") != std::string::npos || chained.owner.find("kernel32") != std::string::npos,
        "chained owner names the DLL");
    check(!Chain::resolveCall(code.at(160), foreign), "a stub into our own module is not foreign");
    check(!Chain::resolveCall(code.at(0), ours), "a non-call instruction is unrecognised");

    // Vtable slots.
    const std::uintptr_t slots[3]{ ours, code.at(64), code.at(32) };
    check(Chain::resolveSlot(reinterpret_cast<std::uintptr_t>(&slots[0]), ours).origin == Chain::Origin::Native,
        "native slot");
    const auto slot = Chain::resolveSlot(reinterpret_cast<std::uintptr_t>(&slots[1]), ours);
    check(slot.foreign() && slot.callee == code.at(64), "slot through two jumps chains");
    check(!Chain::resolveSlot(reinterpret_cast<std::uintptr_t>(&slots[2]), ours), "slot into our own module refuses");

    // Entries: the native prologue, another plugin's 5-byte and 14-byte jumps, a changed binary.
    const std::array<std::uint8_t, 6> prologue{ 0x48, 0x89, 0x5C, 0x24, 0x08, 0x57 };
    code.bytes(256, prologue.data(), prologue.size());
    code.relativeTransfer(288, 0xE9, code.at(0));
    code.absoluteJump(320, foreign);
    const std::array<std::uint8_t, 6> changed{ 0x40, 0x53, 0x48, 0x83, 0xEC, 0x20 };
    code.bytes(352, changed.data(), changed.size());
    check(Chain::resolveEntry(code.at(256), prologue).origin == Chain::Origin::Native, "native prologue");
    const auto shortJump = Chain::resolveEntry(code.at(288), prologue);
    check(shortJump.foreign() && shortJump.callee == code.at(0) && shortJump.entryJumpBytes == 5,
        "5-byte entry jump chains to its destination");
    const auto longJump = Chain::resolveEntry(code.at(320), prologue);
    check(longJump.foreign() && longJump.callee == foreign && longJump.entryJumpBytes == 14,
        "14-byte entry jump chains to its destination");
    check(!Chain::resolveEntry(code.at(352), prologue), "changed prologue without a jump refuses");

    // reaches: a chained entry patch still leads to its hook.
    check(Chain::reaches(code.at(64), foreign), "two jumps reach the hook");
    check(!Chain::reaches(code.at(256), foreign), "a prologue reaches nothing");

    if (g_failures == 0) {
        std::cout << "HookChain tests passed\n";
    }
    return g_failures == 0 ? 0 : 1;
}
