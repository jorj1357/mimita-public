# Shared NPC movement commitment (one direction owner)

Date (UTC): 2026-10-05
EST timestamp: 2026-10-05 13:17:41 EDT
Branch: `afad20a-rebuild`
Commit at handoff: `a1bcea51` (working tree dirty with unrelated concurrent work)

## Result

`PASS_WITH_HUMAN_REVIEW`

Source, build, pure-test, and in-binary runtime evidence are proven. No live
Counter-Strike match was played. Human acceptance of Dust2 spawn escape and
forward pursuit is still required.

## Scope

Implement the shared NPC movement-commitment system requested by the human
handoff: one forward/pursuit direction owner in the shared `NpcNavigator`, real
progress instead of per-tick re-decisions, profile-owned replan/commitment
values in `config/behavior-profiles.json`, combat movement only on genuine
visibility, and bounded structured events. Counter-Strike and Sandbox keep the
same `sandbox_shared` executor; no `if (mode == "counter_strike")` was added to
movement.

## Design (what one owner means now)

- `NpcNavigator` owns the single committed direction
  (`commitmentActive`/`committedDirection`/`commitmentTimeRemaining`/
  `progressTimer`/`commitmentStartPosition`) plus the recent-path rings
  (`recentVisited`/`recentBlocked`).
- `NpcStateMachine::patrolDir` and `patrolRepathTimer` were removed; Patrol and
  `makeNavGoal` now read the navigator commitment. This is the full unification
  the human selected.
- The navigator's repath timer is a floor, not a rebuild command.

## Files changed

### `config/behavior-profiles.json`
- Documented and set `repath_interval_seconds`, `goal_move_threshold_meters`,
  and the nested `movement_commitment` block on all five profiles (units, range,
  applies-to, hot-reload status). Compatibility defaults kept: repath `0.9`,
  goal threshold `2.5`; the handoff's example `2.0`/`8.0` was NOT adopted.

### `src/npc/npc-behavior.h` / `.cpp`
- Added the replan/commitment fields to `BehaviorProfileDefinition` (`-1`
  sentinel on the two timers) and `NpcBehaviorTuning` (concrete defaults).
- `readProfile` parses the nested object; `clampWarn` clamps every numeric field
  and emits an explicit `[BEHAVIOR] <id>.<field>=<x> out of range ...` warning.
- `resolveNpcBehavior` maps the `-1` sentinels to `0.9`/`2.5`.
- Added `behaviorProfileSelfTest`.

### `src/npc/npc-navigator.h` / `.cpp`
- Added `MovementCommitmentSettings`, `NpcCommitmentUpdate`, and
  `NpcNavResult.planCreated/planFailed/replan/replanReason/pathNodeCount`.
- Added `updateCommitment`, `chooseBestOpenDirection`, `pushVisited`,
  `pushBlocked`, `nearestRecentDistance`, `forceRecommit`; `reset()` clears the
  new state.
- Replan policy: rebuild only on route empty/finished, goal moved beyond
  threshold, physically blocked, or progress failed; the timer gates how often.
- When no route is cached, the committed direction (if valid and broadly toward
  the goal) owns forward steering.
- Visible enemy + `visible_enemy_allows_combat_movement` suspends commitment
  without destroying it, so pursuit resumes when the enemy is lost.

### `src/npc/npc.cpp`
- Removed `pushSearchPoint`, `nearestMemoryDistance`, `pickSearchDirection`,
  `updatePatrolHeading` (moved into the navigator).
- Builds `MovementCommitmentSettings` from `npc.behavior`, evaluates the
  commitment before `computeStateMovement`, and passes the settings to
  `NpcNavigator::update`.
- The no-progress watchdog now feeds `navigator.pushVisited/pushBlocked` and
  calls `forceRecommit()`.
- Added bounded event emitters: `emitCommitmentEvents`, `emitNavPlanEvents`,
  `emitGoalChangedEvent`.

### `src/npc/npc-state-machine.h` / `src/npc/npc-states.cpp`
- Removed `patrolDir`/`patrolRepathTimer` and the search rings. Patrol reads the
  navigator's committed direction.

### `src/npc/npc-navigation-settings.h` / `.cpp`, `src/gamemode/match-roles.cpp`
- `search_radius` has one owner/max: the parser clamps to the planner's real
  [4,20] ceiling and records a `warnings` string; `match-roles.cpp` logs it.
  `counter_strike.json` `search_radius` changed `200.0` -> `20.0`.

### `src/network/server-npcs.cpp`
- Emits `npc.target-changed` on server target selection changes.

### `src/game/game-cli.cpp`
- Updated `--npc-search-behavior-selftest` to drive the navigator commitment.
- New `--npc-behavior-profile-selftest`, `--npc-movement-commitment-selftest`,
  `--npc-movement-commitment-event-selftest`.

### `tests/npc-navigation-test.cpp`
- `search_radius` clamp expectation `32` -> `20` plus a warning assertion.

## Structured events (all via `StructuredLogger::instance().writeEvent`)

