#include "RPS/Runtime/PointLight.h"

#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/Memory.h"
#include "RPS/Runtime/NativeReference.h"
#include "RPS/Runtime/WorldAccess.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <cmath>

namespace RPS::Runtime::Rendering
{
    namespace
    {
        namespace Layout = Addresses::Layouts::PointLight;

        using CreatePointLightFunction = void* (*)();
        using RegisterPointLightFunction = void* (*)(void*, void*, bool);
        using UnregisterPointLightFunction = void (*)(void*, void*);
        using UpdateWorldDataFunction = void (*)(void*, void*);

        inline constexpr float MinimumTransformScale = 1.0e-5f;
        inline constexpr float TransformComparisonTolerance = 1.0e-3f;

        struct alignas(8) NativeUpdateData
        {
            float time{};
            std::uint32_t timePadding{};
            void* camera{};
            std::uint32_t flags{};
            std::uint32_t renderObjects{};
            std::uint32_t fadeNodeDepth{};
            std::uint32_t tailPadding{};
        };
        static_assert(sizeof(NativeUpdateData) == Addresses::Layouts::Scene::NiUpdateDataSize);

        struct PointLightFieldSnapshot
        {
            Color3 ambient{};
            Color3 diffuse{};
            Color3 specular{};
            float dimmer{};
            float constantAttenuation{};
            float linearAttenuation{};
            float quadraticAttenuation{};
        };

        [[nodiscard]] void* field(void* const light, const std::ptrdiff_t offset) noexcept
        {
            return reinterpret_cast<void*>(
                reinterpret_cast<std::uintptr_t>(light) + static_cast<std::uintptr_t>(offset));
        }

        [[nodiscard]] const void* field(const void* const light, const std::ptrdiff_t offset) noexcept
        {
            return reinterpret_cast<const void*>(
                reinterpret_cast<std::uintptr_t>(light) + static_cast<std::uintptr_t>(offset));
        }

        [[nodiscard]] PointLightStatus executionStatus(
            const RuntimeModule& module,
            const std::uint32_t ownerThreadId = 0) noexcept
        {
            if (!module) {
                return PointLightStatus::InvalidRuntime;
            }
            if (ownerThreadId != 0 && GetCurrentThreadId() != ownerThreadId) {
                return PointLightStatus::WrongThread;
            }
            switch (Physics::currentThreadPhysicsStepState(module)) {
            case Physics::PhysicsStepState::Inside:
                return PointLightStatus::PhysicsStepActive;
            case Physics::PhysicsStepState::Unknown:
                return PointLightStatus::PhysicsStepStateUnavailable;
            case Physics::PhysicsStepState::Outside:
                return PointLightStatus::Completed;
            }
            return PointLightStatus::PhysicsStepStateUnavailable;
        }

        template <class Function>
        [[nodiscard]] Function checkedFunction(
            const RuntimeModule& module,
            const Addresses::Symbol symbol,
            std::uintptr_t& address) noexcept
        {
            address = module.resolve(symbol);
            if (!Memory::rangeHasAccess(
                    reinterpret_cast<const void*>(address),
                    1,
                    Memory::Access::Execute)) {
                address = 0;
                return nullptr;
            }
            return reinterpret_cast<Function>(address);
        }

        [[nodiscard]] bool resolveManager(const RuntimeModule& module, void*& manager) noexcept
        {
            manager = nullptr;
            const auto global = module.resolve(Addresses::Symbol::Scene_PointLightManager);
            std::uintptr_t vtable{};
            std::uintptr_t firstVirtual{};
            return Memory::read(reinterpret_cast<const void*>(global), manager) &&
                   Memory::rangeHasAccess(
                       manager,
                       Layout::ManagerMinimumReadableSize,
                       Memory::Access::Read) &&
                   Memory::read(manager, vtable) && vtable != 0 &&
                   Memory::read(reinterpret_cast<const void*>(vtable), firstVirtual) && firstVirtual != 0 &&
                   Memory::rangeHasAccess(
                       reinterpret_cast<const void*>(firstVirtual),
                       1,
                       Memory::Access::Execute);
        }

        [[nodiscard]] bool pointLightIdentityValid(
            const RuntimeModule& module,
            const void* const light,
            std::uintptr_t& observedVtable) noexcept
        {
            observedVtable = 0;
            return Memory::rangeHasAccess(light, Layout::ObjectSize, Memory::Access::Read) &&
                   Memory::read(light, observedVtable) &&
                   observedVtable == module.resolve(Addresses::Symbol::Scene_PointLightVtable);
        }

