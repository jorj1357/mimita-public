// 2026-09-30T18:16:01Z (display: 2026-09-30 14:16:01 EDT)
/* purpose
* Record the vendoring of Manifold, the MiMITA boolean wrapper, the first true
* 3D boolean-destruction gameplay slice, the build integration, and the
* handoff document. Arbitrary-mesh breadth, mass/inertia, fracture, and
* multiplayer cut replication remain later slices.
*/

# Task

- Summary: Replace the coarse surface-nets crate cut with a real Manifold
  boolean subtraction behind a MiMITA-owned wrapper, wire it into the existing
  impact/physics path without replacing `PhysicalEntity`, integrate the
  dependency into `build.py`, and write the integration handoff document.
- Status: CODE_COMPLETE / BUILD_VERIFIED / SELFTEST_PASS /
  RUNTIME_VISUAL_AND_MULTIPLAYER_VALIDATION_REQUIRED
- Branch: `afad20a-rebuild`; base commit `61cd8e92`.

# Changes

## Dependency (Milestone A)

- Added `external/manifold` as a git submodule pinned to release **v3.5.4**,
  commit `ce50d78021d64507f89e8c9fc2c2e51018117857`. `.gitmodules` updated.
- License: Apache-2.0 (`external/manifold/LICENSE`); no NOTICE file.
- Built a static, Release, C++-API-only library with the game's GCC 16.2.0
  (CMake 4.2.0 + Ninja), flags `-DBUILD_SHARED_LIBS=OFF -DMANIFOLD_PAR=OFF
  -DMANIFOLD_TEST=OFF -DMANIFOLD_CBIND=OFF -DMANIFOLD_PYBIND=OFF
  -DMANIFOLD_CROSS_SECTION=OFF -DMANIFOLD_DEBUG=OFF`. Output
  `external/manifold-prebuilt/lib/libmanifold.a`; consumer define
  `MANIFOLD_PAR=-1`.
- `.gitignore`: ignore the regenerable `external/manifold-prebuilt/`.
- Added `tools/build_manifold.py` to reproduce the build and report the pinned
  commit.
- Added `tools/manifold_probe.cpp` (standalone binary) proving a 12-triangle
  cube imports clean, `cube - Sphere(0.3)` is valid, volume decreases, and the
  result is closed.

## Wrapper (Milestone B)

- Added `src/impact/boolean-mesh.h` (plain MiMITA types: `BooleanMeshVertex`,
  `BooleanMesh`, `BooleanCutterType`, `BooleanCutter`, `BooleanError`,
  `BooleanCutResult`).
- Added `src/impact/boolean-mesh.cpp`, the only file including `<manifold/...>`.
  Converts MiMITA meshes to `MeshGL`, subtracts (sphere and synthesized capsule),
  converts back with outward winding, preserves base-run UVs, applies a
  triplanar fallback UV for new surfaces, and maps Manifold errors to
  `BooleanError`.
- `booleanSubtractAll(base, cutters)` replays the authoritative cut history
  in memory; a previous output is never re-imported.

## Gameplay wiring (Milestone C)

- `src/impact/destructible-geometry.h/.cpp`: `DestructibleGeometry` now stores a
  canonical `BooleanMesh baseMesh` plus `std::vector<DestructionCut>` (cutter +
  id/source/prediction/energy). `rebuild` calls `booleanSubtractAll`, converts to
  `renderVertices`/`collisionTriangles`, updates volume/component/shell
  diagnostics, and increments `geometryRevision`. A failed boolean rolls back
  the cut.
- Deleted `src/impact/box-surface.h` (tracked) and `src/impact/box-surface.cpp`
  (untracked). This was the whole-crate-shrink / misplaced-hole cause.
- `src/impact/impact-system.cpp/.h`: `submit` builds a `BooleanCutter` in local
  space, de-duplicates by `(sourceEntityId, local center)`, applies the cut,
  copies `collisionTriangles` into `localTriangles`, and logs bounded
  diagnostics. `ImpactResult` gained `triangleCount`, `componentCount`,
  `remainingVolume`, `error`.
