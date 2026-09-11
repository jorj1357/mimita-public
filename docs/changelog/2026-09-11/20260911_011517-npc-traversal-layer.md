// 2026-09-11T01:15:17Z
/* purpose
* record the capability-aware traversal request layer between navigation and input
* preserve exact source, build, and headless runtime evidence
* this file does NOT claim flying, hovering, climbing, or personality work
* this file does NOT replace the append-only regression record
*/

# Capability-aware NPC traversal layer

## Session

- Branch: `8292026stash`
- HEAD commit: `fdac4e2`
- Timestamp (UTC): `2026-09-11T01:15:17Z`
- Display timezone: America/New_York
- Display time: `2026-09-10 21:15:17 EDT`
- Pre-existing changes: all prior actor/role/health/loadout/movement and
  goal->navigation slices were preserved. This session only made traversal
  explicit.
- Unrelated concurrent edits under `docs/regressions/`, `docs/gold/`, and other
  `docs/changelog/` files were left untouched.

## Files changed

New:
- `src/npc/npc-traversal.h`: `TraversalType`, `TraversalRequest`,
  `NpcTraversalStep`, `NpcTraversalExecutor`.
- `src/npc/npc-traversal.cpp`: traversal selection, capability checks, multi-tick
  execution, failure handling, diagnostics.

Modified:
- `src/npc/npc-navigator.h/.cpp`: `NpcNavResult` now carries path annotations
  (`heightDelta`, `distance`, `hasGap`, `valid`) instead of input flags; the
  planner gained gap edges + per-point `pathGap`; `npcMaxJumpHeight` is shared.
- `src/npc/npc.h`: `Npc` gains `NpcTraversalExecutor traversal`.
- `src/npc/npc.cpp`: navigator output is fed to the traversal executor; traversal
  input intent drives the existing `buildInputState` / `physicsMainUpdate`.
- `src/network/server-npcs.cpp`, `src/network/server-gamemode.cpp`: reset the
  navigator and traversal on respawn/gamemode spawn.

## Final traversal abstraction

```cpp
enum class TraversalType : uint8_t { Walk, Jump, Drop, Dash, DashJump };

struct TraversalRequest {          // what to do, where, roughly which way
    TraversalType type;
    glm::vec3 targetPosition;
    glm::vec3 desiredDirection;
};

struct NpcTraversalStep {          // input intent for one tick
    glm::vec3 direction;
    bool jump, dash, downDash, active;
    TraversalType type;
};
```

Kept intentionally small. Future types (`Climb`, `Fly`, `Hover`, ...) extend the
enum and one `select()` case / execution switch; NPC decision code and the
navigator do not change because they only exchange goal/path information.

## Navigator -> traversal -> input data flow

1. `NpcNavigator::update` returns the next waypoint plus `heightDelta`,
   `distance`, `hasGap`, `hasPath`, `detour`, and `pathNodes` (no input flags).
2. `NpcTraversalExecutor::update` chooses a `TraversalType` from that info and
   the actor's effective `MovementConfig`, then maintains it across ticks and
   emits a `NpcTraversalStep` (`direction`, `jump`, `dash`, `downDash`).
3. `npc.cpp` applies the step: it overrides the tactical direction only when the
   route is a detour or the traversal is non-walk, then ORs the traversal
   actions into the existing input, which `buildInputState` and the shared
   `physicsMainUpdate` consume unchanged.
4. The actor's role `MovementConfig` is the single capability source (resolved
   once per update and shared by the navigator, traversal, and kernel).

## Capability checks

`select()` reads only `MovementConfig` + body state:
- `npcMaxJumpHeight(cfg)` = `jumpVerticalSpeed^2 / 2|gravityZ|` gates jumps.
- `cfg.dashEnabled` (and `body.dash.dashAvailable`, plus a short post-failure
  dash ban) gates dash/dash-jump.
- `cfg.downDashEnabled` gates drop acceleration; without it the actor just walks
  off the ledge.
- Walkable slopes (`|dz|/distance <= 1.0` and `|dz| < 2 m`) are always `Walk`,
  so ramps are not mistaken for ledges.

Result: hunter (`retrograd_fast`, dash on, high jump) uses Dash/DashJump; the
juggernaut (`heavy`, dash off, low jump) uses Walk/Jump/alternate route. No
role-name branches exist.

