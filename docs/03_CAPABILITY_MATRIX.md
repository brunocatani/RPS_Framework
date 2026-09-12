# Capability matrix

Date: 2026-08-11; catalog boundary updated 2026-09-11 for `a92d0d5`.

This ledger distinguishes implemented reusable mechanics from catalog-only
evidence and intentionally project-owned policy. `Stable` means the public API
has an explicit runtime/version gate, ownership and epoch contract, fail-closed
behavior, build-enforced source contracts, deterministic tests where possible,
and installed-package compilation coverage. It does not mean every native path
has received a new in-game smoke test inside this extraction session; the engine
contracts came from already proven local source and runtime use by direction.

## Public stable modules

| Module | Public responsibility | Ownership / epoch contract |
| --- | --- | --- |
| `RPS::Addresses` | FO4VR 1.2.72 RVAs, vtables, hook sites, globals, layouts, evidence metadata | RVA-only; never caches absolute addresses |
| `RuntimeModule` | Current-process PE identity, exact executable version, RVA resolution, masked byte checks | Value object; rejects every non-1.2.72 FO4VR image |
| `Memory` | Multi-region access checks and guarded copies | Borrowed synchronous memory only |
| `GeneratorOutput` | Bounded callback-scoped generator tracks and palettes | Non-owning view; no pointer escape from graph callback |
| `AnimationPose` | Pose/WFM capture, copy, blend, map, and scale primitives | Caller storage; callback-scoped native inputs |
| `AnimationMotor` | Checked keyframed/powered/WFM motor-track inspection and tuning | In-place callback-scoped mutation; no heap ownership |
| `CollisionFilter` | Layer/group encoding, matrix construction, pure pair decisions | Pure values; 64-layer domain |
| `CollisionPairPolicy` | Immutable fixed-capacity suppression publication | Consumer owns hook and policy lifetime; lock-free readers fail open |
| `BodyAccess` | Complete hknp body/motion snapshots and collision-object body resolution | Read guard or explicit scene/world witnesses; copied results |
| `WorldAccess` | TLS-aware read/write epochs and native mark/unmark RAII | Guard owns one synchronous epoch on the current thread |
| `PhysicsApi` | Filters, deferred velocity/transform, keyframe, activation, flags, mass, impulse, hard-keyframe | Requires matching `WorldWriteGuard`; borrowed body IDs |
| `PhysicsScale` | Game/Havok conversion snapshots | Atomic cached value; VR player scale remains distinct |
| `PhysicsTiming` | FO4VR substep/timestep sampling | Copied globals; no listener ownership |
| `Shape` | Sphere, convex, static compound, dynamic compound | Move-only Havok refs; dynamic updates require write guard |
| `GeneratedBody` | Static/dynamic/keyframed Bethesda collider graphs | Move-only body plus owner-thread service and eight-step retirement |
| `Constraint` | Ball/socket, limited hinge, prismatic, position motor | Move-only service ownership; removal under write guard; bounded queue |
| `HookPatch` | Exact-target E8 and vtable patch transactions | Caller owns quiescence/trampoline lifetime; rollback on partial commit |
| `WorldQuery` | Bethesda closest ray and fixed-capacity hknp shape casts | Synchronous copied hits; shape casts require read guard |
| `Ragdoll` | Retained graph-manager leases, checked driver/world/body/constraint views, add/remove | Graph lease and world-write guard make lifetimes explicit |
| `Impact` | Impact damage, point impulse, native HitData delivery | Game-thread calls; collision-object ref balanced; impulses require write epoch |
| `ScenePhysics` | Recursive scene motion/collision and body gravity operations | Owner thread outside physics or self-contained world guard as documented |
| `MovementController` | Checked controller inspection, native transitions, planner yaw | Owner-thread synchronous calls; controller identity rechecked |
| `NativeReference` | Havok, Bethesda, and full-width intrusive ref domains | Separate APIs; move-only full-width request owner |
| `CharacterController` | Borrowed controller snapshots and native world-manager insertion | Owner thread outside physics; generation rechecked after insertion |
| `ActorState` | Corrected VR knock/life/movement-authority reads and narrow clears | Owner-thread borrowed actor; high-level arbitration excluded |
| `Scene` | Transform math, object virtual commands, hierarchy attach/detach | Borrowed commands; hierarchy requires live caller refs and checks parent |
| `RootPose` | Skeleton copy/map/scale/local-world, body anchors, bias/delta math | Callback-scoped borrowed driver data; output copied to caller storage |
| `Audio` | Listener distance, followed playback, volume, fade lifecycle | Explicit move-only native handle; caller releases on game thread |
| `ActorPathing` | State, package-loop, direct-movement, current retained request queries | Owner thread outside physics; exact engine-retained request adopted |
| `PointLight` | Native light creation/configuration, renderer registration, hierarchy, placement, teardown | Move-only creation-thread owner; detach/unregister/release ordering |

