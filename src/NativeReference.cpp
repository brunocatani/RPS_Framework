#include "RPS/Runtime/NativeReference.h"

#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/Memory.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <intrin.h>

#include <cstdint>
#include <limits>

namespace RPS::Runtime
{
    namespace
    {
        inline constexpr std::uint32_t UpperReferenceWordMask =
            static_cast<std::uint32_t>((std::numeric_limits<std::uint16_t>::max)()) << 16;

        [[nodiscard]] bool invokeHavokDestroy(void* const object) noexcept
        {
            void** vtable{};
            void* entry{};
            if (!Memory::read(object, vtable) || !vtable ||
                !Memory::read(vtable + Addresses::Layouts::Havok::ReferencedObject_DestroyVtableIndex, entry) ||
                !Memory::rangeHasAccess(entry, 1, Memory::Access::Execute)) {
                return false;
            }
            const auto destroy = reinterpret_cast<void (*)(void*)>(entry);
#if defined(_MSC_VER)
            __try {
                destroy(object);
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
#else
            destroy(object);
            return true;
#endif
        }

        [[nodiscard]] bool invokeBethesdaDestroy(void* const object) noexcept
        {
            void** vtable{};
            void* entry{};
            if (!Memory::read(object, vtable) || !vtable ||
                !Memory::read(vtable + Addresses::Layouts::Bethesda::ReferencedObject_DestroyVtableIndex, entry) ||
                !Memory::rangeHasAccess(entry, 1, Memory::Access::Execute)) {
                return false;
            }
            const auto destroy = reinterpret_cast<void (*)(void*, int)>(entry);
#if defined(_MSC_VER)
            __try {
                destroy(object, 1);
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
#else
            destroy(object, 1);
            return true;
#endif
        }

        template <class Destroy>
        [[nodiscard]] bool releaseReferenceWord(
            void* const object,
            const std::ptrdiff_t referenceOffset,
            const bool requireHavokFlags,
            Destroy destroy) noexcept
        {
            if (!object) {
                return false;
            }
            auto* const referenceWord = reinterpret_cast<volatile long*>(
                reinterpret_cast<std::uintptr_t>(object) + static_cast<std::uintptr_t>(referenceOffset));
            if (!Memory::rangeHasAccess(
                    const_cast<long*>(referenceWord), sizeof(long), Memory::Access::Read) ||
                !Memory::rangeHasAccess(const_cast<long*>(referenceWord), sizeof(long), Memory::Access::Write)) {
                return false;
            }

#if defined(_MSC_VER)
            __try {
#endif
                for (;;) {
                    const long previous = *referenceWord;
                    const auto unsignedPrevious = static_cast<std::uint32_t>(previous);
                    const auto flags = static_cast<std::uint16_t>(unsignedPrevious >> 16);
                    const auto references = static_cast<std::uint16_t>(unsignedPrevious);
                    if ((requireHavokFlags && flags == 0) || references == 0 ||
                        references == (std::numeric_limits<std::uint16_t>::max)()) {
                        return true;
                    }

                    const auto nextReferences = static_cast<std::uint16_t>(references - 1);
                    const auto next = static_cast<long>((unsignedPrevious & UpperReferenceWordMask) | nextReferences);
                    if (_InterlockedCompareExchange(referenceWord, next, previous) != previous) {
                        continue;
                    }
                    return nextReferences != 0 || destroy(object);
                }
#if defined(_MSC_VER)
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
#endif
        }

        [[nodiscard]] bool addBethesdaReferenceWord(const void* const object) noexcept
        {
            if (!object) {
                return false;
            }
            auto* const referenceWord = reinterpret_cast<volatile long*>(
                reinterpret_cast<std::uintptr_t>(object) +
                static_cast<std::uintptr_t>(Addresses::Layouts::Bethesda::ReferencedObject_ReferenceWord));
            if (!Memory::rangeHasAccess(
                    const_cast<long*>(referenceWord), sizeof(long), Memory::Access::Read) ||
                !Memory::rangeHasAccess(const_cast<long*>(referenceWord), sizeof(long), Memory::Access::Write)) {
                return false;
            }

#if defined(_MSC_VER)
            __try {
#endif
                for (;;) {
                    const long previous = *referenceWord;
                    const auto unsignedPrevious = static_cast<std::uint32_t>(previous);
                    const auto references = static_cast<std::uint16_t>(unsignedPrevious);
                    if (references == (std::numeric_limits<std::uint16_t>::max)()) {
                        return true;
                    }
                    const auto next = static_cast<long>(unsignedPrevious + 1u);
                    if (_InterlockedCompareExchange(referenceWord, next, previous) == previous) {
                        return true;
                    }
                }
#if defined(_MSC_VER)
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
#endif
        }
    }

    bool addHavokReference(const void* const object) noexcept
    {
        if (!object) {
            return false;
        }
        auto* const referenceWord = reinterpret_cast<volatile long*>(
            reinterpret_cast<std::uintptr_t>(object) +
            static_cast<std::uintptr_t>(Addresses::Layouts::Havok::ReferencedObject_ReferenceWord));
        if (!Memory::rangeHasAccess(
                const_cast<long*>(referenceWord), sizeof(long), Memory::Access::Read) ||
            !Memory::rangeHasAccess(const_cast<long*>(referenceWord), sizeof(long), Memory::Access::Write)) {
            return false;
        }

#if defined(_MSC_VER)
        __try {
#endif
            for (;;) {
                const long previous = *referenceWord;
                const auto unsignedPrevious = static_cast<std::uint32_t>(previous);
                const auto flags = static_cast<std::uint16_t>(unsignedPrevious >> 16);
                const auto references = static_cast<std::uint16_t>(unsignedPrevious);
                if (flags == 0 || references == (std::numeric_limits<std::uint16_t>::max)()) {
                    return true;
                }
                const auto nextReferences = static_cast<std::uint16_t>(references + 1);
                const auto next = static_cast<long>((unsignedPrevious & UpperReferenceWordMask) | nextReferences);
                if (_InterlockedCompareExchange(referenceWord, next, previous) == previous) {
                    return true;
                }
            }
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
#endif
    }

    bool releaseHavokReference(void* const object) noexcept
    {
        return releaseReferenceWord(
            object,
            Addresses::Layouts::Havok::ReferencedObject_ReferenceWord,
            true,
            invokeHavokDestroy);
    }

    bool addBethesdaReference(const void* const object) noexcept
    {
        return addBethesdaReferenceWord(object);
    }

    bool releaseBethesdaReference(void* const object) noexcept
    {
        return releaseReferenceWord(
            object,
            Addresses::Layouts::Bethesda::ReferencedObject_ReferenceWord,
            false,
            invokeBethesdaDestroy);
    }
}