`npc.target-changed`, `npc.goal-changed`, `npc.nav-plan-created`,
`npc.nav-plan-failed`, `npc.nav-replan`, `npc.movement-commitment-created`,
`npc.movement-commitment-replaced`, `npc.movement-commitment-blocked`,
`npc.movement-progress-failed`. Change-edge only; the event selftest proved
commitment/goal/plan events stay far below one per tick.

## Reasoning

The movement math was already in one owner; the missing piece was a stable
direction and a reason-based replan. Moving the direction state into the
navigator removes the parallel `patrolDir` owner and makes the actor follow a
route instead of re-deciding each tick. Profile-owned values keep tuning in JSON
and hot-reloadable. The user chose "respect profile `after_reaching_last_known`"
for remembered targets, so memory circling remains profile-governed and bounded;
forward pursuit is the default while the target is far.

## Documents and skills used

- `AGENTS.md`, `docs/ROUTER.md`
- `docs/architecture/json-configuration/json-configuration.md`
- `docs/architecture/player-npc-systems/player-npc-systems.md`
  (`npc-movement.md` is informal historical context, not authoritative)
- `docs/specs/debug-logging/debug-logging.md`, `docs/specs/debug-logging/canonical-jsonl.md`
- `docs/specs/movement/movement.md`, `docs/architecture/collision/collision.md`
- `docs/skills/spec-behavior-review-v1.md` — result: PASS. The handoff's
  functional requirements are met; the only divergence is the compatibility
  default (0.9/2.5) chosen over the example 2.0/8.0.
- `docs/skills/logging-checker-v1.md` — result: PASS. Events have owners,
  category `npc_movement`, level `Important`, reasons, and are change-edge.
- `docs/operations/build-and-exe/build-and-exe.md`,
  `docs/operations/task-completion/task-completion.md`

## Validation

### Source

All listed files compile. `git diff --check`: no whitespace errors (CRLF
warnings only, environment).

### Build

```text
python build_agent.py            -> BUILD SUCCESS; mimita.exe relinked
MIMITA_FORCE_LINK=1 python build_agent.py -> relink verified
```

### Pure tests

```text
build/npc-movement-policy-test.exe   PASS (72 checks)
build/npc-navigation-test.exe        PASS (35 checks)   [rebuilt: parser changed]
build/npc-movement-executor-test.exe PASS (15 checks)
```

### In-binary selftests (all PASS)

```text
--npc-movement-policy-selftest            PASS (lateral=0.84)
--npc-search-behavior-selftest            PASS
--npc-navigation-selftest                 PASS
--npc-movement-executor-selftest          PASS
--npc-behavior-profile-selftest           PASS
--npc-movement-commitment-selftest        PASS
--npc-movement-commitment-event-selftest  PASS
    commitmentCreated=1 replaced=0 planCreated=1 planFailed=0 goalChanged=1
--npc-wall-escape-event-selftest          PASS
--counterstrike-acceptance-selftest       PASS
--grenade-reasoning-selftest / --grenade-selftest / --area-effect-selftest PASS
--npc-targeting / --aim-fov / --spawn-tag / --cs-round / --gamemode PASS
--npc-nav-request / --npc-perception / --npc-utility / --team-brain PASS
--objective / --map-config PASS
```

Sample record (`logs/10-05-2026/20261005_131707/events.jsonl`):

```json
{"category":"NPC_MOVEMENT","event":"npc.movement-commitment-created","fields":{"actor":9602,"commit_seconds":10.0,"committed_dir":[0.707, -0.707],"executor":"sandbox_shared","pos":[0,0,1.9],"preset":"counter_strike","profile":"balanced","progress_distance":0.0,"team":-1},"reason":"created"}
```

### Pre-existing, not caused by this session

- `--npc-radar-selftest`: FAIL on `rage2 uses perfect radar` because
  `config/behavior-profiles.json` sets rage2 `information_mode: "memory"` while
  the test (game-cli.cpp:1336) expects `perfect_radar`. Both are unchanged by
  this session (confirmed against `HEAD`). Left as-is.
- `--actor-preset-selftest`: pre-existing weapon-value assertion mismatch.

## Human review still needed

Live `dust2cyberiav4` Counter-Strike round:
1. CT NPCs leave spawn; T NPCs leave spawn.
2. NPCs do not repeatedly reverse direction.
3. NPCs route around walls and ramps.
4. Remembered (hidden) targets produce forward pursuit.
5. Visible enemies can trigger combat movement (circle/strafe/juke).
6. Hidden enemies do not cause endless circling.
Read `logs/<date>/<run>/events.jsonl` for `npc.movement-commitment-*`,
`npc.nav-*`, `npc.goal-changed`, `npc.target-changed`. `npc_movement` must stay
at least `important` in `config/debuglogger.json`.

## Pre-existing / unrelated changes

The working tree also contains unrelated concurrent edits (mode packs, disaster
runtime, bomb-site placeholders, `ragdoll-mode.h`, `config/impact_decals.json`,
etc.). They were not touched or reverted and are not claimed by this session.
