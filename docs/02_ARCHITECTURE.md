# Architecture

Date: 2026-08-11

## Package boundaries

`RPS::Addresses` is a static, dependency-free library. Its enum and record table
are generated from one versioned `.inc` source. Every record includes symbol
kind, subsystem, source project, evidence class, runtime version, and RVA. It
never stores absolute VAs.

Updated 2026-09-11 for `a92d0d5`: standalone aliases already supplied by
CommonLibF4VR or F4VR-CommonFramework are omitted. Local bindings used by RPS
checked operations remain when ABI, layout, epoch, ownership or failure
semantics differ. Removing catalog-only aliases adds no shared-library build
dependency. Current migration guidance and verification scope are recorded in
[CurrentReference](../../../Docs/RPS_Framework/docs/CurrentReference.md).

`RPS::Runtime` depends on `RPS::Addresses`. It validates that the current module
is the x64 `Fallout4VR.exe` file version 1.2.72.0 and refuses resolution on any
other executable or version. Checked byte-pattern validation is available for
wrappers and hook owners that carry live signature gates.

The first runtime modules are:

- multi-region Windows memory access validation plus SEH-guarded copy;
- checked animation generator-output track views with distinct active-read,
  inactive-storage-read, and mutable-storage access;
- allocation-free pose/world-from-model copy and blend operations plus checked
  drive-to-pose keyframed/powered motor-control mutation;
- pure collision-filter and symmetric matrix primitives;
- complete-record hknp body/motion snapshots;
- TLS-aware world read/write epochs with tri-state ownership and RAII marking;
- FO4VR scale and substep timing snapshots;
- checked native body mutation calls that require an owning write guard.
- owned convex/static/dynamic compound shapes backed by the FO4VR Havok
  allocator and reference-count domain.
- a generated-body ownership service that creates complete Bethesda wrapper
  graphs and separates owner-thread world removal from post-solve deferred
  native-object release.
- stock ball-and-socket, limited-hinge, and prismatic builders with complete
  Havok reference balancing, plus a move-only constraint handle and bounded
  owner-thread retirement service.
- detached position-motor allocation using ROCK's proven FO4VR native layout.
- exact-target direct-call and vtable patch transactions with page-protection
  restoration, instruction-cache flushing, and partial-commit rollback.
- synchronized Bethesda closest-ray queries and explicitly read-guarded hknp
  shape casts with fixed-capacity copied results and no hot-path allocation.
- retained graph-manager leases, bounded graph locks, checked native ragdoll
  views, and world-write-guarded add/remove manager operations.
- game-thread native impact-damage and full HitData delivery with explicit
  collision-object reference ownership and observable cleanup outcomes.
- owner-thread recursive scene motion/collision commands for complete
  multi-child object subtrees.
- checked per-body gravity snapshots and self-locking native writes that reject
  physics-step re-entry and verify results under separate read epochs.
- copied movement-controller inspection plus owner-thread native state
  transitions and planner-yaw commands with controller-generation rechecks.
- move-only ownership for full-width native intrusive references, kept separate
  from Havok and Bethesda packed reference-word domains.
- borrowed actor character-controller snapshots plus the engine's self-locking,
  deduplicating world insertion command, with generation verification after the
  call and no unsynchronized manager-list access.
- corrected FO4VR actor-state snapshots, live-vtable knock-state access, and
  narrow owner-thread movement-authority bit clears with post-write identity
  checks; high-level active-ragdoll arbitration remains consumer policy.
- consumer-owned, fixed-capacity collision-pair suppression snapshots with
  serialized writers, lock-free physics readers, exact entry-signature
  metadata, and fail-open behavior during publication.
- guarded collision-object to hknp body-ID resolution using mandatory scene
  owner/world witnesses, complete native-record copies, bounds checks, and a
  second-generation read before success.
- borrowed `NiAVObject` material/cull/bound commands with live executable-slot
  validation, checked NiNode attach/detach with temporary references and exact
  parent postconditions, plus world/parent transform conversion that rejects
  malformed rotation and scale inputs.
- callback-scoped root-pose primitives for bounded skeleton copying, high/low
  mapping, pose/world scale conversion, local-to-world expansion, and native
  body-anchor reads, paired with pure root-bias and delta math; live WFM
  mutation remains outside the stable core.
- owner-thread audio primitives for VR listener distance, complete followed-
  descriptor playback, volume/fade control, and explicit move-only eight-byte
  native handle ownership without addon-specific descriptor-selection policy.
- bounded owner-thread actor-pathing queries for engine state, package-loop
  permission, direct movement intent, and move-only adoption of the engine's
  retained current-request reference; mutation and repair remain consumer
  policy.
- owned non-shadow FO4VR point lights with exact factory/vtable/manager checks,
  configurable native light fields, hierarchy-aware parent retention, checked
  world-space placement with rollback, and ordered renderer teardown without
  emitter-specific presentation policy.

The function layer will grow by coherent modules. A module is admitted only
when its address/layout inputs, ownership model, thread/physics epoch, failure
behavior, cleanup path, and tests are explicit.

## Ownership rules

- Engine pointers are borrowed unless a type explicitly states a retained ref.
- Body and constraint lifetimes must be represented by owned handles/services;
  explicit destruction without a retirement service is not a complete public API.
- World mutation requires an explicit physics-write epoch or native write
  guard. Self-locking engine commands must instead reject active physics-step
  context and must never be nested inside that guard.
- Process-lifetime native listener slots use one shared service, not one listener
  per consumer.
- Hooks have a single transactional owner, exact-target/signature preflight,
  rollback on partial install, and deterministic restore when safe.
- Collision-pair policy objects publish immutable data but never claim the
  engine detour; the consumer that owns hook quiescence also owns policy
  lifetime and must call the vanilla comparison before suppression evaluation.
- No exceptions cross the engine/plugin boundary.

## Build enforcement

- Raw engine-sized constants are forbidden outside the address/layout catalogs.
- CommonLib, REL, and F4SE headers are forbidden in the dependency-free foundation.
- F4SE loader-version and executable-version domains may not be conflated.
- The catalog may not silently shrink below its enforced minimum.
- Public mechanics have deterministic tests and source-boundary tests.

`custom-fast` builds Release binaries and auto-deploys headers and static
libraries to `build-fast/stage`. It is a development stage, not a project release.
