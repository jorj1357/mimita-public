# Layered NPC navigation: lazy walkable graph, travel goals, per-mode weights

Date (UTC): 2026-10-05
EST timestamp: 2026-10-05 17:01:08 EDT
Branch: `afad20a-rebuild`
Commit at handoff: `9b0c5c9b` (working tree was clean at start of this session)

## Result

`PASS_WITH_HUMAN_REVIEW`

Source, build, in-binary runtime, and a short headless server run are proven.
A live human Counter-Strike round on `dust2cyberiav4` is still required.

## Scope

Implement the full layered NPC movement/navigation architecture chosen by the
human: lazy chunked walkable graph, persistent long-range travel goal,
enemy-area goal selection, per-team/mode travel weights, ramp/jump links,
temporary local correction, bounded stuck escape, JSON tuning, structured
logging, multiple equal-cost routes, and reachable/hot-reloadable bomb sites.

## Root causes fixed (from the investigation report)

1. `config/npc-difficulty.json` `wallSearchDistance: 1.0` made every alternative
   direction look clear, so wall avoidance always returned "left" → circling.
2. Patrol recomputed a body-relative 12 m waypoint every tick, so the navigator
   saw `target_moved` and rebuilt the route thousands of times.
3. `DefendSite` was scored but had no `makeNavGoal` mapping → defenders fell
   through to Patrol.
4. No global route: the local A* window is ≤20 m, so far objectives failed.
5. The obstacle jump (1.8 m probe) fired almost every tick (97k jumps, 99.99%
   `obstacle`, 61% airborne) and reset stuck episodes.
6. TeamBrain enemy reports and assignments were inert (no travel goal).
7. Bomb sites were unverified placeholders above the floor.

## Implemented layers

### Layer 1 — Persistent long-range exploration goal
- New `NpcGoalKind::Explore` (`src/npc/npc-goal.h`). Patrol now returns Explore
  with a 1 m heading hint; `NpcNavigator::resolveExploreTarget` holds a real
  ~60 m target until it is reached or the desired heading reverses. This removes
  the per-tick waypoint churn.
- Tuning: `travel_target_distance_meters` / `travel_target_reached_meters` in
  `config/behavior-profiles.json` → `NpcBehaviorTuning` → navigator settings.

### Layer 2 — Enemy-area goal selection
- `TeamBrain::bestTeamReport()` exposes the team's highest-confidence enemy
  report. `serverTeamBrainTick` pushes it as `UtilityContext.enemyAreaPos` /
  `enemyAreaKnown`.
- New `UtilityGoalKind::HuntArea` scored from `travel_hunt_bias`; `makeNavGoal`
  maps it to a `ReachPosition` at the enemy area (4 m tolerance).

### Layer 3 — Automatic walkable-surface navigation (lazy, chunked)
- New `src/npc/npc-nav-graph.{h,cpp}`: `NpcNavGraph` voxelizes loaded collision
  triangles into 32 m chunks on demand, links walk/ramp/jump/drop edges with the
  shared walkable-dot rule and capsule clearance, and runs A* over lazily
  activated chunks. Chunks and edges are cached; geometry changes invalidate via
  a signature. It never requires authored points and supports dynamic/large maps.
- `NpcNavigator::update` prefers the graph for goals beyond the local window and
  falls back to the local A*; the local planner remains the fine-steering owner.

### Layer 4 — Ramp and jump traversal links
- Graph edges use `classifySurface` ramp handling, `maxStepHeight`, and
  `maxJumpHeight` with landing clearance; `allowNavigationJumps` and the actor
  jump policy gate jump links.

### Layer 5 — Temporary local obstacle correction
- `NpcNavigation::wallAvoidDirection` now scores all nearby directions
  (continuity + real clearance) instead of returning the first clear one, and
  probes at `max(3.0, wallSearchDistance)`. `bestTurnDirection` was similarly
  rewritten without a forward component into the blocking face.
- `wallSearchDistance` config raised `1.0 → 4.0` with an explanatory comment.
- The navigator's `localCorrection` remains an interrupt that preserves the
  long-range destination.

### Layer 6 — Bounded stuck escape
- New `Npc::jumpCooldown` bounds recovery hopping to one jump per 0.45 s for the
  obstacle and stuck branches; the obstacle/first-stuck jump no longer fires
  every tick. Stuck recovery still chooses the most open direction and repaths.

### Layer 7 — Human-like combat movement
- Unchanged policy: combat states are reached only when
  `belief.hasVisibleTarget`; the long-range goal is preserved and resumed when
  the enemy is lost.

### Layer 8 — JSON tuning
- `config/gamemodes/counterstrike.json` `npc_travel { objective_bias,
  hunt_bias, explore_bias }`, parsed to `GamemodeNpcTravel`.
