# Vehicle Arena Phase 0 + Map Editor Proof

**Session:** 2026-11-05 21:05 UTC
**Scope:** Vehicle Arena data/launch foundation and the first live map-editor proof
on `arena1`.

## Source changes

- Added `config/gamemodes/vehicle_arena.json` with 1v1 teams, map `arena1`,
  first-to-5, 300 s limit, and `vehicle_profile: "arcade_arena"`.
- Added `config/vehicle-profiles/arcade_arena.json` as data for the shared
  vehicle system. It is not consumed by the vehicle system yet.
- Added `vehicle_profile` to the gamemode masterlist, `Gamemode`, and parser.
  All gamemode JSON files were canonicalized with
  `devscripts/gamemode_json_sync.py --write`.
- Added `config/onlinemodes.json` entry mapping community mode `vehicle_arena`
  to gamemode `vehicle_arena`.
- Added dev launch mode 50 and `devscripts/dev-profiles/vehicle-arena.json`.
- Added `config/maps/arena1.json` with authored walls/goal backs, arena ball,
  goal, kickoff, and boost-pad data. The arena/boost-pad blocks are authored
  data for the next implementation slice; the current map-config parser loads
  the block geometry only.
- Added `assets/maps/arena1.glb` as a minimal generated base floor with two
  spawn markers (`spawn_blue`, `spawn_orange`).
- Updated `docs/specs/gamemodes/vehicle-arena.md`,
  `docs/specs/vehicles/vehicles.md`,
  `docs/specs/entity-editor/entity-editor.md`, and
  `docs/specs/dev-loop/dev-loop.md` for controls, boost pads, curves,
  triangle-only collision, launch identity, dashboard visibility, and AI
  command transcripts.
- Dev-loop status now resolves and prints the effective launch identity
  (`map`, `mode`, `gamemode`, `weapon_set`, `npcs`) from the actual server args.
- Dev-loop launch mode 50 declares expected identity values and the daemon can
  block a mismatched launch.

## Build evidence

- Canonical dev-loop build succeeded.
- Running artifact for run `20261105_210112`:
  `2026-11-05T21:00:56.411769Z`.
- A later artifact `2026-11-05T21:04:32.805138Z` was published after the
  documentation/dev-loop source edits. The gameplay process in this evidence
  had already been launched; this is a build-identity note, not a gameplay
  defect.

## Runtime evidence

Live run: `20261105_210112`, launch mode 50.

- Server journal confirms `map=arena1`, `mode=vehicle_arena`, `npcs=0`.
- `gamemode.match.configured` reports `goal_value=5`, two teams with capacity
  1 (`blue`, `orange`), `time_limit_seconds=300`,
  `win_condition=first_to_score`, `weapon_set_id=1`.
- `map.server-load-success` reports `spawn_points=2`; the server no longer
  rotates away from `arena1` because the base map has no spawn points.
- `server.process.heartbeat` remains `map=arena1`, `mode=vehicle_arena`,
  `npcs=0`, `players=2`.
- `--status` now prints:
  `LAUNCH IDENTITY map=arena1 mode=vehicle_arena gamemode=vehicle_arena weapon_set=1 npcs=0`.

## Map editor evidence

Commands were sent through the existing dev loop with
`python devscripts/dev-loop.py --client client-01 --command "..."`.

- `mapeditor 1` entered the editor.
- `mapeditor add block arena_editor_probe` logged `map-block.created` with
  `map=arena1`.
- `mapeditor set_position -5 0 2` succeeded.
- `mapeditor save` logged `map-entity.saved` with `map=arena1`,
  `block_count=9`.
- `mapeditor reload` logged `map-entity.loaded` for `arena1`.
- `mapeditor select arena_editor_probe` succeeded.
- The probe block was then deleted and saved; `config/maps/arena1.json` is back
  to 8 authored blocks and contains no probe block.

## Not claimed

- Ball entity, goals runtime, scoring, jump/boost/air roll, boost pads runtime,
  prediction, and replay are not implemented by this slice.
- No human acceptance of driving or arena feel is claimed.
- The boost-pad and arena blocks are authored map data only; their runtime
  consumers are future slices.

## Follow-up: arena data parsing + dashboard auto-open (2026-11-05 21:25 UTC)

### Arena and boost-pad data

- Added `ArenaBallConfig`, `ArenaGoalConfig`, `ArenaKickoffConfig`,
  `ArenaConfig`, and `BoostPadConfig` to `MapObjectiveConfig`.
- `config/maps/<map>.json` now parses `arena.ball`, `arena.goals`,
  `arena.kickoffs`, and `boost_pads`, and saves them back on editor save.
- Added `validateArenaAuthoring`: missing arena, missing ball spawn, no goals,
  no enabled goals, duplicate goal/pad ids, invalid extents, and invalid pad
  radii/recharge are errors. A missing goal never creates a default scoring
  zone.
- Added a fail-closed self-test for missing arena data and a valid authored
  arena.
- Live run `20261105_212323` journal:
  `arena.config.loaded` with
  `ball_present=true`, `goal_count=2`, `kickoff_count=2`,
  `boost_pad_count=3`, `error_count=0`, `result=success`.