## Multi-tick traversal handling

The executor keeps minimal state: active type, target, tick count, whether jump/
dash were issued, last distance, and no-progress ticks. A traversal continues
until it completes, fails, is invalidated by a material target/path change
(`> 1.6 m`), or the direct destination is reached. Jump/dash are issued once per
traversal (edge-style), avoiding per-frame input flicker.

## Failure and replan behavior

- No-progress (`~1.25 s`) or timeout (`6 s`) marks the traversal failed with a
  reason (`blocked`/`timeout`), requests `navigator.requestRepath()`, and starts
  a `0.7 s` fail cooldown so the same action is not retried every frame.
- A failed dash/dash-jump also sets a `1.5 s` dash ban, forcing Walk/Jump and a
  fresh route.
- Capability-infeasible traversal (e.g., a rise above the actor's jump height
  with no dash) fails immediately with `no_jump`/`no_dash` and replans.

## Diagnostics

Event logs only (NpcMovement category, off by default):
```text
[NPC TRAVERSAL] actor=1004 type=drop target=(-8.6,28.3,330.7)
[NPC TRAVERSAL] actor=1004 complete type=drop
[NPC TRAVERSAL] actor=1005 fail type=jump reason=no_jump
```
Consecutive walk waypoints are not logged, so the log stays event-scale.

## Tests and results

Build: `python build_agent.py` => `Status: SUCCESS` (return code 0).

TDM, funworld3, 8 NPCs, 45 s:
- All five types observed: `walk=688`, `jump=43`, `drop=1201`, `dash=80`,
  `dash_jump=8`.
- `starts=2020`, `complete=1948` (96.4%), `fail=60` (3.0%).
- Role split: hunter `dash=78`, `dash_jump=8`; juggernaut `dash=2`
  (pre-role-assignment global config only), no dash after role assignment.
- Test A Walk, Test B Jump, Test C Drop, Test D Dash, Test E DashJump (gap
  edges), Test F capability difference: all present.
- Test G failures: reasons `jump/no_jump=59`, `drop/blocked=1`; per-actor fails
  were bounded (max 20 over 45 s, ~0.44/s) by the cooldown + replan, so no
  repeated same-movement spam.
- 43-63 kills per run: combat still works.

Regressions:
- FFA, 10 NPCs, 35 s: `Match ACTIVE mode=ffa`, 285 kills, 2525 starts, 74
  fails.
- Elimination, 10 NPCs, 45 s: `Match ACTIVE mode=elimination`, match ended
  (`[PERSISTENCE] Match result emitted: mode=elimination winner=blue`), 316
  starts, 1 fail.
- 30 NPCs, funworld3, 40 s: completed in 43.3 s wall time (no stall), 104
  kills, 1036 starts, 11 fails.

## Performance impact

Traversal work is O(1) per NPC per tick (a few float comparisons and a switch);
no pathfinding or broadphase queries run in the traversal layer. Plans remain
rate-limited by the navigator token bucket. The 30-NPC run finished in 43.3 s
for a 40 s timeout with ~0.9 traversal starts per NPC per second, and logging is
event-scale, so CPU and log load stay bounded.

## Regressions found

None.

## Documents and skills

- `AGENTS.md`, `docs/ROUTER.md`
- `docs/specs/movement/movement.md` (shared kernel + capabilities)
- `docs/specs/gamemodes/gamemodes.md`
- `docs/architecture/player-npc-systems/player-npc-systems.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`: PASS; decision/navigation/traversal/
  execution responsibilities are separated and capability data-driven.
- `docs/skills/efficiency-checker-v1.md`: PASS; O(1) per-tick, no hot-loop
  allocations or searches.
- `docs/skills/logging-checker-v1.md`: PASS; event-only traversal logs.

## Human review still needed

- A GUI client on a multi-level map should confirm jump/drop/dash-jump feel and
  that ramps are walked rather than dropped, over several minutes.
- Live human vs NPC parity is unchanged; the traversal layer is NPC-only.

## Smallest logical next phase

Let the navigator annotate gap width and required reach on gap edges so
`select()` can distinguish a dash gap from a dash-jump gap numerically instead
of from `heightDelta` alone, enabling more precise traversal choice (and the
first step toward climb/fly/hover as data-driven traversal types).
