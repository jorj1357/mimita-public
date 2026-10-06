# MiMITA NPC navigation library evaluation and migration plan

Date: 2026-10-06
Status: Planning only; no implementation is authorized by this document.
Audience: MiMITA developers, reviewers, AI agents, and future navigation work.

## Purpose

Investigate whether an established open-source navigation library can replace
or simplify MiMITA's custom NPC navigation while preserving MiMITA ownership of
gameplay decisions, actor movement, physics, collision, combat, networking, and
server authority.

The desired long-term result is an NPC that can navigate complex maps, follow
moving targets, use cover and useful positions, avoid other actors, recover
from blockage, and eventually use special movement such as jumping, dashing,
air strafing, rocket jumping, climbing, teleporting, and custom traversal.

The navigation system must produce movement intent. It must not become a second
character controller.

## Does not

This plan does not:

- authorize immediate C++ changes;
- replace the NPC brain, perception, targeting, utility goals, or combat AI;
- allow a library to directly set actor position or final velocity;
- make DetourCrowd or any external crowd controller the owner of MiMITA
  movement physics;
- claim that a successful build proves navigation behavior;
- require replacing the current system before a parity experiment succeeds;
- define final gameplay difficulty, personality, aim, weapon, or game-mode
  decisions;
- solve destructible-world navigation in the first implementation phase.

## Authority and related documents

The following documents remain authoritative:

- `docs/ROUTER.md`
- `docs/specs/movement/movement.md`
- `docs/specs/networking/networking.md`
- `docs/specs/performance/performance.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/architecture/player-npc-systems/player-npc-systems.md`
- `docs/architecture/collision/collision.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/documentation-checker-v1.md`
- `docs/regressions/regressions-v1.md`

Existing dated plans use the established filename style `YYYYMMDDplan.md`.
This plan is therefore `docs/specs/20261006plan.md`, beside
`docs/specs/20261004plan.md`. The universal timestamp document's newer
generated-artifact convention applies to new generated artifacts; this plan
keeps the repository's existing dated-plan convention for discoverability.

## Decision summary

The strongest candidate is:

```text
Recast      = build walkable navigation geometry
Detour      = runtime navmesh queries, paths, corridors, filters, links
MiMITA      = goals, tactical decisions, traversal policy, actor input, physics
```

DetourCrowd and RVO2 remain optional later components for local avoidance. They
are not replacements for global navigation or MiMITA's actor simulation.

MicroPather and Boost.Graph are generic graph/A* libraries. They could replace
an algorithmic A* implementation but would leave MiMITA owning the difficult
geometry extraction, multi-floor representation, dynamic updates, route
debugging, and traversal system. They are not the preferred migration target.

## External research

### Recast Navigation

- Repository: https://github.com/recastnavigation/recastnavigation
- Introduction: https://github.com/recastnavigation/recastnavigation/blob/main/Docs/_1_Introduction.md
- License: https://github.com/recastnavigation/recastnavigation/blob/main/License.txt
- Modules: `Recast`, `Detour`, `DetourTileCache`, `DetourCrowd`, and
  `DebugUtils`.

Relevant capabilities:

- rasterize collision geometry into walkable navigation;
- build single or tiled navmeshes;
- query nearest polygons and paths;
- maintain path corridors;
- apply query filters and area/capability flags;
- support user-authored off-mesh connections;
- visualize navmesh and query data;
- rebuild or stream tiles for larger worlds.

Recast/Detour is zlib-licensed and low-dependency. Its own documentation
separates navigation paths from collision geometry and final actor movement,
which matches the MiMITA architecture.

### DetourCrowd

- Repository: https://github.com/recastnavigation/recastnavigation/tree/main/DetourCrowd
- API: https://github.com/recastnavigation/recastnavigation/blob/main/DetourCrowd/Include/DetourCrowd.h

DetourCrowd adds path-corridor management, neighbor queries, desired velocity,
and local obstacle avoidance. It also maintains crowd-agent velocity and
position integration. That integration must not replace MiMITA's shared actor
movement kernel.

