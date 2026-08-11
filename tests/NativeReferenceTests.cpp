#include "RPS/Runtime/NativeReference.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <utility>

namespace
{
    struct FakeIntrusiveObject
    {
        std::uintptr_t* vtable{};
        std::int32_t referenceCount{};
        std::uint32_t padding{};
    };
    static_assert(offsetof(FakeIntrusiveObject, referenceCount) == sizeof(void*));

    std::uint32_t destroyCalls{};
    int lastDestroyFlag{};

    void destroyFake(void*, const int flag)
    {
        ++destroyCalls;
        lastDestroyFlag = flag;
    }
}

int main()
{
    using namespace RPS::Runtime;

    std::uintptr_t vtable[]{ reinterpret_cast<std::uintptr_t>(&destroyFake) };
    FakeIntrusiveObject object{ .vtable = vtable, .referenceCount = 1 };

    const auto initial = inspectNativeIntrusiveReference(&object);
    const auto add = addNativeIntrusiveReference(&object);
    const auto release = releaseNativeIntrusiveReference(&object);
    if (!initial.valid || initial.object != &object || initial.referenceCount != 1 || initial.destructorAddress == 0 ||
        !add.succeeded || add.before.referenceCount != 1 || add.referenceCountAfter != 2 ||
        !release.succeeded || release.before.referenceCount != 2 || release.referenceCountAfter != 1 ||
        release.destroyed || destroyCalls != 0) {
        std::cerr << "native intrusive ref operations failed\n";
        return 1;
    }

    NativeIntrusiveReferenceOperation retainOperation{};
    {
        auto retained = NativeIntrusivePtr::retain(&object, retainOperation);
        auto moved = std::move(retained);
        if (!retainOperation.succeeded || object.referenceCount != 2 || retained || !moved ||
            moved.inspect().referenceCount != 2) {
            std::cerr << "native intrusive retain/move failed\n";
            return 1;
        }
    }
    if (object.referenceCount != 1 || destroyCalls != 0) {
        std::cerr << "native intrusive RAII release failed\n";
        return 1;
    }

    auto adopted = NativeIntrusivePtr::adoptRetained(&object);
    const auto finalRelease = adopted.releaseNow();
    if (!finalRelease.succeeded || finalRelease.referenceCountAfter != 0 || !finalRelease.destroyed || adopted ||
        destroyCalls != 1 || lastDestroyFlag != 1) {
        std::cerr << "native intrusive final destruction failed\n";
        return 1;
    }

    FakeIntrusiveObject invalid{ .vtable = vtable, .referenceCount = -1 };
    NativeIntrusiveReferenceOperation invalidRetain{};
    auto invalidPointer = NativeIntrusivePtr::retain(&invalid, invalidRetain);
    NativeIntrusivePtr empty{};
    if (inspectNativeIntrusiveReference(&invalid).valid || invalidRetain.succeeded || invalidPointer ||
        !empty.releaseNow().succeeded || NativeIntrusivePtr::adoptRetained(nullptr)) {
        std::cerr << "native intrusive invalid state did not fail closed\n";
        return 1;
    }

    return 0;
}