        [[nodiscard]] bool rendererProxyValid(const void* const proxy) noexcept
        {
            std::uintptr_t vtable{};
            std::uintptr_t firstVirtual{};
            return Memory::rangeHasAccess(
                       proxy,
                       Layout::RendererProxyMinimumReadableSize,
                       Memory::Access::Read) &&
                   Memory::read(proxy, vtable) && vtable != 0 &&
                   Memory::read(reinterpret_cast<const void*>(vtable), firstVirtual) && firstVirtual != 0 &&
                   Memory::rangeHasAccess(
                       reinterpret_cast<const void*>(firstVirtual),
                       1,
                       Memory::Access::Execute);
        }

        [[nodiscard]] bool hasLiveBethesdaReference(const void* const object) noexcept
        {
            std::uint32_t referenceWord{};
            return object &&
                   Memory::read(
                       field(object, Addresses::Layouts::Bethesda::ReferencedObject_ReferenceWord),
                       referenceWord) &&
                   (referenceWord & Addresses::Layouts::Bethesda::ReferencedObject_ReferenceCountMask) != 0;
        }

        [[nodiscard]] bool readSettings(const void* const light, PointLightFieldSnapshot& output) noexcept
        {
            return Memory::read(field(light, Layout::Ambient), output.ambient) &&
                   Memory::read(field(light, Layout::Diffuse), output.diffuse) &&
                   Memory::read(field(light, Layout::Specular), output.specular) &&
                   Memory::read(field(light, Layout::Dimmer), output.dimmer) &&
                   Memory::read(field(light, Layout::ConstantAttenuation), output.constantAttenuation) &&
                   Memory::read(field(light, Layout::LinearAttenuation), output.linearAttenuation) &&
                   Memory::read(field(light, Layout::QuadraticAttenuation), output.quadraticAttenuation);
        }

        [[nodiscard]] bool settingsFieldsWritable(void* const light) noexcept
        {
            return Memory::rangeHasAccess(field(light, Layout::Ambient), sizeof(Color3), Memory::Access::Write) &&
                   Memory::rangeHasAccess(field(light, Layout::Diffuse), sizeof(Color3), Memory::Access::Write) &&
                   Memory::rangeHasAccess(field(light, Layout::Specular), sizeof(Color3), Memory::Access::Write) &&
                   Memory::rangeHasAccess(field(light, Layout::Dimmer), sizeof(float), Memory::Access::Write) &&
                   Memory::rangeHasAccess(
                       field(light, Layout::ConstantAttenuation),
                       sizeof(float),
                       Memory::Access::Write) &&
                   Memory::rangeHasAccess(
                       field(light, Layout::LinearAttenuation),
                       sizeof(float),
                       Memory::Access::Write) &&
                   Memory::rangeHasAccess(
                       field(light, Layout::QuadraticAttenuation),
                       sizeof(float),
                       Memory::Access::Write);
        }

        [[nodiscard]] bool writeSettings(void* const light, const PointLightFieldSnapshot& values) noexcept
        {
            return Memory::write(field(light, Layout::Ambient), values.ambient) &&
                   Memory::write(field(light, Layout::Diffuse), values.diffuse) &&
                   Memory::write(field(light, Layout::Specular), values.specular) &&
                   Memory::write(field(light, Layout::ConstantAttenuation), values.constantAttenuation) &&
                   Memory::write(field(light, Layout::LinearAttenuation), values.linearAttenuation) &&
                   Memory::write(field(light, Layout::QuadraticAttenuation), values.quadraticAttenuation) &&
                   Memory::write(field(light, Layout::Dimmer), values.dimmer);
        }

        [[nodiscard]] bool applySettings(
            void* const light,
            const PointLightSettings& settings,
            PointLightCommandResult& result) noexcept
        {
            PointLightFieldSnapshot before{};
            if (!settingsFieldsWritable(light) || !readSettings(light, before)) {
                return false;
            }
            const PointLightFieldSnapshot desired{
                settings.ambient,
                settings.diffuse,
                settings.specular,
                settings.dimmer,
                settings.constantAttenuation,
                settings.linearAttenuation,
                settings.quadraticAttenuation,
            };
            if (writeSettings(light, desired)) {
                return true;
            }
            result.rollbackAttempted = true;
            result.rollbackSucceeded = writeSettings(light, before);
            return false;
        }

