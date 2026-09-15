# Server generic spatial authority bridge (Transform/Velocity as the spatial store)

- EST timestamp: 2026-09-15 10:42:04 EDT (UTC 2026-09-15T14:42:04Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + `--server-spatial-authority-selftest`
  9/9 + full suite 24/24)

## 1. Concurrent movement-state audit (phase 1)
The concurrent agent's phase 1 (uncommitted) is the **local-player** movement
ability state:
- `MovementRuntimeStateComponent` created once in `Ecs::setMovementIntent`
  (`actor-entities.cpp`).
- `movement-system.cpp` (hot `movement.main`, local player) reads/writes it
  through the generic component bridge instead of DLL globals (`gGrounded`,
  `gDashCooldown`, `gFreezePrev`).
- `live-behavior.cpp` exposes `GAME_COMPONENT_MOVEMENT_RUNTIME_STATE`.
This is **not** the server authoritative typed→generic flip, so this pass does
not duplicate it. Their files were left untouched.

## 2. Authority invariant (implemented)
For the migrated actor path, generic `TransformComponent`/`VelocityComponent`
are the persistent spatial store; typed fields are projections refreshed through
the bridge:
- `serverProjectActorSpatialFromGeneric(entity)`: generic -> typed player/NPC.
- `serverProjectActorSpatialToGeneric(entity)`: typed -> generic (single writer).

## 3. Player movement bridge
- `simulatePlayer` refreshes the typed actor from generic at the top when generic
  state exists; otherwise seeds generic from typed on first sight. A scope guard
  projects typed back to generic + health on **every** return path.
- Every authoritative transform assignment (`beginAuthoritativeTransform`:
  join/respawn/teleport/rewrite/reconnect) now writes generic immediately, so
  the next movement tick honors it.

## 4. NPC movement bridge
- `applyLiveActorBehavior` refreshes `npc.body.pos/yaw/vel/externalImpulse` from
  the generic Transform/Velocity before the hot actor decision, then projects.
  The existing `server-npcs.cpp` mirror already writes generic.

## 5. Mechanism vs policy / hot movement algorithm
- **Not done this pass.** The movement *algorithm* (acceleration, air accel,
  friction, jump, gravity, dash, down-dash, freeze, collision response) is still
  owned by the cold typed server pipeline. Separating cold physics
  (`physics.move`/`moveCapsule`) from hot velocity policy and migrating one real
  algorithm function (e.g. air acceleration) into a hot gameplay system is the
  next movement work. No algorithm hot-edit proof was run.

## 6. Actual function hot-edit proof
Not run / not claimed. `LIVE MOVEMENT HOT-EDIT PROVEN: no`.

## 7. Server + client shared movement logic audit
- Server movement owner: `server-players.cpp` `simulatePlayer` +
  `physics/movement/*` (`applyPreCollisionBasicMovement`,
  `applySpecialMovementPreCollision`, `applyPostCollisionMovementWithSpecials`,
  `resolveWorldCollision`).
- Client prediction owner: hot `movement-system.cpp` (`movement.main`) + the
  client's `simulate` path.
- Shared: `physics/movement/*` step functions and `MovementState`.
- Divergent branches: server uses validated-report re-sim with typed
  `ServerPlayer.movement`; client uses the hot local movement override. Unifying
  to one logical generation is the next large owner (prediction).

## 8. Rewind / history bridge
- Not migrated. `pushPositionHistory` still samples typed broadcast positions.
  Documented as a remaining typed history bridge; the rewind algorithm is
  unchanged.

## 9. Snapshot projection cleanup
- Relevance candidates already read generic Transform (previous pass). The
  snapshot payload still carries typed positions (client interpolation
  unchanged); both derive from the same generic authority after this bridge, so
  there is no longer an alternate spatial truth on the migrated path.

## 10. Relevance policy held steady
- No new relevance features added, per instruction.

## 11. Tests
`--server-spatial-authority-selftest` PASS 9/9: player generic->typed mirror;
typed->generic projection; `actor.spawn` and movement share the same components;
NPC generic->typed mirror; generic receives NPC movement; destroyed actor not
projected; deterministic projection. Full suite 24/24.

## 12. Concurrency
No collision: the phase-1 local movement component work is separate; this pass
adds only the server typed<->generic bridge and consumes the existing component
bridge.

## Status labels
- SELFTEST PROVEN: generic<->typed spatial bridge (player + NPC), spawn/teleport
  and movement share the same generic components, destroyed-entity exclusion,
  deterministic projection.
- COMPILED INTEGRATION: `simulatePlayer` reads-early/projects-late;
  `applyLiveActorBehavior` NPC read-bridge; `beginAuthoritativeTransform` writes
  generic; full suite 24/24.
- LIVE MULTIPLAYER PROVEN: no.
- LIVE MOVEMENT HOT-EDIT PROVEN: no.
- HUMAN VERIFICATION NEEDED: in-game movement parity (generic authority
  round-trip) on real clients.

## Honest limits (success-bar gaps)
- The per-tick movement **integration** still runs on the typed working copy
  (`p.pos/vel`, `npc.body`); generic is refreshed from it at the end. This is a
  read-early/write-late bridge, not an algorithm that mutates generic
  internally. Full generic-first integration requires converting
  `resolveWorldCollision` and the movement step functions.
- Rewind/history still samples typed broadcast positions (items 4).
- The server movement **algorithm** is not hot-owned and no algorithm hot-edit
  was proven (items 7/8).
- Client prediction/interpolation/reconciliation untouched.

## Files changed
`src/network/server-context.h`, `src/network/server-gamemode.cpp`
(spatial bridge helpers), `src/network/server-players.cpp` (player bridge),
`src/npc/npc.cpp` (NPC read-bridge), `src/network/server-packets.cpp`
(authoritative transform writes generic),
`src/network/server-spatial-authority-selftest.{h,cpp}` (new),
`src/game/game-cli.cpp`; docs + this changelog.

## Next (auto-selected)
Convert the movement integrator to mutate generic Transform/Velocity internally
(typed as projection only), migrate one real movement algorithm function into a
hot gameplay system with a hot-edit proof, and make rewind/history sample
generic Transform. Then client prediction/interpolation/reconciliation.
