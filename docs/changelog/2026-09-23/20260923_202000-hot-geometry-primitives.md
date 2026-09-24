# Hot geometry primitives (`net.geometry`)

Date (UTC): 2026-09-23T20:20:00Z
Status: implemented; cold build and selftests verified; hot provider resolved and exercised

## Scope

Make the shared geometry primitives hot-editable so their math can change and new
primitives can be added live, without a rebuild.

## Changes

1. **New `src/hot-reload/hot-geometry.h`**: the generic POD query
   (`GameGeometryQueryV1` — ray/segment/box/triangle/sphere/point in one struct),
   the primitive descriptor (`GameGeometryPrimitiveV1`), the lookup doorway, the
   primitive id hashes, and the ONE shared implementation
   (`HotGeometryImpl`) of:
   - `geom.ray.aabb` (ray vs AABB)
   - `geom.ray.triangle` (Möller–Trumbore)
   - `geom.swept.point-sphere` (swept point vs sphere)
   - `geom.point.aabb` (point containment)
   - `geom.closest.segment-point` (closest point on a segment)

2. **New `src/hot-reload/modules/geometry-primitives.cpp`**: registers the
   `net.geometry` capability with a primitive lookup table. Adding a new primitive
   is one more table entry (a hot source edit), no EXE call site.

3. **New `src/hot-reload/hot-geometry-dispatch.cpp`**: cold `runGeometryPrimitive`
   resolves the hot provider by capability id and calls the named primitive,
   falling back to the shared implementation when no package is loaded.

4. **Cold callers rewired to the doorway** (primitive math only; traversal and
   callers stay cold):
   - `src/combat/weapon-execution.cpp`: `rayAabb`, `closestPointOnSegment`,
     `sweptPointSphere` (used by hitscan AABB traces and physical-contact tests).
   - `src/network/server-packets.cpp`: `serverRayTriangle` (used by the world
     raycast/swept-sphere traces).

5. **Manifest + selftest**: added `hot-geometry.h` to `headers`; added
   `--live-code-selftest` checks (provider resolves, ray.aabb hit distance,
   point.aabb containment).

## Evidence

- Build (source/build evidence): `python build_agent.py` -> `Status: SUCCESS`,
  `mimita-20260923T201611.exe`.
- Automated tests (test evidence), all PASS: `--live-code-selftest` (with
  `[CAPABILITY_RESOLVED] provider=net.geometry` and the new checks),
  `--packet-codec-selftest`, `--snapshot-chunk-selftest`, `--movement-selftest`,
  `--movement-parity-selftest`, `--hot-authoritative-selftest`,
  `--actor-lifecycle-selftest`, `--npc-entity-selftest`,
  `--transport-generation-selftest`, `--lagcomp-history-selftest`.
- Runtime / human acceptance: pending.

## Notes

- The broadphase/DDA traversal (collision chunks, world iteration) is mechanism
  and stays cold; only the per-triangle/per-box/per-point math is hot.
- Editing `hot-reload/hot-geometry.h` (in the manifest `headers`) hot-rebuilds
  the DLL; the cold dispatcher then serves the new math at the next activation.
  A new primitive id + table entry is likewise live with no EXE call site.
