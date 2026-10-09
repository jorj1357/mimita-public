# Recast stage diagnostics for Juggernaut map loading

## Scope

Added bounded owner-level StructuredLogger diagnostics to the Recast navigation
builder. This is diagnostic instrumentation only; it does not change the
navigation algorithm or Juggernaut behavior.

## Source

- `src/npc/recast-navigation.h`: records heightfield, compact-heightfield,
  region, contour, polygon, and Detour-stage counters.
- `src/npc/recast-navigation.cpp`: records per-filter walkable spans,
  compact connectivity, distance-field maximum, region assignment, and final
  mesh counts in `npc.navmesh.prepare-failed`.

## Build evidence

- `mimita-20261009T034200.exe` built successfully on 2026-10-08.
- The touched `src/npc/recast-navigation.cpp` translation unit compiled;
  the canonical linker completed successfully.
- `--versioninfo` identified the executable and structured journal path before
  the runtime scenario.

## Runtime evidence

Fresh server run:

- Executable: `mimita-20261009T034200.exe`
- Scenario: `--server --map juggernaut3 --mode juggernaut --npcs 3`
- Journal: `logs/10-08-2026/20261008_232904/events.jsonl`
- Map load succeeded with 19,982 collision triangles and 2 spawn points.
- 3,373 source triangles were slope-walkable.
- 17,677,674 walkable compact spans survived erosion.
- 17,677,674 spans had neighbor connections; zero were isolated.
- `compact_max_distance=65535`, `compact_region_count=0`, and
  `compact_assigned_region_span_count=0`.
- Contour count and navmesh polygon count were both zero, followed by
  `detour_data_failed`.

Control run:

- `juggernaut2` on the same executable and mode completed navigation baking
  successfully with 1,856 navmesh polygons.
- Journal: `logs/10-08-2026/20261008_232937/events.jsonl`

## Current conclusion

The GLB loader and collision extraction are not the failing owners. The
Juggernaut3-specific failure is in Recast distance/region generation: the
distance field saturates at `65535`, watershed region assignment produces no
regions, and the empty contour set causes Detour mesh creation to fail. A
likely fix should address the map's Recast scale/heightfield representation or
the distance-field/region input, then be verified with the same stage counters.

## Review boundary

No behavior fix or map asset change was made in this diagnostic step. Human
runtime acceptance remains open.