- `src/physics/physical-entity.cpp`: the destructible self-test now builds a
  `DestructionCut`/`BooleanCutter`. No rigid-body behavior changed.
- `src/impact/destructible-selftest.cpp`: updated to the cut-history model and
  added the regression check "localized cut does not shrink the whole crate"
  (corner stays at the authored half extent).

## Build integration (Milestone D)

- `build.py`: added `-Iexternal/manifold-prebuilt/include`, `-DMANIFOLD_PAR=-1`,
  and `external/manifold-prebuilt/lib/libmanifold.a` to the link, with a clear
  failure message if the prebuilt library is missing.
- Per-cut diagnostics at the boolean owner: successful cuts rate-limited (every
  4th); failures always recorded via `Debug::error` with entity, source, radius,
  cut count, triangle count, volume, shell/component counts, and boolean ms.

## Documentation (Milestone F)

- Added `docs/specs/manifold-destructible-integration-plan.md`: dependency
  record, verified API, architecture map, ownership rules, wrapper contract,
  local-space flow, first slice, prediction/replication determinism with a
  concrete cut-event packet design, texture handling, diagnostics/performance,
  acceptance tests, and follow-up notes for mass/inertia, performance, and GLB
  breadth.

# Reasoning

The old surface-nets grid was bounded exactly to the crate with no outside-air
border, so zero-valued boundary samples eroded the shell (crate shrank) and the
coarse whole-box grid lost small holes. Manifold guarantees a closed manifold
boolean result, so the surface is regenerated exactly from the canonical base
plus the ordered cut history. Keeping the history authoritative (rather than the
output mesh) avoids drift and lets both prediction and authority reproduce the
same geometry.

# Validation

- Dependency build: `python tools/build_manifold.py` -> SUCCESS; commit
  `ce50d780...`; installed `external/manifold-prebuilt/lib/libmanifold.a`.
- Probe: `build/manifold_probe.exe` -> PROBE PASS.
- Wrapper check: `build/boolean_mesh_check.exe` -> WRAPPER PASS (12-tri base,
  sphere cut, capsule tunnel, non-touching no-op, idempotent replay).
- MiMITA build: `python build.py build-only` -> BUILD SUCCESS; `mimita.exe`
  linked.
- `mimita.exe --destructible-selftest` -> PASS, including "localized cut does
  not shrink the whole crate", pass-through hole, winding, and triangle budget.
- `mimita.exe --moving-crate-selftest` -> PASS (fall/rest/collide/push/sleep).
- Live visual and multiplayer acceptance were NOT performed.

# Pre-existing work

- Pre-existing uncommitted edits in `config/accounts/default.json`,
  `config/aimbody.json`, `config/analytics.json`,
  `config/procedural-world/rooms/procedural-mimitasizing5.json`,
  `config/ragdoll.json`, `devscripts/dev-launch-modes.json`,
  `devscripts/dev-loop.py`, `docs/specs/20260930plan.md`,
  `src/combat/client-collision-world-view.*`, `src/entities/aimbody-config.*`,
  `src/network/*`, `src/procedural/*`, `src/ragdoll/*`, and `src/main.cpp` were
  preserved and not modified by this session's boolean work.

# Documents and skills reviewed

- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/specs/destructible-world/destructible-world.md`
- `docs/specs/moving-physical-objects/moving-physical-objects.md`
- `docs/specs/20260930plan.md`
- `docs/architecture/collision/collision.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
- `docs/skills/documentation-checker-v1.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/efficiency-checker-v1.md`

# Human review still needed

- Spawn a crate in-game and confirm one shot makes a correctly placed hole, that
  repeated shots deepen it, and that a through-hole can be walked/shot through.
- Confirm frame rate near the crate stays acceptable under a long burst (the
  boolean rebuild currently replays the cut history).
- Confirm separate-process multiplayer behavior once a cut-event packet exists;
  it is not implemented yet.
