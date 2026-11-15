# Map objects shift on generation activation

Date: 2026-11-15
Status: FIX IN PROGRESS — bases now owned by resident World
Owner: map authored geometry

## Symptom

A long block near the payload spawn/cylinder moves when a new DLL becomes ready
for activation. The cart GLB is also missing.

## Suspected owner

`src/map/map-authored-geometry.cpp` applies GLB object target positions from
base positions. Reapplying overrides to already-moved vertices or reloading the
map on activation can shift objects.

## Implemented fix

- `World` now owns `authoredGLBBases` (pristine transform/positions/normals),
  keyed by stable GLB object id; it survives hot generation swaps.
- `MapGLBAuthoredState` is seeded from the resident base after each swap.
- Map load/replace clears the resident base store.
- Inverse-transform recovery remains only as a legacy fallback.

## Required fix

- Capture base positions once per map asset hash.
- Apply overrides idempotently from base positions.
- Do not reload/reapply map objects unless the map asset/revision changed.
- Preserve resident map state across generation swaps.
- Add `map.object.stability` and `payload.cart.bind` evidence.
