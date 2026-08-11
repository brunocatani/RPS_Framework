#include "RPS/Addresses/Catalog.h"
#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/Constraint.h"

#include <iostream>
#include <limits>

int main()
{
    using namespace RPS::Runtime;
    using namespace RPS::Runtime::Physics;

    static_assert(InvalidConstraintId.value == InvalidBodyId);
    static_assert(RPS::Addresses::Layouts::Constraint::CreationInfoSize == 0x18);
    static_assert(RPS::Addresses::Layouts::Constraint::PositionMotorSize == 0x30);

    PositionMotorTuning valid{};
    valid.minimumForce = -100.0f;
    valid.maximumForce = 100.0f;
    if (!validPositionMotorTuning(valid)) {
        std::cerr << "valid position motor tuning was rejected\n";
        return 1;
    }
    valid.maximumForce = (std::numeric_limits<float>::quiet_NaN)();
    if (validPositionMotorTuning(valid)) {
        std::cerr << "non-finite position motor tuning was accepted\n";
        return 1;
    }

    const auto module = RuntimeModule::detect();
    PositionMotor motor = PositionMotor::create(module, {});
    ConstraintApi api{ module, nullptr };
    ConstraintService service{ module, nullptr };
    WorldWriteGuard guard{ module, nullptr };
    BallAndSocketConstraintInfo ball{};
    ball.bodyA = BodyId{ 1 };
    ball.bodyB = BodyId{ 2 };
    const auto result = api.createBallAndSocket(guard, ball);
    ConstraintId id{ 1 };
    const auto ownedResult = service.createBallAndSocket(guard, ball);
    if (module || motor || service || result || result.error != ConstraintError::InvalidRuntime || ownedResult ||
        ownedResult.error != ConstraintError::InvalidRuntime || api.destroy(guard, id) || id.value != 1 ||
        service.servicePendingRetirements(guard) != 0 || service.retirementCount() != 0 ||
        service.shutdownAfterWorldLoss() != 0) {
        std::cerr << "invalid runtime constraint API did not fail closed\n";
        return 1;
    }

    const auto& create = RPS::Addresses::record(RPS::Addresses::Symbol::Constraint_Create);
    const auto& destroy = RPS::Addresses::record(RPS::Addresses::Symbol::Constraint_Destroy);
    if (create.rva != 0x15469B0 || destroy.rva != 0x1546B40) {
        std::cerr << "constraint world addresses changed\n";
        return 1;
    }
    return 0;
}
