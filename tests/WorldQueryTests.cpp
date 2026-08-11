#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/Shape.h"
#include "RPS/Runtime/WorldQuery.h"

#include <bit>
#include <cmath>
#include <iostream>
#include <limits>

int main()
{
    using namespace RPS::Runtime;
    using namespace RPS::Runtime::Physics;

    static_assert(sizeof(ShapeCastQuery) == RPS::Addresses::Layouts::Query::ShapeCastQuerySize);
    static_assert(RPS::Addresses::Layouts::Query::PickDataSize == 0xE0);
    static_assert(RPS::Addresses::Layouts::Query::CollisionResultSize == 0x60);

    Vector4 normalized{};
    if (!normalizeQueryDirection(Vector4{ 3.0f, 4.0f, 0.0f, 9.0f }, normalized) ||
        std::fabs(normalized.x - 0.6f) > 0.00001f ||
        std::fabs(normalized.y - 0.8f) > 0.00001f || normalized.w != 0.0f ||
        normalizeQueryDirection({}, normalized)) {
        std::cerr << "query direction normalization failed\n";
        return 1;
    }

    int filter{};
    int shape{};
    const Vector4 start{ 1.0f, 2.0f, 3.0f, 0.0f };
    const Vector4 displacement{ 2.0f, -4.0f, 0.0f, 0.0f };
    const auto query = buildShapeCastQuery(&filter, &shape, 45, start, displacement);
    const auto signBits = std::bit_cast<std::uint32_t>(query.inverseDisplacementAndSign.w);
    if (query.filter != &filter || query.shape != &shape || query.collisionFilterInfo != 45 ||
        query.displacement.w != 1.0f || query.inverseDisplacementAndSign.x != 0.5f ||
        query.inverseDisplacementAndSign.y != -0.25f ||
        query.inverseDisplacementAndSign.z != (std::numeric_limits<float>::max)() ||
        signBits != (RPS::Addresses::Layouts::Query::PositiveSignMaskBase | 5u)) {
        std::cerr << "shape cast query construction failed\n";
        return 1;
    }

    QueryHit farther{};
    farther.fraction = 0.8f;
    QueryHit nearer{};
    nearer.fraction = 0.2f;
    QueryHit middle{};
    middle.fraction = 0.5f;
    QueryHit invalid{};
    invalid.fraction = (std::numeric_limits<float>::quiet_NaN)();
    std::array hits{ farther, invalid, nearer, middle };
    sortQueryHitsByFraction(hits);
    if (hits[0].fraction != 0.2f || hits[1].fraction != 0.5f || hits[2].fraction != 0.8f ||
        !std::isnan(hits[3].fraction)) {
        std::cerr << "query hit sorting failed\n";
        return 1;
    }

    if (RPS::Addresses::record(RPS::Addresses::Symbol::World_CastShape).rva != 0x15A6C00 ||
        RPS::Addresses::record(RPS::Addresses::Symbol::World_PickObject).rva != 0x1DF8D60 ||
        RPS::Addresses::record(RPS::Addresses::Symbol::Query_PickDataCtor).rva != 0x001F930 ||
        RPS::Addresses::record(RPS::Addresses::Symbol::Query_PickDataSetStartEnd).rva != 0x0027170 ||
        RPS::Addresses::record(RPS::Addresses::Symbol::Query_PickDataHasHit).rva != 0x1DFB6F0 ||
        RPS::Addresses::record(RPS::Addresses::Symbol::Query_PickDataGetHitFraction).rva != 0x1DFB710) {
        std::cerr << "world query address catalog changed\n";
        return 1;
    }

    const auto module = RuntimeModule::detect();
    WorldQueryApi api{ module, nullptr, nullptr };
    const auto ray = api.castClosestRayGame({});
    WorldReadGuard guard{ module, nullptr };
    std::array<QueryHit, 1> output{};
    const auto cast = api.castShapeGame(guard, {}, output);
    ShapeFactory factory{ module };
    if (module || ray.error != QueryError::InvalidRuntime || cast.error != QueryError::InvalidRuntime ||
        factory.buildSphereHavok(1.0f) || factory.buildSphereGame(1.0f)) {
        std::cerr << "invalid runtime query API did not fail closed\n";
        return 1;
    }

    return 0;
}
