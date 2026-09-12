// 09 12 2026
/* purpose
* Record the completed ragdoll live + network migration: the component solver
* is the single owner, driven by the editable ragdoll.solver domain, with
* entity-to-entity grabs, deterministic corpse spawns, and replication.
* this file DOES NOT define the solver math or replace hot-kernel.md
* this file DOES NOT replace the ragdoll specification
*/

# Ragdoll live + network (migration)

## Goal

Make the ragdoll entity/component state the single source of truth, run the
solver as one fixed domain at an editable rate, and replicate limbs, grabs, and
deterministic corpse deaths without a second authoritative body.

## Canonical ownership

One owner per concern:

- **Canonical dynamic state** — `Ragdoll::LimbComponent` / `JointComponent` /
  `GrabComponent` / `RagdollRootComponent` in the registry
  (`src/ragdoll/ragdoll-components.h`).
- **Canonical solve** — `Ragdoll::Solver::solveSubstep`
  (`src/ragdoll/ragdoll-solver.h/.cpp`) reads the components into a per-substep
  `RagdollBody` workspace, solves, and writes the dynamic state back.
- **Derived projection** — `RagdollBody` / `RagdollModePart` hold the static
  template (mesh mapping, anchors, rotation limits) and the render/skeleton
  pose. They are rebuilt from components via
  `RagdollEntities::syncToBody`, never solved directly.
- **Clock** — `Sim::DomainScheduler` domain `ragdoll.solver`.
- **`RagdollModeSystem`** — input/control mapping, body creation, camera,
  corpse lifecycle, and the render adapter only.

## Editable rate primitives

`config/ragdoll.json` (hot-reloadable via `RagdollModeConfig::pollReload()`):

- `solver_hz` (default `120`) — substeps/sec of the `ragdoll.solver` domain.
  `RagdollModeSystem::update` re-applies it to `DomainScheduler::add` every
  gameplay tick, so editing the file changes the rate live.
- `snapshot_send_interval_ticks` (default `3`) — client replication cadence.
- `replicate_corpses` (default `true`) — corpse-spawn replication.

The **60 Hz gameplay tick is untouched** (`MimitaNet::GAMEPLAY_SIMULATION_HZ`).
`update` applies input/look motors once per 60 Hz tick, then advances the
domain; the domain substeps the solver by `1/solver_hz` with catch-up clamped by
`SimulationDomain::maxCatchup`.

## Entity/component model

- `bind(owner, body)` — stable `RagdollLimb` entity per limb + root/grab.
- `bindLimbCount(owner, n)` — minimal limb set for a remote owner.
- `syncFromBody` / `syncToBody` — publish/load dynamic state.
- `setGrab` / `setGrabTarget` — hand state and entity-to-entity target.
- `solveParams` — asks the hot module for policy, else config base.
- `writeSnapshot` / `applySnapshot` — codec; `applySnapshot` reconstructs an
  unknown owner from the first snapshot and reuses ids afterwards.

## Hot solver policy

`GAME_EVENT_RAGDOLL_SOLVE` + `RagdollPolicyV1` (stiffness, damping, iterations,
gravity) dispatch through `LiveBehavior::dispatchRagdollPolicy`. Stiffness
scales the joint position beta, damping relaxes limb velocity per substep,
iterations and gravity feed the solver. Editing
`src/hot-reload/modules/rocket-behavior.cpp` changes policy live; no relink.

## Grabs as entity constraints

`GrabComponent` carries the hand, hand-local anchor, target entity
(`kInvalidLimb` = world anchor), strength, and world point. `Solver::solveGrabs`
resolves a same-body `targetPart` as a **two-body point constraint** (momentum
exchanged) and otherwise pins to the world point. Wire format:
`RagdollGrabStatePacket` inside `RagdollStatePacket`. `RagdollGrabState` is the
derived copy.

## Corpses

A corpse is a `RagdollBody` clone simulated by the same solve core at
`solver_hz/60` substeps. Its spawn is deterministic:

```
seed = splitmix64( FNV1a(worldSeed, actorId, ownerId, deathTick, deathEventId) )
```

The angular tumble streams from that seed (`seededSigned`), so every client that
knows the same death identity derives the same corpse. `PACKET_CORPSE_SPAWN`
(72) relays `owner / deathTick / deathEventId / impulse`; peers call
`noteNetworkDeath`, then `consumeNetworkDeath` when presenting the remote death
so the local `spawnCorpse` uses the shared identity.

## Config cleanup

`config/ragdolldeath.json` is never rewritten at runtime. The gamemode ragdoll
override is a runtime component on `RagdollDeathConfig`
(`setEnabledOverride` / `clearEnabledOverride`), cleared on
`CommunityMatchClient::reset`. `enabled()` prefers the override in memory.

## Telemetry and inspector

`MIMITA_TELEMETRY_SCOPE("RagdollSolverSubstep")` plus per-owner
`EntityCounters` (updates, contacts, tick) are recorded each substep;
replicated snapshots add `networkUpdates`/`networkBytes`. Creation-mode
inspection (`CreationMode::describe`) prints limb, joint, grab, and root fields.

## Verification

- `mimita.exe --ragdoll-slice-selftest`: limb entity ids distinct/stable,
  limb/joint components, grab state, entity-to-entity target + snapshot codec,
  solver policy read-back, `syncToBody`, remote reconstruction with stable ids,
  network death identity round trip, snapshot round-trip, DLL reload identity.
- `--live-code-selftest`, `--hot-authoritative-selftest`, `--entity-slice-selftest`.

## Related

- `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md`
- `docs/architecture/live-development/hot-kernel.md`
- `docs/architecture/live-development/live-authored-world.md`
