# Client projectile presentation bridge removed (generic path)

Date: 2026-09-14 22:10 EST (UTC 2026-09-15T02:10:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW`

## Scope

Client projectile presentation only. No server authority, gameplay.60,
prediction/interpolation math, or transport changes.

## 1. Client projectile identity map

- Predicted: `provisionalProjectileId = 0x80000000 | requestId`
  (`multiplayer-projectiles.cpp:127`), stored in `ctx.networkProjectiles`,
  `predicted=true`; reconciled to the authoritative `uint32 projectileId` by
  `fireSerial`/`requestId` via `adoptPredictedProjectile` (`:269`) /
  `mpProcessAttackResultPacket` (`:1519`).
- Network: server-assigned `uint32 projectileId` (`server-projectiles.cpp`),
  stored in `networkProjectiles`; no EntityId mapping existed.
- Generic lifecycle (`dynamic-replication.cpp`): `EntityRegistry::adopt` uses the
  exact server packed EntityId and replicates dynamic blobs/edges only, NOT typed
  Transform/Velocity.
- Replay: `gReplayRocketState` + `replayEventId` (`engine-tick-camera.cpp`).

## 2-3. Generic client materialization + predicted entity

New `src/render/presentation-entities.{h,cpp}`:
`resourcesForWeapon` (legacy network weapon id -> logical mesh/texture ids; data,
not a renderer branch), `ensure` (creates/updates a `ClientReplicated` +
`Projectile` entity with `Transform`, `Velocity`, and the `PresentationState`
dynamic component), `has` (typed suppression gate), and `beginSync`/`endSync`
mark-sweep retirement.

## 4. Reconciliation / no double draw

`mpRenderNetworkProjectiles` is the single client presentation site for network
and predicted projectiles. It materializes the generic entity per live projectile
and skips the typed `renderProjectile` whenever the bridge owns it, so exactly
one owner draws. No prediction math changed.

## 5. Network projectile presentation

The same loop handles remote/authoritative and local/predicted projectiles; both
now resolve to generic `PresentationState` and are drawn by
`hot.presentation-mesh` -> `render.mesh`.

## 6. Typed caller classification after migration

- MIGRATED: `multiplayer-projectiles.cpp` rocket/grenade (network + predicted).
- COMPATIBILITY: single-player `weapon-system.cpp` (rocket/grenade) and
  `npc-combat.cpp` (NPC rocket) — populated only when `mpContext` is inactive.
- COMPATIBILITY: replay rocket (`engine-tick-render.cpp:415`).
- DEAD (unchanged): `projectile-render.cpp::clearProjectileMeshes`,
  `effect-part-render.cpp` `replay_rocket` branch.
`projectile-render.cpp` is not deletable yet (serves the compatibility paths).

## 7. Double-draw falsification

Selftest asserts the bridge `has()` gate (the exact suppression condition) and
that materialize -> hot draw -> retire each behave. Full two-client visual
duplicate check was not run.

## 8. Destroy / retire

`beginSync`/`endSync` retires any presentation entity not touched this frame, and
`Ecs::despawn` purges its components; exploded/removed/stale projectiles are
skipped and retired, so no stale visual remains.

## 9. Replay

Documented as the remaining compatibility owner; not migrated this pass (would
need a transform/velocity/PresentationState materialization; deferred).

## 10. Live visual proof

Not run (no visible client). SELFTEST PROVEN chain; multiplayer handoff is
COMPILED INTEGRATION; LIVE VISUAL PROVEN = none.

## Evidence

- `python build_agent.py` -> `Status: SUCCESS`.
- `build_game_dll.py` -> `build/mimita-game.dll` (sources=25).
- `--hot-combat-selftest` -> PASS incl. bridge checks; full suite PASS.

## Classification

- SELFTEST PROVEN: bridge materialize/update/draw/suppress/retire; logical
  resource mapping; provider generation/no-op/swap/last-good.
- COMPILED INTEGRATION: `mpRenderNetworkProjectiles` generic handoff; rocket/
  grenade meshes+textures.
- LIVE VISUAL PROVEN: none.
- HUMAN VERIFICATION NEEDED: two clients, predicted->authority no-duplicate,
  remote visibility, clean death, live presentation edit.

## Files changed

`src/render/presentation-entities.{h,cpp}` (new),
`src/network/multiplayer-projectiles.cpp`, `src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.

## Next cold owner selected

GLB mesh loading through the existing `PresentationResourceProvider` (no separate
subsystem), then migrating single-player/replay projectile visuals, then the hot
HUD widget tree.
