#include <RPS/Addresses/Catalog.h>
#include <RPS/Runtime/AnimationMotor.h>
#include <RPS/Runtime/AnimationPose.h>
#include <RPS/Runtime/CollisionFilter.h>
#include <RPS/Runtime/Constraint.h>
#include <RPS/Runtime/GeneratedBody.h>
#include <RPS/Runtime/PhysicsTypes.h>
#include <RPS/Runtime/Shape.h>

int main()
{
    const auto& symbol = RPS::Addresses::record(RPS::Addresses::Symbol::Physics_SetBodyVelocity);
    const RPS::Runtime::Collision::FilterInfo filter{ 43 };
    const RPS::Runtime::Physics::BodyId bodyId{ 1 };
    const RPS::Runtime::Physics::ChildTransform childTransform{};
    const RPS::Runtime::Physics::PositionMotorTuning motor{};
    const RPS::Runtime::Animation::MotorControlSettings animationMotor{};
    const RPS::Runtime::Animation::HkQsTransform animationTransform{};
    const auto retirement = RPS::Runtime::Physics::advanceGeneratedBodyRetirement(8, 1);
    return symbol.rva != 0 && filter.layer() == RPS::Runtime::Collision::RockHandLayer && bodyId.valid() &&
                   childTransform.finite() && RPS::Runtime::Physics::validPositionMotorTuning(motor) &&
                   RPS::Runtime::Animation::sanitizeMotorControlSettings(animationMotor).forceKeyframedControls &&
                   animationTransform.finite() && retirement == 7 ?
        0 :
        1;
}