Initial use is therefore rejected. A later adapter may consume a crowd
preferred velocity or avoidance suggestion and convert it into `ActorIntent`,
but MiMITA remains responsible for final simulation.

### DetourTileCache

- Source: https://github.com/recastnavigation/recastnavigation/tree/main/DetourTileCache

TileCache supports incremental updates for temporary cylinder, box, and
oriented-box obstacles. It is useful for doors, crates, and temporary blockers.
It does not automatically solve arbitrary destructible triangle changes; those
need MiMITA geometry-change events, affected-tile selection, rebuild scheduling,
and nav-data versioning.

### RVO2 / ORCA

- Repository: https://github.com/snape/RVO2

RVO2 is an Apache-2.0 C++98 local reciprocal-avoidance library. It is useful
for agent-vs-agent separation and dense local crowds, but it does not provide
navmesh generation, stairs, ramps, multi-floor routing, doors, destructible
geometry, or traversal links.

It is a possible later local-avoidance experiment, not the first replacement.

### MicroPather

- Repository: https://github.com/leethomason/MicroPather

MicroPather is a small, simple generic A* solver. It is a credible option for
pathfinding over an already-created graph, but it does not replace MiMITA's
current collision-to-navigation conversion or dynamic surface model.

### Boost.Graph

- Documentation: https://www.boost.org/doc/libs/latest/libs/graph/doc/html/graph/index.html

Boost.Graph provides generic graph representations and algorithms. It is not a
navigation system. It would add generic graph machinery without solving the
hardest MiMITA-specific ownership problems.

## Current MiMITA architecture

Current runtime flow:

```text
server target selection
  -> NPC perception, LOS, memory
  -> utility goal and legacy state selection
  -> NpcGoal
  -> NpcNavigator
       -> NpcNavGraph for long-range/custom graph routes
       -> planLocalPath for rolling-horizon local A*
       -> wall avoidance, commitment, stuck recovery, area escape
  -> NpcTraversalExecutor
  -> ActorIntent / InputState
  -> physicsMainUpdate
  -> shared collision and movement simulation
```

### Current ownership map

| Concept | Current owner | Planned disposition |
|---|---|---|
| Target selection | `src/network/server-npcs.cpp` | Keep in server/brain layer |
| Perception and LOS | `src/npc/npc.cpp`, `npc-perception.cpp`, combat LOS helpers | Keep; consolidate only if proof shows duplicate LOS work |
| Utility goals | `src/npc/npc-utility.*` | Keep in NPC brain |
| Legacy tactical states | `src/npc/npc-state-machine.*`, `npc-states.cpp` | Gradually reduce as intent arbitration becomes explicit |
| Goal translation | `makeNavGoal()` in `src/npc/npc.cpp` | Keep temporarily; move toward `NavigationRequest` |
| Global navigation | `src/npc/npc-nav-graph.*` | Candidate for Recast/Detour replacement |
| Local pathfinding | `planLocalPath()` in `src/npc/npc-navigator.cpp` | Candidate for Detour corridor/straight-path replacement |
| Wall avoidance | `src/npc/npc-navigation.*` | Keep temporarily; later make one explicit local steering owner |
| Stuck detection | `NpcNavigation::isStuck`, state timers, navigator progress, traversal timers | Consolidate after parity |
| Area escape | `NpcNavigator::updateAreaEscape()` | Keep as bounded recovery, then integrate into motor intent arbitration |
| Traversal policy | `src/npc/npc-traversal.*` | Replace inferred height logic with semantic link handling |
| Movement intent | `computeStateMovement`, navigator, traversal, recovery overrides | Consolidate into explicit priority arbitration |
| Physics execution | shared `physicsMainUpdate()` and movement kernel | Keep as sole final movement owner |
| Server authority | `src/network/server-npcs.cpp` and shared server simulation | Keep; navigation must run authoritatively on the server |
| Diagnostics | structured JSONL plus debug logging | Expand with navmesh/corridor/link state |

## Duplicate ownership and regression risks

### Movement direction

