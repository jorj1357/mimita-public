// 2026-09-30T11:35:00-04:00
/* purpose
* Record the first implementation slice for true 3D boolean crate cuts and
* local projectile prediction. Arbitrary projectile shapes and fracture pieces
* remain later slices.
*/

# Task

- Summary: Replace the planar box-face hole approximation with a signed-distance
  boolean surface for a destructible crate, and let the local client query and
  predict physical-entity projectile impacts.
- Status: CODE_COMPLETE / BUILD_VERIFIED / SELFTEST_PASS /
  RUNTIME_VISUAL_VALIDATION_REQUIRED

# Changes

- Added `src/impact/box-surface.cpp` as a real 3D surface-nets boolean builder.
  It evaluates the box minus stored spherical cuts, generates the boundary
  between solid and empty space, and writes the same triangle soup used by
  rendering and collision.
- Added client physical-entity swept-triangle queries in
  `src/combat/client-collision-world-view.h/.cpp`.
- Added local projectile-rifle entity-impact prediction in
  `src/network/multiplayer-projectiles.cpp`, using the shared `ImpactSystem`.
- Added duplicate-cut protection in `src/impact/impact-system.cpp` for the
  listen-server prediction/authority case.

# Scope limits

- The current first slice supports spherical cuts, including real 3D cavities
  and pass-through geometry when the removed volume reaches through the box.
- It does not yet support arbitrary projectile meshes, swept capsule cuts,
  adaptive tiny 0.01 m local refinement, client/server cut packets for
  separate processes, mass/inertia recomputation after material removal, or
  connected-piece fracture.
- The whole-box grid is intentionally bounded for human-scale testing. Tiny
  cuts need the planned adaptive refinement pass rather than increasing the
  whole crate resolution.

# Validation

- `python build.py build-only`: SUCCESS; executable linked.
- `mimita.exe --destructible-selftest`: PASS, including empty cut volume,
  pass-through ray, low triangle count, winding, and projectile EntityImpact.
- `mimita.exe --moving-crate-selftest`: PASS.
- `git diff --check`: PASS; only line-ending warnings for pre-existing or
  touched Windows text files.
- Live in-game visual acceptance was not performed. Human review still needs
  to confirm that a client sees the predicted cavity, can walk through a
  through-hole, and does not experience a frame-rate collapse.

# Pre-existing work

- Existing unrelated edits in `config/accounts/default.json`,
  `config/ragdoll.json`, `docs/changelog/2026-09-30/`,
  `docs/regressions/2026-09-20/`, and `src/ragdoll/` were preserved.
