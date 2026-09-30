// 2026-09-30T18:31:46Z (display: 2026-09-30 14:31:46 EDT)
/* purpose
* Record move 1 of the destructible follow-up plan: rigid-body mass, center of
* mass, and inertia now follow the material that remains after each boolean cut.
* Performance work (incremental rebuild, contact cache) and GLB geometry breadth
* remain later moves.
*/

# Task

- Summary: Derive volume, center of mass, and inertia from the closed cut
  surface and feed them into the existing `PhysicalEntity` rigid-body owner
  without changing movement, velocity, or orientation.
- Status: CODE_COMPLETE / BUILD_VERIFIED / SELFTEST_PASS /
  RUNTIME_VISUAL_VALIDATION_REQUIRED
- Branch: `afad20a-rebuild`; base commit `61cd8e92`. The Manifold/boolean
  baseline in the working tree is pre-existing uncommitted work from the prior
  session (`20260930_181601-manifold-boolean-destruction.md`); this session adds
  only the mass-properties slice on top.

# Changes

## New owner: mesh mass properties

- Added `src/physics/mesh-mass-properties.h/.cpp`: the single owner for
  mesh-derived mass properties. Integrates a closed triangle mesh with signed
  tetrahedron (divergence) integrals to get volume, first moment (center of
  mass), and second moments, applies the parallel-axis shift to the center of
  mass, and returns the unit-density inertia diagonal. Inward-wound cavity walls
  subtract automatically. Degenerate, empty, or non-finite input returns
  `valid=false`.

## Destructible geometry cache

- `src/impact/destructible-geometry.h/.cpp`: added `baseVolume`,
  `massCenterOfMass`, and `unitInertiaDiagonal`. `initialize` seeds the pre-cut
  box values; `rebuild` integrates the generated surface once per rebuild and
  stores the result, so the fixed-tick physics path only reads cached values.

## Physics wiring

- `src/physics/physical-entity.h/.cpp`: `refreshBoxMassProperties` renamed to
  `refreshMassProperties`. When an entity is destructible and
  `geometryRevision > 0`, it now sets `mass = density * remainingVolume`,
  `centerOfMass = cached`, and `inertia = (mass / remainingVolume) *
  unitInertiaDiagonal`; otherwise the original box path runs unchanged. Added
  the public `refreshEntityMassProperties` wrapper so a cut can refresh
  immediately.
- `src/impact/impact-system.cpp`: after a successful cut and collision-mesh
  copy, refreshes the entity's mass properties. The rate-limited `[BOOLEAN]`
  diagnostic now also reports mass and center of mass.

## Tests

- `src/impact/destructible-selftest.cpp`: new check 12 verifies the integrator
  against the analytic box (volume 48; unit-density inertia 208/160/80). New
  check 13 verifies a cut removes a proportional amount of mass, moves the
  center of mass away from the hole, leaves `PhysicalEntity::centerOfMass` equal
  to the cached value, and changes inertia.

## Documentation

- `docs/specs/manifold-destructible-integration-plan.md`: marked section 13.1
  implemented (with the diagonal-approximation decision), updated the risks
  list, and narrowed the next-agent instructions to performance and GLB breadth.

# Reasoning

- Specification alignment: `docs/specs/moving-physical-objects/
  moving-physical-objects.md` section 12 states `mass = density × volume`;
  after a cut the remaining volume is the honest volume.
- Reuse the closed mesh the boolean already produces instead of adding a second
  geometry source; integrate once per `geometryRevision` so the 60 Hz refresh
  stays a read (efficiency-checker priority 1: no per-tick integration of up to
  120k triangles).
- Diagonal inertia keeps the existing `PhysicalEntity` single-vector contract;
  the full tensor and principal axes are explicitly out of scope.

# Validation

- Build: incremental MinGW build completed and linked `mimita.exe` after all
  source edits (object files and the executable are newer than every edited
  source). A repeated `python build.py build-only` then reported "Nothing
  changed".
- `mimita.exe --destructible-selftest` -> PASS (31 checks, including the 5 new
  mass/inertia checks; no existing check regressed).
- `mimita.exe --moving-crate-selftest` -> PASS (22 checks, including destructible
  crate fall/rest and off-center push spin).
- `python tools/check-debug-logging.py` -> no new findings in `src/impact/` or
  `src/physics/`; repo-wide pre-existing bypasses unchanged.
- Live visual/runtime acceptance was NOT performed.

# Pre-existing work

- The Manifold dependency, `boolean-mesh.*`, the canonical-base + cut-history
  design, `build.py` Manifold integration, `box-surface` deletion, and the
  handoff document are uncommitted work from the prior session and were
  preserved, not re-authored here.
- Pre-existing uncommitted edits in `config/accounts/default.json`,
  `config/aimbody.json`, `config/analytics.json`,
  `config/procedural-world/rooms/procedural-mimitasizing5.json`,
  `config/ragdoll.json`, `devscripts/dev-launch-modes.json`,
  `devscripts/dev-loop.py`, `src/combat/client-collision-world-view.*`,
  `src/entities/aimbody-config.*`, `src/main.cpp`, `src/network/*`,
  `src/procedural/*`, `src/ragdoll/*`, and the dated changelogs/regressions
  under `docs/` were left untouched.

# Documents and skills reviewed

- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/specs/moving-physical-objects/moving-physical-objects.md` (section 12)
- `docs/specs/manifold-destructible-integration-plan.md` (section 13)
- `docs/architecture/time-and-formatting/time-and-formatting.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/efficiency-checker-v1.md`

# Human review still needed

- Spawn a crate, shoot one side repeatedly, and confirm the crate starts tipping
  from the shifted center of mass rather than behaving like a uniform box.
- Confirm the reported `mass`/`com` in the `[BOOLEAN]` log matches the visible
  remaining material.
- Frame-rate and multiplayer acceptance remain unverified.