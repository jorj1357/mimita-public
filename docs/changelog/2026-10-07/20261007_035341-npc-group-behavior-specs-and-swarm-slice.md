# NPC group behavior + swarm slice (Juggernaut), with specs

- Status: `SPECS_AND_A_B_SLICE_VERIFIED_WITH_HUMAN_REVIEW_PENDING`
- UTC timestamp: `2026-10-07T03:53:41Z`
- Display timezone: `America/New_York` (2026-10-06 23:53)
- Branch: `afad20a-rebuild`
- Scope: write the missing general group/focus and local-avoidance specs, then
  implement A (general external-navigation adoption) and B (behavior-profile
  focus + TeamFocus/SquadCoordinator + utility/goal wiring). First acceptance map
  `dust2cyberiav4`; first mode `juggernaut`.

## Specs written

- NEW `docs/architecture/player-npc-systems/npc-group-behavior.md`
  - TeamFocus + SquadCoordinator ownership, squad modes (rally/advance/engage/
    hold/regroup), slots/spread, cohesion, config in behavior profiles, events,
    acceptance, non-goals. Group behavior is a suggestion producer only.
- NEW `docs/architecture/player-npc-systems/npc-local-avoidance.md`
  - RVO2 and DetourCrowd are optional preferred-velocity providers; MiMITA
    physics/collision remain authoritative; budget/determinism/dynamic-tile
    rules; acceptance vs a no-avoidance baseline.
- UPDATED `docs/specs/gamemodes/juggernaut.md`
  - Phase 6 expanded with Fighter unit/swarm behavior, the first acceptance map
    `dust2cyberiav4`, Juggernaut behavior, and initial acceptance metrics.
- UPDATED `docs/specs/gamemodes/gamemodes.md`
  - Added the general NPC intent contract: objective / focus / patrol, focus
    configured in behavior profiles, no mode-specific AI.
- UPDATED `docs/architecture/player-npc-systems/npc-navigation-implementation.md`
  - Added the "general external-library adoption" section (every mode uses the
    shared Recast/Detour owner) and a pointer to the avoidance contract.

## Implementation A — general external navigation

- `src/npc/npc-behavior.h/.cpp`: added `navigation_backend`
  (`custom|compare|recast`) to behavior profiles.
- `src/npc/npc-navigator.cpp`: backend resolution is now
  env override > behavior profile > legacy compare env > custom. The profile is
  the durable general configuration; the env var stays a dev override.
- `src/network/server-npcs.cpp`: prewarms the navmesh when any loaded behavior
  profile selects `recast`/`compare`, so the first bake never lands inside a
  fixed gameplay tick.

## Implementation B — focus / group behavior

- `src/npc/npc-behavior.h/.cpp`: added group/focus profile fields
  (`focus_enabled`, `focus_mode`, `swarm`, `cohesion`,
  `spread_radius_meters`, `approach_style`, `max_attackers_per_target`,
  `rally_distance_meters`, `focus_reacquire_seconds`, `slot_stickiness`).
- `src/npc/team-brain.h/.cpp`: added `SquadMember`, `SquadTuning`, `SquadState`,
  `TeamBrain::updateSquad()` and `squadSlotFor()`. The anchor advances toward the
  focus so the whole squad moves as a unit; attackers get a ring of slots around
  the focus, the rest get anchor slots. Deterministic by actor id.
- `src/npc/npc-utility.h/.cpp`: added `UtilityGoalKind::FocusTarget` and
  `UtilityContext` focus/squad fields; scores FocusTarget when a focus exists,
  and maps it to Approach/HoldAngle.
- `src/npc/npc.cpp`: `makeNavGoal` maps FocusTarget to a `ReachPosition` at the
  actor's squad slot (or the focus), so the squad swarms instead of stacking.
  A focus is a travel target, never an aim permission.
- `src/network/server-gamemode.cpp`: `serverTeamBrainTick` resolves per-team
  squad tuning from members' profiles, runs the coordinator for all modes, and
  pushes focus/anchor/slot into each NPC's `UtilityContext`. Emits bounded
  change-edge `npc.focus-changed` and `npc.squad-mode-changed`.

## Config

- `config/behavior-profiles.json`: new `juggernaut_swarm` profile
  (`navigation_backend: recast`, focus/swarm enabled, aggressive combat tuning).
- `config/roles.json`: `juggernaut_fighter` now references `juggernaut_swarm`.
- `config/gamemodes/juggernaut.json`: added `dust2cyberiav4` to the map list.

## Build evidence

- `mimita-20261006T-swarm-v10.exe` (specs + A + B core): BUILD SUCCESS.
- `mimita-20261006T-swarm-v11.exe` (+ anchor advance + focus/squad events):
  BUILD SUCCESS.
- Command: `MIMITA_EXE_NAME=<name> python build_agent.py`.

## Runtime evidence

- Journal: `logs/10-06-2026/20261006_225044/events.jsonl`.
- Command: `MIMITA_CS_HEADLESS_MATCH=1 mimita-...v11.exe --server --bind
  127.0.0.1:0 --mode juggernaut --map dust2cyberiav4 --npcs 0 --timeout 30
  --no-map-rotation --no-discord-notification`.
- `npc.navmesh.bake-success`: `coordinate_system=mimita_z_up_to_recast_y_up`,
  `walkable_triangles=1339`, `navmesh_polys=1436`, `build_ms≈943` — Recast
  prewarmed from the behavior profile (A works).
- `npc.nav.result` = 299 with `backend_authoritative=recast_detour` — the
  external backend routes the Fighters.
- Round lifecycle: `round.start` (mode=juggernaut) -> `round.go` ->
  `round.active`.
- Group behavior: `npc.squad-mode-changed` (team 0, mode=1 advance) and
  `npc.focus-changed` (focus near the Juggernaut area ~(-600,28,2366)).
- Fighter trajectory (actor 100010): leaves spawn `(-880,125,2348)` and advances
  toward the focus with sustained positive net progress
  (22, 19, 16, 23, 29, 23, 18, 21, 24, 30, 34) until it reaches `(-639,60,2351)`
  near the Juggernauts. This is the requested "move as a unit to the
  Juggernauts" behavior.

## Focused checks

- `--counterstrike-acceptance-selftest`: PASS (all subsystems) on v11.
- Every inspected journal parsed as valid JSONL.
- Build success reported separately from runtime behavior; human visual review
  not performed (no GUI client).

## Finding outside scope (recorded, not fixed)

The headless Juggernaut run showed 32 concurrent team-0 Fighters instead of the
configured 16. `buildObjectiveRoster` clears ids >= 100000 from the server `npcs`
map but the warmup roster bodies appeared to persist in the real `NpcSystem`, so
the round roster doubled the live Fighters. This inflates crowding/stuck
counters. It is a roster/NpcSystem lifecycle issue that pre-exists this slice and
affects any round mode; it should be investigated separately before judging
swarm crowding or NPC-count performance.

## Remaining work (not claimed complete)

- Local avoidance (RVO2/DetourCrowd) is specified but not implemented yet
  (phase C). The swarm currently relies on slots + physical collision only, and
  `npc.stuck` remains high.
- Focus oscillates within ~10 m around the Juggernaut area; focus
  hysteresis/decay tuning is still needed.
- The roster/NpcSystem duplication above.
- Juggernaut/Fighter balance, weapon tuning, and remaining Juggernaut spec
  phases (5, 7).

## Human review still needed

Run Juggernaut with a real client and confirm the Fighters visibly leave spawn as
a unit, converge on the Juggernauts, spread out, and keep shooting. Machine
evidence proves routes, focus, squad mode, and sustained advance only.
