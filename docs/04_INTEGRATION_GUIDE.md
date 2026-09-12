# Integration guide

Date: 2026-08-11; catalog example updated 2026-09-11 for `a92d0d5`.

RPS Framework is a pair of static C++ libraries. It does not replace F4SE's
plugin loader and it does not export ROCK's runtime provider ABI; `RPS_SDK`
continues to own that ABI. A mod can link the framework, use the SDK, or use
both.

## Build and consume

The workspace development build is:

```powershell
cmake --preset custom-fast
cmake --build --preset custom-fast --config Release -- /m:1 /p:CL_MPCount=2
ctest --test-dir build-fast -C Release --output-on-failure -j 4
```

`custom-fast` installs the current headers, static libraries, and CMake package
files under `build-fast/stage`.

In a consumer CMake project:

```cmake
find_package(RPSFramework CONFIG REQUIRED)

target_link_libraries(MyFo4VrPlugin PRIVATE RPS::Runtime)
```

`RPS::Runtime` brings `RPS::Addresses` transitively. Address-only tools may link
`RPS::Addresses` directly.

## Runtime entry gate

Detect the executable once after the plugin loader has established the game
module, and fail closed before exposing any framework-backed feature:

```cpp
#include <RPS/Runtime/RuntimeModule.h>

const auto module = RPS::Runtime::RuntimeModule::detect();
if (!module) {
    return false;
}
```

This is separate from the F4SEVR query-compatibility value. The framework checks
the actual `Fallout4VR.exe` PE identity and exact file version 1.2.72.0.

## Address-only access

```cpp
#include <RPS/Addresses/Catalog.h>

const auto& entry = RPS::Addresses::record(
    RPS::Addresses::Symbol::Physics_SetBodyVelocityDeferred);

// `entry.rva` is versioned metadata, never an absolute process address.
```

Standalone shared-library aliases were removed from the RPS catalog in
`a92d0d5`. The example inspects the retained deferred-velocity binding; it is
not a rename for the former immediate-velocity alias. Use the matching shared
declaration for a removed alias and rebuild consumers with matching headers
and libraries. See [CurrentReference](../../../Docs/RPS_Framework/docs/CurrentReference.md).

Prefer a checked `RPS::Runtime` wrapper whenever one exists. Direct resolution
is intended for a consumer that owns a hook or native integration contract the
framework cannot own on its behalf.

## Physics access pattern

World mutation is explicit. A body ID and a world pointer do not by themselves
authorize a write:

```cpp
#include <RPS/Runtime/PhysicsApi.h>
#include <RPS/Runtime/WorldAccess.h>

RPS::Runtime::Physics::WorldWriteGuard write{ module, hknpWorld };
if (!write.active()) {
    return;
}

RPS::Runtime::Physics::Api physics{ module, hknpWorld };
const bool moved = physics.setTransformDeferred(
    write,
    bodyId,
    targetTransform);
```

Use a `WorldReadGuard` for body snapshots and direct hknp shape casts. Bethesda
`PickObject` raycasts own their synchronization internally and intentionally do
not take an external guard.

## Shape and generated collider lifecycle

```cpp
#include <RPS/Runtime/GeneratedBody.h>
#include <RPS/Runtime/Shape.h>

RPS::Runtime::Physics::ShapeFactory shapes{ module };
auto sphere = shapes.buildSphereGame(radiusGame);
if (!sphere) {
    return;
}

RPS::Runtime::Physics::GeneratedBodyService bodies{ module };
RPS::Runtime::Physics::GeneratedBodyCreateError error{};
auto body = bodies.create(
    {
        .hknpWorld = hknpWorld,
        .bhkWorld = bhkWorld,
        .shape = sphere.get(),
        .collisionFilterInfo = filterInfo,
        .materialId = materialId,
        .motionType = RPS::Runtime::Physics::GeneratedMotionType::Keyframed,
        .name = "MyCollider",
    },
    error);
```

Keep the service alive for every body it created. Service owner-thread
retirements regularly, and call `serviceCompletedPhysicsSteps()` only after real
completed post-solve steps. Native references are released after eight such
steps. `shutdownAfterWorldLoss()` is only for a world that can no longer have a
physics reader.

## Stock constraint lifecycle

Construct `ConstraintService` on the owner thread for one hknp world, acquire a
matching `WorldWriteGuard`, and create an `OwnedConstraint`. Destruction from
another thread enters the fixed pending queue; service it on the owner thread
under a write guard. Never release a `PositionMotor` while a native constraint
can still reference it.

## Ragdoll callback data

Generator-output, pose, motor, and root-pose views are synchronous callback
views. Copy values into consumer storage if they must survive the graph callback.
Do not cache track, palette, skeleton, driver, or graph pointers.

## Actor path requests

`ActorPathingApi::queryCurrentRequest()` adopts the one retained reference the
engine returns. The result is move-only. Destroy it on the engine-owning thread
or deliberately transfer it with the native intrusive owner; do not add a
second release path.

## Point lights

```cpp
#include <RPS/Runtime/PointLight.h>

RPS::Runtime::Rendering::PointLightApi lights{ module };
auto created = lights.create({});
if (!created) {
    return;
}

auto light = std::move(created.light);
if (!light.attach(worldRoot) || !light.placeWorld(desiredWorld)) {
    (void)light.reset();
    return;
}

RPS::Runtime::Scene::ObjectApi sceneObject{ module, light.object() };
(void)sceneObject.setAppCulled(false);
```

Keep every command and final `reset()` on the creation thread outside physics.
Normal cleanup is detach, unregister, renderer-proxy release, then light release.
Inspect reset failures: an unknown unregister result is intentionally not retried.

## Hooks and shared native ownership

`HookPatch` validates and applies direct-call/vtable transactions; it does not
own a global trampoline allocator, callback registry, or shutdown quiescence for
the consumer. Exactly one component must own each process-global hook. Collision
pair policy similarly publishes immutable decisions but does not claim the
engine detour.

## Failure handling

- Treat every failed status as no authority to continue the native operation.
- Preserve result structs in diagnostics; they expose stage, address, generation,
  invocation, cleanup, rollback, or ownership outcomes without retaining engine
  pointers unless the type explicitly owns one.
- Do not retry an operation whose result says native ownership is unknown.
- Do not destroy move-only engine owners from arbitrary threads.
- Do not bypass epoch guards because a raw address appears valid.
