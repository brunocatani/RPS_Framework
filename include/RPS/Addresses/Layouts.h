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
        inline constexpr std::size_t CollisionObjectSize = 0x30;
        inline constexpr std::ptrdiff_t CollisionObject_OwnerNode = 0x10;
        inline constexpr std::ptrdiff_t CollisionObject_PhysicsSystem = 0x20;
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
        inline constexpr std::ptrdiff_t GraphDriver_Interface = 0x70;
        inline constexpr std::ptrdiff_t Graph_Driver = 0x208;
        inline constexpr std::ptrdiff_t GraphManager_ActiveGraphIndex = 0xD8;
    }
}
