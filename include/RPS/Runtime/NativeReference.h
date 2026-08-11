#pragma once

#include <cstdint>

namespace RPS::Runtime
{
    [[nodiscard]] bool addHavokReference(const void* object) noexcept;
    [[nodiscard]] bool addBethesdaReference(const void* object) noexcept;
    [[nodiscard]] bool releaseHavokReference(void* object) noexcept;
    [[nodiscard]] bool releaseBethesdaReference(void* object) noexcept;

    struct NativeIntrusiveReferenceSnapshot
    {
        bool valid{};
        void* object{};
        std::int32_t referenceCount{};
        std::uintptr_t destructorAddress{};
    };

    struct NativeIntrusiveReferenceOperation
    {
        NativeIntrusiveReferenceSnapshot before{};
        std::int32_t referenceCountAfter{};
        bool destroyed{};
        bool succeeded{};
    };

    /** Full-width BSIntrusiveRefCounted contract used by native path requests. */
    [[nodiscard]] NativeIntrusiveReferenceSnapshot inspectNativeIntrusiveReference(void* object) noexcept;
    [[nodiscard]] NativeIntrusiveReferenceOperation addNativeIntrusiveReference(void* object) noexcept;
    [[nodiscard]] NativeIntrusiveReferenceOperation releaseNativeIntrusiveReference(void* object) noexcept;

    /**
     * Move-only ownership for a single full-width native intrusive reference.
     * Destroy or release it on the engine-owning thread required by the
     * underlying object's destructor. detach() transfers that responsibility.
     */
    class NativeIntrusivePtr
    {
    public:
        NativeIntrusivePtr() noexcept = default;
        ~NativeIntrusivePtr() noexcept;

        NativeIntrusivePtr(const NativeIntrusivePtr&) = delete;
        NativeIntrusivePtr& operator=(const NativeIntrusivePtr&) = delete;
        NativeIntrusivePtr(NativeIntrusivePtr&& other) noexcept;
        NativeIntrusivePtr& operator=(NativeIntrusivePtr&& other) noexcept;

        [[nodiscard]] static NativeIntrusivePtr adoptRetained(void* object) noexcept;
        [[nodiscard]] static NativeIntrusivePtr retain(
            void* object,
            NativeIntrusiveReferenceOperation& operation) noexcept;

        [[nodiscard]] void* get() const noexcept { return _object; }
        [[nodiscard]] explicit operator bool() const noexcept { return _object != nullptr; }
        [[nodiscard]] NativeIntrusiveReferenceSnapshot inspect() const noexcept;
        [[nodiscard]] NativeIntrusiveReferenceOperation releaseNow() noexcept;
        [[nodiscard]] void* detach() noexcept;

    private:
        explicit NativeIntrusivePtr(void* object) noexcept : _object(object) {}

        void* _object{};
    };
}
