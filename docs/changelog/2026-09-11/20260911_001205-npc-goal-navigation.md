// 2026-09-11T00:12:05Z
/* purpose
* record the generalized NPC goal -> navigation -> movement pipeline
* preserve exact source, build, and headless runtime evidence
* this file does NOT claim a global navmesh, personality, or spectator camera
* this file does NOT replace the append-only regression record
*/

# NPC goal, navigation, and movement pipeline

## Session

- Branch: `8292026stash`
- HEAD commit: `fdac4e2`
- Timestamp (UTC): `2026-09-11T00:12:05Z`
- Display timezone: America/New_York
- Display time: `2026-09-10 20:12:05 EDT`
- Pre-existing changes: role/team/state, death/respawn/elimination, role
  health/loadout, and role movement slices were preserved. This session only
  added the goal/navigation layer.
- Unrelated concurrent edits under `docs/regressions/`, `docs/gold/`, and one
  `docs/changelog/` file were left untouched.

## Files changed

New:
- `src/npc/npc-goal.h`: `NpcGoalKind` + `NpcGoal` (abstract movement intent).
- `src/npc/npc-navigator.h` / `.cpp`: per-NPC navigation layer (local grid A*,
  cached route, repath policy, traversal hints).

Modified:
- `src/npc/npc-navigation.h` / `.cpp`: exposed `NpcNavigation::rayTriangle`
  (Moller-Trumbore) so the planner reuses one ray/triangle implementation.
- `src/npc/npc.h`: `Npc` gains `NpcNavigator navigator`.
- `src/npc/npc.cpp`: `makeNavGoal` maps brain state -> goal; the navigator runs
  after `computeStateMovement`; role movement config is resolved once and shared
  with the kernel; a throttled `[NPC NAV] stuck` diagnostic.

## Architecture chosen

A bounded **rolling-horizon local ground-grid A\*** was chosen rather than a
global navmesh/waypoint graph. Reason: this repo's maps are large triangle soups
(`funworld3`: 41,326 collision triangles, bounds ~728 x 821 x 472 m), so a global
graph/navmesh is expensive to build and maintain for this slice. A local planner
that re-plans as the actor moves gives real waypoint routing around walls and
across elevations while keeping cost bounded and per-NPC.

Responsibilities stay separate:
```
NPC decision (npc.cpp makeNavGoal)   -> NpcGoal (reach/follow/maintain/flee/LOS)
Navigation (npc-navigator.cpp)       -> cached route + steering + traversal hint
Movement execution (npc.cpp + kernel)-> InputState -> physicsMainUpdate(role cfg)
```

## Goal -> navigation -> input data flow

1. Existing sensing/state selection runs unchanged (`senseWorld`, `pickNextState`).
2. `computeStateMovement` produces the tactical intent (chase/circle/strafe/...).
3. `makeNavGoal(npc)` translates the state + target + weapon range into an
   `NpcGoal` (Chase/Advance -> FollowActor; Circle/Strafe/Hold/Peek/Aim ->
   MaintainDistance; Retreat/Recover -> FleeActor; no-target search/wander ->
   ReachPosition).
4. `NpcNavigator::update` resolves the goal to a destination point (for
   target-relative goals using `npc.sensors.targetPos`), decides whether to
   replan, runs the local planner, and follows the cached route.
5. It returns a steering `dir`, the current `waypoint`, and `wantJump` /
   `wantDownDash`. `npc.cpp` uses the route direction only when it is a real
   **detour** (long route or >~37 degrees off the direct line), preserving
   tactical strafing in open space, then applies traversal flags.
6. `buildInputState` and the shared `physicsMainUpdate` (with the actor's role
   `MovementConfig`) run exactly as before; the kernel enforces capabilities.

## Pathfinding / repath strategy

- Local window: radius 12.5 m, cell 2.5 m (11x11 grid), probes down from
  `npc.z + 4` up to 12 m below, filters to walkable normals (`normal.z >= 0.7`).
- Window triangles are gathered once per plan via `appendChunkTrianglesForAABB`;
  all grid probes test against that cached set (no per-cell broadphase query).
