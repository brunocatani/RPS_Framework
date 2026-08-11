#pragma once

#include <cstddef>
#include <cstdint>

namespace RPS::Addresses::Layouts
{
    namespace Havok
    {
        inline constexpr std::ptrdiff_t ReferencedObject_ReferenceWord = 0x08;
        inline constexpr std::size_t ReferencedObject_DestroyVtableIndex = 3;
        inline constexpr std::ptrdiff_t HkThreadMemory_Allocator = 0x58;
        inline constexpr std::ptrdiff_t Array_Data = 0x00;
        inline constexpr std::ptrdiff_t Array_Size = 0x08;
        inline constexpr std::ptrdiff_t Array_CapacityAndFlags = 0x0C;
        inline constexpr std::uint32_t Array_CapacityMask = 0x3FFFFFFF;
        inline constexpr std::ptrdiff_t BhkWorld_HknpWorld = 0x60;
        inline constexpr std::ptrdiff_t HknpWorld_BodyArray = 0x20;
        inline constexpr std::ptrdiff_t HknpWorld_BodyHighWaterMark = 0x70;
        inline constexpr std::ptrdiff_t HknpWorld_MotionArray = 0xE0;
        inline constexpr std::ptrdiff_t HknpWorld_ConstraintManager = 0x120;
        inline constexpr std::ptrdiff_t HknpWorld_ModifierManager = 0x150;
        inline constexpr std::ptrdiff_t HknpWorld_MotionPropertiesLibrary = 0x5D0;
        inline constexpr std::ptrdiff_t HknpWorld_AccessLock = 0x6D8;
        inline constexpr std::size_t HknpBody_Stride = 0x90;
        inline constexpr std::size_t HknpMotion_Stride = 0x80;
        inline constexpr std::uint32_t MaxReadableBodyIndex = 0x000FFFFF;
        inline constexpr std::uint32_t MaxUsableMotionIndex = 4096;
        inline constexpr std::ptrdiff_t HknpBody_Flags = 0x40;
        inline constexpr std::ptrdiff_t HknpBody_CollisionFilterInfo = 0x44;
        inline constexpr std::ptrdiff_t HknpBody_Shape = 0x48;
        inline constexpr std::ptrdiff_t HknpBody_Id = 0x60;
        inline constexpr std::ptrdiff_t HknpBody_NextAttachedBodyId = 0x64;
        inline constexpr std::ptrdiff_t HknpBody_MotionIndex = 0x68;
        inline constexpr std::ptrdiff_t HknpBody_DeactivationIslandId = 0x6C;
        inline constexpr std::ptrdiff_t HknpBody_MaterialId = 0x70;
        inline constexpr std::ptrdiff_t HknpBody_MotionPropertiesId = 0x72;
        inline constexpr std::ptrdiff_t HknpBody_QualityId = 0x7E;
        inline constexpr std::ptrdiff_t HknpBody_CollisionObject = 0x88;
        inline constexpr std::ptrdiff_t HknpMotion_Position = 0x00;
        inline constexpr std::ptrdiff_t HknpMotion_Orientation = 0x10;
        inline constexpr std::ptrdiff_t HknpMotion_PackedInverseInertia = 0x20;
        inline constexpr std::ptrdiff_t HknpMotion_FirstBodyId = 0x28;
        inline constexpr std::ptrdiff_t HknpMotion_PropertiesId = 0x38;
        inline constexpr std::ptrdiff_t HknpMotion_MaxLinearVelocityPacked = 0x3A;
        inline constexpr std::ptrdiff_t HknpMotion_MaxAngularVelocityPacked = 0x3C;
        inline constexpr std::ptrdiff_t HknpMotion_DeactivationState = 0x3F;
        inline constexpr std::ptrdiff_t HknpMotion_LinearVelocity = 0x40;
        inline constexpr std::ptrdiff_t HknpMotion_AngularVelocity = 0x50;
        inline constexpr std::ptrdiff_t HknpMotion_PreviousLinearVelocity = 0x60;
        inline constexpr std::ptrdiff_t HknpMotion_PreviousAngularVelocity = 0x70;
        inline constexpr std::ptrdiff_t MotionPropertiesLibrary_Data = 0x28;
        inline constexpr std::size_t MotionProperties_Stride = 0x40;
        inline constexpr std::ptrdiff_t MotionProperties_GravityFactor = 0x08;
        inline constexpr std::ptrdiff_t MotionProperties_LinearDamping = 0x18;
        inline constexpr std::ptrdiff_t MotionProperties_AngularDamping = 0x1C;
        inline constexpr std::ptrdiff_t ExeTls_InPhysicsStepFlag = 0x1529;
    }