`moveDir` can be changed by state movement, navigation, traversal, wall
avoidance, backtracking, stuck recovery, area escape, bomb-tag behavior, mirror
replay, and forced detours. The current order is meaningful but implicit.

### Stuck state

Stuck behavior is measured by several independent timers and progress models:

- `NpcNavigation::isStuck()`;
- `stateMachine.stuckTimer`;
- `patrolNoProgressTimer`;
- navigator commitment progress;
- recent visited/blocked points;
- area-escape timer;
- traversal no-progress counters.

These are useful safeguards, but they must not permanently become separate
owners of “what should the actor do now?”

### Global/local path duplication

`NpcNavGraph` and `planLocalPath()` both reason about walkability, surface
height, gaps, jump capability, and route construction. The migration must make
one navigation backend authoritative before deleting either implementation.

### Traversal inference

`NpcTraversalExecutor` currently infers walk/jump/drop/dash from waypoint height,
slope, distance, and gap flags. This is insufficient for named traversal such
as rocket jump, climb, teleport, or authored custom movement.

### Server correction

The server ground clamp in `src/network/server-npcs.cpp` is a physics safety
correction, not navigation. It must remain clearly separated from route logic.

## Proposed ownership boundary

```text
NPCBrain
  chooses target, objective, combat position, cover, and desired destination

NavigationRequest
  actor id, start, destination, capabilities, team/mode filters, nav version

NavigationSystem
  nearest polygon, route query, corridor, next corner, link metadata,
  route validity, dynamic tile versions, navigation diagnostics

NPCMotor
  converts next corner/link into normal ActorIntent

ActorIntent
  movement, look direction, jump, dash, down-dash, crouch, fire

Shared movement kernel
  final velocity, gravity, collision, movement abilities, knockback, physics
```

The navigation system may return a desired direction or preferred velocity. It
must not write final velocity, position, grounded state, jump state, dash state,
or collision results.

## Traversal design

Navigation links should have semantic types:

- `WALK`
- `JUMP`
- `DROP`
- `DASH`
- `ROCKET_JUMP`
- `TELEPORT`
- `CLIMB`
- `CUSTOM`

Each link should identify:

- start and end positions;
- link id;
- required actor capability;
- preferred input direction;
- maximum attempt time;
- whether failure should replan, retry, or mark the edge unavailable;
- optional gameplay metadata.

The sequence is:

```text
Detour selects link
  -> NPCMotor emits normal input
  -> MiMITA physics attempts traversal
  -> observed actor result determines success/failure
  -> NavigationSystem continues or replans
```

Navigation never simulates a second movement controller.

## Dynamic and destructible geometry

Use the least complex architecture that leaves room for expansion:

1. Bake a static tiled navmesh for current maps.
2. Emit a geometry-change event when a door, crate, or destructible surface changes.
3. Identify affected navmesh tiles.
4. Use temporary obstacle marking for simple blockers.
5. Rebuild affected tiles asynchronously for changed geometry.
6. Publish a new immutable navmesh version.
7. Replan NPCs whose corridor intersects invalidated tiles.
8. Keep a bounded local safety fallback while a tile rebuild is pending.

Do not synchronously rebuild the whole world during a fixed simulation tick.

## Determinism and multiplayer

The authoritative server owns navigation results. Clients receive actor state
and normal replicated movement results; they do not independently decide the
authoritative route.

For deterministic behavior:

- use fixed 60 Hz navigation decisions where gameplay-visible;
- use stable entity-id ordering for multi-agent processing;
- use stable tie-breaking for equal-cost paths;
- quantize or normalize inputs at the MiMITA boundary if required;
- record navmesh version and query inputs in diagnostics;
- never let render-frame timing affect route decisions;
- treat library floating-point behavior as requiring validation, not as
  guaranteed deterministic by default.

## Debugging requirements

Selecting an NPC should expose:

- current goal and target;
- target visibility and LOS state;
- navmesh version;
- current and destination polygon references;
- full corridor or bounded corridor summary;
- path corners and next waypoint;
- desired navigation direction;
- local avoidance contribution;
- actual velocity;
- progress distance;
- stuck timer;
- replan reason;
- failed query status;
- traversal link being attempted.

