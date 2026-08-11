#include "RPS/Runtime/RootPose.h"

#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/Memory.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace RPS::Runtime::Animation
{
    namespace
    {
        namespace Layout = Addresses::Layouts::Ragdoll;

        inline constexpr float MinimumQuaternionLengthSquared = 1.0e-6f;
        inline constexpr float MinimumVectorLength = 1.0e-6f;
        inline constexpr float Pi = 3.14159265358979323846f;

        using GetLowSkeletonFunction = void* (*)(void*);
        using GetBodyHandleFunction = const void* (*)(void*, int);
        using ResolvePhysicsInterfaceFunction = void* (*)(const void*);
        using GetBodyOffsetFunction = void (*)(void*, const void*, float*);
        using GetBodyTransformFunction = void (*)(void*, const void*, HkQsTransform*);
        using MapHighToLowFunction = void (*)(const void*, const HkQsTransform*, std::uint32_t, HkQsTransform*);
        using CopyAndApplyScaleFunction = void (*)(std::uint8_t, int, const HkQsTransform*, HkQsTransform*, float);
        using CopyAndScaleTransformFunction = void (*)(std::uint8_t, const HkQsTransform*, HkQsTransform*);
        using LocalToWorldFunction = void (*)(int, const std::int16_t*, const HkQsTransform*, const HkQsTransform*, HkQsTransform*);

        struct SkeletonView
        {
            RootPoseStatus status{ RootPoseStatus::InvalidDriver };
            void* ragdollInterface{};
            void* skeleton{};
            const std::int16_t* parentIndices{};
            std::size_t boneCount{};
        };

        [[nodiscard]] bool executable(const void* function) noexcept
        {
            return Memory::rangeHasAccess(function, 1, Memory::Access::Execute);
        }

        template <class Function>
        [[nodiscard]] Function virtualFunction(void* object, const std::ptrdiff_t slot) noexcept
        {
            std::uintptr_t vtable{};
            std::uintptr_t function{};
            if (!Memory::read(object, vtable) || vtable == 0 ||
                !Memory::read(reinterpret_cast<const void*>(vtable + slot), function) || function == 0 ||
                !executable(reinterpret_cast<const void*>(function))) {
                return nullptr;
            }
            return reinterpret_cast<Function>(function);
        }

        template <class Function>
        [[nodiscard]] Function nativeFunction(const RuntimeModule& module, const Addresses::Symbol symbol) noexcept
        {
            const auto function = module.resolveFunction<Function>(symbol);
            return executable(reinterpret_cast<const void*>(function)) ? function : nullptr;
        }

        [[nodiscard]] bool allFinite(const std::span<const HkQsTransform> transforms) noexcept
        {
            return std::all_of(transforms.begin(), transforms.end(), [](const HkQsTransform& transform) {
                return transform.finite();
            });
        }

        [[nodiscard]] bool transformsOverlap(
            const std::span<const HkQsTransform> input,
            const std::span<HkQsTransform> output) noexcept
        {
            if (input.empty() || output.empty()) {
                return false;
            }
            constexpr auto maximumAddress = (std::numeric_limits<std::uintptr_t>::max)();
            if (input.size() > maximumAddress / sizeof(HkQsTransform) ||
                output.size() > maximumAddress / sizeof(HkQsTransform)) {
                return true;
            }
            const auto inputBegin = reinterpret_cast<std::uintptr_t>(input.data());
            const auto outputBegin = reinterpret_cast<std::uintptr_t>(output.data());
            if (inputBegin > maximumAddress - input.size_bytes() ||
                outputBegin > maximumAddress - output.size_bytes()) {
                return true;
            }
            const auto inputEnd = inputBegin + input.size_bytes();
            const auto outputEnd = outputBegin + output.size_bytes();
            return inputBegin < outputEnd && outputBegin < inputEnd;
        }

        [[nodiscard]] bool transformInside(
            const HkQsTransform& input,
            const std::span<HkQsTransform> output) noexcept
        {
            if (output.empty()) {
                return false;
            }
            constexpr auto maximumAddress = (std::numeric_limits<std::uintptr_t>::max)();
            if (output.size() > maximumAddress / sizeof(HkQsTransform)) {
                return true;
            }
            const auto inputAddress = reinterpret_cast<std::uintptr_t>(&input);
            const auto outputBegin = reinterpret_cast<std::uintptr_t>(output.data());
            if (outputBegin > maximumAddress - output.size_bytes()) {
                return true;
            }
            return inputAddress >= outputBegin && inputAddress < outputBegin + output.size_bytes();
        }

        [[nodiscard]] void* ragdollInterface(const void* driver) noexcept
        {
            void* result{};
            if (!Memory::read(
                    reinterpret_cast<const void*>(
                        reinterpret_cast<std::uintptr_t>(driver) + Layout::Driver_RagdollInterface),
                    result) ||
                !Memory::rangeHasAccess(result, sizeof(std::uintptr_t), Memory::Access::Read)) {
                return nullptr;
            }
            return result;
        }

        [[nodiscard]] void* invokeGetLowSkeleton(GetLowSkeletonFunction function, void* interfaceValue) noexcept
        {
#if defined(_MSC_VER)
            __try {
                return function(interfaceValue);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return nullptr;
            }
#else
            return function(interfaceValue);
#endif
        }

        [[nodiscard]] SkeletonView skeletonView(const void* driver) noexcept
        {
            SkeletonView result{};
            if (!Memory::rangeHasAccess(driver, Layout::Driver_RagdollInterface + sizeof(void*), Memory::Access::Read)) {
                return result;
            }
            result.ragdollInterface = ragdollInterface(driver);
            if (!result.ragdollInterface) {
                result.status = RootPoseStatus::InvalidRagdollInterface;
                return result;
            }
            const auto getLowSkeleton = virtualFunction<GetLowSkeletonFunction>(
                result.ragdollInterface,
                Layout::Interface_GetLowSkeletonVtableSlot);
            if (!getLowSkeleton) {
                result.status = RootPoseStatus::VtableUnavailable;
                return result;
            }
            result.skeleton = invokeGetLowSkeleton(getLowSkeleton, result.ragdollInterface);
            if (!Memory::rangeHasAccess(
                    result.skeleton,
                    Layout::LowSkeleton_MinimumReadableSize,
                    Memory::Access::Read)) {
                result.status = RootPoseStatus::InvalidSkeleton;
                return result;
            }
            std::int32_t count{};
            if (!Memory::read(
                    static_cast<const std::byte*>(result.skeleton) + Layout::LowSkeleton_ParentIndices,
                    result.parentIndices) ||
                !Memory::read(
                    static_cast<const std::byte*>(result.skeleton) + Layout::LowSkeleton_BoneCount,
                    count) ||
                count <= 0 || static_cast<std::size_t>(count) > Layout::MaximumLowPoseBoneCount ||
                !Memory::rangeHasAccess(
                    result.parentIndices,
                    static_cast<std::size_t>(count) * sizeof(std::int16_t),
                    Memory::Access::Read)) {
                result.status = RootPoseStatus::InvalidSkeleton;
                return result;
            }
            result.boneCount = static_cast<std::size_t>(count);
            result.status = RootPoseStatus::Ready;
            return result;
        }

        [[nodiscard]] bool sameSkeleton(const SkeletonView& first, const SkeletonView& second) noexcept
        {
            return first.status == RootPoseStatus::Ready && second.status == RootPoseStatus::Ready &&
                   first.ragdollInterface == second.ragdollInterface && first.skeleton == second.skeleton &&
                   first.parentIndices == second.parentIndices && first.boneCount == second.boneCount;
        }

        [[nodiscard]] bool normalizeQuaternion(float* quaternion) noexcept
        {
            const auto lengthSquared = quaternion[0] * quaternion[0] + quaternion[1] * quaternion[1] +
                                       quaternion[2] * quaternion[2] + quaternion[3] * quaternion[3];
            if (!std::isfinite(lengthSquared) || lengthSquared <= MinimumQuaternionLengthSquared) {
                return false;
            }
            const auto inverseLength = 1.0f / std::sqrt(lengthSquared);
            for (std::size_t index = 0; index < 4; ++index) {
                quaternion[index] *= inverseLength;
            }
            return true;
        }

        [[nodiscard]] float length3(const float x, const float y, const float z) noexcept
        {
            const auto squared = x * x + y * y + z * z;
            return std::isfinite(squared) ? std::sqrt(squared) : (std::numeric_limits<float>::infinity)();
        }

        [[nodiscard]] float normalizedAngle(const float input) noexcept
        {
            if (!std::isfinite(input)) {
                return (std::numeric_limits<float>::quiet_NaN)();
            }
            return std::remainder(input, 2.0f * Pi);
        }

        [[nodiscard]] float yawFromQuaternion(const float* quaternion) noexcept
        {
            const float forwardX = 2.0f *
                                   (quaternion[0] * quaternion[1] - quaternion[2] * quaternion[3]);
            const float forwardY = 1.0f -
                                   2.0f * (quaternion[0] * quaternion[0] + quaternion[2] * quaternion[2]);
            return std::atan2(forwardX, forwardY);
        }
    }

    std::string_view toString(const RootPoseStatus status) noexcept
    {
        switch (status) {
        case RootPoseStatus::Ready: return "ready";
        case RootPoseStatus::InvalidRuntime: return "invalid-runtime";
        case RootPoseStatus::InvalidDriver: return "invalid-driver";
        case RootPoseStatus::InvalidContext: return "invalid-context";
        case RootPoseStatus::InvalidRagdollInterface: return "invalid-ragdoll-interface";
        case RootPoseStatus::VtableUnavailable: return "vtable-unavailable";
        case RootPoseStatus::FunctionUnavailable: return "function-unavailable";
        case RootPoseStatus::InvalidSkeleton: return "invalid-skeleton";
        case RootPoseStatus::OutputTooSmall: return "output-too-small";
        case RootPoseStatus::InvalidBoneIndex: return "invalid-bone-index";
        case RootPoseStatus::BodyUnavailable: return "body-unavailable";
        case RootPoseStatus::InvalidPose: return "invalid-pose";
        case RootPoseStatus::InvalidScale: return "invalid-scale";
        case RootPoseStatus::NativeFault: return "native-fault";
        case RootPoseStatus::NonFiniteResult: return "non-finite-result";
        case RootPoseStatus::GenerationChanged: return "generation-changed";
        default: return "unknown";
        }
    }

    LowSkeletonResult RootPoseApi::copyLowSkeletonParents(const std::span<std::int16_t> output) const noexcept
    {
        LowSkeletonResult result{};
        if (!_module) {
            return result;
        }
        const auto view = skeletonView(_driver);
        result.status = view.status;
        result.ragdollInterfaceAddress = reinterpret_cast<std::uintptr_t>(view.ragdollInterface);
        result.skeletonAddress = reinterpret_cast<std::uintptr_t>(view.skeleton);
        result.boneCount = view.boneCount;
        if (view.status != RootPoseStatus::Ready) {
            return result;
        }
        if (output.size() < view.boneCount) {
            result.status = RootPoseStatus::OutputTooSmall;
            return result;
        }
        if (!Memory::copyFrom(view.parentIndices, output.data(), view.boneCount * sizeof(std::int16_t))) {
            result.status = RootPoseStatus::InvalidSkeleton;
            return result;
        }
        for (std::size_t index = 0; index < view.boneCount; ++index) {
            if (output[index] < -1 || output[index] >= static_cast<std::int16_t>(index)) {
                result.status = RootPoseStatus::InvalidSkeleton;
                return result;
            }
        }
        const auto current = skeletonView(_driver);
        std::array<std::int16_t, Layout::MaximumLowPoseBoneCount> confirmation{};
        if (!sameSkeleton(view, current) ||
            !Memory::copyFrom(
                current.parentIndices,
                confirmation.data(),
                view.boneCount * sizeof(std::int16_t)) ||
            !std::equal(output.begin(), output.begin() + view.boneCount, confirmation.begin())) {
            result.status = RootPoseStatus::GenerationChanged;
            return result;
        }
        result.copiedCount = view.boneCount;
        result.status = RootPoseStatus::Ready;
        return result;
    }

    RootBodyAnchorResult RootPoseApi::readBodyAnchor(const std::uint32_t boneIndex) const noexcept
    {
        RootBodyAnchorResult result{};
        result.boneIndex = boneIndex;
        result.bodyId = Layout::InvalidBodyId;
        if (!_module) {
            return result;
        }
        const auto skeleton = skeletonView(_driver);
        if (skeleton.status != RootPoseStatus::Ready) {
            result.status = skeleton.status;
            return result;
        }
        if (boneIndex >= skeleton.boneCount || boneIndex > static_cast<std::uint32_t>((std::numeric_limits<int>::max)())) {
            result.status = RootPoseStatus::InvalidBoneIndex;
            return result;
        }
        result.ragdollInterfaceAddress = reinterpret_cast<std::uintptr_t>(skeleton.ragdollInterface);
        const auto getBodyHandle = virtualFunction<GetBodyHandleFunction>(
            skeleton.ragdollInterface,
            Layout::Interface_GetBodyHandleVtableSlot);
        if (!getBodyHandle) {
            result.status = RootPoseStatus::VtableUnavailable;
            return result;
        }
        const void* bodyHandle{};
#if defined(_MSC_VER)
        __try {
            bodyHandle = getBodyHandle(skeleton.ragdollInterface, static_cast<int>(boneIndex));
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            result.status = RootPoseStatus::NativeFault;
            return result;
        }
#else
        bodyHandle = getBodyHandle(skeleton.ragdollInterface, static_cast<int>(boneIndex));
#endif
        if (!Memory::rangeHasAccess(bodyHandle, Layout::BodyHandle_MinimumReadableSize, Memory::Access::Read) ||
            !Memory::read(
                static_cast<const std::byte*>(bodyHandle) + Layout::BodyHandle_BodyId,
                result.bodyId) ||
            result.bodyId == Layout::InvalidBodyId) {
            result.status = RootPoseStatus::BodyUnavailable;
            return result;
        }
        result.bodyHandleAddress = reinterpret_cast<std::uintptr_t>(bodyHandle);
        if (!Memory::rangeHasAccess(_context, sizeof(std::uintptr_t), Memory::Access::Read)) {
            result.status = RootPoseStatus::InvalidContext;
            return result;
        }
        const auto resolvePhysics = nativeFunction<ResolvePhysicsInterfaceFunction>(
            _module,
            Addresses::Symbol::Ragdoll_ResolvePhysicsInterface);
        if (!resolvePhysics) {
            result.status = RootPoseStatus::FunctionUnavailable;
            return result;
        }
        void* physicsInterface{};
#if defined(_MSC_VER)
        __try {
            physicsInterface = resolvePhysics(_context);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            result.status = RootPoseStatus::NativeFault;
            return result;
        }
#else
        physicsInterface = resolvePhysics(_context);
#endif
        if (!Memory::rangeHasAccess(physicsInterface, sizeof(std::uintptr_t), Memory::Access::Read)) {
            result.status = RootPoseStatus::InvalidContext;
            return result;
        }
        result.physicsInterfaceAddress = reinterpret_cast<std::uintptr_t>(physicsInterface);
        const auto getOffset = virtualFunction<GetBodyOffsetFunction>(
            physicsInterface,
            Layout::PhysicsInterface_GetBodyOffsetVtableSlot);
        const auto getTransform = virtualFunction<GetBodyTransformFunction>(
            physicsInterface,
            Layout::PhysicsInterface_GetBodyTransformVtableSlot);
        if (!getOffset || !getTransform) {
            result.status = RootPoseStatus::VtableUnavailable;
            return result;
        }
#if defined(_MSC_VER)
        __try {
            getOffset(physicsInterface, bodyHandle, result.localOffset.data());
            getTransform(physicsInterface, bodyHandle, &result.worldTransform);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            result.status = RootPoseStatus::NativeFault;
            return result;
        }
#else
        getOffset(physicsInterface, bodyHandle, result.localOffset.data());
        getTransform(physicsInterface, bodyHandle, &result.worldTransform);
#endif
        result.localOffset[3] = 0.0f;
        if (!std::all_of(result.localOffset.begin(), result.localOffset.end(), [](const float value) {
                return std::isfinite(value);
            }) ||
            !result.worldTransform.finite()) {
            result.status = RootPoseStatus::NonFiniteResult;
            return result;
        }
        const auto currentSkeleton = skeletonView(_driver);
        const void* currentHandle{};
        std::uint32_t currentBodyId{ Layout::InvalidBodyId };
        void* currentPhysicsInterface{};
#if defined(_MSC_VER)
        __try {
            if (sameSkeleton(skeleton, currentSkeleton)) {
                currentHandle = getBodyHandle(currentSkeleton.ragdollInterface, static_cast<int>(boneIndex));
                if (Memory::rangeHasAccess(currentHandle, Layout::BodyHandle_MinimumReadableSize, Memory::Access::Read)) {
                    if (!Memory::read(
                            static_cast<const std::byte*>(currentHandle) + Layout::BodyHandle_BodyId,
                            currentBodyId)) {
                        currentHandle = nullptr;
                    }
                }
                currentPhysicsInterface = resolvePhysics(_context);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            currentHandle = nullptr;
        }
#else
        if (sameSkeleton(skeleton, currentSkeleton)) {
            currentHandle = getBodyHandle(currentSkeleton.ragdollInterface, static_cast<int>(boneIndex));
            if (Memory::rangeHasAccess(currentHandle, Layout::BodyHandle_MinimumReadableSize, Memory::Access::Read)) {
                if (!Memory::read(
                        static_cast<const std::byte*>(currentHandle) + Layout::BodyHandle_BodyId,
                        currentBodyId)) {
                    currentHandle = nullptr;
                }
            }
            currentPhysicsInterface = resolvePhysics(_context);
        }
#endif
        if (currentHandle != bodyHandle || currentBodyId != result.bodyId || currentPhysicsInterface != physicsInterface) {
            result.status = RootPoseStatus::GenerationChanged;
            return result;
        }
        result.status = RootPoseStatus::Ready;
        return result;
    }

    RootPoseResult RootPoseApi::mapHighToLow(
        const std::span<const HkQsTransform> highPoseLocal,
        const std::span<HkQsTransform> lowPoseLocal) const noexcept
    {
        if (!_module) {
            return {};
        }
        const auto skeleton = skeletonView(_driver);
        if (skeleton.status != RootPoseStatus::Ready) {
            return { skeleton.status };
        }
        if (highPoseLocal.empty() || highPoseLocal.size() > Layout::MaximumHighPoseBoneCount ||
            !allFinite(highPoseLocal) || transformsOverlap(highPoseLocal, lowPoseLocal)) {
            return { RootPoseStatus::InvalidPose, 0, skeleton.boneCount };
        }
        if (lowPoseLocal.size() < skeleton.boneCount) {
            return { RootPoseStatus::OutputTooSmall, 0, skeleton.boneCount };
        }
        const auto function = nativeFunction<MapHighToLowFunction>(_module, Addresses::Symbol::Ragdoll_MapHighToLowPose);
        if (!function) {
            return { RootPoseStatus::FunctionUnavailable, 0, skeleton.boneCount };
        }
#if defined(_MSC_VER)
        __try {
            function(_driver, highPoseLocal.data(), static_cast<std::uint32_t>(highPoseLocal.size()), lowPoseLocal.data());
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return { RootPoseStatus::NativeFault, 0, skeleton.boneCount };
        }
#else
        function(_driver, highPoseLocal.data(), static_cast<std::uint32_t>(highPoseLocal.size()), lowPoseLocal.data());
#endif
        if (!sameSkeleton(skeleton, skeletonView(_driver))) {
            return { RootPoseStatus::GenerationChanged, 0, skeleton.boneCount };
        }
        if (!allFinite(lowPoseLocal.first(skeleton.boneCount))) {
            return { RootPoseStatus::NonFiniteResult, 0, skeleton.boneCount };
        }
        return { RootPoseStatus::Ready, skeleton.boneCount, skeleton.boneCount };
    }

    RootPoseResult RootPoseApi::copyAndApplyScale(
        const std::span<const HkQsTransform> input,
        const std::span<HkQsTransform> output,
        const float worldScale) const noexcept
    {
        if (!_module) {
            return {};
        }
        if (input.empty() || input.size() > Layout::MaximumLowPoseBoneCount || !allFinite(input) ||
            transformsOverlap(input, output)) {
            return { RootPoseStatus::InvalidPose };
        }
        if (output.size() < input.size()) {
            return { RootPoseStatus::OutputTooSmall, 0, input.size() };
        }
        if (!std::isfinite(worldScale) || worldScale <= 0.0f) {
            return { RootPoseStatus::InvalidScale, 0, input.size() };
        }
        const auto function = nativeFunction<CopyAndApplyScaleFunction>(
            _module,
            Addresses::Symbol::Ragdoll_CopyAndApplyScaleToPose);
        if (!function) {
            return { RootPoseStatus::FunctionUnavailable, 0, input.size() };
        }
#if defined(_MSC_VER)
        __try {
            function(1, static_cast<int>(input.size()), input.data(), output.data(), worldScale);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return { RootPoseStatus::NativeFault, 0, input.size() };
        }
#else
        function(1, static_cast<int>(input.size()), input.data(), output.data(), worldScale);
#endif
        if (!allFinite(output.first(input.size()))) {
            return { RootPoseStatus::NonFiniteResult, 0, input.size() };
        }
        return { RootPoseStatus::Ready, input.size(), input.size() };
    }

    RootPoseResult RootPoseApi::copyAndScaleTransform(
        const HkQsTransform& input,
        HkQsTransform& output) const noexcept
    {
        if (!_module) {
            return {};
        }
        if (!input.finite() || &input == &output) {
            return { RootPoseStatus::InvalidPose, 0, 1 };
        }
        const auto function = nativeFunction<CopyAndScaleTransformFunction>(
            _module,
            Addresses::Symbol::Ragdoll_CopyAndScaleTransform);
        if (!function) {
            return { RootPoseStatus::FunctionUnavailable, 0, 1 };
        }
#if defined(_MSC_VER)
        __try {
            function(1, &input, &output);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            output = {};
            return { RootPoseStatus::NativeFault, 0, 1 };
        }
#else
        function(1, &input, &output);
#endif
        return output.finite() ? RootPoseResult{ RootPoseStatus::Ready, 1, 1 } :
                                 RootPoseResult{ RootPoseStatus::NonFiniteResult, 0, 1 };
    }

    RootPoseResult RootPoseApi::localToWorld(
        const std::span<const std::int16_t> parentIndices,
        const HkQsTransform& worldFromModel,
        const std::span<const HkQsTransform> localPose,
        const std::span<HkQsTransform> worldPose) const noexcept
    {
        if (!_module) {
            return {};
        }
        const auto count = localPose.size();
        if (count == 0 || count > Layout::MaximumLowPoseBoneCount || parentIndices.size() != count ||
            !worldFromModel.finite() || !allFinite(localPose) || transformsOverlap(localPose, worldPose) ||
            transformInside(worldFromModel, worldPose)) {
            return { RootPoseStatus::InvalidPose };
        }
        for (std::size_t index = 0; index < count; ++index) {
            if (parentIndices[index] < -1 || parentIndices[index] >= static_cast<std::int16_t>(index)) {
                return { RootPoseStatus::InvalidSkeleton, 0, count };
            }
        }
        if (worldPose.size() < count) {
            return { RootPoseStatus::OutputTooSmall, 0, count };
        }
        const auto function = nativeFunction<LocalToWorldFunction>(_module, Addresses::Symbol::Ragdoll_PoseLocalToWorld);
        if (!function) {
            return { RootPoseStatus::FunctionUnavailable, 0, count };
        }
#if defined(_MSC_VER)
        __try {
            function(
                static_cast<int>(count),
                parentIndices.data(),
                &worldFromModel,
                localPose.data(),
                worldPose.data());
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return { RootPoseStatus::NativeFault, 0, count };
        }
#else
        function(
            static_cast<int>(count),
            parentIndices.data(),
            &worldFromModel,
            localPose.data(),
            worldPose.data());
#endif
        if (!allFinite(worldPose.first(count))) {
            return { RootPoseStatus::NonFiniteResult, 0, count };
        }
        return { RootPoseStatus::Ready, count, count };
    }

    HkQsTransform identityHkQsTransform() noexcept
    {
        HkQsTransform result{};
        result.rotation[3] = 1.0f;
        std::fill(std::begin(result.scale), std::end(result.scale), 1.0f);
        return result;
    }

    RootAnchorResult applyBodyAnchorOffset(
        const HkQsTransform& transform,
        const std::span<const float, 3> localOffset,
        const float maximumOffset) noexcept
    {
        RootAnchorResult result{};
        result.transform = transform;
        if (!transform.finite()) {
            return result;
        }
        if (!std::isfinite(localOffset[0]) || !std::isfinite(localOffset[1]) || !std::isfinite(localOffset[2])) {
            result.status = RootMathStatus::NonFiniteOffset;
            return result;
        }
        if (!std::isfinite(maximumOffset) || maximumOffset < 0.0f) {
            result.status = RootMathStatus::InvalidMaximumOffset;
            return result;
        }
        result.offsetLength = length3(localOffset[0], localOffset[1], localOffset[2]);
        if (!std::isfinite(result.offsetLength)) {
            result.status = RootMathStatus::NonFiniteOffset;
            return result;
        }
        if (result.offsetLength > maximumOffset) {
            result.status = RootMathStatus::OffsetTooLarge;
            return result;
        }
        if (!normalizeQuaternion(result.transform.rotation)) {
            result.status = RootMathStatus::InvalidQuaternion;
            return result;
        }
        if (result.offsetLength > MinimumVectorLength) {
            const float qx = result.transform.rotation[0];
            const float qy = result.transform.rotation[1];
            const float qz = result.transform.rotation[2];
            const float qw = result.transform.rotation[3];
            const float tx = 2.0f * (qy * localOffset[2] - qz * localOffset[1]);
            const float ty = 2.0f * (qz * localOffset[0] - qx * localOffset[2]);
            const float tz = 2.0f * (qx * localOffset[1] - qy * localOffset[0]);
            result.transform.translation[0] += localOffset[0] + qw * tx + (qy * tz - qz * ty);
            result.transform.translation[1] += localOffset[1] + qw * ty + (qz * tx - qx * tz);
            result.transform.translation[2] += localOffset[2] + qw * tz + (qx * ty - qy * tx);
        }
        result.status = result.transform.finite() ? RootMathStatus::Ready : RootMathStatus::NonFiniteTransform;
        return result;
    }

    RootBiasResult computeRootTranslationBias(
        const HkQsTransform& animationAnchor,
        const HkQsTransform& actualAnchor) noexcept
    {
        RootBiasResult result{};
        result.adjustedAnimationAnchor = animationAnchor;
        if (!animationAnchor.finite() || !actualAnchor.finite()) {
            return result;
        }
        for (std::size_t index = 0; index < 3; ++index) {
            result.bias[index] = actualAnchor.translation[index] - animationAnchor.translation[index];
            result.adjustedAnimationAnchor.translation[index] += result.bias[index];
        }
        result.separation = length3(
            result.adjustedAnimationAnchor.translation[0] - actualAnchor.translation[0],
            result.adjustedAnimationAnchor.translation[1] - actualAnchor.translation[1],
            result.adjustedAnimationAnchor.translation[2] - actualAnchor.translation[2]);
        if (!std::all_of(result.bias.begin(), result.bias.end(), [](const float value) { return std::isfinite(value); }) ||
            !std::isfinite(result.separation) || !result.adjustedAnimationAnchor.finite()) {
            result.status = RootMathStatus::NonFiniteBias;
            return result;
        }
        result.status = RootMathStatus::Ready;
        return result;
    }

    RootBiasResult applyRootTranslationBias(
        const HkQsTransform& animationAnchor,
        const HkQsTransform& actualAnchor,
        const std::span<const float, 3> bias,
        const float maximumSeparation) noexcept
    {
        RootBiasResult result{};
        result.adjustedAnimationAnchor = animationAnchor;
        if (!animationAnchor.finite() || !actualAnchor.finite()) {
            return result;
        }
        if (!std::isfinite(maximumSeparation) || maximumSeparation <= 0.0f) {
            result.status = RootMathStatus::InvalidMaximumSeparation;
            return result;
        }
        for (std::size_t index = 0; index < 3; ++index) {
            if (!std::isfinite(bias[index])) {
                result.status = RootMathStatus::NonFiniteBias;
                return result;
            }
            result.bias[index] = bias[index];
            result.adjustedAnimationAnchor.translation[index] += bias[index];
        }
        result.separation = length3(
            result.adjustedAnimationAnchor.translation[0] - actualAnchor.translation[0],
            result.adjustedAnimationAnchor.translation[1] - actualAnchor.translation[1],
            result.adjustedAnimationAnchor.translation[2] - actualAnchor.translation[2]);
        if (!std::isfinite(result.separation) || !result.adjustedAnimationAnchor.finite()) {
            result.status = RootMathStatus::NonFiniteBias;
        } else if (result.separation > maximumSeparation) {
            result.status = RootMathStatus::SpaceMismatch;
        } else {
            result.status = RootMathStatus::Ready;
        }
        return result;
    }

    RootDeltaResult sampleRootDelta(
        const HkQsTransform& previousAnimationRoot,
        const HkQsTransform& actualRoot) noexcept
    {
        RootDeltaResult result{};
        if (!previousAnimationRoot.finite() || !actualRoot.finite()) {
            return result;
        }
        auto previous = previousAnimationRoot;
        auto actual = actualRoot;
        if (!normalizeQuaternion(previous.rotation) || !normalizeQuaternion(actual.rotation)) {
            result.status = RootMathStatus::InvalidQuaternion;
            return result;
        }
        for (std::size_t index = 0; index < 3; ++index) {
            result.offset[index] = actual.translation[index] - previous.translation[index];
        }
        result.length = length3(result.offset[0], result.offset[1], result.offset[2]);
        result.horizontalLength = length3(result.offset[0], result.offset[1], 0.0f);
        result.yawRadians = normalizedAngle(yawFromQuaternion(actual.rotation) - yawFromQuaternion(previous.rotation));
        if (!std::isfinite(result.length) || !std::isfinite(result.horizontalLength) ||
            !std::isfinite(result.yawRadians)) {
            result.status = RootMathStatus::NonFiniteTransform;
            return result;
        }
        result.status = RootMathStatus::Ready;
        return result;
    }

    std::string_view toString(const RootMathStatus status) noexcept
    {
        switch (status) {
        case RootMathStatus::Ready: return "ready";
        case RootMathStatus::NonFiniteTransform: return "non-finite-transform";
        case RootMathStatus::InvalidQuaternion: return "invalid-quaternion";
        case RootMathStatus::NonFiniteOffset: return "non-finite-offset";
        case RootMathStatus::InvalidMaximumOffset: return "invalid-maximum-offset";
        case RootMathStatus::OffsetTooLarge: return "offset-too-large";
        case RootMathStatus::NonFiniteBias: return "non-finite-bias";
        case RootMathStatus::InvalidMaximumSeparation: return "invalid-maximum-separation";
        case RootMathStatus::SpaceMismatch: return "space-mismatch";
        default: return "unknown";
        }
    }
}
