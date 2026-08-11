#include <RPS/Addresses/Catalog.h>
#include <RPS/Runtime/CollisionFilter.h>

int main()
{
    const auto& symbol = RPS::Addresses::record(RPS::Addresses::Symbol::Physics_SetBodyVelocity);
    const RPS::Runtime::Collision::FilterInfo filter{ 43 };
    return symbol.rva != 0 && filter.layer() == RPS::Runtime::Collision::RockHandLayer ? 0 : 1;
}