    namespace Shape
    {
        inline constexpr std::size_t ConvexBuildConfigSize = 0x80;
        inline constexpr std::size_t MaximumConvexPointCount = 0xFC;
        inline constexpr std::size_t InstanceSize = 0x80;
        inline constexpr std::ptrdiff_t Instance_Flags = 0x0C;
        inline constexpr std::ptrdiff_t Instance_Shape = 0x50;
        inline constexpr std::ptrdiff_t Instance_Index = 0x58;
        inline constexpr std::uint32_t InstanceDefaultFlags = 0x3F000040;
        inline constexpr std::size_t CompoundCinfoSize = 0x28;
        inline constexpr std::size_t CompoundStorageSize = 0xD0;
        inline constexpr std::size_t MaximumCompoundChildren = 0x7FFE;
        inline constexpr std::size_t ChildTransformSize = 0x60;
    }

    namespace Bethesda
    {
        inline constexpr std::ptrdiff_t ReferencedObject_ReferenceWord = 0x08;
        inline constexpr std::uint32_t ReferencedObject_ReferenceCountMask = 0x0000FFFF;
        inline constexpr std::size_t ReferencedObject_DestroyVtableIndex = 0;
        inline constexpr std::uint32_t AllocatorReadyState = 2;
        inline constexpr std::uint32_t CollisionObjectAllocatorContext = 0x41;
        inline constexpr std::ptrdiff_t ExeTls_AllocatorContext = 0x9C0;

        inline constexpr std::size_t PhysicsSystemDataSize = 0x78;
        inline constexpr std::ptrdiff_t PhysicsSystemData_Materials = 0x10;
        inline constexpr std::ptrdiff_t PhysicsSystemData_MotionCinfos = 0x30;
        inline constexpr std::ptrdiff_t PhysicsSystemData_BodyCinfos = 0x40;
        inline constexpr std::ptrdiff_t PhysicsSystemData_Shapes = 0x60;
        inline constexpr std::size_t MotionCinfoSize = 0x70;
        inline constexpr std::size_t BodyCinfoSize = 0x60;
        inline constexpr std::size_t MaterialSize = 0x50;
        inline constexpr std::size_t ShapeReferenceSize = 0x08;

        inline constexpr std::ptrdiff_t BodyCinfo_Shape = 0x00;
        inline constexpr std::ptrdiff_t BodyCinfo_ReservedBodyId = 0x08;
        inline constexpr std::ptrdiff_t BodyCinfo_LocalMotionIndex = 0x0C;
        inline constexpr std::ptrdiff_t BodyCinfo_QualityId = 0x10;
        inline constexpr std::ptrdiff_t BodyCinfo_LocalMaterialIndex = 0x12;
        inline constexpr std::ptrdiff_t BodyCinfo_CollisionFilterInfo = 0x14;
        inline constexpr std::ptrdiff_t BodyCinfo_Name = 0x20;
        inline constexpr std::ptrdiff_t BodyCinfo_UserData = 0x28;

        inline constexpr std::size_t PhysicsSystemSize = 0x28;
        inline constexpr std::ptrdiff_t PhysicsSystem_Instance = 0x18;
        inline constexpr std::ptrdiff_t PhysicsSystemInstance_World = 0x18;
        inline constexpr std::ptrdiff_t PhysicsSystemInstance_BodyIds = 0x20;
        inline constexpr std::ptrdiff_t PhysicsSystemInstance_BodyCount = 0x28;
        inline constexpr std::size_t PhysicsSystemInstance_MinimumReadableSize = 0x30;
        inline constexpr std::size_t CollisionObjectSize = 0x30;
        inline constexpr std::ptrdiff_t CollisionObject_OwnerNode = 0x10;
        inline constexpr std::ptrdiff_t CollisionObject_PhysicsSystem = 0x20;
        inline constexpr std::ptrdiff_t CollisionObject_BodyIndex = 0x28;
        inline constexpr std::int32_t MaximumPhysicsSystemBodyCount = 4096;
        inline constexpr std::size_t NiNodeSize = 0x180;
        inline constexpr std::size_t NiNodeAlignment = 0x10;
        inline constexpr std::ptrdiff_t NiAvObject_CollisionObject = 0x100;