Suggested world colors:

- green: valid route;
- blue: path corners;
- yellow: current polygon;
- purple: destination;
- orange: avoidance direction;
- red: blocked LOS;
- magenta: traversal link.

Suggested bounded JSONL events:

- `npc.nav.query`
- `npc.nav.plan-created`
- `npc.nav.plan-failed`
- `npc.nav.replan`
- `npc.nav.link-started`
- `npc.nav.link-failed`
- `npc.nav.route-invalidated`
- `npc.nav.stuck`

Events should be emitted on state changes or bounded intervals, not every tick
unless an explicit diagnostic mode is enabled.

## Staged migration plan

### Phase 0 — document and instrument current behavior

Before adding an external backend:

- record which layer changed movement intent;
- record route owner, route length, waypoint, and replan reason;
- count duplicate stuck/recovery decisions;
- preserve current tests and selftests;
- capture representative static-map and Counter-Strike traces.

Exit criteria:

- current behavior has a reproducible diagnostic trace;
- route and movement ownership conflicts are observable;
- no current owner is deleted.

### Phase 1 — add a Recast/Detour backend behind an adapter

Likely additions:

- `src/npc/navigation-system.h/.cpp`;
- `src/npc/navigation-recast.h/.cpp`;
- navmesh build/import adapter;
- Detour query wrapper;
- navmesh version and tile ownership;
- debug export/query helpers.

Add explicit backend selection:

```text
custom
recast
compare
```

Default remains `custom` until the proof experiment passes.

### Phase 2 — smallest static-map experiment

Create one minimal collision world containing:

- a floor;
- a wall;
- a doorway;
- a ramp;
- an upper platform;
- one jump connection.

Run one NPC for 600 fixed ticks through:

```text
NpcGoal -> Detour corridor -> ActorIntent -> shared movement kernel
```

Success requires reaching the destination without teleporting or bypassing
shared physics.

### Phase 3 — parity mode

Run the custom and Recast planners on identical inputs. Only one affects
movement. Compare:

- reachable/unreachable result;
- route length;
- route topology;
- next waypoint;
- traversal capability;
- progress and stuck behavior;
- replan count;
- fixed-seed repeatability.

Do not switch callers merely because both planners return some route.

### Phase 4 — migrate ordinary global routes

Switch normal non-combat global navigation to Recast/Detour. Keep MiMITA local
steering only where it is still proven necessary to convert a corridor into
actor input.

### Phase 5 — delete old global owners

After parity, build, runtime, and human acceptance:

- delete `src/npc/npc-nav-graph.*`;
- delete `planLocalPath()` and its route-construction helpers;
- remove duplicate surface extraction;
- remove obsolete graph settings and fallback flags;
- update tests and diagnostics to name the single owner.

### Phase 6 — consolidate motor intent arbitration

Create one explicit priority order for combat movement, navigation, traversal,
wall avoidance, stuck recovery, and area escape. Each layer produces a request;
one owner resolves the final `ActorIntent`.

### Phase 7 — add semantic off-mesh traversal

Replace height-only inference with named link metadata and capability filters.
The motor attempts links using normal input; the physics result decides whether
the link succeeded.

### Phase 8 — integrate dynamic/destructible updates

Add affected-tile invalidation, asynchronous rebuilds, immutable navmesh
versions, stale-route detection, and bounded pending-rebuild fallback.

### Phase 9 — evaluate local crowd avoidance

Only after measuring real NPC-vs-NPC failures, compare:

- MiMITA local steering;
- DetourCrowd preferred velocity;
- RVO2/ORCA preferred velocity.

The winner must feed MiMITA input and must not own final actor physics.

## Files likely to change

Likely new:

- `src/npc/navigation-system.h/.cpp`;
- `src/npc/navigation-recast.h/.cpp`;
- `src/npc/navigation-debug.h/.cpp`;
- `tests/npc-recast-navigation-test.cpp`;
- pinned Recast/Detour source or dependency files.

Likely modified:

