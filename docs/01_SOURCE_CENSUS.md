# Source census

Date: 2026-08-11

## Repositories inspected

| Repository | Branch/state at census | Authority used |
| --- | --- | --- |
| ROCK | `develop`, clean | Native physics, shapes, generated bodies, queries, scale/timing, hooks |
| PAPER | `experiment/tactical-reload-bridge`, dirty | Current local native animation evidence and reload hook contracts; dirty feature work not copied |
| PAPER_Toolkit | `develop`, clean | Duplicate animation loader/telemetry and shell-eject evidence |
| SCISSORS | `develop`, clean/ahead | Ragdoll, collision, controller, constraints, damage/impulse, generator output |
| RPS_SDK | `master`, clean | Boundary only; provider SDK remains independent |
| ROCK_Addons | mixed child states | Scene, renderer, audio, interactive-object address evidence |

No source project was modified.

## Highest-value reusable mechanics

- Native memory readable/writable/executable range validation and guarded copy.
- Versioned RVA catalog, corrected layouts, vtables, hook/callsite identities,
  and source/evidence metadata.
- hknp body/motion lookup and checked body snapshots.
- Bethesda-native static, dynamic, and keyframed generated-body creation.
- Convex, static-compound, and dynamic-compound shape construction.
- Deferred eight-physics-step retirement for bodies and constraints.
- Stock constraint constructors and position-motor construction.
- Generated keyframed/dynamic target drive and hard-keyframe computation.
- World query locks, write scopes, ray/shape queries, and physics timing.
- Scale conversion that keeps game/Havok scale distinct from VR player scale.
- Checked animation generator-output, pose, and motor track views.
- Collision filter encoding, 64x64 matrix mechanics, and pair-policy primitives.
- Ragdoll graph/world access, controller transitions, gravity, hit/damage/impulse.
- Verified direct-call/vtable/entry-hook preflight and transactional rollback.
- Scene node, light, geometry/effect, renderer, stereo, audio, and interactive-object helpers.

## World-query evidence promoted

- `bhkWorld::PickObject` remains the synchronized closest-ray authority used by
  ROCK selection and its public provider raycast. Its supporting `bhkPickData`
  constructor, start/end setter, hit predicate, and fraction accessor resolve
  through the same local FO4VR relocation database already exercised by ROCK.
- Direct shape casts use ROCK's shipped `0x80` query record, query-filter chain,
  game-to-Havok conversion, and world read guard. Results are copied at the
  synchronous collector callback boundary; native result pointers never escape.
- The framework replaces ROCK's potentially growing all-hits array with an ABI-
  compatible fixed collector. It preserves the verified six-slot collector
  interface and `0x20` base layout while reporting overflow instead of allocating.
- Sphere queries receive owned sphere shapes from the same engine factory and
  Havok reference domain already used for ROCK selection shapes.

## Scene-object evidence promoted

- ROCK Addons consistently uses the FO4VR `NiAVObject` virtual slots `0x2E`,
  `0x30`, and `0x36` for material invalidation, application culling, and world-
  bound refresh. The framework exposes these as borrowed synchronous commands
  with live executable-slot validation and native-fault containment.
- ROCK and ROCK Emitters use the VR `NiNode` attach/detach virtuals at slots
  `0x3D` and `0x40`, with the child parent link at `+0x28`. The framework
  requires caller-owned live Ni references, temporarily retains both objects
  across the call, and verifies the exact parent postcondition afterward.
- ROCK Emitters' uniform-scale `NiTransform` world-to-parent conversion is
  promoted together with its derived inverse. Both directions validate finite
  values, nondegenerate scales, orthonormal rotations, and a positive unit
  determinant before using transpose as the rotation inverse.
- Long-lived node/reference ownership, renderer proxy registration, light
  lifetime, and addon-specific laser/emitter geometry remain outside this
  module. Those concerns require separate coherent ownership APIs.

## Root-pose evidence promoted

- SCISSORS' shipped high-to-low ragdoll pose mapper, pose scale conversion,
  transform scale conversion, local-to-world pose conversion, and native
  physics-interface resolver are promoted as exact-version native primitives.
- Low-skeleton parents are copied into consumer storage with the `+0x18`
  parent array and `+0x30` count contract, a 256-bone bound, hierarchy checks,
  and a second complete copy. Body anchors use the `+0x38` body-handle,
  `+0x50` low-skeleton, `+0x60` body-offset, and `+0x70` body-transform virtual
  slots with live executable-target and generation checks.