        inline constexpr std::uint16_t GeneratedLocalMaterialIndex = 0;
        inline constexpr std::uint32_t GeneratedBodyRuntimeFlags = 0x08020000;
        inline constexpr std::uint32_t RebuildBodyCollisionState = 0;
    }

    namespace Collision
    {
        inline constexpr std::ptrdiff_t HknpWorld_ModifierManager = 0x150;
        inline constexpr std::ptrdiff_t ModifierManager_Filter = 0x5E8;
        inline constexpr std::ptrdiff_t CollisionFilter_Matrix = 0x1A0;
        inline constexpr std::uint32_t MatrixLayerCount = 64;
    }

    namespace Constraint
    {
        inline constexpr std::size_t CreationInfoSize = 0x18;
        inline constexpr std::size_t BallAndSocketDataSize = 0x70;
        inline constexpr std::size_t LimitedHingeDataSize = 0x130;
        inline constexpr std::ptrdiff_t LimitedHinge_LimitEnabled = 0xFA;
        inline constexpr std::ptrdiff_t LimitedHinge_MinimumAngle = 0xFC;
        inline constexpr std::ptrdiff_t LimitedHinge_MaximumAngle = 0x100;
        inline constexpr std::size_t PrismaticDataSize = 0x120;
        inline constexpr std::ptrdiff_t Prismatic_LimitEnabled = 0x10A;
        inline constexpr std::ptrdiff_t Prismatic_MinimumDistance = 0x10C;
        inline constexpr std::ptrdiff_t Prismatic_MaximumDistance = 0x110;
        inline constexpr std::size_t PositionMotorSize = 0x30;
        inline constexpr std::uint32_t PositionMotorInitialReferenceWord = 0xFFFF0001;
        inline constexpr std::uint8_t PositionMotorType = 1;
        inline constexpr std::ptrdiff_t Manager_Entries = 0x08;
        inline constexpr std::ptrdiff_t Manager_CapacityFlags = 0x10;
        inline constexpr std::ptrdiff_t Manager_ActiveCount = 0x20;
        inline constexpr std::ptrdiff_t Manager_HighWaterMark = 0x28;
        inline constexpr std::size_t EntryStride = 0x38;
        inline constexpr std::ptrdiff_t Entry_BodyA = 0x00;
        inline constexpr std::ptrdiff_t Entry_BodyB = 0x04;
        inline constexpr std::ptrdiff_t Entry_Data = 0x08;
        inline constexpr std::ptrdiff_t Entry_Id = 0x10;
        inline constexpr std::ptrdiff_t Entry_Flags = 0x16;
        inline constexpr std::ptrdiff_t Entry_Type = 0x17;
    }

    namespace Animation
    {
        inline constexpr std::size_t GeneratorMasterHeader = 0x10;
        inline constexpr std::size_t GeneratorTrackHeader = 0x10;
        inline constexpr std::size_t HkQsTransform = 0x30;
        inline constexpr std::size_t KeyframedControlData = 0x30;
        inline constexpr std::size_t PoweredControlData = 0x14;
        inline constexpr std::size_t WorldFromModelModeData = 0x08;
        inline constexpr std::ptrdiff_t WorldFromModelMode_Value = 0x06;
        inline constexpr std::ptrdiff_t GraphDriver_Interface = 0x70;
        inline constexpr std::ptrdiff_t Graph_Driver = 0x208;
        inline constexpr std::ptrdiff_t GraphManager_ActiveGraphIndex = 0xD8;
    }

    namespace Ragdoll
    {
        inline constexpr std::ptrdiff_t Actor_GetGraphManagerVtable = 0x20;
        inline constexpr std::size_t GraphManagerSize = 0xE0;
        inline constexpr std::ptrdiff_t GraphManager_ReferenceCount = 0x08;
        inline constexpr std::ptrdiff_t GraphManager_GraphArrayFlags = 0x40;
        inline constexpr std::ptrdiff_t GraphManager_GraphArrayStorage = 0x48;
        inline constexpr std::ptrdiff_t GraphManager_GraphArrayCount = 0x50;
        inline constexpr std::ptrdiff_t GraphManager_UpdateLock = 0xC8;
        inline constexpr std::uint32_t GraphArrayInlineFlag = 0x80000000;
        inline constexpr std::uint32_t GraphArrayCapacityMask = 0x7FFFFFFF;
        inline constexpr std::size_t MaximumGraphCount = 32;
        inline constexpr std::uint32_t GraphLockAttemptLimit = 4096;

