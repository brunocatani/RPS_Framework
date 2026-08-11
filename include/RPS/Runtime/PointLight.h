#pragma once

#include "RPS/Runtime/RuntimeModule.h"
#include "RPS/Runtime/Scene.h"

#include <cstdint>
#include <string_view>

namespace RPS::Runtime::Rendering
{
    struct Color3
    {
        float red{};
        float green{};
        float blue{};

        [[nodiscard]] bool finiteNonNegative() const noexcept;
    };
    static_assert(sizeof(Color3) == 0x0C);

    struct PointLightSettings
    {
        Color3 ambient{};
        Color3 diffuse{ 1.0f, 1.0f, 1.0f };
        Color3 specular{ 1.0f, 1.0f, 1.0f };
        float dimmer{ 1.0f };
        float constantAttenuation{};
        float linearAttenuation{ 1.0f };
        float quadraticAttenuation{ 2.0f };
    };

    [[nodiscard]] bool validPointLightSettings(const PointLightSettings& settings) noexcept;

    enum class PointLightStatus : std::uint8_t
    {
        Completed,
        AlreadyAttached,
        AlreadyDetached,
        ManagerReplaced,
        InvalidRuntime,
        WrongThread,
        PhysicsStepActive,
        PhysicsStepStateUnavailable,
        InvalidSettings,
        InvalidLight,
        InvalidParent,
        FunctionUnavailable,
        ManagerUnavailable,
        FactoryRejected,
        UnexpectedVtable,
        ReferenceUnavailable,
        ChildAlreadyAttached,
        HierarchyFailed,
        RegistrationStateUnknown,
        NativeRejected,
        NativeFault,
        PostconditionFailed,
        ReferenceReleaseFailed,
    };

    struct PointLightCommandResult
    {
        PointLightStatus status{ PointLightStatus::InvalidRuntime };
        Scene::HierarchyStatus hierarchyStatus{ Scene::HierarchyStatus::InvalidRuntime };
        std::uintptr_t lightAddress{};
        std::uintptr_t proxyAddress{};
        std::uintptr_t managerAddress{};
        std::uintptr_t parentAddress{};
        std::uintptr_t functionAddress{};
        bool invoked{};
        bool rollbackAttempted{};
        bool rollbackSucceeded{};
        bool managerReplaced{};
        bool proxyReferenceReleased{};
        bool lightReferenceReleased{};
        bool parentReferenceReleased{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return status == PointLightStatus::Completed || status == PointLightStatus::AlreadyAttached ||
                   status == PointLightStatus::AlreadyDetached || status == PointLightStatus::ManagerReplaced;
        }
    };

    [[nodiscard]] std::string_view toString(PointLightStatus status) noexcept;

    class PointLight
    {
    public:
        PointLight() noexcept = default;
        ~PointLight() noexcept;

        PointLight(const PointLight&) = delete;
        PointLight& operator=(const PointLight&) = delete;
        PointLight(PointLight&& other) noexcept;
        PointLight& operator=(PointLight&&) = delete;

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
        [[nodiscard]] void* object() const noexcept { return _light; }
        [[nodiscard]] void* rendererProxy() const noexcept { return _proxy; }
        [[nodiscard]] void* sceneManager() const noexcept { return _manager; }
        [[nodiscard]] void* parent() const noexcept { return _parent; }
        [[nodiscard]] bool registrationStateKnown() const noexcept { return _registrationStateKnown; }
        [[nodiscard]] std::uint32_t ownerThreadId() const noexcept { return _ownerThreadId; }

        [[nodiscard]] PointLightCommandResult configure(const PointLightSettings& settings) noexcept;
        [[nodiscard]] PointLightCommandResult attach(void* parent, bool firstAvailable = true) noexcept;
        [[nodiscard]] PointLightCommandResult detach() noexcept;
        [[nodiscard]] PointLightCommandResult reset() noexcept;

    private:
        friend class PointLightApi;

        void moveFrom(PointLight&& other) noexcept;
        void clearWithoutRelease() noexcept;

        RuntimeModule _module{};
        void* _light{};
        void* _proxy{};
        void* _manager{};
        void* _parent{};
        std::uint32_t _ownerThreadId{};
        bool _registered{};
        bool _registrationStateKnown{ true };
    };

    struct PointLightCreateResult
    {
        PointLightStatus status{ PointLightStatus::InvalidRuntime };
        std::uintptr_t factoryAddress{};
        std::uintptr_t registerAddress{};
        std::uintptr_t unregisterAddress{};
        std::uintptr_t managerAddress{};
        std::uintptr_t observedVtable{};
        bool factoryInvoked{};
        bool registerInvoked{};
        bool ownershipAbandoned{};
        PointLight light{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return status == PointLightStatus::Completed && light.valid();
        }
    };

    /**
     * Owned non-shadow FO4VR NiPointLight registration. Destroy or reset the
     * handle on the owning game/frame thread outside physics. If destruction
     * occurs in an unsafe context or after an unknown native unregister fault,
     * references are intentionally leaked rather than risking renderer UAF or
     * a double unregister. Normal teardown order is strictly:
     * detach -> unregister -> proxy release -> light release.
     */
    class PointLightApi
    {
    public:
        explicit PointLightApi(RuntimeModule module) noexcept : _module(module) {}

        [[nodiscard]] PointLightCreateResult create(const PointLightSettings& settings) const noexcept;

    private:
        RuntimeModule _module{};
    };
}
