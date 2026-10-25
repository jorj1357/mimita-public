# Payload NPC replication, team spawns, and route authoring

- **Time UTC:** `2026-10-25T15:29:08Z`
- **Display time:** `2026-10-25 11:29:08 EDT` (`America/New_York`)
- **Branch:** `20261011zt`
- **HEAD:** `0f33025c`
- **Result:** `PASS_WITH_HUMAN_REVIEW`

## Request

Investigate the Payload handoff: why NPCs were invisible, whether the code
matches the specification, and how to make Payload route/checkpoint authoring
work. Human decisions:

1. Terminal commands and editor labels first; GUI property panel later.
2. Add validation so the human re-authors the route, do not auto-fix map data.
3. Make `radius` and `waitTicks` real gameplay inputs.

## Root causes found

### Roster NPC ids collided with player ids

`buildObjectiveRoster` only reserved `100000+` when `npcs` was empty. A normal
GUI server starts with one startup NPC, so roster ids began at `0`. A roster NPC
with id `1` collided with the host player id. The client rejects a whole
snapshot when entity ids repeat (`src/network/snapshot-chunks.cpp`,
`duplicate-entity`), so roster NPCs never replicated. The old cleanup only
erased ids `>=100000`, so those low ids also accumulated every rebuild.

### Payload used the shared spawn strategy

`config/gamemodes/payload.json` had no `spawn_strategy`. `Gamemode` defaults to
`"shared"`, which returns the single map anchor before team spawn groups are
read. Both teams spawned at the untagged `spawnpoint_1`, fought, died, and
respawned there.

## Files changed

### `src/network/server-gamemode.h`

- Added `std::unordered_set<uint32_t> objectiveRosterNpcIds;` beside
  `roundNextNpcId` so a rebuild retires exactly its own roster.

### `src/network/server-gamemode.cpp`

- `buildObjectiveRoster`:
  - objective roster ids now always seed at `kObjectiveRosterNpcIdBase = 100000`,
    regardless of startup/map NPCs
  - allocation skips player ids, existing NPC ids, `matchTeams`, and
    `matchActors`
  - tracked roster ids are erased with their match/participant/score state
  - `roundNextNpcId` resets to the base after cleanup so ids are reused instead
    of growing every round
  - emits `npc.roster-id.allocated` with actor, team, base, next id, and player
    collision flag
- `gamemodeSpawnPoint` split into `resolveGamemodeSpawnPoint` plus a thin
  wrapper. The resolver:
  - returns a `SpawnResolutionInfo` with group, tag filter, candidate count, and
    fallback reason
  - refuses untagged authored points when a team declares spawn tagging
  - records `spawn-strategy-shared`, `authored-team-group`, `team-cluster`,
    `team-anchor-a/b`, or `neutral-anchor`
- `gamemodeSpawnPointForActor` emits `gamemode.spawn-resolved` with actor, role,
  team, group, tag filter, candidate count, fallback, position, reason, mode, and
  map.
- `assignGamemodeSpawns` emits `gamemode.spawn-groups-resolved` with both team
  cluster counts, positions, separation, and resolution state.
- Payload init emits `payload.authoring-validated` with route/cart warnings.
- Payload tick logs `payload.cart-wait` when an authored pause starts and reports
  wait state `5` in `payload.cart-state`.
- `serverSpawnTagSelfTest` now asserts Payload is not `shared` and both Payload
  maps contain enabled `attacker_spawn`/`defender_spawn` entities.

### `config/gamemodes/payload.json`

- Added `"spawn_strategy": "team_shared"`.

### `src/gamemode/map-config.h` / `map-config.cpp`

- Added `PayloadAuthoringIssue`, `validatePayloadAuthoring`, and
  `payloadAuthoringSelfTest`.
- Validation reports duplicate/empty ids, <2 enabled points, zero route length,
  cart-origin/route-start mismatch, segments >250 m, vertical jumps >150 m,
  implicit final, zero checkpoint bonuses, tiny radii, and authored wait points.
  Warnings keep the map playable; errors set `valid=false`.
- `payloadAuthoringSelfTest` loads the real `payload` mode and `payload1`/
  `payload2` maps, checks spawn strategy/group/tag consistency, and prints route
  warnings.

### `src/gamemode/payload-runtime.h` / `payload-runtime.cpp`

- `PayloadCheckpoint` now carries `radius` and `waitTicks`.
- Added `PayloadWaypoint` (route index, distance, radius, wait ticks, reached)
  for every enabled route point.
- Added `waitTicksRemaining` / `waiting` runtime state and `waitStarted` /
  `waitTicksRemaining` tick results.
- Checkpoint/waypoint activation now uses the authored arrival sphere in cart
  world space instead of only crossing the authored distance.
- `waitTicks` holds the cart's distance for the authored tick count. Overtime
  presence still refreshes while the cart is paused.
- Rollback clamp records the cart's actual trigger distance, not a forward snap
  to the authored point distance.
- Selftest covers early radius activation and a `waitTicks` hold.

### `src/network/server-packets.cpp`

- Snapshot build now claims each entity id once. Duplicate ids are dropped and
  logged as `server.snapshot.duplicate-entity` with duplicate count, first id,
  type, and snapshot entity count.

### `src/network/multiplayer-tick.cpp`

- Chunk reassembly now passes the error string and logs
  `client.snapshot.reassemble-failed` with tick, chunk count, entity count, and
  reason.

### `src/render/render-player.cpp`

