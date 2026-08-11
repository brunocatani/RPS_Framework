#include <RPS/Addresses/Catalog.h>
#include <RPS/Runtime/CollisionFilter.h>
#include <RPS/Runtime/PhysicsTypes.h>
#include <RPS/Runtime/Shape.h>

int main()
{
    const auto& symbol = RPS::Addresses::record(RPS::Addresses::Symbol::Physics_SetBodyVelocity);
    const RPS::Runtime::Collision::FilterInfo filter{ 43 };
    const RPS::Runtime::Physics::BodyId bodyId{ 1 };
    const RPS::Runtime::Physics::ChildTransform childTransform{};
    return symbol.rva != 0 && filter.layer() == RPS::Runtime::Collision::RockHandLayer && bodyId.valid() && childTransform.finite() ? 0 : 1;
}