        inline constexpr std::ptrdiff_t Graph_RagdollDriver = 0x208;
        inline constexpr std::ptrdiff_t Driver_RagdollInterface = 0x70;
        inline constexpr std::ptrdiff_t Interface_GetBodyHandleVtableSlot = 0x38;
        inline constexpr std::ptrdiff_t Interface_GetLowSkeletonVtableSlot = 0x50;
        inline constexpr std::ptrdiff_t Interface_WorldReference = 0x10;
        inline constexpr std::ptrdiff_t Interface_Ragdoll = 0x18;
        inline constexpr std::ptrdiff_t PhysicsInterface_GetBodyOffsetVtableSlot = 0x60;
        inline constexpr std::ptrdiff_t PhysicsInterface_GetBodyTransformVtableSlot = 0x70;
        inline constexpr std::ptrdiff_t LowSkeleton_ParentIndices = 0x18;
        inline constexpr std::ptrdiff_t LowSkeleton_BoneCount = 0x30;
        inline constexpr std::size_t LowSkeleton_MinimumReadableSize = 0x34;
        inline constexpr std::ptrdiff_t BodyHandle_BodyId = 0x10;
        inline constexpr std::size_t BodyHandle_MinimumReadableSize = 0x14;
        inline constexpr std::ptrdiff_t WorldReference_World = 0x18;
        inline constexpr std::ptrdiff_t Ragdoll_BodyIds = 0x20;
        inline constexpr std::ptrdiff_t Ragdoll_BodyIdCount = 0x28;
        inline constexpr std::ptrdiff_t Ragdoll_BodyIdCapacityAndFlags = 0x2C;
        inline constexpr std::ptrdiff_t Ragdoll_ConstraintIds = 0x30;
        inline constexpr std::ptrdiff_t Ragdoll_ConstraintIdCount = 0x38;
        inline constexpr std::ptrdiff_t Ragdoll_BodyMap = 0x48;
        inline constexpr std::ptrdiff_t Ragdoll_BodyMapCount = 0x50;
        inline constexpr std::ptrdiff_t Ragdoll_BodyMapCapacityAndFlags = 0x54;
        inline constexpr std::uint32_t ArrayCapacityMask = 0x3FFFFFFF;
        inline constexpr std::size_t MaximumBodyCount = 255;
        inline constexpr std::size_t MaximumLowPoseBoneCount = 256;
        inline constexpr std::size_t MaximumHighPoseBoneCount = 1024;
        inline constexpr std::uint32_t InvalidBodyId = 0x7FFFFFFF;

        inline constexpr std::size_t SetPhysicsWorldStateSize = 0x10;
        inline constexpr std::ptrdiff_t SetPhysicsWorldState_Applied = 0x08;
    }

    namespace Query
    {
        inline constexpr std::size_t PickDataSize = 0xE0;
        inline constexpr std::ptrdiff_t PickData_CollisionFilterInfo = 0x0A;
        inline constexpr std::ptrdiff_t PickData_Result = 0x58;

        inline constexpr std::size_t CollisionResultSize = 0x60;
        inline constexpr std::ptrdiff_t CollisionResult_Position = 0x00;
        inline constexpr std::ptrdiff_t CollisionResult_Normal = 0x10;
        inline constexpr std::ptrdiff_t CollisionResult_Fraction = 0x20;
        inline constexpr std::ptrdiff_t CollisionResult_HitBodyInfo = 0x40;
        inline constexpr std::ptrdiff_t BodyInfo_BodyId = 0x00;
        inline constexpr std::ptrdiff_t BodyInfo_MaterialId = 0x04;
        inline constexpr std::ptrdiff_t BodyInfo_ShapeKey = 0x08;
        inline constexpr std::ptrdiff_t BodyInfo_CollisionFilterInfo = 0x0C;
        inline constexpr std::ptrdiff_t BodyInfo_UserData = 0x10;

        inline constexpr std::size_t CollectorBaseSize = 0x20;
        inline constexpr std::ptrdiff_t Collector_Hints = 0x08;
        inline constexpr std::ptrdiff_t Collector_EarlyOutThreshold = 0x10;

