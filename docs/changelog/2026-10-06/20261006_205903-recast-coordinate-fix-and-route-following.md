# Recast coordinate fix and route following

- Status: `P0-P2_NAVIGATION_SLICE_VERIFIED_WITH_HUMAN_REVIEW_PENDING`
- UTC timestamp: `2026-10-06T20:59:03Z`
- Display timezone: `America/New_York`
- Branch: `afad20a-rebuild`
- Scope: unblock Recast/Detour on the real Dust 2 map, prove route quality,
  and let the obtained route drive existing NPC movement behind an explicit
  backend selector. No Counter-Strike tactical behavior or round-loop changes.

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
  - Added build diagnostics (coordinate system, source/walkable triangle
    counts, navmesh poly/vert counts, Recast bounds) and query-stage
    diagnostics (start/dest poly found, refs, nearest points, projection
    distances). Added `buildMilliseconds`.
- `src/npc/recast-navigation.cpp`
  - Z-up/Y-up conversion on all inputs and outputs.
  - Walkable-triangle counting after `rcMarkWalkableTriangles`.
  - `prepare()` now reports build stats only. It no longer performs a
    degenerate center-to-center query that made a successful bake look like a
    successful route.
  - Added `nearestPolyEscalated()`, a bounded ascending nearest-poly search
    (agent box -> 3x box -> 40-unit box) that reports how far a point was
    projected. Off-surface goals are now visible instead of silently snapped.
  - Failure reasons are split into `nearest_polygon_failed_start`,
    `nearest_polygon_failed_dest`, `nearest_polygon_failed_both`,
    `find_path_query_failed`, `path_not_found`, `straight_path_failed`.
- `src/network/server-npcs.cpp`
  - `npc.navmesh.bake-success` now reports `coordinate_system`,
    `walkable_triangles`, `navmesh_polys`, `navmesh_verts`, `recast_bounds_*`,
    and `build_ms`. Removed the misleading `triangle_count`-only claim.
  - The navmesh is prewarmed for `MIMITA_NPC_NAV_BACKEND=recast|compare` so the
    ~1s first bake never lands in a fixed gameplay tick.
- `src/npc/npc-navigator.h` / `.cpp`
  - Explicit backend selector `RecastBackend { custom, compare, recast }` via
    `MIMITA_NPC_NAV_BACKEND` (default `custom`; `MIMITA_NPC_NAV_COMPARE`
    continues to imply `compare`). Backend is never inferred from gamemode.
  - `recast` mode promotes a successful Detour corridor into the existing
    `path`/`pathGap`/`pathCapability` format, so the shared follow, traversal,
    and movement execution code is reused unchanged. If Recast fails, the
    custom plan remains the bounded fallback.
  - Carries all new Recast diagnostics plus `recastAuthoritative`.
- `src/npc/npc.cpp`
  - Emits `npc.nav.result` when Recast is authoritative, otherwise
    `npc.nav.compare`. Records `backend_authoritative`, start/dest poly refs,
    nearest points, and projection distances.

## Build evidence

- `mimita-20261006T-recast-coord-v1.exe` (initial conversion + diagnostics):
  BUILD SUCCESS.
- `mimita-20261006T-recast-coord-v2.exe` (extent escalation + projection):
  BUILD SUCCESS.
- `mimita-20261006T-recast-route-v3.exe` (authoritative route):
  BUILD SUCCESS.
- `mimita-20261006T-recast-route-v4.exe` (prewarm for recast backend):
  BUILD SUCCESS.
- Command: `MIMITA_EXE_NAME=<name> python build_agent.py`.

## Runtime evidence

All runs used the real headless server against the real map.

### P0 bake + route quality

- Executable: `mimita-20261006T-recast-coord-v2.exe`
- Journal: `logs/10-06-2026/20261006_164809/events.jsonl`
- `npc.navmesh.bake-success`: `coordinate_system=mimita_z_up_to_recast_y_up`,
  `walkable_triangles=1339`, `navmesh_polys=1436`, `navmesh_verts=3349`,
  `build_ms≈986`.
- Before the fix the same map produced no usable walkable area. After the fix,
  the compare run produced real Dust 2 routes for actor 1001: 21-23 corridor
  polygons, ~144-151 m path length, `failure=""` (success). `path_not_found`
  no longer appears.
- The only remaining compare failure was `nearest_polygon_failed_dest` for
  actor 1000, whose destination was genuinely outside the navmesh
  (`dest_projection_m` reported; no silent snap). This is a goal/destination
  problem owned by the brain, not an adapter defect.

### P2 route following

- Executable: `mimita-20261006T-recast-route-v4.exe`
- Journal: `logs/10-06-2026/20261006_165838/events.jsonl`
- Command:
  `set MIMITA_NPC_NAV_BACKEND=recast && mimita-20261006T-recast-route-v4.exe --server --bind 127.0.0.1:0 --map dust2cyberiav4 --gamemode counterstrike --npcs 2 --timeout 8 --no-map-rotation --no-discord-notification`
- Event chain: `npc.navmesh.bake-success` (prewarm, `walkable_triangles=1339`,
  `navmesh_polys=1436`) -> `npc.nav.result` records with
  `backend_authoritative=recast_detour` (7) -> `npc.movement-decision` samples
  with `distance_moved>0` and positive `net_progress_toward_goal`.
- Actor 1001 made positive net progress (e.g. `net_progress_toward_goal=6.75`)
  while following the Recast route through the existing shared movement
  executor.
- `npc.nav.compare` (2) remained for requests where Recast failed and the
  custom plan was authoritative, proving bounded fallback.

## Focused checks

- Structured JSONL parsed for every journal inspected.
- Baseline comparison: custom backend produced 175-186 `npc.stuck` events on
  the same scenario; recast backend produced 58-129. The high stuck count is
  pre-existing custom behavior, not a Recast regression.
- No source, configuration, or asset behavior outside navigation was changed.
- Build success is reported separately from runtime behavior, which is reported
  separately from human acceptance.

## Pre-existing work preserved

The worktree still contains unrelated modified/deleted/untracked files from
earlier work (ragdoll logging, gamemode specs, walkthrough docs). They were not
reset, cleaned, deleted, or rewritten.

## Remaining work (not claimed complete)

- P1 isolated query scenarios (same-floor, wall, ramp, invalid start/dest,
  no-path) as named, repeatable isolated evidence.
- The brain frequently selects destinations 10+ m off the navmesh
  (`dest_projection_m`), which ends Recast routes short. Goal selection and
  reachable destination selection need work before the route-quality acceptance
  thresholds can pass.
- Traversal links on Recast routes are `Walk` only in this slice.
- Counter-Strike tactical behavior, TeamBrain coordination, and the core
  round/bomb loop were not touched in this session.
- Dynamic tile updates, DetourCrowd/RVO2, and custom-navigator deletion remain
  later phases.

## Human review still needed

Confirm visually that Dust 2 NPC routes look intentional and that following a
Recast route does not visibly differ from the expected movement. Machine
evidence proves route creation and movement progress only.