## Cataloged but not yet public mechanics

These symbols are preserved in the address layer so consumers do not need to
rediscover them, but no stable wrapper claims their ownership contract yet.

| Area | Preserved evidence | Missing stable boundary |
| --- | --- | --- |
| Animation preharvest | Holder/resource/subgraph/HKX/binding/skeleton functions and layouts from PAPER/Toolkit | One shared loader/sampler job owner, cancellation, resource teardown, residency tests |
| Passive clip telemetry | Clip activate/update/deactivate functions, graph/binding/spline layouts | Single process hook owner, callback quiescence, bounded publication API |
| Physics contact bridge | Signal subscription and contact extraction functions | Process-lifetime subscription owner and decoded copied event ABI |
| Generated body drive | Low-level hard-keyframe and body mutation primitives are public | Producer/substep coordinator with bounded target publication and readback |
| Scene geometry/effects | Tri-shape and effect-property factories | Owned geometry/property graph, material lifetime, renderer update contract |
| Interactive objects | Open/close dispatcher, callsites, activate source, open-state getter | Shared observer hook with deterministic registration and quiescence |
| Camera/stereo | Cube-camera, frustum, stereo-submit functions and submit callsites | D3D state ownership, render-target lifetime, stereo hook chaining |
| Actor path submission | Goal/path/controller functions and request layouts | RAII request builder, retained target/process restore transaction |
| Character physics hooks | Step/apply slots, manager functions, push-away | Shared hook owner and stale-controller generation protection |
| Global collision matrix | Matrix/filter manager layouts | Scoped world/phase owner plus restore generation across consumers |
| Collision audio global | Minimum-velocity global | Cross-plugin scoped restore owner |

Standalone aliases removed in `a92d0d5` are supplied by their shared-library
owners. Physics-step listener registration is available through CommonLibF4VR;
RPS still owns timing snapshots but exposes no listener service. CommonFramework
supplies the removed main-loop and primary-draw aliases. RPS's generic checked
hook primitives remain available. See the
[current reference](../../../Docs/RPS_Framework/docs/CurrentReference.md)
for the catalog boundary and complete public migration table.

## Intentionally project-owned behavior

The following are not framework omissions. They are product policy or large
state machines whose ownership belongs to their current project.

- ROCK hand, weapon, body-collider descriptors, grab acquisition, pull/throw,
  collision reaction, visual/IK authority, and contact/cooldown policy.
- PAPER reload observation/control, Tactical Reload contract, weapon-motion
  intelligence, and ROCK visual-authority coordination.
- PAPER_Toolkit authoring UI, motion libraries, learning, projection, drive
  sandbox, and shell-ejection policy.
- SCISSORS actor-ragdoll and active-ragdoll runtime state machines, controller
  repair, direct stepping, deferred maintenance, and behavioral arbitration.
- Addon inventory, backpack, scope, camera composition, laser presentation,
  interactive mechanism semantics, persistence, and compatibility policy.

## Experimental or withheld native paths

- Direct ragdoll constraint/pivot mutation: restoration identity is understood,
  but a general mutation token and integrated runtime tests are still required.
- Live world-from-model compensation: shipped evidence includes actor feedback
  and cross-space yaw artifacts; root sampling/math is public, mutation is not.
- Graph active-index `+0xD8`: single-witness diagnostic evidence only.
- Custom six-axis ROCK grab constraint vtable/shellcode construction: useful,
  but not yet suitable for generic multi-consumer ownership.
- Toolkit shell ejection and several legacy REL-ID-only paths: address evidence
  is weaker than the framework's callable boundary standard.
