#include "RPS/Addresses/Catalog.h"
#include "RPS/Runtime/PointLight.h"

#include <iostream>
#include <limits>

int main()
{
    using namespace RPS;
    using namespace RPS::Runtime;
    using namespace RPS::Runtime::Rendering;

    if (Addresses::record(Addresses::Symbol::Scene_PointLightCreate).rva != 0x1C27DB0 ||
        Addresses::record(Addresses::Symbol::Scene_PointLightVtable).rva != 0x2E58BC8) {
        std::cerr << "point-light catalog mismatch\n";
        return 1;
    }

    PointLightSettings settings{};
    if (!validPointLightSettings(settings)) {
        std::cerr << "default point-light settings rejected\n";
        return 1;
    }
    settings.dimmer = -1.0f;
    if (validPointLightSettings(settings)) {
        std::cerr << "negative point-light dimmer accepted\n";
        return 1;
    }
    settings = {};
    settings.diffuse.red = (std::numeric_limits<float>::quiet_NaN)();
    if (validPointLightSettings(settings)) {
        std::cerr << "non-finite point-light color accepted\n";
        return 1;
    }
    settings = {};
    settings.constantAttenuation = 0.0f;
    settings.linearAttenuation = 0.0f;
    settings.quadraticAttenuation = 0.0f;
    if (validPointLightSettings(settings)) {
        std::cerr << "zero point-light attenuation accepted\n";
        return 1;
    }

    const auto module = RuntimeModule::detect();
    PointLightApi api{ module };
    const auto created = api.create(PointLightSettings{});
    PointLight empty{};
    const auto configured = empty.configure(PointLightSettings{});
    const auto attached = empty.attach(&settings);
    const auto detached = empty.detach();
    const auto reset = empty.reset();
    if (module || created.status != PointLightStatus::InvalidRuntime || created || created.light ||
        configured.status != PointLightStatus::InvalidRuntime || configured ||
        attached.status != PointLightStatus::InvalidRuntime || attached ||
        detached.status != PointLightStatus::InvalidRuntime || detached ||
        reset.status != PointLightStatus::AlreadyDetached || !reset ||
        toString(PointLightStatus::RegistrationStateUnknown) != "registration-state-unknown") {
        std::cerr << "point-light API did not fail closed without FO4VR\n";
        return 1;
    }

    return 0;
}
