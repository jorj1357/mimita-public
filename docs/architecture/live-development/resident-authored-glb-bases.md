# Resident authored GLB bases

Date: 2026-11-15

## Problem

Authored GLB overrides move contiguous vertex ranges from their pristine base
positions. `MapGLBAuthoredState` lived in the hot game DLL, so a generation swap
recreated it empty. Reapplying overrides then captured already-moved vertices as
the "base" and shifted the object again.

## Owner

`World` is resident (`StateObjectId::World`). It now owns
`AuthoredGLBBase` records keyed by stable GLB object id:

- `transform`: the base model transform;
- `positions`: pristine per-vertex positions;
- `normals`: pristine per-vertex normals.

`MapGLBAuthoredState` remains a hot-side accelerator. It is seeded from the
resident record after a swap and writes the record back after capture.

## Rules

- Clear resident bases only when the map asset is replaced.
- Never capture a base from vertices that have already been authored.
- Apply overrides from the resident base, not from the current rendered mesh.
- Keep inverse-transform recovery only as a legacy fallback for worlds loaded
  before the resident record existed.
- Do not store base arrays inside `MapGLBAuthoredState` as the only copy.

## Acceptance

- Activating a new game generation does not move authored objects.
- `map.glb-object.applied` reports the same target position before and after a
  swap with unchanged map config.
- A map reload clears old bases and captures the new map's pristine bases.
