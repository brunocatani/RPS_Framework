#include <RPS/Addresses/Catalog.h>
#include <RPS/Runtime/ActorState.h>
#include <RPS/Runtime/AnimationMotor.h>
#include <RPS/Runtime/AnimationPose.h>
#include <RPS/Runtime/BodyGravity.h>
#include <RPS/Runtime/CharacterController.h>
#include <RPS/Runtime/CollisionFilter.h>
#include <RPS/Runtime/Constraint.h>
#include <RPS/Runtime/GeneratedBody.h>
#include <RPS/Runtime/HookPatch.h>
#include <RPS/Runtime/Impact.h>
#include <RPS/Runtime/MovementController.h>
#include <RPS/Runtime/NativeReference.h>
#include <RPS/Runtime/PhysicsTypes.h>
#include <RPS/Runtime/Ragdoll.h>
#include <RPS/Runtime/ScenePhysics.h>
#include <RPS/Runtime/Shape.h>
#include <RPS/Runtime/WorldQuery.h>

int main()
{
    const auto& symbol = RPS::Addresses::record(RPS::Addresses::Symbol::Physics_SetBodyVelocity);
    const RPS::Runtime::Collision::FilterInfo filter{ 43 };
    const RPS::Runtime::Physics::BodyId bodyId{ 1 };
    const RPS::Runtime::Physics::ChildTransform childTransform{};
    const RPS::Runtime::Physics::PositionMotorTuning motor{};
    const RPS::Runtime::Animation::MotorControlSettings animationMotor{};
    const RPS::Runtime::Animation::HkQsTransform animationTransform{};
    const RPS::Runtime::Hooks::DirectCallPatch hookPatch{};
    const RPS::Runtime::Physics::RayRequest rayRequest{};
    const RPS::Runtime::Physics::RagdollPointers ragdollPointers{};
    const RPS::Runtime::Physics::PhysicsImpactContact impactContact{};
    const RPS::Runtime::Physics::RecursiveMotionRequest recursiveMotion{};
    const RPS::Runtime::Physics::BodyGravitySnapshot gravity{};
    const RPS::Runtime::Character::MovementControllerSnapshot movementController{};
    const RPS::Runtime::NativeIntrusivePtr intrusiveReference{};
    const RPS::Runtime::Character::CharacterControllerSnapshot characterController{};
    const RPS::Runtime::Character::ActorStateSnapshot actorState{};
    const auto retirement = RPS::Runtime::Physics::advanceGeneratedBodyRetirement(8, 1);
    return symbol.rva != 0 && filter.layer() == RPS::Runtime::Collision::RockHandLayer && bodyId.valid() &&
                   childTransform.finite() && RPS::Runtime::Physics::validPositionMotorTuning(motor) &&
                   RPS::Runtime::Animation::sanitizeMotorControlSettings(animationMotor).forceKeyframedControls &&
                   animationTransform.finite() && hookPatch.callsite == 0 && rayRequest.maxDistanceGame == 0.0f &&
                   !ragdollPointers.complete() && !impactContact.sourceBodyId.valid() &&
                   recursiveMotion.preset == RPS::Runtime::Physics::MotionPreset::Dynamic && !gravity.valid &&
                   !movementController.complete() && !intrusiveReference && !characterController.rigidBodyComplete() &&
                   !actorState.actorReadable && retirement == 7 ?
        0 :
        1;
}
