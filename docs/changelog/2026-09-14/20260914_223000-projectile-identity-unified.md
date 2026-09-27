# Projectile client identity unified on the replicated authoritative EntityId

Date: 2026-09-14 22:30 EST (UTC 2026-09-15T02:30:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW`

## 1. Identity architecture before

- Authoritative: server hot tool -> `entityCreate` (`createGeneric`) -> writes
  `HotProjectileState` + `PresentationState`; generic replication ships a CREATE
  with the packed EntityId (`EntityRegistry::adopt`) plus dynamic blobs/edges.
  Typed Transform/Velocity are NOT replicated.
- Bridge: `PresentationEntities::ensure()` created a separate
  `ClientReplicated/Projectile/projectileId` entity for every network projectile,
  i.e. a parallel identity to the replicated entity (future duplicate risk).
- `networkProjectiles` (uint32 `projectileId`) is the prediction/legacy channel
  and carries no EntityId.

## 2. Authoritative EntityId mapping

No new packet/field was required. The authoritative entity already carries
`HotProjectileState`; `PresentationEntities::projectReplicatedProjectiles()`
enumerates that component and projects `position`/`velocity` onto the same real
EntityId's typed Transform/Velocity. The entity's own replicated
`PresentationState` drives the draw. Generic and weapon-agnostic.

## 3. Prediction -> authority identity flow

Unchanged prediction math. The provisional predicted entity is created only for
`projectile.predicted`; on adoption/termination it retires by mark/sweep. The
explicit association key (`PredictedEntityState`-style) that would retire the
provisional entity exactly when the authoritative entity appears is NOT yet
implemented (documented).

## 4. Component ownership after

- `HotProjectileState` (dynamic, replicated): canonical simulation state.
- `PresentationState` (dynamic, replicated): canonical presentation description.
- Transform/Velocity: client-side projection of replicated state (temporary,
  acceptable; not dual authority).
- `networkProjectiles`: networking/prediction mechanism + typed compatibility
  fields; no longer a presentation identity owner for remote projectiles.

## 5. PresentationEntities fate

Thin prediction adapter only. It no longer creates an entity for remote /
non-predicted projectiles (success bar 2). Not deleted (predicted path uses it).

## 6. Double-entity / double-draw proof

Remote: exactly one client entity per authoritative projectile (the replicated
EntityId); the hot system draws it once. The selftest asserts the authoritative
entity gains Transform via projection and produces a mesh submission. The typed
drawer is bypassed for migrated multiplayer projectiles.

## 7. Join-in-progress proof

Projection requires only replicated state (no prediction history); the selftest
creates a projectile with `HotProjectileState` + `PresentationState` and no typed
Transform and proves it presents. Late-join materialization is therefore
COMPILED INTEGRATION over a SELFTEST PROVEN projection.

## 8. Stale / destroy proof

`dynamicReplicationApply` DESTROY (`EntityRegistry::destroy`) purges the shell
and its components; projection skips non-alive entities, so a stale update cannot
recreate presentation. The bridge mark/sweep retires untouched predicted
entities.

## 9. Compatibility paths remaining

Single-player `weapon-system.cpp` rocket/grenade, `npc-combat.cpp` NPC rocket,
and the replay rocket path remain typed (`projectile-render.cpp` fallback).
Predicted->authoritative explicit association remains.

## 10. GLB

Not reached this pass.

## Evidence

- `python build_agent.py` -> `Status: SUCCESS`.
- `--hot-combat-selftest` -> PASS incl. "replicated projectile projects onto the
  authoritative entity" and "authoritative replicated projectile presents via its
  own EntityId".
- Full suite PASS.

## Classification

- SELFTEST PROVEN: replicated-state -> authoritative-entity projection; hot
  presentation; bridge predicted materialize/suppress/retire; provider checks.
- COMPILED INTEGRATION: client render-tick projection call; multiplayer generic
  handoff.
- LIVE VISUAL PROVEN: none.
- HUMAN VERIFICATION NEEDED: two clients (predicted->authority no duplicate,
  remote visibility, death), join-in-progress, live presentation edit.

## Files changed

`src/render/presentation-entities.{h,cpp}`, `src/network/multiplayer-projectiles.cpp`,
`src/engine/engine-tick-render.cpp`, `src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.

## Next cold owner selected

Generic predicted->authoritative entity association key (reusable for predicted
items/effects), then GLB loading through `PresentationResourceProvider`, then the
hot HUD widget tree.