- Server heartbeat remains `map=arena1`, `mode=vehicle_arena`, `npcs=0`,
  `players=2`.

### Dashboard auto-open/reuse

- Added a dashboard lock and a detached dashboard starter.
- A launch, build, command, or ensure-running request now opens one dashboard
  if none is already running, without starting a second watchdog or daemon.
- The dashboard still uses the existing journal `recent` pane; command records
  now include the AI command text, client id, timestamp, and result.
- A live dashboard process was observed at
  `C:\mimita-v9\.dev\dev-loop-dashboard.lock` with window title
  `MiMITA Dev Loop Dashboard — mimita-v9`. The auto-open path is implemented in
  source and is designed to reuse an existing dashboard rather than open a
  second one.

### Build note

The dev loop was continuously rebuilding because other concurrent work was
editing source files. `build/obj-debug/gamemode_map-config.o` was rebuilt at
16:35:20 local time, after the final arena validation/save-guard edits, with no
`map-config.cpp` compile error. The run `20261105_212323` launched the latest
successful artifact at that moment (`2026-11-05T21:22:19Z`) and proved arena
parsing live. The full link was still completing when this changelog was
written.

## Follow-up: ball entity (2026-11-05 22:12 UTC)

- Added `buildSphereCollisionTriangles`; spheres use the shared triangle
  collision path, not a second solver.
- Added sphere analytic inertia (`2/5 m r^2`) in the shared mass owner.
- Added procedural sphere rendering through the existing physical-entity render
  path.
- Bumped `PROTOCOL_VERSION` 50 -> 51 and added
  `PHYSICAL_ENTITY_SOURCE_SPHERE`.
- Client mirror spawn rebuilds the same tessellated sphere and marks the entity
  as `PhysicalEntityShape::Sphere`.
- Added `ArenaBall::spawnFromMap` and `ArenaBall::reset` in
  `src/gamemode/arena-ball.cpp`. The server spawns the authored ball as a
  dynamic, replicated, `serverSimulated` entity. Reset clears velocity, angular
  velocity, contact, and sleep state.
- Dedicated and listen server startup spawn the ball when the loaded map has an
  arena ball.

Live run `20261105_221144`:
- Server: `arena.ball.spawned` with `network_id=1`, `radius=0.9125`,
  `mass=30`, `position=[0,0,2]`.
- Client: `physical_entity.mirror_spawned` with `source_kind=2`,
  `motion=2` (Dynamic), and matching half extents.
- Server heartbeat remained `map=arena1`, `mode=vehicle_arena`, `npcs=0`,
  `players=2`.

Not yet implemented: goal detection/scoring/reset runtime, ball contact event,
ball prediction, and vehicle wheel-vs-ball contacts.

## Follow-up: arena vehicle spawn + actor-ball contact (2026-11-05 18:00 UTC)

- `arena1.json` now authors one `car_sedan` placement per team at the kickoff
  points.
- `serverTickVehicles` auto-seats an active, unseated `vehicle_arena` player
  into the nearest free authoritative vehicle and switches it to automatic
  transmission with the engine on. It logs `vehicle.auto_seat`.
- Actor-ball contact uses the existing `applyPlayerPush` and
  `applyPlayerContactPush` paths and now emits `arena.ball.contact` with source,
  actor, network id, tick, velocity before/after, contact point, and normal.
- No new collision or vehicle solver was added.

Compile evidence:
- `build/obj-debug/physics_physical-entity.o` rebuilt at 17:24:39 after the
  contact-event edit.
- `build/obj-debug/network_vehicle-networking.o` rebuilt at 17:50:04 after the
  auto-seat edit.
- `build/obj-debug/gamemode_arena-ball.o` rebuilt at 17:07:46.

The full main-checkout build was blocked by an unrelated concurrent
`src/pobjects/persistent-physics.cpp:296: 'tick' was not declared` error, so a
fresh live `vehicle.auto_seat` / `arena.ball.contact` observation is still
pending. An isolated agent lane (`vehicle-arena-20261105`) was created from the
current snapshot and is ready for the next incremental build.

## Follow-up: 1 human vs 1 NPC car (2026-11-05 18:30 UTC)

- Added a Vehicle Arena NPC car assignment in `serverTickVehicles`: one free
  authoritative car nearest the orange kickoff receives a synthetic NPC driver
  id and is switched to automatic + engine-on.
- The NPC car is driven through `VehicleSystem::setInput` with a steering/
  throttle `VehicleInput` toward the ball, matching the human input surface.
- Seat clearing now preserves NPC driver ids.
- Added `arena.ball.contact` for physical-entity/vehicle contact, with the
  ball's pre/post velocity, other entity identity, contact point, and normal.
- Intended first game mode is 1 human vs 1 NPC car; team capacity and later
  2v2/3v3 remain data-driven.

Live evidence for this slice is pending the next incremental build; no build
was started intentionally.

## Unrelated pre-existing failure

`python devscripts/config_selftest.py` still fails on unrelated avatar JSON
content (missing normalized/advanced keys). Gamemode sync passes
(`[GAMEMODE SYNC] PASS`).
