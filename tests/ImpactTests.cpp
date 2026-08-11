#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/Impact.h"
#include "RPS/Runtime/NativeReference.h"

#include <array>
#include <cmath>
#include <cstddef>
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
    static_assert(
        offsetof(FakeReferencedObject, referenceWord) ==
        RPS::Addresses::Layouts::Bethesda::ReferencedObject_ReferenceWord);

    void destroyBethesda(void* object, int releaseStorage)
    {
        static_cast<FakeReferencedObject*>(object)->destroyed = releaseStorage == 1;
    }
}

int main()
{
    using namespace RPS::Runtime;
    using namespace RPS::Runtime::Physics;

    const PhysicsImpactContact contact{
        BodyId{ 7 },
        {},
        Vector4{ 1.0f, -2.0f, 3.0f, 0.0f },
        Vector4{ 0.0f, 3.0f, 4.0f, 0.0f },
        Vector4{ 0.5f, 1.0f, -1.5f, 0.0f },
    };
    const auto geometry = preparePhysicsImpactGeometry(contact, 10.0f);
    if (!geometry || geometry.geometry.pointGame.x != 10.0f || geometry.geometry.pointGame.y != -20.0f ||
        geometry.geometry.pointGame.z != 30.0f || std::abs(geometry.geometry.normal.y - 0.6f) > 0.00001f ||
        std::abs(geometry.geometry.normal.z - 0.8f) > 0.00001f ||
        geometry.geometry.sourceVelocityGame.z != -15.0f) {
        std::cerr << "impact geometry preparation failed\n";
        return 1;
    }

    auto invalidContact = contact;
    invalidContact.reserved[0] = 1;
    if (preparePhysicsImpactGeometry(invalidContact, 10.0f).error != ImpactError::InvalidInput) {
        std::cerr << "nonzero reserved impact fields were accepted\n";
        return 1;
    }
    invalidContact = contact;
    invalidContact.normalHavok = {};
    if (preparePhysicsImpactGeometry(invalidContact, 10.0f).error != ImpactError::InvalidNormal ||
        preparePhysicsImpactGeometry(contact, 0.0f).error != ImpactError::InvalidScale) {
        std::cerr << "invalid impact geometry was accepted\n";
        return 1;
    }
    invalidContact = contact;
    invalidContact.pointHavok.x = (std::numeric_limits<float>::quiet_NaN)();
    if (preparePhysicsImpactGeometry(invalidContact, 10.0f).error != ImpactError::NonFiniteContact) {
        std::cerr << "non-finite impact geometry was accepted\n";
        return 1;
    }

    std::array<void*, 1> bethesdaVtable{ reinterpret_cast<void*>(destroyBethesda) };
    FakeReferencedObject bethesdaObject{ bethesdaVtable.data(), 1 };
    if (!addBethesdaReference(&bethesdaObject) || bethesdaObject.referenceWord != 2 ||
        !releaseBethesdaReference(&bethesdaObject) || bethesdaObject.referenceWord != 1 || bethesdaObject.destroyed ||
        !releaseBethesdaReference(&bethesdaObject) || !bethesdaObject.destroyed) {
        std::cerr << "Bethesda retained-reference ownership failed\n";
        return 1;
    }

    const auto module = RuntimeModule::detect();
    if (module || calculateImpactDamage(module, 10.0f, 400.0f).error != ImpactError::InvalidRuntime ||
        calculateImpactDamage(module, -1.0f, 400.0f).error != ImpactError::InvalidInput) {
        std::cerr << "invalid runtime impact damage did not fail closed\n";
        return 1;
    }

    const PhysicsHitRequest request{
        reinterpret_cast<void*>(1),
        reinterpret_cast<void*>(1),
        nullptr,
        contact,
        1.0f,
        10.0f,
    };
    if (deliverPhysicsHit(module, request).error != ImpactError::InvalidRuntime) {
        std::cerr << "invalid runtime physics hit did not fail closed\n";
        return 1;
    }

    return 0;
}
