# NPC navigation library investigation

Date (UTC): 2026-10-06
EST timestamp: 2026-10-06 00:58:04 EDT
Branch: `afad20a-rebuild`
Commit at investigation start: `51a63c5a`

## Result

`PASS_WITH_HUMAN_REVIEW`

Investigation and architecture planning only. No C++ gameplay code, build
configuration, or runtime configuration was changed. The only repository
change is this changelog required by the project workflow.

## Scope

Inspected the current NPC navigation, movement, perception, targeting,
traversal, diagnostics, tests, Counter-Strike integration, and current
documentation. Researched Recast Navigation/Detour, DetourCrowd,
DetourTileCache, RVO2, MicroPather, and Boost.Graph from primary project
repositories and documentation. Produced the recommendation and staged
migration plan in the accompanying task response.

## Key conclusion

Recast + Detour is the strongest replacement boundary for world-derived
walkability, global path queries, path corridors, tiled navigation, and
off-mesh links. DetourCrowd should not initially own MiMITA actor movement;
its integration performs its own crowd-agent velocity/position update. RVO2
is a possible later local-avoidance experiment, not a global navigation
replacement. MicroPather and Boost.Graph are generic graph solvers and would
leave the difficult geometry/navigation ownership inside MiMITA.

## Current source evidence inspected

- `src/npc/npc.cpp`: target sensing, utility-to-goal mapping, state movement,
  navigator invocation, traversal invocation, wall avoidance, backtrack,
  stuck recovery, area escape, ActorIntent conversion, and shared
  `physicsMainUpdate` execution.
- `src/npc/npc-navigator.{h,cpp}`: local rolling-horizon planner, lazy graph
  integration, route cache, commitment, recent-path memory, replanning, and
  area escape.
- `src/npc/npc-nav-graph.{h,cpp}`: custom lazy chunked collision voxel graph
  and A* route generation.
- `src/npc/npc-navigation.{h,cpp}`: raycasts, wall avoidance, stuck checks,
  obstacle probes, climbable-wall checks, cover probing, and ground queries.
- `src/npc/npc-traversal.{h,cpp}`: walk/jump/drop/dash/dash-jump selection and
  input emission, but no stable authored off-mesh-link identity.
- `src/npc/npc-state-machine.{h,cpp}`, `src/npc/npc-states.cpp`: legacy state
  selection and tactical movement generation still remain active owners.
- `src/npc/npc-perception.{h,cpp}`, `src/npc/npc-combat.cpp`, and
  `src/network/server-npcs.cpp`: perception/LOS and server-authoritative
  target selection/mirroring.
- `src/physics/movement/*` and `src/physics/physics-mini.cpp`: shared actor
  movement and collision execution used by NPCs after intent conversion.
- `tests/npc-navigation-test.cpp`, `tests/npc-movement-policy-test.cpp`,
  `tests/npc-movement-executor-test.cpp`, and the in-binary NPC selftests.

## Documentation and focused review inputs

- `docs/ROUTER.md`
- `docs/specs/movement/movement.md`
- `docs/specs/networking/networking.md`
- `docs/specs/performance/performance.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/architecture/player-npc-systems/player-npc-systems.md`
- `docs/regressions/2026-10-04/counterstrike-npc-targeting-movement-REG.md`
- `docs/changelog/2026-10-05/20261005_210108-layered-npc-navigation.md`
- `docs/changelog/2026-10-05/20261005_234827-npc-area-escape.md`
- `docs/changelog/2026-10-05/20261005_232354-npc-corner-oscillation-fix.md`
- `docs/architecture/player-npc-systems/npc-movement.md` contains the
  unresolved TODO at line 3: `jorj todo - explain`.

## Validation

Repository inspection and external-source research completed. No build or
runtime acceptance was run because no code was changed and the requested
boundary was investigation before implementation.

## Pre-existing worktree state

The worktree was already substantially modified before this investigation,
including audio, configuration, world/collision, server-NPC, and unrelated
asset changes. Those edits were preserved and not attributed to this work.

## Human review still needed

Before implementation, approve the proposed Recast/Detour boundary, the
server-authoritative nav-data ownership model, the initial static-map proof,
and whether deterministic path tie-breaking must be a hard compatibility
requirement for replay/network debugging.
