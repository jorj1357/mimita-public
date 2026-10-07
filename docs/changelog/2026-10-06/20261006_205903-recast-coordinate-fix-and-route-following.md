# Recast coordinate fix, isolated route proof, and route following

- Status: `P0_P1_P2_NAVIGATION_SLICE_VERIFIED_WITH_HUMAN_REVIEW_PENDING`
- UTC timestamp: `2026-10-06T21:28:00Z`
- Display timezone: `America/New_York`
- Branch: `afad20a-rebuild`
- Scope: unblock Recast/Detour on the real Dust 2 map (P0), prove isolated
  route quality (P1), and let the obtained route drive existing NPC movement
  behind an explicit backend selector with corridor retention and bounded
  fallback (P2). No Counter-Strike tactical behavior or round-loop changes.

## Root cause found and fixed

MiMITA is Z-up (gravity acts on `.z`; collision floor normals default to
`(0,0,1)`). Recast is Y-up and marks a triangle walkable only when `norm[1]`
exceeds `cos(slope)` (`external/recastnavigation/Recast/Source/Recast.cpp:353`).
The adapter previously fed raw Z-up triangles, so every floor looked like a
vertical wall and the navmesh had no usable walkable area. This was the real
cause of the long-standing `path_not_found`, not missing geometry.

The adapter now converts Z-up -> Y-up with a handedness-preserving rotation
`(x,y,z) -> (x, z, -y)` and converts query results back with
`(x,y,z) -> (x, -z, y)`. The conversion is recorded in the navmesh identity
(`coordinate_system`).

## Source changes

- `src/npc/recast-navigation.h`
  - Build diagnostics (coordinate system, source/walkable triangle counts,
    navmesh poly/vert counts, Recast bounds) and query-stage diagnostics
    (start/dest poly found, refs, nearest points, projection distances) plus
    `buildMilliseconds`.
  - Public `navmeshVersion()` accessor for cached-corridor invalidation.
- `src/npc/recast-navigation.cpp`
  - Z-up/Y-up conversion on all inputs and outputs; walkable-triangle counting.
  - `prepare()` reports build stats only (no degenerate center-to-center query).
  - `nearestPolyEscalated()`: bounded ascending nearest-poly search (agent box
    -> 3x box -> 40-unit box) that reports projection distance instead of
    silently snapping. Split failure reasons: `nearest_polygon_failed_start`,
    `nearest_polygon_failed_dest`, `_both`, `find_path_query_failed`,
    `path_not_found`, `straight_path_failed`.
- `src/network/server-npcs.cpp`
  - `npc.navmesh.bake-success` reports `coordinate_system`,
    `walkable_triangles`, `navmesh_polys`, `navmesh_verts`, `recast_bounds_*`,
    `build_ms`.
  - Prewarms the navmesh for `MIMITA_NPC_NAV_BACKEND=recast|compare`.
  - Added `MIMITA_NPC_NAV_SELFTEST` isolated proof emitting `npc.nav.selftest`
    for `recorded_long_route`, `spawn_to_spawn`, and `outside_map`.
- `src/npc/npc-navigator.h` / `.cpp`
  - Explicit backend selector `custom|compare|recast` via
    `MIMITA_NPC_NAV_BACKEND` (default `custom`); never inferred from gamemode.
  - `recast` mode promotes a successful Detour corridor into the existing
    `path`/`pathGap`/`pathCapability` format; shared follow/traversal/movement
    code is reused unchanged. Custom plan remains bounded fallback.
  - Corridor retention: cached Recast routes carry the navmesh version they
    were planned against (`recastRouteVersion`); a published new version forces
    a `navmesh_stale` replan. Existing `blocked`/`progress`/`finished`/
    `target_moved` replans also apply.
- `src/npc/npc.cpp`
  - Emits `npc.nav.result` when Recast is authoritative, else
    `npc.nav.compare`, with backend, poly refs, nearest points, and projection
    distances.

## Build evidence

- `mimita-20261006T-recast-coord-v1.exe` (conversion + diagnostics): SUCCESS.
- `mimita-20261006T-recast-coord-v2.exe` (extent escalation): SUCCESS.
- `mimita-20261006T-recast-route-v3.exe` (authoritative route): SUCCESS.
- `mimita-20261006T-recast-route-v4.exe` (recast prewarm): SUCCESS.
- `mimita-20261006T-recast-nav-v5.exe` (self-test + stale replan): SUCCESS.
- Command: `MIMITA_EXE_NAME=<name> python build_agent.py`.

## Runtime evidence

### P0 bake + real map route

- Journal: `logs/10-06-2026/20261006_164809/events.jsonl`.
- `npc.navmesh.bake-success`: `coordinate_system=mimita_z_up_to_recast_y_up`,
  `walkable_triangles=1339`, `navmesh_polys=1436`, `navmesh_verts=3349`.
- Real Dust 2 routes for actor 1001: 21-23 corridor polys, ~144-151 m.
  `path_not_found` eliminated.

### P1 isolated route proof

- Journal: `logs/10-06-2026/20261006_172201/events.jsonl`.
- `MIMITA_NPC_NAV_SELFTEST=1`:
  - `recorded_long_route`: success, 21 polys, 146.10 m (the P0 blocker route).
  - `spawn_to_spawn`: success, 60 polys, 389.30 m (full cross-map route).
  - `outside_map`: explicit `nearest_polygon_failed_both` (no silent snap).

### P2 route following

- Journal: `logs/10-06-2026/20261006_172451/events.jsonl`.
- `npc.navmesh.bake-success` prewarm -> `npc.nav.result` with
  `backend_authoritative=recast_detour` (7) -> `npc.movement-decision` samples
  with movement. `npc.nav.compare` (2) records where Recast failed and the
  custom plan stayed authoritative (bounded fallback).
- Stuck baseline: custom backend 175-186 `npc.stuck`; recast backend 58-129.
  The high stuck count is pre-existing custom behavior, not a Recast
  regression.

## Focused checks

- Every inspected journal parsed as valid JSONL.
- Build success is reported separately from runtime behavior, which is
  reported separately from human acceptance.
- No source, config, or asset behavior outside navigation changed.

## Pre-existing work preserved

The worktree still contains unrelated modified/deleted/untracked files from
earlier work (ragdoll logging, gamemode specs, navigation plan docs). They were
not reset, cleaned, deleted, or rewritten.

## Remaining work (not claimed complete)

- The brain still selects destinations 10+ m off the navmesh
  (`dest_projection_m`), which ends Recast routes short and causes stuck/zero
  net progress. Goal and destination selection need work before the route
  acceptance thresholds can pass.
- Traversal links on Recast routes are `Walk` only in this slice.
- P3 Counter-Strike tactical behavior, TeamBrain coordination, and the core
  round/bomb loop were not touched.
- Dynamic tile updates, DetourCrowd/RVO2, and custom-navigator deletion remain
  later phases.

## Human review still needed

Confirm visually that Dust 2 NPC routes look intentional and that following a
Recast route does not visibly differ from expected movement. Machine evidence
proves route creation and movement intent only.