        inline constexpr std::size_t ShapeCastQuerySize = 0x80;
        inline constexpr std::ptrdiff_t ShapeCast_Filter = 0x00;
        inline constexpr std::ptrdiff_t ShapeCast_CollisionFilterInfo = 0x0C;
        inline constexpr std::ptrdiff_t ShapeCast_Shape = 0x20;
        inline constexpr std::ptrdiff_t ShapeCast_Start = 0x30;
        inline constexpr std::ptrdiff_t ShapeCast_Displacement = 0x40;
        inline constexpr std::ptrdiff_t ShapeCast_InverseDisplacementAndSign = 0x50;
        inline constexpr std::ptrdiff_t ShapeCast_Tolerance = 0x60;
        inline constexpr std::uint16_t AnyMaterialId = 0xFFFF;
        inline constexpr std::uint32_t PositiveSignMaskBase = 0x3F000000;
        inline constexpr float ShapeCastTolerance = 0.001f;
        inline constexpr std::size_t MaximumCollectedHits = 16;
    }

    namespace Impact
    {
        inline constexpr std::size_t PhysicsImpactContactSize = 0x40;
        inline constexpr std::size_t DamageImpactDataSize = 0x40;
        inline constexpr std::ptrdiff_t DamageImpactData_Location = 0x00;
        inline constexpr std::ptrdiff_t DamageImpactData_Normal = 0x10;
        inline constexpr std::ptrdiff_t DamageImpactData_Velocity = 0x20;
        inline constexpr std::ptrdiff_t DamageImpactData_CollisionObject = 0x30;
        inline constexpr std::size_t HitDataSize = 0xE0;
        inline constexpr std::size_t HitDataAlignment = 0x10;
        inline constexpr std::ptrdiff_t HitData_AggressorHandle = 0x40;
    }

    namespace Movement
    {
        inline constexpr std::ptrdiff_t Actor_ControllerSmartPointer = 0x318;
        inline constexpr std::ptrdiff_t Controller_MotionDrivenInterface = 0x128;
        inline constexpr std::ptrdiff_t Controller_PlannerDirectInterface = 0x140;
        inline constexpr std::ptrdiff_t Controller_Mode = 0x198;
        inline constexpr std::ptrdiff_t Controller_PathingFlags = 0x1A0;
        inline constexpr std::size_t Controller_PathingFlagCount = 8;
        inline constexpr std::size_t Controller_PathingMasterFlagIndex = 5;
        inline constexpr std::size_t Controller_MinimumReadableSize = 0x1A8;
    }

    namespace CharacterController
    {
        inline constexpr std::ptrdiff_t BhkWorld_RigidBodyManager = 0xD8;
        inline constexpr std::size_t BhkWorld_MinimumReadableSize = 0x148;
        inline constexpr std::size_t RigidBodyManager_MinimumReadableSize = 0x70;
        inline constexpr std::ptrdiff_t RigidBodyManager_ControllerList = 0x10;
        inline constexpr std::ptrdiff_t RigidBodyManager_ControllerCount = 0x20;
        inline constexpr std::ptrdiff_t Controller_RigidBody = 0x470;
        inline constexpr std::size_t Controller_MinimumReadableSize = 0x478;
        inline constexpr std::ptrdiff_t RigidBody_StepGate = 0xA8;
        inline constexpr std::size_t RigidBody_MinimumReadableSize = 0xA9;
        inline constexpr std::uint32_t MaximumManagerControllerScan = 8192;
    }

    namespace Pathing
    {
        inline constexpr std::size_t Actor_MinimumReadableSize = sizeof(std::uintptr_t);
        inline constexpr std::uint32_t PackageLoopAllowedMask = 0xFF;
        inline constexpr std::size_t DirectMovementVectorCount = 3;
    }

    namespace ActorState
    {
        inline constexpr std::ptrdiff_t Actor_StateSubobject = 0x128;
        inline constexpr std::ptrdiff_t Actor_LifeFlags = 0x130;
        inline constexpr std::ptrdiff_t Actor_KnockFlags = 0x134;
        inline constexpr std::ptrdiff_t Actor_AIProcess = 0x300;
        inline constexpr std::ptrdiff_t Actor_RagdollMovementFlags = 0x43C;
        inline constexpr std::size_t Actor_MinimumReadableSize = 0x440;

