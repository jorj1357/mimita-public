# Actor collision skin and triangle-query performance

- Date: 2026-09-27
- Scope: actor triangle collision and static/moving physical-object contact queries
- Status: implemented and self-tested; in-game corner/seam acceptance remains open

## Changes

- Kept actor triangle collision enabled and retained triangle geometry as the authoritative hitbox/contact source.
- Changed actor triangle velocity response to remove only inward normal velocity, preventing arm/leg/head/weapon contacts from launching the root actor outward.
- Unified actor contact slop with the shared `COLLISION_SKIN` value (`0.02`) instead of the actor collector's separate hardcoded value.
- Reduced broadphase gather padding from `0.25` to the shared collision skin. The query AABB already contains the swept collider geometry, so the larger padding was multiplying narrowphase work near dense geometry.
- Added a per-body-part swept-AABB filter so each arm, leg, head, and weapon only tests gathered world triangles that overlap that part's swept bounds.
- Kept moving physical-entity contacts on the exact entity query path with zero artificial contact skin so actors remain grounded on moving crates.

## Validation

- `mimita-20260927T201300.exe --moving-crate-selftest`: PASS
- `mimita-20260927T201300.exe --actor-triangle-solve-selftest`: PASS
- `mimita-20260927T201300.exe --collision-selftest`: PASS
- Build: unique executable linked successfully.

## Follow-up

- Reproduce the reported seam/corner catch in a live map and capture collision trace counts before adding a seam-specific contact rule. A generic shallow-contact filter was rejected because it broke valid moving-crate support contacts.
- Profile the remaining cost in the physical-entity path: it still builds a temporary entity world and full candidate list per overlapping entity. Phase 2 should add a cached entity broadphase/scratch storage before expanding persistent destructible-object behavior.