- `src/npc/npc-navigator.*`;
- `src/npc/npc-traversal.*`;
- `src/npc/npc.cpp`;
- `src/npc/npc.h`;
- `src/npc/npc-navigation-settings.*`;
- `src/world/world.*`;
- build files;
- `config/actor-presets/counter_strike.json`;
- `config/npc-difficulty.json`;
- navigation diagnostics and developer-selection tools.

Potential eventual deletions:

- `src/npc/npc-nav-graph.*`;
- local A* route generation;
- duplicate graph/local surface classification;
- permanent direct-navigation fallback;
- redundant stuck timers after a unified progress owner exists;
- height-only traversal heuristics;
- obsolete navigation configuration keys.

## Test plan

### Navigation tests

1. Reach a target around one wall.
2. Pass through a doorway.
3. Traverse stairs and ramps.
4. Navigate multiple floors.
5. Follow a moving target.
6. Replan after a route becomes blocked.
7. Stop pushing into a wall.
8. Recover after being shoved off-route.
9. Execute a jump traversal link.
10. Reject a traversal link when capability is unavailable.
11. Preserve deterministic equal-cost route selection.

### Crowd and scale tests

12. Two NPCs pass each other.
13. Thirty NPCs navigate simultaneously.
14. One hundred NPCs navigate with bounded CPU and allocation behavior.
15. One thousand-agent synthetic query/avoidance benchmark, without claiming
    gameplay support until the real server budget is measured.

### Dynamic-world tests

16. Door closes across an active corridor.
17. Temporary obstacle appears and disappears.
18. Destructible geometry invalidates one tile.
19. NPC replans after asynchronous tile publication.
20. NPC remains safe while a tile rebuild is pending.

Each test must report PASS/FAIL plus map, seed, actor configuration,
navmesh version, query result, route/replan details, traversal attempts,
progress, stuck duration, and relevant JSONL events.

## Smallest useful implementation experiment

The first implementation should be deliberately narrow:

1. Add Recast/Detour as a buildable, isolated dependency.
2. Convert one minimal collision world into one navmesh tile.
3. Query one route around a wall and through a doorway.
4. Return only the next Detour path corner to the existing NPC motor.
5. Run the existing shared movement and collision kernel unchanged.
6. Compare the result with the current custom navigator.

Do not add crowd avoidance, destructible rebuilds, combat positioning, or
advanced traversal in this experiment.

The experiment disproves the approach if it requires Recast/Detour to own
MiMITA physics, cannot preserve fixed-tick/server-authoritative behavior, or
does not reduce the number of MiMITA navigation owners.

## Risks and mitigations

| Risk | Mitigation |
|---|---|
| Dynamic/destructible geometry | Tile invalidation, async rebuilds, versioned nav data |
| Floating-point nondeterminism | Stable ordering, tie-breaks, fixed inputs, parity tests |
| Server/client disagreement | Server owns authoritative queries and route decisions |
| Advanced movement | Semantic off-mesh links feeding normal actor input |
| DetourCrowd bypasses physics | Do not use it as the first movement executor |
| Performance spikes | Bounded tile builds, query budgets, profiling, no synchronous world rebuild |
| 100+ NPC scaling | Measure query, rebuild, avoidance, and allocation budgets separately |
| License/dependency drift | Pin upstream revision and retain license files |
| Stale fallback ownership | `ADD NEW OWNER -> VERIFY -> MIGRATE CALLERS -> DELETE OLD OWNER` |
| Debug opacity | Require polygon, corridor, link, route-version, and replan diagnostics |

## Acceptance gates before implementation expands

Human approval is required for:

1. Recast + Detour as the preferred global navigation backend.
2. Keeping MiMITA physics and ActorIntent as the movement owner.
3. Starting with a static single-tile experiment. 
4. Maintaining custom/Recast compare mode during parity.
5. Treating DetourCrowd/RVO2 as optional later local avoidance.
6. Requiring server-authoritative route decisions and deterministic tie-breaking.

No old navigation owner should be deleted until the relevant phase has source,
build, test, runtime, and human-review evidence recorded separately.