- Added one-time `client.actor.draw-submission` evidence per remote actor:
  alive, model loaded, avatar, valid parts/batches, fallback batches, tint,
  position, camera distance. This distinguishes a replication problem from a
  draw/tint problem.

### `src/engine/engine-tick-render.cpp`

- `client.npc.viewport-window` now reports `dead` and `sample_npc_alive` in
  addition to `alive`, `model_loaded`, `in_view`, map, camera, and sample NPC.

### `src/mapeditor/map-editor.h` / `map-editor.cpp`

- Added selected-route authoring methods:
  `setSelectedRouteCheckpoint`, `setSelectedRouteFinal`,
  `setSelectedRouteEnabled`, `setSelectedRouteRadius`,
  `setSelectedRouteTimeBonus`, `setSelectedRouteWaitTicks`,
  `moveSelectedRoute`, and `validatePayloadRoute`.
- `setSelectedRouteFinal(true)` clears other final flags.
- Route labels now show `CP`, `FINAL`, `OFF`, radius, wait ticks, and time bonus.
  Colors distinguish normal, checkpoint, final, disabled, and selected points.
- Disabled route points remain visible, pickable, and listed in the outliner so
  they can be re-enabled.
- `save()` runs validation and logs `map-payload-route.validated`.

### `src/mapeditor/map-editor-commands.cpp`

- Added `mapeditor route_checkpoint/final/enabled`, `route_radius`,
  `route_bonus`, `route_wait`, `route_move`, and `route_validate`.
- Updated command usage and help text.

### `src/game/game-cli.cpp`

- Added `--payload-authoring-selftest`.

### `devscripts/dev-launch-modes.json`

- Added mode `21` (Payload 1, `--npcs 1`) and mode `22` (Payload 2,
  `--npcs 1`) to reproduce and verify startup-NPC conditions.

### Documentation

- `docs/specs/gamemodes/payload.md`: updated spawn/invisibility status, route
  `radius`/`waitTicks` behavior, authoring/validation commands, verification
  commands, next todos, and acceptance status.
- `docs/specs/entity-editor/entity-editor.md`: documented route flags, labels,
  validation, and radius/wait semantics.
- `docs/regressions/2026-10-25/payload-npc-replication-and-spawn-collapse-REG.md`:
  append-only regression record.

## Validation

- Canonical dev-loop build: `3197` (runtime), `3212` (final source build).\n- `3212` identity probe: `EVENTS_JSONL_PATH=logs/10-25-2026/20261025_113920/events.jsonl`.
- Selftests on `3212`:
  - `--payload-selftest` PASS
  - `--payload-authoring-selftest` PASS (warns about `r1`/`r2` long/steep
    segments in `payload1` and `payload2`)
  - `--map-config-selftest` PASS
- Runtime journal: `logs/2026-10-25/20261025_151724/events.jsonl`
  (mode 21, Payload 1, one startup NPC, build 3197):
  - roster ids `100000..100022`; 46 allocations across warmup + countdown, 46
    unique, zero duplicate allocations
  - no `server.snapshot.duplicate-entity`
  - no `client.snapshot.reassemble-failed`
  - `gamemode.spawn-groups-resolved`: team A `(-312.59,-28.12,1150.36)`, team B
    `(-292.56,-64.83,2256.30)`, separation `1106.74`
  - `gamemode.spawn-resolved`: `candidate_count=1`,
    `fallback=authored-team-group`
  - `client.npc.viewport-window`: `remote_npcs=24`, `alive=24`, `dead=0`,
    `model_loaded=24`, `in_view` rising to `10`
- Human observation: all NPCs spawned and were visible; continue past this.
- `mapeditor route_validate` was sent through the real dev-loop command queue
  and completed successfully (`terminal.command.completed`, success=true).
- `--spawn-tag-selftest` was not used as Payload evidence because it has a
  pre-existing unrelated failure (`zombie_tower max_count`) in the current
  working tree.

## Focused review

- `docs/skills/spec-behavior-review-v1.md`: `PASS_WITH_HUMAN_REVIEW`. The
  specification said fixed team spawns and visible NPCs; the code now follows
  that for the tested Payload 1 case. Payload 2 content is still copied and not
  authored.
- `docs/skills/logging-checker-v1.md`: `PASS`. New diagnostics are owned by the
  spawn resolver, roster builder, snapshot builder/reassembler, and renderer.
  Repeated events are bounded: roster id events per allocation, spawn events per
  spawn, duplicate/reassembly events rate-limited to 1 s, draw evidence once per
  actor, viewport evidence 1 s.

## Human review still needed

- Re-author `payload1` and `payload2` route points using the new commands/labels.
  The validator currently flags `r1` and `r2` as ~816 m and ~786 m segments with
  ~800 m vertical jumps.
- Playtest `radius` and `waitTicks` values; the defaults are now real but no
  authored map uses `waitTicks > 0`.
- Playtest the full Payload sequence, remote damage numbers, decal opacity, and
  ragdolls.
- Review `payload2` content against its actual geometry.

## Pre-existing working-tree edits

Preserved all unrelated edits that were present before this session, including
`config/accounts/default.json`, `config/analytics.json`, `devscripts/dev-loop.py`,
`docs/specs/dev-loop/dev-loop.md`, `docs/specs/gamemodes/zombie-tower/zombie-tower.md`,
`docs/specs/open-gameplay-issues.md`, `docs/specs/replay/replay.md`, and the
other modified/untracked files shown by `git status`.
