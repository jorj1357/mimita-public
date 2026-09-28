# Separate collision skin from physical penetration

- Date: 2026-09-28
- Scope: actor triangle contact depth and edge/seam response
- Status: implemented and self-tested; live corner acceptance remains open

## Root cause

- Actor triangle contacts below `collisionSkin` were being inflated to the full skin depth.
- Increasing the skin therefore made contacts act like a spring: edges pushed too hard and bounce could repeat instead of merely providing query/recovery room.

## Fix

- Actor mesh contacts now preserve their real penetration depth.
- Swept crossings receive only the small `edgeTouchTolerance` solver entry depth.
- `collisionSkin` remains a hot-reloadable broadphase/recovery margin in `config/collision.json`, but is no longer used as artificial physical penetration.
- Existing near-zero static edge-touch filtering remains active.

## Validation

- `mimita-20260928T004000.exe --actor-triangle-solve-selftest`: PASS
- `mimita-20260928T004000.exe --moving-crate-selftest`: PASS
- `mimita-20260928T004000.exe --collision-selftest`: PASS
- Unique executable build/link: PASS
