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
        inline constexpr std::int32_t MaximumReasonableIntrusiveReferenceCount = 1'000'000;
        using NativeIntrusiveDestructor = void (*)(void*, int);

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

        [[nodiscard]] bool invokeNativeIntrusiveDestructor(
            const NativeIntrusiveDestructor destructor,
            void* const object) noexcept
        {
            if (!destructor || !object) {
                return false;
            }
#if defined(_MSC_VER)
            __try {
                destructor(object, 1);
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
#else
            destructor(object, 1);
            return true;
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

    NativeIntrusiveReferenceSnapshot inspectNativeIntrusiveReference(void* const object) noexcept
    {
        NativeIntrusiveReferenceSnapshot result{};
        result.object = object;
        const auto address = reinterpret_cast<std::uintptr_t>(object);
        if (address == 0 || address > (std::numeric_limits<std::uintptr_t>::max)() - sizeof(void*)) {
            return result;
        }

        std::uintptr_t vtable{};
        NativeIntrusiveDestructor destructor{};
        if (!Memory::read(object, vtable) || vtable == 0 ||
            !Memory::read(reinterpret_cast<const void*>(address + sizeof(void*)), result.referenceCount) ||
            result.referenceCount < 0 || result.referenceCount > MaximumReasonableIntrusiveReferenceCount ||
            !Memory::read(reinterpret_cast<const void*>(vtable), destructor) || !destructor ||
            !Memory::rangeHasAccess(
                reinterpret_cast<const void*>(destructor),
                1,
                Memory::Access::Execute)) {
            result.referenceCount = 0;
            return result;
        }

        result.destructorAddress = reinterpret_cast<std::uintptr_t>(destructor);
        result.valid = true;
        return result;
    }

    NativeIntrusiveReferenceOperation addNativeIntrusiveReference(void* const object) noexcept
    {
        NativeIntrusiveReferenceOperation result{};
        result.before = inspectNativeIntrusiveReference(object);
        if (!result.before.valid || result.before.referenceCount >= MaximumReasonableIntrusiveReferenceCount) {
            return result;
        }

        auto* const referenceCount = reinterpret_cast<volatile long*>(
            reinterpret_cast<std::uintptr_t>(object) + sizeof(void*));
#if defined(_MSC_VER)
        __try {
            result.referenceCountAfter = static_cast<std::int32_t>(_InterlockedIncrement(referenceCount));
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return result;
        }
#else
        result.referenceCountAfter = ++(*referenceCount);
#endif
        result.succeeded = result.referenceCountAfter > 0 &&
                           result.referenceCountAfter <= MaximumReasonableIntrusiveReferenceCount;
        if (!result.succeeded) {
#if defined(_MSC_VER)
            __try {
                result.referenceCountAfter = static_cast<std::int32_t>(_InterlockedDecrement(referenceCount));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return result;
            }
#else
            result.referenceCountAfter = --(*referenceCount);
#endif
        }
        return result;
    }

    NativeIntrusiveReferenceOperation releaseNativeIntrusiveReference(void* const object) noexcept
    {
        NativeIntrusiveReferenceOperation result{};
        result.before = inspectNativeIntrusiveReference(object);
        if (!result.before.valid || result.before.referenceCount <= 0) {
            return result;
        }

        auto* const referenceCount = reinterpret_cast<volatile long*>(
            reinterpret_cast<std::uintptr_t>(object) + sizeof(void*));
#if defined(_MSC_VER)
        __try {
            result.referenceCountAfter = static_cast<std::int32_t>(_InterlockedDecrement(referenceCount));
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return result;
        }
#else
        result.referenceCountAfter = --(*referenceCount);
#endif
        if (result.referenceCountAfter < 0) {
            return result;
        }
        if (result.referenceCountAfter == 0) {
            result.destroyed = invokeNativeIntrusiveDestructor(
                reinterpret_cast<NativeIntrusiveDestructor>(result.before.destructorAddress),
                object);
            result.succeeded = result.destroyed;
            return result;
        }

        result.succeeded = true;
        return result;
    }

    NativeIntrusivePtr::~NativeIntrusivePtr() noexcept
    {
        (void)releaseNow();
    }

    NativeIntrusivePtr::NativeIntrusivePtr(NativeIntrusivePtr&& other) noexcept :
        _object(other.detach())
    {}

    NativeIntrusivePtr& NativeIntrusivePtr::operator=(NativeIntrusivePtr&& other) noexcept
    {
        if (this != &other) {
            (void)releaseNow();
            _object = other.detach();
        }
        return *this;
    }

    NativeIntrusivePtr NativeIntrusivePtr::adoptRetained(void* const object) noexcept
    {
        const auto snapshot = inspectNativeIntrusiveReference(object);
        return snapshot.valid && snapshot.referenceCount > 0 ? NativeIntrusivePtr{ object } : NativeIntrusivePtr{};
    }

    NativeIntrusivePtr NativeIntrusivePtr::retain(
        void* const object,
        NativeIntrusiveReferenceOperation& operation) noexcept
    {
        operation = addNativeIntrusiveReference(object);
        return operation.succeeded ? NativeIntrusivePtr{ object } : NativeIntrusivePtr{};
    }

    NativeIntrusiveReferenceSnapshot NativeIntrusivePtr::inspect() const noexcept
    {
        return inspectNativeIntrusiveReference(_object);
    }

    NativeIntrusiveReferenceOperation NativeIntrusivePtr::releaseNow() noexcept
    {
        if (!_object) {
            NativeIntrusiveReferenceOperation result{};
            result.succeeded = true;
            return result;
        }
        auto* const object = _object;
        _object = nullptr;
        return releaseNativeIntrusiveReference(object);
    }

    void* NativeIntrusivePtr::detach() noexcept
    {
        auto* const result = _object;
        _object = nullptr;
        return result;
    }
}
