// 2026-10-01T23:21:21Z (display: 2026-10-01 19:21:21 EDT)
/* purpose
* Record the pass that reverted the full-auto batch regression to the pass-10
* cadence, fixed fragment bounds (default half-extents bug), converted every
* weapon to config-triangle hitboxes through the shared actor path, implemented
* the overlay word wrap, and added an arbitrary-GLB import selftest.
*/

# Task

- Summary: fix the full-auto fps regression; correct fragment sizing; weapon
  triangle hitboxes; overlay wrap; arbitrary GLB objects.
- Status: PASS_WITH_HUMAN_REVIEW
- Branch: `afad20a-rebuild`.
- Result states: build PASS; 7 selftests PASS; full-auto probe back under budget.

# Changes

- **Batch cadence reverted (H0).** `config/destructible-world.json`
  `destruction.cutBatchIntervalTicks` 8 -> 1, `cutBatchMax` 24 -> 4 (the pass-10
  behavior). Full-auto probe: avg ~0.33 ms / max 1.4 ms per flush.
- **Fragment bounds fixed (H1).** `applyFracture` recenters each piece on its own
  origin and sets `halfExtents` from the piece mesh (`meshHalfExtents`). Children
  place via `parentTransform * translate(centroid)`; the primary via `transform`.
  Previously fragments kept the default `halfExtents = 0.5`, so broadphase was
  wrong for large shards and the size-based deletion was meaningless.
- **Fragment cleanup defaults conservative:**
  `fragmentInstantDeleteMaxDimMeters` 0.25 -> 0.15, `fragmentDeleteMaxDimMeters`
  0.5 -> 0.3, `fragmentIdleDeleteSeconds` 5 -> 8 (all in the JSON).
- **Weapon triangle hitboxes (H4).** `config/weaponcollisions.json`: every weapon
  now `source:"boxes"` (a box per weapon; each face 2 triangles). These become
  `weaponColliderMesh` and join the actor-vs-world triangle path.
  `solveActorTriangleCollision` skips `collectBodyWeaponSpheres`/
  `collectBodyWeaponContacts` when the weapon is in triangle mode, so one owner.
- **Overlay word wrap (H5).** `perf-overlay.cpp` `text()` wraps to the right edge.
- **Arbitrary GLB objects (H6).** `--destructible-selftest` imports every
  watertight GLB in `assets/objects/things/physics-objects`, spawns it
  destructible, and cuts it.
- **Irregular-geometry normal (G4).** `resolveWorldContactVelocity` uses the exact
  world-surface normal (`contact.normal`) for the rigid-body impulse instead of
  the rounded-rim `responseNormal`, so a hole rim cannot inject upward velocity.

# Reasoning

- The user explicitly asked to reproduce the pass-10 behavior, expose the
  cut-batch numbers in `config/destructible-world.json`, fix fragment cleanup
  sizes, make weapon hitboxes config triangles reusing the actor collision path,
  fix the overlay wrap, and test arbitrary physics-object GLBs.

# Files changed

- `config/destructible-world.json`, `config/weaponcollisions.json`
- `src/impact/impact-system.cpp`, `destructible-world-config.h`,
  `destructible-selftest.cpp`
- `src/physics/movement/actor-triangle-solver.cpp`
- `src/perf/perf-overlay.cpp`
- `src/physics/physical-entity.cpp`
- `docs/regressions/2026-10-01/physical-objects-collision-REG.md` (Attempt 12)

# Pre-existing (not authored this session)

`config/accounts/default.json`, `config/analytics.json`,
`config/movement/movement-source.json`, `docs/specs/20261001plan.md`,
`src/devtools/dev-log-commands.cpp` were already modified; left untouched.

# Validation

- Build: `python build.py build-only` -> success.
- `--destructible-selftest` (incl. GLB import/cut), `--moving-crate-selftest`,
  `--destruction-replication-selftest`, `--destruction-stress-selftest`,
  `--actor-collision-mesh-selftest`, `--actor-triangle-solve-selftest`,
  `--physical-perf-selftest` PASS.
- Cold-build debt: appended `Cold-build occurrence 46`.

# Human review still needed

- Full-auto into a crate: confirm dips are gone (pass-10 feel).
- Fragments: small chunks removed, large fragments kept, corner detaches.
- Cylinder: hole-side contacts no longer bounce up (G4) — verify.
- Weapons: box hitboxes collide via the shared triangle path; no fling.

# Not done (open)

- H2 detach-stuck-piece; near-cylinder broadphase; true GLB weapon triangles.
