#include "RPS/Addresses/Catalog.h"
#include "RPS/Runtime/ActorPathing.h"

#include <iostream>

int main()
{
    using namespace RPS;
    using namespace RPS::Runtime;
    using namespace RPS::Runtime::Character;

    if (Addresses::record(Addresses::Symbol::Character_ActorNativePackageLoopGuard).rva != 0x03DD040 ||
        Addresses::record(Addresses::Symbol::Character_ActorQueryCurrentPathRequest).rva != 0x0DFF5D0 ||
        Addresses::record(Addresses::Symbol::Character_ActorQueryDirectMovementTargetAngle).rva != 0x0E00AB0) {
        std::cerr << "actor pathing catalog mismatch\n";
        return 1;
    }

    const auto module = RuntimeModule::detect();
    int actor{};
    ActorPathingApi api{ module, &actor };
    const auto state = api.queryState();
    const auto direct = api.queryDirectMovement();
    const auto request = api.queryCurrentRequest();
    if (module || state.status != PathingQueryStatus::InvalidRuntime || state ||
        direct.status != PathingQueryStatus::InvalidRuntime || direct ||
        request.status != PathingQueryStatus::InvalidRuntime || request || request.present || request.request ||
        toString(PathingQueryStatus::InvalidRetainedRequest) != "invalid-retained-request" ||
        toString(PathingStateStage::NativePackageLoopGuard) != "native-package-loop-guard") {
        std::cerr << "actor pathing API did not fail closed without FO4VR\n";
        return 1;
    }

    return 0;
}
