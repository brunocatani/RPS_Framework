#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/HavokAllocator.h"
#include "RPS/Runtime/NativeReference.h"
#include "RPS/Runtime/Shape.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>

namespace
{
    struct FakeReferencedObject
    {
        void** vtable{};
        volatile long referenceWord{};
        std::uint32_t padding{};
        bool destroyed{};
    };
    static_assert(offsetof(FakeReferencedObject, referenceWord) == RPS::Addresses::Layouts::Havok::ReferencedObject_ReferenceWord);

    void destroyHavok(void* object)
    {
        static_cast<FakeReferencedObject*>(object)->destroyed = true;
    }

    void destroyBethesda(void* object, int releaseStorage)
    {
        static_cast<FakeReferencedObject*>(object)->destroyed = releaseStorage == 1;
    }
}

int main()
{
    using namespace RPS::Runtime;
    using namespace Physics;

    static_assert(sizeof(ChildTransform) == RPS::Addresses::Layouts::Shape::ChildTransformSize);

    ChildTransform transform{};
    if (!transform.finite()) {
        std::cerr << "default child transform is invalid\n";
        return 1;
    }
    transform.scale.x = (std::numeric_limits<float>::quiet_NaN)();
    if (transform.finite()) {
        std::cerr << "non-finite child transform was accepted\n";
        return 1;
    }

    ShapeHandle first{};
    ShapeHandle second{ std::move(first) };
    second = ShapeHandle{};
    if (first || second || ShapeHandle::adopt(nullptr) || addHavokReference(nullptr) ||
        releaseHavokReference(nullptr) || releaseBethesdaReference(nullptr)) {
        std::cerr << "empty ownership contract failed\n";
        return 1;
    }

    std::array<void*, 4> havokVtable{};
    havokVtable[3] = reinterpret_cast<void*>(destroyHavok);
    FakeReferencedObject havokObject{ havokVtable.data(), static_cast<long>((1u << 16) | 1u) };
    if (!addHavokReference(&havokObject) || (havokObject.referenceWord & 0xFFFF) != 2 ||
        !releaseHavokReference(&havokObject) || havokObject.destroyed ||
        !releaseHavokReference(&havokObject) || !havokObject.destroyed) {
        std::cerr << "Havok reference ownership failed\n";
        return 1;
    }

    std::array<void*, 1> bethesdaVtable{ reinterpret_cast<void*>(destroyBethesda) };
    FakeReferencedObject bethesdaObject{ bethesdaVtable.data(), 1 };
    if (!releaseBethesdaReference(&bethesdaObject) || !bethesdaObject.destroyed) {
        std::cerr << "Bethesda reference ownership failed\n";
        return 1;
    }

    const auto module = RuntimeModule::detect();
    ShapeFactory factory{ module };
    const std::array points{
        Vector4{ 0.0f, 0.0f, 0.0f, 0.0f },
        Vector4{ 1.0f, 0.0f, 0.0f, 0.0f },
        Vector4{ 0.0f, 1.0f, 0.0f, 0.0f },
        Vector4{ 0.0f, 0.0f, 1.0f, 0.0f },
    };
    if (module || factory.buildSphereHavok(1.0f) || factory.buildSphereGame(70.0f) ||
        factory.buildConvex(points, 0.01f) || factory.buildStaticCompound({})) {
        std::cerr << "invalid runtime shape factory did not fail closed\n";
        return 1;
    }

    HavokAllocator allocator{ module };
    if (allocator.allocate(16) || allocator.deallocate(nullptr, 16) || allocator.reserveArray(nullptr, 16)) {
        std::cerr << "invalid runtime allocator did not fail closed\n";
        return 1;
    }

    DynamicCompoundShape dynamic{ module };
    if (dynamic.create({}) || dynamic || dynamic.childCount() != 0) {
        std::cerr << "empty dynamic compound was accepted\n";
        return 1;
    }

    return 0;
}
