#pragma once

#include <cstddef>
#include <cstdint>

namespace RPS::Addresses::Layouts
{
    namespace Havok
    {
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