        inline constexpr std::ptrdiff_t ActorState_SetKnockStateVtableSlot = 0x120;
        inline constexpr std::ptrdiff_t ActorState_GetKnockStateVtableSlot = 0x128;
        inline constexpr std::uint32_t KnockCodeShift = 19;
        inline constexpr std::uint32_t KnockCodeMask = 0x3;
        inline constexpr std::uint32_t MaximumKnockState = 8;
        inline constexpr std::uint32_t NormalKnockState = 0;
        inline constexpr std::uint32_t LifeStateShift = 18;
        inline constexpr std::uint32_t LifeStateMask = 0xF;
        inline constexpr std::uint32_t RagdollMovementFlagMask = 0x100;

        inline constexpr std::ptrdiff_t AIProcess_KnockData = 0x8;
        inline constexpr std::ptrdiff_t AIProcess_HighData = 0x10;
        inline constexpr std::size_t AIProcess_MinimumReadableSize = 0x18;
        inline constexpr std::ptrdiff_t KnockData_CurrentHandle = 0x3B0;
        inline constexpr std::ptrdiff_t KnockData_RagdollFlag = 0x4BF;
        inline constexpr std::size_t KnockData_MinimumReadableSize = 0x4C0;
        inline constexpr std::ptrdiff_t HighProcess_FullRagdollFlagA = 0x58E;
        inline constexpr std::ptrdiff_t HighProcess_FullRagdollFlagB = 0x58F;
        inline constexpr std::size_t HighProcess_MinimumReadableSize = 0x590;
    }

    namespace Scene
    {
        inline constexpr std::size_t NiAVObject_SetMaterialNeedsUpdateVtableIndex = 0x2E;
        inline constexpr std::size_t NiAVObject_SetAppCulledVtableIndex = 0x30;
        inline constexpr std::size_t NiAVObject_UpdateWorldBoundVtableIndex = 0x36;
        inline constexpr std::size_t NiNode_AttachChildVtableIndex = 0x3D;
        inline constexpr std::size_t NiNode_DetachChildVtableIndex = 0x40;
        inline constexpr std::ptrdiff_t NiAVObject_Parent = 0x28;
        inline constexpr std::ptrdiff_t NiAVObject_LocalTransform = 0x30;
        inline constexpr std::ptrdiff_t NiAVObject_WorldTransform = 0x70;
        inline constexpr std::ptrdiff_t NiAVObject_PreviousWorldTransform = 0xC0;
        inline constexpr std::size_t NiAVObject_UpdateWorldDataVtableIndex = 0x37;
        inline constexpr std::size_t NiUpdateDataSize = 0x20;
        inline constexpr std::size_t NiAVObject_MinimumReadableSize = 0x30;
        inline constexpr std::size_t NiNode_MinimumReadableSize = 0x180;
    }

    namespace PointLight
    {
        inline constexpr std::size_t ObjectSize = 0x1D0;
        inline constexpr std::ptrdiff_t Ambient = 0x160;
        inline constexpr std::ptrdiff_t Diffuse = 0x16C;
        inline constexpr std::ptrdiff_t Specular = 0x178;
        inline constexpr std::ptrdiff_t Dimmer = 0x184;
        inline constexpr std::ptrdiff_t ModelBound = 0x190;
        inline constexpr std::ptrdiff_t RendererData = 0x1A0;
        inline constexpr std::ptrdiff_t ConstantAttenuation = 0x1B0;
        inline constexpr std::ptrdiff_t LinearAttenuation = 0x1B4;
        inline constexpr std::ptrdiff_t QuadraticAttenuation = 0x1B8;
        inline constexpr std::size_t ColorSize = 0x0C;
        inline constexpr std::size_t RendererProxyMinimumReadableSize = 0x10;
        inline constexpr std::size_t ManagerMinimumReadableSize = sizeof(std::uintptr_t);
    }

    namespace Audio
    {
        inline constexpr std::size_t SoundHandleSize = 0x08;
        inline constexpr std::ptrdiff_t SoundHandle_Id = 0x00;
        inline constexpr std::ptrdiff_t SoundHandle_AssumeSuccess = 0x04;
        inline constexpr std::ptrdiff_t SoundHandle_State = 0x05;
        inline constexpr std::uint32_t InvalidSoundId = 0xFFFFFFFF;
        inline constexpr std::size_t AudioManager_MinimumReadableSize = sizeof(std::uintptr_t);
        inline constexpr std::size_t Descriptor_MinimumReadableSize = sizeof(std::uintptr_t);
        inline constexpr std::size_t SceneObject_MinimumReadableSize = sizeof(std::uintptr_t);
    }
}
