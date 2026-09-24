# Geometry primitives: remaining callers routed through `net.geometry`

Date (UTC): 2026-09-23T20:21:00Z
Status: implemented; cold build and selftests verified

## Scope

Route the last local primitive operations through the hot geometry library so
they are editable from the same place as the rest.

## Changes

1. `src/network/server-attack.cpp`: `claimedHitInBodyParts` now runs its
   point-in-body-part-box test through `geom.point.aabb` (the claimed-part
   resolution stays hot via `net.claim-part`).

2. `src/network/movement-validation.cpp`: `rayTriangle` now runs through
   `geom.ray.triangle`; the wall-sweep traversal stays cold.

3. Includes added (`hot-reload/hot-geometry.h`) where needed.

Note: `src/network/server-physical-contact.cpp` already reaches the hot
`sweptPointSphere` via `WeaponExecution::testPhysicalContact`, which is routed
to `geom.swept.point-sphere` (done with the previous geometry change).

## Evidence

- Build (source/build evidence): `python build_agent.py` -> `Status: SUCCESS`,
  `mimita-20260923T202034.exe`.
- Automated tests (test evidence), all PASS: `--live-code-selftest` (geometry
  provider + checks), `--movement-selftest`, `--movement-parity-selftest`,
  `--packet-codec-selftest`, `--snapshot-chunk-selftest`,
  `--hot-authoritative-selftest`, `--lagcomp-history-selftest`.
- Runtime / human acceptance: pending.

## Result

Ray-AABB, ray-triangle, swept point-sphere, point-in-AABB, and closest-point
primitives are all served from `hot-reload/hot-geometry.h`. Every caller
(hitscan body-part traces, world raycast/swept-sphere, physical-contact
detection, movement wall sweep, attack hit-claim volume) resolves them through
`runGeometryPrimitive`, so the math and the primitive set are hot-editable with
no EXE rebuild.
