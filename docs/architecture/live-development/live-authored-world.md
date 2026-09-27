// 09 12 2026
/* purpose
* Record the first live-authored-world fundamentals slice: batched held-fire
* intent, creation/inspection mode, the local authoring fork, and telemetry.
* this file DOES NOT define the ragdoll slice or replace hot-kernel.md
* this file DOES NOT replace the feature specification
*/

# Live authored world (fundamentals slice)

## Held-fire intent (batched high-rate action)

- `PACKET_FIRE_INTENT_REQUEST` carries START / STOP / HEARTBEAT with
  `intentId`, `weaponDefNetworkId`, `attackVariant`, `startTick`, `endTick`,
  `count`, `deterministicSeed`, and aim. One packet per window/heartbeat, never
  one per projectile.
- Client (`src/engine/engine-tick-combat.cpp`) opens a window on held fire for
  automatic projectile weapons, heartbeats every ~6 ticks, and closes on
  release/weapon change/disconnect.
- Server (`src/network/server-attack.cpp`) stores `HeldFireState` per player and
  `tickHeldFireIntents` simulates **one authoritative projectile per gameplay
  tick** via the existing `handleGenericProjectileAttack`, so each rocket keeps
  its own `nextProjectileId`, spawn tick, pose, collision, and damage history.
- Ammo is server-authoritative for this path; the window closes at zero.
- Each held tick dispatches `GAME_EVENT_FIRE_INTENT` to the hot gameplay module;
  editing `src/hot-reload/modules/rocket-behavior.cpp` changes the schedule live.
- The spy-knife batch should migrate to this START/STOP/HEARTBEAT model later.

## Creation mode and the local authoring fork

- `modecreate 0|1` (`src/terminal/creation-commands.cpp`) toggles
  `Editor::CreationMode`.
- Raycast picking reuses `selectWorldTriangle` / `castWorldRay`
  (`src/physics/ray-utils.*`); the picked triangle becomes a `WorldObject` entity
  (`EntityDomain::WorldObject`).
- `Editor::CreationMode::describe` emits terminal-first inspection text
  (entity id, kind, transform, base map hash, mesh/material hashes, components,
  active generation/hash, telemetry) — the same data a GUI can consume.
- Authoring operations (copy/paste/duplicate, move, rotate, scale, delete)
  record `PatchOp`s against the base map. The base asset is never rewritten;
  `forkHashFor(baseHash, patch)` is a deterministic hash of the whole fork.
- Journal events: `creation_mode_changed`, `entity_duplicated`,
  `entity_transform_changed`, `entity_deleted_from_fork`, with base/fork hashes.

## Telemetry

- `src/telemetry/` registry stores rolling per-scope aggregates (calls,
  calls/sec, inclusive/self/avg/max ns, last tick, generation, hash) and generic
  per-entity counters (updates, contacts, render submissions, network
  bytes/updates, last touched tick).
- `MIMITA_TELEMETRY_SCOPE` is the sampling primitive; `telemetry` /
  `telemetry_entity` terminal commands expose the same data the inspector and a
  future heat visualization will read.

## Non-destructive and non-authoritative

Local fork edits are not promoted to the authoritative world in this slice. The
data model keeps base hash, parent/fork hash, and process/session identity so
publish/merge/distributed cache/Blender export can be added later.

## Verification

- `mimita.exe --telemetry-selftest` (PASS standalone).
- `mimita.exe --creation-selftest`, `--project-selftest`, `--phase456-selftest`.
- `mimita.exe --hot-authoritative-selftest` now also checks the hot fire-intent
  policy dispatch.

## Related

- `docs/architecture/live-development/hot-kernel.md`
- `docs/architecture/live-development/project-layer.md`
- `docs/architecture/live-development/hot-kernel-next-steps.md`
