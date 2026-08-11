#include "RPS/Runtime/GeneratedBody.h"

#include <iostream>
#include <utility>

int main()
{
    using namespace RPS::Runtime;
    using namespace Physics;

    if (advanceGeneratedBodyRetirement(GeneratedBodyRetirementGraceSteps, 0) != GeneratedBodyRetirementGraceSteps ||
        advanceGeneratedBodyRetirement(GeneratedBodyRetirementGraceSteps, 1) != GeneratedBodyRetirementGraceSteps - 1 ||
        advanceGeneratedBodyRetirement(GeneratedBodyRetirementGraceSteps, GeneratedBodyRetirementGraceSteps) != 0 ||
        advanceGeneratedBodyRetirement(1, GeneratedBodyRetirementGraceSteps) != 0) {
        std::cerr << "deferred retirement schedule failed\n";
        return 1;
    }

    GeneratedBody empty{};
    GeneratedBody moved{ std::move(empty) };
    empty = std::move(moved);
    if (empty || moved || empty.bodyId().valid()) {
        std::cerr << "empty generated-body ownership failed\n";
        return 1;
    }

    const auto module = RuntimeModule::detect();
    GeneratedBodyService service{ module };
    GeneratedBodyCreateError error{};
    const auto body = service.create({}, error);
    if (module || service || body || error != GeneratedBodyCreateError::InvalidRuntime ||
        toString(error) != "invalid-runtime" || service.retirementCount() != 0 ||
        service.serviceOwnerThreadRetirements() != 0 || service.serviceCompletedPhysicsSteps() != 0 ||
        service.shutdownAfterWorldLoss() != 0) {
        std::cerr << "invalid runtime generated-body service did not fail closed\n";
        return 1;
    }

    return 0;
}