- Root-anchor offset rotation, translation-bias calibration/rejection, and root
  translation/yaw delta sampling are dependency-free value operations. Unlike
  SCISSORS' historical helper, an invalid quaternion fails explicitly instead
  of silently becoming identity.
- Live world-from-model mutation remains withheld. SCISSORS ships translation
  and rotation compensation disabled because actor feedback and cross-space
  yaw observations made it unsafe as a generic stable framework operation.

## Audio evidence promoted

- ROCK Interactive Objects' complete following-descriptor playback path is
  promoted with the VR-aware listener-distance helper, native descriptor/node
  binding helper, volume setter, and the handle fade-in/fade-out lifecycle.
- The native sound handle is exactly eight bytes: sound ID at `+0x00`, assumed-
  success flag at `+0x04`, and state byte at `+0x05`. Framework handles are
  move-transfer-only so one active registration cannot be copied into two
  apparent owners.
- Playback resolves the established audio-manager singleton, computes distance
  from an explicit finite world-space position, and refuses to overwrite an
  active handle. Descriptor selection from door/container/activator records and
  velocity-to-volume policy remain addon/consumer semantics.
- The minimum collision-sound-velocity global remains cataloged but is not
  exposed as an unscoped setter; safe use needs one explicit restore owner and
  cross-plugin coordination.

## Actor-pathing evidence promoted

- SCISSORS' exact-version actor queries for active pathing state, native
  package-loop permission, goal-submission availability, direct movement
  state/offset/angle, and the current path request are promoted as bounded
  owner-thread operations outside the physics step.
- The current-request query returns one retained full-width native intrusive
  reference. The framework adopts that exact reference into its existing
  move-only owner without adding a second reference; destruction or explicit
  transfer must remain on the engine-owning thread.
- Native query faults, missing executable targets, non-finite direct-movement
  output, invalid actors, and malformed retained requests fail distinctly.
- Goal construction/submission, pathing-flag writes, controller repair, package
  procedure telemetry, target restoration, and SCISSORS' active-ragdoll policy
  remain outside the stable query layer.

## Point-light evidence promoted

- ROCK Emitters' native FO4VR point-light factory, renderer registration and
  unregistration functions, scene-manager singleton, `0x1D0` object layout,
  and point-light vtable are promoted into one owned non-shadow-light API.
- Ambient, diffuse, specular, dimmer, and three attenuation fields use the
  complete VR offsets. Configuration preflights every field, writes dimmer
  last, and restores the copied prior values if any guarded write fails.
- The handle owns explicit Ni references for the light, renderer proxy, and any
  attached parent. Normal teardown is detach, unregister, proxy release, then
  light release. Creation captures the owner thread; parent identity and the
  live manager generation are checked.
- A native unregister fault makes registration state unknown and forbids retry;
  unsafe-context destruction intentionally leaks instead of risking renderer
  use-after-free or double unregister. Consumers should call `reset()` on the
  owning frame thread and inspect its result.
- World placement uses the verified local/world/previous-world transform fields
  and VR `UpdateWorldData` virtual. It derives local space from a copied parent
  transform, validates parent generation and the resulting world pose, updates
  bounds, and attempts a native local-transform rollback after any mutation-
  phase failure.
- Laser pose, color selection, intensity falloff, surface offset, and visibility
  policy remain ROCK Emitters behavior rather than framework defaults.

## Superseded or withheld paths

- `BodyCollisionControl` and much of `PhysicsUtils` are aliases over the real
  Havok runtime facade and will not be duplicated.
- The duplicate physics entry-trampoline installers need one transactional owner.
- `registerContactSignal()` is a stub and is not a capability.
- `setPointVelocity()` only writes linear velocity and is withheld under that name.
- The superseded dynamic-body keyframed initializer is forbidden. Non-static
  generated bodies use the verified `MotionCinfo` constructor at RVA `0x17A2FC0`.
- Direct constraint mutation, WFM compensation, and the single-witness graph
  active-index layout remain experimental rather than stable runtime APIs.

## Extraction order

1. Address/layout catalog, runtime identity, memory guards, pure checked views.
2. Physics/body/shape/constraint mechanics with explicit ownership and epochs.
3. Animation evidence loader and telemetry mechanics.
4. Ragdoll/controller/damage mechanics.
5. Shared hook ownership and opt-in scene/render/audio modules.
