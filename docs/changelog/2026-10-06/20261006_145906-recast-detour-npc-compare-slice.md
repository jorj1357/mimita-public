# Recast/Detour NPC compare slice

Time: 2026-10-06 14:59:06 EDT
Branch: current working tree

## Outcome

PARTIAL_PHASE_1_COMPLETE_WITH_ROUTE_QUALITY_BLOCKER

Implemented the first safe migration slice from the NPC navigation plan. The
vendored Recast, Detour, and DetourTileCache sources are now compiled and
linked by MiMITA. A MiMITA-owned adapter can build a navmesh from the
authoritative server collision mesh and query Detour without exposing external
types to NPC gameplay code.

The existing custom navigator remains authoritative. Recast/Detour is queried
only when `MIMITA_NPC_NAV_COMPARE=1`, and its result is written to the canonical
`events.jsonl` stream.

## Source changes

- `build.py`: added Recast/Detour include paths, vendored source discovery,
  isolated object paths, and compilation/linking for Recast, Detour, and
  DetourTileCache.
- `src/npc/recast-navigation.h` and
  `src/npc/recast-navigation.cpp`: added the MiMITA-owned agent profile,
  navmesh preparation, collision-triangle rasterization, Detour mesh creation,
  nearest-polygon/path/corridor queries, versioning, invalidation, and
  diagnostics boundary.
- `src/npc/npc-navigator.*`: added opt-in compare queries using the same NPC
  route request while preserving the custom route.
- `src/npc/npc.cpp`: added bounded `npc.nav.compare` records with backend,
  availability, success/failure, navmesh version, path length, polygon count,
  destination, and query timing.
- `src/network/server-npcs.cpp`: prewarms the Recast representation after the
  server collision world is built and emits `npc.navmesh.bake-success` or
  `npc.navmesh.bake-failed`.

## Build evidence

- Executable: `mimita-20261006T-npc-nav-compare-v3.exe`
- Command: `MIMITA_EXE_NAME=mimita-20261006T-npc-nav-compare-v3.exe python build_agent.py`
- Status: SUCCESS
- Compiled: 1 translation unit after the initial full integration build
- Vendored Recast/Detour translation units: linked successfully

## Runtime evidence

`--versioninfo` identified the new executable and canonical journal:

- `mimita-20261006T-npc-nav-compare-v3.exe`
- `logs/10-06-2026/20261006_145843/events.jsonl`

The bounded server run used:

`--server --bind 127.0.0.1:0 --map dust2cyberiav4 --gamemode counterstrike --npcs 2 --timeout 5 --no-map-rotation --no-discord-notification`

with `MIMITA_NPC_NAV_COMPARE=1`.

Observed journal evidence:

- `npc.navmesh.bake-success` recorded `available=true`, version `1`, and
  `triangle_count=7442`.
- The first bake took approximately `1315 ms`, but it occurred during server
  world preparation rather than the first fixed NPC tick.
- `npc.nav.compare` records were emitted for live NPC route requests.
- At least one Detour query succeeded with a one-polygon route.
- Most long Dust 2 route requests returned `path_not_found`.
- The custom route continued to be created and executed, proving fallback/
  authority separation was preserved.
- The server remained alive and produced normal NPC movement and performance
  records through the bounded run.

## Remaining blocker

This is not ready for Recast promotion or deletion of the custom navigator.
The Dust 2 navmesh/query path still needs route-connectivity investigation:
profile projection, Recast region/erosion parameters, map collision geometry
coverage, path-polygon limits, and multi-surface connectivity must be checked
against the recorded `path_not_found` requests. The next slice should improve
the adapter/query diagnostics and make a real Dust 2 spawn-to-tactical route
successful in compare mode before any movement authority changes.

No DetourCrowd/RVO2 integration, dynamic tile updates, traversal-link
migration, tactical Counter-Strike behavior rewrite, or custom-navigation
deletion was claimed or performed in this slice.