        [[nodiscard]] bool invokeCreate(const CreatePointLightFunction function, void*& light) noexcept
        {
            light = nullptr;
#if defined(_MSC_VER)
            __try {
                light = function();
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                light = nullptr;
                return false;
            }
#else
            light = function();
            return true;
#endif
        }

        [[nodiscard]] bool invokeRegister(
            const RegisterPointLightFunction function,
            void* const manager,
            void* const light,
            void*& proxy) noexcept
        {
            proxy = nullptr;
#if defined(_MSC_VER)
            __try {
                proxy = function(manager, light, true);
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                proxy = nullptr;
                return false;
            }
#else
            proxy = function(manager, light, true);
            return true;
#endif
        }

        [[nodiscard]] bool invokeUnregister(
            const UnregisterPointLightFunction function,
            void* const manager,
            void* const light) noexcept
        {
#if defined(_MSC_VER)
            __try {
                function(manager, light);
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
#else
            function(manager, light);
            return true;
#endif
        }

        [[nodiscard]] bool readObjectParent(void* const object, std::uintptr_t& parent) noexcept
        {
            return Memory::read(
                reinterpret_cast<const void*>(
                    reinterpret_cast<std::uintptr_t>(object) +
                    static_cast<std::uintptr_t>(Addresses::Layouts::Scene::NiAVObject_Parent)),
                parent);
        }

        [[nodiscard]] bool resolveObjectVirtual(
            void* const object,
            const std::size_t index,
            std::uintptr_t& functionAddress) noexcept
        {
            std::uintptr_t vtable{};
            return Memory::read(object, vtable) && vtable != 0 &&
                   Memory::read(
                       reinterpret_cast<const void*>(vtable + index * sizeof(void*)),
                       functionAddress) &&
                   functionAddress != 0 &&
                   Memory::rangeHasAccess(
                       reinterpret_cast<const void*>(functionAddress),
                       1,
                       Memory::Access::Execute);
        }

        [[nodiscard]] bool invokeUpdateWorldData(
            const UpdateWorldDataFunction function,
            void* const object,
            NativeUpdateData& updateData) noexcept
        {
#if defined(_MSC_VER)
            __try {
                function(object, &updateData);
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
#else
            function(object, &updateData);
            return true;
#endif
        }

        [[nodiscard]] bool validSceneTransform(const Scene::Transform& transform) noexcept
        {
            return transform.finite() && transform.rotate.properRotation() &&
                   std::abs(transform.scale) > MinimumTransformScale;
        }

        [[nodiscard]] bool nearFloat(const float first, const float second) noexcept
        {
            const auto scale = (std::max)({ 1.0f, std::abs(first), std::abs(second) });
            return std::abs(first - second) <= TransformComparisonTolerance * scale;
        }

        [[nodiscard]] bool nearTransform(
            const Scene::Transform& first,
            const Scene::Transform& second) noexcept
        {
            for (std::size_t row = 0; row < 3; ++row) {
                for (std::size_t column = 0; column < 3; ++column) {
                    if (!nearFloat(first.rotate.entry[row][column], second.rotate.entry[row][column])) {
                        return false;
                    }
                }
            }
            return nearFloat(first.translate.x, second.translate.x) &&
                   nearFloat(first.translate.y, second.translate.y) &&
                   nearFloat(first.translate.z, second.translate.z) && nearFloat(first.scale, second.scale);
        }

        [[nodiscard]] bool restoreLocalTransform(
            const RuntimeModule& module,
            void* const light,
            const Scene::Transform& local,
            const Scene::Transform& previousWorld,
            const UpdateWorldDataFunction updateWorldData,
            PointLightCommandResult& result) noexcept
        {
            result.rollbackAttempted = true;
            NativeUpdateData updateData{};
            const auto localRestored = Memory::write(
                field(light, Addresses::Layouts::Scene::NiAVObject_LocalTransform),
                local);
            const auto worldUpdated = localRestored && invokeUpdateWorldData(updateWorldData, light, updateData);
            const Scene::ObjectApi objectApi{ module, light };
            const auto boundUpdated = worldUpdated && static_cast<bool>(objectApi.updateWorldBound());
            const auto previousWorldRestored = boundUpdated && Memory::write(
                field(light, Addresses::Layouts::Scene::NiAVObject_PreviousWorldTransform),
                previousWorld);
            result.rollbackSucceeded = localRestored && worldUpdated && boundUpdated && previousWorldRestored;
            return result.rollbackSucceeded;
        }
    }

    bool Color3::finiteNonNegative() const noexcept
    {
        return std::isfinite(red) && std::isfinite(green) && std::isfinite(blue) && red >= 0.0f && green >= 0.0f &&
               blue >= 0.0f;
    }

    bool validPointLightSettings(const PointLightSettings& settings) noexcept
    {
        return settings.ambient.finiteNonNegative() && settings.diffuse.finiteNonNegative() &&
               settings.specular.finiteNonNegative() && std::isfinite(settings.dimmer) && settings.dimmer >= 0.0f &&
               std::isfinite(settings.constantAttenuation) && settings.constantAttenuation >= 0.0f &&
               std::isfinite(settings.linearAttenuation) && settings.linearAttenuation >= 0.0f &&
               std::isfinite(settings.quadraticAttenuation) && settings.quadraticAttenuation >= 0.0f &&
               (settings.constantAttenuation > 0.0f || settings.linearAttenuation > 0.0f ||
                settings.quadraticAttenuation > 0.0f);
    }

    std::string_view toString(const PointLightStatus status) noexcept
    {
        switch (status) {
        case PointLightStatus::Completed: return "completed";
        case PointLightStatus::AlreadyAttached: return "already-attached";
        case PointLightStatus::AlreadyDetached: return "already-detached";
        case PointLightStatus::ManagerReplaced: return "manager-replaced";
        case PointLightStatus::InvalidRuntime: return "invalid-runtime";
        case PointLightStatus::WrongThread: return "wrong-thread";
        case PointLightStatus::PhysicsStepActive: return "physics-step-active";
        case PointLightStatus::PhysicsStepStateUnavailable: return "physics-step-state-unavailable";
        case PointLightStatus::InvalidSettings: return "invalid-settings";
        case PointLightStatus::InvalidTransform: return "invalid-transform";
        case PointLightStatus::InvalidLight: return "invalid-light";
        case PointLightStatus::InvalidParent: return "invalid-parent";
        case PointLightStatus::ParentChanged: return "parent-changed";
        case PointLightStatus::FunctionUnavailable: return "function-unavailable";
        case PointLightStatus::ManagerUnavailable: return "manager-unavailable";
        case PointLightStatus::FactoryRejected: return "factory-rejected";
        case PointLightStatus::UnexpectedVtable: return "unexpected-vtable";
        case PointLightStatus::ReferenceUnavailable: return "reference-unavailable";
        case PointLightStatus::ChildAlreadyAttached: return "child-already-attached";
        case PointLightStatus::HierarchyFailed: return "hierarchy-failed";
        case PointLightStatus::RegistrationStateUnknown: return "registration-state-unknown";
        case PointLightStatus::NativeRejected: return "native-rejected";
        case PointLightStatus::NativeFault: return "native-fault";
        case PointLightStatus::PostconditionFailed: return "postcondition-failed";
        case PointLightStatus::ReferenceReleaseFailed: return "reference-release-failed";
        default: return "unknown";
        }
    }

    PointLight::~PointLight() noexcept
    {
        (void)reset();
    }

    PointLight::PointLight(PointLight&& other) noexcept
    {
        moveFrom(static_cast<PointLight&&>(other));
    }

    bool PointLight::valid() const noexcept
    {
        return _module && _light && _proxy && _manager && _registered && _registrationStateKnown;
    }

    void PointLight::moveFrom(PointLight&& other) noexcept
    {
        _module = other._module;
        _light = other._light;
        _proxy = other._proxy;
        _manager = other._manager;
        _parent = other._parent;
        _ownerThreadId = other._ownerThreadId;
        _registered = other._registered;
        _registrationStateKnown = other._registrationStateKnown;
        other.clearWithoutRelease();
    }

    void PointLight::clearWithoutRelease() noexcept
    {
        _module = {};
        _light = nullptr;
        _proxy = nullptr;
        _manager = nullptr;
        _parent = nullptr;
        _ownerThreadId = 0;
        _registered = false;
        _registrationStateKnown = true;
    }

    PointLightCommandResult PointLight::configure(const PointLightSettings& settings) noexcept
    {
        PointLightCommandResult result{};
        result.lightAddress = reinterpret_cast<std::uintptr_t>(_light);
        result.proxyAddress = reinterpret_cast<std::uintptr_t>(_proxy);
        result.managerAddress = reinterpret_cast<std::uintptr_t>(_manager);
        result.parentAddress = reinterpret_cast<std::uintptr_t>(_parent);
        result.status = executionStatus(_module, _ownerThreadId);
        if (result.status != PointLightStatus::Completed) {
            return result;
        }
        if (!validPointLightSettings(settings)) {
            result.status = PointLightStatus::InvalidSettings;
            return result;
        }
        std::uintptr_t observedVtable{};
        if (!valid() || !pointLightIdentityValid(_module, _light, observedVtable)) {
            result.status = PointLightStatus::InvalidLight;
            return result;
        }
        result.status = applySettings(_light, settings, result) ? PointLightStatus::Completed :
                        result.rollbackAttempted && !result.rollbackSucceeded ?
                            PointLightStatus::PostconditionFailed :
                            PointLightStatus::NativeFault;
        return result;
    }

    PointLightCommandResult PointLight::placeWorld(const Scene::Transform& world) noexcept
    {
        PointLightCommandResult result{};
        result.lightAddress = reinterpret_cast<std::uintptr_t>(_light);
        result.proxyAddress = reinterpret_cast<std::uintptr_t>(_proxy);
        result.managerAddress = reinterpret_cast<std::uintptr_t>(_manager);
        result.parentAddress = reinterpret_cast<std::uintptr_t>(_parent);
        result.requestedWorld = world;
        result.status = executionStatus(_module, _ownerThreadId);
        if (result.status != PointLightStatus::Completed) {
            return result;
        }
        if (!valid()) {
            result.status = PointLightStatus::InvalidLight;
            return result;
        }
        if (!validSceneTransform(world)) {
            result.status = PointLightStatus::InvalidTransform;
            return result;
        }

        std::uintptr_t observedVtable{};
        std::uintptr_t observedParent{};
        if (!pointLightIdentityValid(_module, _light, observedVtable) ||
            !readObjectParent(_light, observedParent)) {
            result.status = PointLightStatus::InvalidLight;
            return result;
        }
        if (observedParent != reinterpret_cast<std::uintptr_t>(_parent)) {
            result.status = PointLightStatus::ParentChanged;
            return result;
        }

        Scene::Transform desiredLocal = world;
        if (_parent) {
            Scene::Transform parentWorld{};
            if (!Memory::read(
                    field(_parent, Addresses::Layouts::Scene::NiAVObject_WorldTransform),
                    parentWorld) ||
                !validSceneTransform(parentWorld)) {
                result.status = PointLightStatus::InvalidParent;
                return result;
            }
            const auto converted = Scene::worldToParentLocal(parentWorld, world);
            if (!converted) {
                result.status = PointLightStatus::InvalidTransform;
                return result;
            }
            desiredLocal = converted.value;
        }

        Scene::Transform originalLocal{};
        Scene::Transform originalPreviousWorld{};
        if (!Memory::read(
                field(_light, Addresses::Layouts::Scene::NiAVObject_LocalTransform),
                originalLocal) ||
            !Memory::read(
                field(_light, Addresses::Layouts::Scene::NiAVObject_PreviousWorldTransform),
                originalPreviousWorld) ||
            !Memory::rangeHasAccess(
                field(_light, Addresses::Layouts::Scene::NiAVObject_LocalTransform),
                sizeof(Scene::Transform),
                Memory::Access::Write) ||
            !Memory::rangeHasAccess(
                field(_light, Addresses::Layouts::Scene::NiAVObject_PreviousWorldTransform),
                sizeof(Scene::Transform),
                Memory::Access::Write)) {
            result.status = PointLightStatus::InvalidLight;
            return result;
        }

        if (!resolveObjectVirtual(
                _light,
                Addresses::Layouts::Scene::NiAVObject_UpdateWorldDataVtableIndex,
                result.functionAddress)) {
            result.status = PointLightStatus::FunctionUnavailable;
            return result;
        }
        const auto updateWorldData = reinterpret_cast<UpdateWorldDataFunction>(result.functionAddress);
        if (!Memory::write(
                field(_light, Addresses::Layouts::Scene::NiAVObject_LocalTransform),
                desiredLocal)) {
            result.status = PointLightStatus::NativeFault;
            return result;
        }
        result.transformWritten = true;
        result.invoked = true;
        NativeUpdateData updateData{};
        if (!invokeUpdateWorldData(updateWorldData, _light, updateData)) {
            result.status = PointLightStatus::NativeFault;
            (void)restoreLocalTransform(
                _module,
                _light,
                originalLocal,
                originalPreviousWorld,
                updateWorldData,
                result);
            return result;
        }
        const Scene::ObjectApi objectApi{ _module, _light };
        if (!objectApi.updateWorldBound()) {
            result.status = PointLightStatus::NativeFault;
            (void)restoreLocalTransform(
                _module,
                _light,
                originalLocal,
                originalPreviousWorld,
                updateWorldData,
                result);
            return result;
        }

        std::uintptr_t parentAfter{};
        if (!readObjectParent(_light, parentAfter) || parentAfter != observedParent) {
            result.status = PointLightStatus::ParentChanged;
            (void)restoreLocalTransform(
                _module,
                _light,
                originalLocal,
                originalPreviousWorld,
                updateWorldData,
                result);
            return result;
        }
        if (!Memory::read(
                field(_light, Addresses::Layouts::Scene::NiAVObject_WorldTransform),
                result.observedWorld) ||
            !validSceneTransform(result.observedWorld) || !nearTransform(result.observedWorld, world)) {
            result.status = PointLightStatus::PostconditionFailed;
            (void)restoreLocalTransform(
                _module,
                _light,
                originalLocal,
                originalPreviousWorld,
                updateWorldData,
                result);
            return result;
        }
        if (!Memory::write(
                field(_light, Addresses::Layouts::Scene::NiAVObject_PreviousWorldTransform),
                result.observedWorld)) {
            result.status = PointLightStatus::NativeFault;
            (void)restoreLocalTransform(
                _module,
                _light,
                originalLocal,
                originalPreviousWorld,
                updateWorldData,
                result);
            return result;
        }
        result.status = PointLightStatus::Completed;
        return result;
    }

    PointLightCommandResult PointLight::attach(void* const parent, const bool firstAvailable) noexcept
    {
        PointLightCommandResult result{};
        result.lightAddress = reinterpret_cast<std::uintptr_t>(_light);
        result.proxyAddress = reinterpret_cast<std::uintptr_t>(_proxy);
        result.managerAddress = reinterpret_cast<std::uintptr_t>(_manager);
        result.parentAddress = reinterpret_cast<std::uintptr_t>(parent);
        result.status = executionStatus(_module, _ownerThreadId);
        if (result.status != PointLightStatus::Completed) {
            return result;
        }
        if (!valid()) {
            result.status = PointLightStatus::InvalidLight;
            return result;
        }
        if (!Memory::rangeHasAccess(
                parent,
                Addresses::Layouts::Scene::NiNode_MinimumReadableSize,
                Memory::Access::Read)) {
            result.status = PointLightStatus::InvalidParent;
            return result;
        }
        if (_parent) {
            result.status = _parent == parent ? PointLightStatus::AlreadyAttached :
                                               PointLightStatus::ChildAlreadyAttached;
            return result;
        }
        if (!hasLiveBethesdaReference(parent) || !addBethesdaReference(parent)) {
            result.status = PointLightStatus::ReferenceUnavailable;
            return result;
        }
        Scene::HierarchyApi hierarchy{ _module };
        const auto hierarchyResult = hierarchy.attachChild(parent, _light, firstAvailable);
        result.hierarchyStatus = hierarchyResult.status;
        result.invoked = hierarchyResult.invoked;
        result.functionAddress = hierarchyResult.functionAddress;
        if (hierarchyResult.parentAfter == reinterpret_cast<std::uintptr_t>(parent)) {
            _parent = parent;
        } else {
            result.parentReferenceReleased = releaseBethesdaReference(parent);
        }
        if (hierarchyResult.status == Scene::HierarchyStatus::AlreadyAttached) {
            result.status = PointLightStatus::AlreadyAttached;
        } else if (hierarchyResult.status == Scene::HierarchyStatus::Completed) {
            result.status = PointLightStatus::Completed;
        } else {
            result.status = PointLightStatus::HierarchyFailed;
        }
        return result;
    }

    PointLightCommandResult PointLight::detach() noexcept
    {
        PointLightCommandResult result{};
        result.lightAddress = reinterpret_cast<std::uintptr_t>(_light);
        result.proxyAddress = reinterpret_cast<std::uintptr_t>(_proxy);
        result.managerAddress = reinterpret_cast<std::uintptr_t>(_manager);
        result.parentAddress = reinterpret_cast<std::uintptr_t>(_parent);
        result.status = executionStatus(_module, _ownerThreadId);
        if (result.status != PointLightStatus::Completed) {
            return result;
        }
        if (!_light) {
            result.status = PointLightStatus::InvalidLight;
            return result;
        }
        if (!_parent) {
            std::uintptr_t observedParent{};
            if (!readObjectParent(_light, observedParent)) {
                result.status = PointLightStatus::InvalidLight;
            } else {
                result.status = observedParent == 0 ? PointLightStatus::AlreadyDetached :
                                                     PointLightStatus::PostconditionFailed;
            }
            return result;
        }

        auto* const parent = _parent;
        Scene::HierarchyApi hierarchy{ _module };
        const auto hierarchyResult = hierarchy.detachChild(parent, _light);
        result.hierarchyStatus = hierarchyResult.status;
        result.invoked = hierarchyResult.invoked;
        result.functionAddress = hierarchyResult.functionAddress;
        if (hierarchyResult.parentAfter == 0) {
            _parent = nullptr;
            result.parentReferenceReleased = releaseBethesdaReference(parent);
            if (!result.parentReferenceReleased) {
                result.status = PointLightStatus::ReferenceReleaseFailed;
                return result;
            }
        }
        result.status = hierarchyResult.status == Scene::HierarchyStatus::Completed ? PointLightStatus::Completed :
                                                                                     PointLightStatus::HierarchyFailed;
        return result;
    }

    PointLightCommandResult PointLight::reset() noexcept
    {
        PointLightCommandResult result{};
        result.lightAddress = reinterpret_cast<std::uintptr_t>(_light);
        result.proxyAddress = reinterpret_cast<std::uintptr_t>(_proxy);
        result.managerAddress = reinterpret_cast<std::uintptr_t>(_manager);
        result.parentAddress = reinterpret_cast<std::uintptr_t>(_parent);
        if (!_light && !_proxy && !_manager && !_parent) {
            result.status = PointLightStatus::AlreadyDetached;
            return result;
        }
        result.status = executionStatus(_module, _ownerThreadId);
        if (result.status != PointLightStatus::Completed) {
            return result;
        }

        auto detachStatus = PointLightStatus::Completed;
        if (_parent) {
            const auto detached = detach();
            result.hierarchyStatus = detached.hierarchyStatus;
            result.parentReferenceReleased = detached.parentReferenceReleased;
            detachStatus = detached.status;
            if (_parent) {
                result.status = PointLightStatus::HierarchyFailed;
                return result;
            }
        }
        if (!_registrationStateKnown) {
            result.status = PointLightStatus::RegistrationStateUnknown;
            return result;
        }

        void* currentManager{};
        if (_registered) {
            if (!resolveManager(_module, currentManager)) {
                result.status = PointLightStatus::ManagerUnavailable;
                return result;
            }
            if (currentManager != _manager) {
                result.managerReplaced = true;
                _registered = false;
            } else {
                const auto unregister = checkedFunction<UnregisterPointLightFunction>(
                    _module,
                    Addresses::Symbol::Scene_PointLightUnregister,
                    result.functionAddress);
                if (!unregister) {
                    result.status = PointLightStatus::FunctionUnavailable;
                    return result;
                }
                result.invoked = true;
                if (!invokeUnregister(unregister, _manager, _light)) {
                    _registrationStateKnown = false;
                    result.status = PointLightStatus::NativeFault;
                    return result;
                }
                _registered = false;
            }
        }

        auto* const proxy = _proxy;
        auto* const light = _light;
        clearWithoutRelease();
        result.proxyReferenceReleased = !proxy || releaseBethesdaReference(proxy);
        result.lightReferenceReleased = !light || releaseBethesdaReference(light);
        if (!result.proxyReferenceReleased || !result.lightReferenceReleased) {
            result.status = PointLightStatus::ReferenceReleaseFailed;
        } else if (detachStatus == PointLightStatus::HierarchyFailed) {
            result.status = PointLightStatus::HierarchyFailed;
        } else {
            result.status = result.managerReplaced ? PointLightStatus::ManagerReplaced :
                                                    PointLightStatus::Completed;
        }
        return result;
    }

    PointLightCreateResult PointLightApi::create(const PointLightSettings& settings) const noexcept
    {
        PointLightCreateResult result{};
        result.status = executionStatus(_module);
        if (result.status != PointLightStatus::Completed) {
            return result;
        }
        if (!validPointLightSettings(settings)) {
            result.status = PointLightStatus::InvalidSettings;
            return result;
        }
        void* manager{};
        if (!resolveManager(_module, manager)) {
            result.status = PointLightStatus::ManagerUnavailable;
            return result;
        }
        result.managerAddress = reinterpret_cast<std::uintptr_t>(manager);
        const auto factory = checkedFunction<CreatePointLightFunction>(
            _module,
            Addresses::Symbol::Scene_PointLightCreate,
            result.factoryAddress);
        const auto registerLight = checkedFunction<RegisterPointLightFunction>(
            _module,
            Addresses::Symbol::Scene_PointLightRegister,
            result.registerAddress);
        const auto unregisterLight = checkedFunction<UnregisterPointLightFunction>(
            _module,
            Addresses::Symbol::Scene_PointLightUnregister,
            result.unregisterAddress);
        if (!factory || !registerLight || !unregisterLight) {
            result.status = PointLightStatus::FunctionUnavailable;
            return result;
        }

        void* light{};
        result.factoryInvoked = true;
        if (!invokeCreate(factory, light)) {
            result.status = PointLightStatus::NativeFault;
            return result;
        }
        if (!light) {
            result.status = PointLightStatus::FactoryRejected;
            return result;
        }
        if (!pointLightIdentityValid(_module, light, result.observedVtable)) {
            result.status = PointLightStatus::UnexpectedVtable;
            result.ownershipAbandoned = true;
            return result;
        }
        if (!addBethesdaReference(light)) {
            result.status = PointLightStatus::ReferenceUnavailable;
            result.ownershipAbandoned = true;
            return result;
        }

        PointLightCommandResult configuration{};
        if (!applySettings(light, settings, configuration)) {
            result.status = configuration.rollbackAttempted && !configuration.rollbackSucceeded ?
                PointLightStatus::PostconditionFailed :
                PointLightStatus::NativeFault;
            (void)releaseBethesdaReference(light);
            return result;
        }
        const Scene::ObjectApi objectApi{ _module, light };
        if (!objectApi.setAppCulled(true)) {
            result.status = PointLightStatus::NativeFault;
            (void)releaseBethesdaReference(light);
            return result;
        }

        void* proxy{};
        result.registerInvoked = true;
        if (!invokeRegister(registerLight, manager, light, proxy)) {
            result.status = PointLightStatus::RegistrationStateUnknown;
            result.ownershipAbandoned = true;
            return result;
        }
        if (!proxy || !rendererProxyValid(proxy)) {
            if (!invokeUnregister(unregisterLight, manager, light)) {
                result.status = PointLightStatus::RegistrationStateUnknown;
                result.ownershipAbandoned = true;
                return result;
            }
            result.status = PointLightStatus::NativeRejected;
            (void)releaseBethesdaReference(light);
            return result;
        }
        if (!addBethesdaReference(proxy)) {
            if (!invokeUnregister(unregisterLight, manager, light)) {
                result.status = PointLightStatus::RegistrationStateUnknown;
                result.ownershipAbandoned = true;
                return result;
            }
            result.status = PointLightStatus::ReferenceUnavailable;
            (void)releaseBethesdaReference(light);
            return result;
        }

        void* currentManager{};
        if (!resolveManager(_module, currentManager) || currentManager != manager) {
            if (!invokeUnregister(unregisterLight, manager, light)) {
                result.status = PointLightStatus::RegistrationStateUnknown;
                result.ownershipAbandoned = true;
                return result;
            }
            (void)releaseBethesdaReference(proxy);
            (void)releaseBethesdaReference(light);
            result.status = PointLightStatus::ManagerReplaced;
            return result;
        }

        result.light._module = _module;
        result.light._light = light;
        result.light._proxy = proxy;
        result.light._manager = manager;
        result.light._ownerThreadId = GetCurrentThreadId();
        result.light._registered = true;
        result.light._registrationStateKnown = true;
        result.status = PointLightStatus::Completed;
        return result;
    }
}