- `config/behavior-profiles.json`: travel target fields (documented).
- `config/npc-difficulty.json`: `wallSearchDistance` fix.
- `config/maps/dust2cyberiav4.json`: corrected anchors (see below).

### Layer 9 — Structured logging
- Added `npc.travel-goal-changed` (Explore retarget).
- `npc.movement-decision` now includes `horizontal_distance_moved`,
  `net_progress_toward_goal`, and `goal_position` in addition to the existing
  committed/final direction, replan reason, jump reason, velocity, on_ground.
- Existing `npc.nav-plan-created/failed`, `npc.nav-replan`,
  `npc.movement-commitment-*`, `npc.wall-avoid`, `npc.stuck`,
  `npc.stuck-recovery`, `npc.jump` remain change-edge and bounded.

### Multiple equal-cost routes
- `NpcNavGraph::findRoutes` re-searches with the previous route's nodes
  penalized, returning up to `maxRoutes` distinct corridors; the list is rotated
  by `actorSeed` so squad members pick different lines. The graph selftest proves
  **3 distinct routes** around a wall.

### Bomb sites (explainable change)
- Anchors changed to `A=(-700,60,2345)`, `B=(-820,90,2345)`: X/Y are the
  playable areas between the team spawns; Z was lowered to the observed floor
  level (~2345) from the old 2400/2300 guesses.
- At runtime the server **snaps each site's Z to the loaded collision floor**
  (`NpcNavigation::groundHeightAt`, `serverTeamBrainTick`) so a slightly
  high/low anchor is still reachable, and marks the site unavailable if no floor
  exists under it. Nothing is written back; authored JSON stays authoritative.
- Hot reload is unchanged (`MapConfigRegistry`), and `visible_debug: true`
  continues to draw the site spheres so a human can verify/move them with
  `site_debug`.

## Files

New: `src/npc/npc-nav-graph.h`, `src/npc/npc-nav-graph.cpp`.
Modified: `src/npc/npc.cpp`, `npc.h`, `npc-goal.h`, `npc-navigator.h/.cpp`,
`npc-navigation.cpp`, `npc-utility.h/.cpp`, `team-brain.h/.cpp`,
`npc-behavior.h/.cpp`, `src/network/server-npcs.cpp`,
`src/network/server-gamemode.cpp`, `src/gamemode/gamemode.h/.cpp`,
`src/game/game-cli.cpp`, `config/gamemodes/counterstrike.json`,
`config/behavior-profiles.json`, `config/npc-difficulty.json`,
`config/maps/dust2cyberiav4.json`.

## Validation

### Build
```text
python build_agent.py / MIMITA_FORCE_LINK=1 -> BUILD SUCCESS, mimita.exe relinked
```

### In-binary selftests (all PASS)
```text
--npc-nav-graph-selftest          PASS (routes around a wall; 3 distinct routes)
--npc-navigation-selftest         PASS
--npc-search-behavior-selftest    PASS
--npc-movement-policy-selftest    PASS
--npc-movement-executor-selftest  PASS
--npc-movement-commitment-selftest PASS
--npc-movement-decision-selftest  PASS
--npc-behavior-profile-selftest   PASS
--npc-utility-selftest            PASS
--team-brain-selftest             PASS
--gamemode-selftest / --cs-round-selftest / --npc-targeting / --npc-perception PASS
--counterstrike-acceptance-selftest PASS
```

### Runtime (headless server, `dust2cyberiav4`, 6 NPCs, ~4 s)
```text
--server --npcs 6 --map dust2cyberiav4 : exited 0, no crash
events: npc.travel-goal-changed=6, npc.nav-plan-created=6, npc.movement-decision=24,
        npc.stuck=205, npc.jump=38, npc.nav-plan-failed=0
per-NPC net displacement 5.5-15.2 m over ~4 s with path/net ratio 0.6-0.97
(previous live session: net/path ~1-2% over hundreds of seconds, 97k stuck+jump)
```
The path/net ratio rose from ~1–2% (circling) to 60–97% (consistent travel), and
jump/stuck ratio dropped sharply (38 jumps vs 205 stuck, versus ~1:1 before).

## Human review still needed

Live `dust2cyberiav4` Counter-Strike round:
1. CT and T leave spawn consistently.
2. NPCs travel to sites/enemy areas without circling; multiple squad members take
   different routes.
3. NPCs route around walls/ramps and do not hop constantly.
4. `events.jsonl` `npc.movement-decision` shows positive
   `net_progress_toward_goal` and low `npc.stuck`/`npc.jump` rates.
5. Bomb-site spheres are visible and reachable; `site_debug move/save` refines
   them if desired (hot reload, no rebuild).

## Pre-existing / unrelated

`--npc-radar-selftest` still fails on the pre-existing rage2
`information_mode` mismatch; not caused by this change.