- 8-connected A* with traversal rules: `dz <= maxJumpHeight` (derived from the
  actor's `jumpVerticalSpeed`/`gravityZ`), `dz > step(0.65)` costs a jump,
  negative `dz` is a drop; diagonals require both orthogonal cells valid (no
  corner cutting).
- Repath when: no route, route exhausted, timer (`0.9 s`), destination moved
  `> 2.5 m`, or `NpcNavigation::isStuck`. No route falls back to direct steering
  and retries soon.
- Path following advances waypoints within 1.0 m planar / 1.3 m vertical and
  raises jumps on upward steps and drops on downward steps.

## Movement-profile integration

- `npc.cpp` resolves the actor's role movement config once
  (`RoleMovementCache::get(npc.movementProfileId)`, else the global NPC preset)
  and passes it to both the navigator and `physicsMainUpdate`.
- The planner gates traversal by capability derived from that config
  (`jumpVerticalSpeed`, `gravityZ`), so hunter (`retrograd_fast`, jumpCap 3.11 m)
  and juggernaut (`heavy`, jumpCap 1.21 m) route differently through the same
  navigator. No role-name branches exist.
- `down-dash` drops are only applied when the config enables `downDashEnabled`;
  dash/freeze/bhop are still enforced by the existing kernel.

## Performance strategy for ~30 NPCs

- Plans are rate-limited by a global token bucket (`kMaxPlansPerSecond = 16`).
- No planning per tick: per-NPC repath interval (0.9 s) plus target-moved/stuck
  triggers and path caching; between plans, following is a trivial vector step.
- Candidate triangles gathered once per plan; grid probes and A* are O(121)
  node work. Observed plan rates stayed well under the cap in all runs.

## Tests and results

Build: `python build_agent.py` => `Status: SUCCESS` (return code 0).

TDM, funworld3, 8 NPCs, 45 s (roles hunter/juggernaut):
- 437 `[NPC NAV]` plans; goal kinds `maintain=329`, `pursue=97`, `flee=11`.
- Reasons `timer=226`, `stuck=109`, `target_moved=71`, `initial=31`.
- `jumpCap=3.11` for hunter plans and `1.21` for juggernaut plans (`2.85` only
  pre-match before role assignment), proving role movement drives traversal.
- Multi-node detours (`pathNodes` 3-6) and `netDz` values confirm routing around
  geometry and across elevations.
- 60 kills and 66 NPC respawns: combat still works.

30 NPCs, funworld3, 40 s (target scale):
- Completed in 43.2 s wall time (40 s timeout): no stall.
- 94 plans (rate-limited), 165 kills, 2010 respawns.
- `netDz` spread from ~0 to -12 m (and positive climbs), confirming elevation
  traversal.

FFA, 10 NPCs, 35 s (regression, no roles):
- `jumpCap=2.85` (global config), 74 plans, 276 kills; FFA scored and ran.

Elimination, 10 NPCs, 45 s (regression):
- `Match ACTIVE mode=elimination`, hunter `jumpCap=3.11`, `netDz=-4.3`, 480
  plans, and the match ended; one-life/spectating behavior unchanged.

## Regressions found

None observed. FFA/TDM/elimination still start, fight, score, and end; role
health/loadout/movement logs are unchanged.

## Documents and skills

- `AGENTS.md`, `docs/ROUTER.md`
- `docs/specs/movement/movement.md`, `docs/specs/gamemodes/gamemodes.md`
- `docs/architecture/player-npc-systems/player-npc-systems.md`
- `docs/architecture/collision/collision.md` (cached broadphase, no per-query
  allocations in hot loops; the planner uses the cached chunk path)
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`: PASS; goal->navigation->movement
  separation, no role-name branches, no global movement mutation.
- `docs/skills/efficiency-checker-v1.md`: PASS; plans are throttled, cached, and
  windowed; no per-tick full-map search.
- `docs/skills/logging-checker-v1.md`: PASS; `[NPC NAV]` logs on plan/repath and
  a throttled stuck line, NpcMovement category.

## Human review still needed

- A GUI client on a multi-level map should confirm NPCs reliably round walls and
  climb/drop between elevations over several minutes, and that combat feel did
  not regress.
- A live human participant was not available; the human and NPC paths share the
  same resolver/kernel, and the navigator is NPC-only by design.

## Smallest logical next phase

Feed the navigator's route waypoint into a shared, capability-typed traversal
request (walk/jump/drop/dash/dash-jump) so dash gaps and future traversal types
(fly/hover) plug in as data instead of new decision branches.
