# Hot-reloadable collision package (migration phase 1)

- EST timestamp: 2026-09-17 12:00:00 -04:00
- Branch: `8292026stash`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Goal

Start the migration to a fully hot-reloadable collision system: one generic
collision package under `src/hot-reload/packages/collision/` with a plain-data
ABI, a cached broadphase, and generic solve requests, routed from `movement.main`.
The old per-sphere kernel is kept only as a temporary fallback and is scheduled
for removal after parity proof.

## Changes

- `src/hot-reload/packages/collision/` (new hot package, auto-discovered)
  - `collision-abi.h`: versioned `CollisionSolveV1` request/result, collider,
    contact, and impact plain data, shape/mask/policy/part enums, the
    `collision.main` capability id + signature, and the one bounce policy owner.
  - `collision-world.h` / `collision-package-world.cpp`: hot world triangle
    cache and a multi-resolution index (2-unit fine cells, 8-unit coarse cells,
    `always` list for pathological geometry), one `gatherCandidates` union-AABB
    query with a stamp-deduplicated result, and sphere narrowphase.
  - `collision-package-solver.cpp`: the `collision.main` solve. One union AABB
    for every collider plus the swept move, one shared candidate list, per-shape
    sphere/capsule tests, depenetration, slide, bounce, grounding, contact
    merging, pass limits, contact output, and per-part impact selection.
  - `collision-package-selftest.cpp`: candidate self-test (floor, wall, slope,
    local non-duplicated gather, large triangle, cache rebuild after map change,
    invalid geometry, multi-part shared candidates).
- `build_game_dll.py`
  - Added `headerGlobs` so package headers participate in change detection and
    code hashing without a hand-maintained list.
  - Object stems are now path-derived, so two files with the same basename in
    different package directories never collide on one object file.
- `src/hot-reload/hot-modules.json`
  - Added `src/hot-reload/packages/**/*.cpp` to `globs` and `**/*.h` to
    `headerGlobs`. Adding, deleting, renaming, or splitting a package file now
    changes the package hash and rebuilds automatically.
- `src/hot-reload/modules/movement-system.cpp`
  - `resolveCollisions` now builds the generic collider list (capsule + head,
    torso, arms, legs from `socket.raw`) and calls `collision.main`. If the
    capability is absent it falls back to the old in-DLL kernel, so exactly one
    collision owner mutates the local player per tick.
- `src/hot-reload/hot-collision-kernel.h`,
  `src/hot-reload/modules/collision-policy.cpp`
  - Bounce constants now alias the package ABI (one owner). The old kernel is
    marked as the scheduled-removal fallback.
- `src/effects/effect-part.cpp`
  - Candidate self-test also runs the collision-package invariants.

## Evidence

- Hot build: `python devscripts/live-build.py` produced
  `build/hotreload/mimita-live-g000032.dll`; no `MiMITA.exe` is written.
- Package discovery: `load_manifest` reports 75 sources, including the three
  package `.cpp` files, and 27 headers including both package headers.
- Headless load/validation: `mimita-20260917T151237.exe --live-code-selftest`
  reported `[LIVE CODE SELFTEST] PASS` with `[ok] GameAPI load + ABI + self-test`
  while the package self-test ran.
- Deterministic package tests: floor grounding (capsule resolves to ~0.5, finite
  velocity), wall normal response and no pass-through, tilted slope normal,
  local non-duplicated union gather over 1681 triangles, large-triangle indexing,
  index rebuild after a map change, invalid-NaN geometry safety, and four
  colliders sharing one candidate set.
- The duplicate-symbol link failure found at generation 29 (package
  `collision-solver.cpp` colliding with `modules/collision-solver.cpp`) was fixed
  by unique package filenames and path-derived object stems; the record is kept
  here because it is the exact reason the naming rule exists.
- `git diff --check` passed (exit 0).
- No `.exe` build was required: no manifest `cold` source changed, and no running
  process was killed, restarted, or replaced.

## Specification and behavior review

Skill: `docs/skills/spec-behavior-review-v1.md`.

### Finding 1

- Severity: medium
- Type: spec-code disagreement (package location)
- Specification: the task names `src/hot-reload/packages/collision/`.
- Exact quoted requirement: "Add a package such as:
  `src/hot-reload/packages/collision/`".
- Code path: package files use path-derived object stems and unique basenames
  (`collision-package-*.cpp`).
- Actual behavior: the directory is as requested; filenames are
  package-prefixed.
- Expected behavior: the request allows "such as", so the directory is the
  contract and the file names are free.
- Evidence: generation 29 failed to link until the basename collision was
  removed.
- Recommended wording or implementation action: keep the directory; treat
  basenames as package-local and free to split/rename.
- Human decision required: no.

### Finding 2

- Severity: high
- Type: missing acceptance
- Specification: performance acceptance (no 20-24 ms solve, non-linear scaling).
- Code path: the package has no timing instrumentation yet.
- Actual behavior: the package gathers one union-AABB candidate set and never
  scans the full map, but the required per-phase timing capture is not yet
  wired.
- Expected behavior: separate broadphase/narrowphase/solver/total measurements.
- Evidence: the cache and gather design, but no measured numbers.
- Recommended wording or implementation action: add throttled timing capture to
  the package owner before claiming performance acceptance.
- Human decision required: yes (accept the numbers once produced).

## Documents and focused reviews

- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/architecture/collision/collision.md`
- `docs/architecture/live-development/hot-kernel.md`
- `docs/specs/movement/movement.md`
- `docs/features/live-code-development/live-code-development.md`
- `docs/skills/spec-behavior-review-v1.md`

## Pre-existing unrelated work preserved

- `config/accounts/default.json`, `config/analytics.json`
- `src/config/collision-config.*`, `src/physics/movement/physics-collision-shared.h`,
  `src/engine/engine-tick-setup.cpp` (prior live-collision-bounce work)
- `src/hot-reload/hot-animation-clips.h`,
  `src/hot-reload/modules/presentation/animation-policy.cpp`,
  `src/render/presentation-entities.cpp` (prior animation work)
- Existing `docs/changelog/2026-09-17/` files

## Human review still required

- Runtime: confirm the local player solves correctly through `collision.main`
  (walk, sprint, fall, jump, dash, high-speed movement) and that limb/wall bounces
  look right.
- Multiplayer: the portable hot-world broadphase (ENT/SUB/MAT/TRI) and remote
  entity colliders are not implemented, so the package is not yet active on both
  client and authoritative server.
- Map-change cache rebuild in a live session; entity IDs, world state, network
  session, and player state must remain unchanged after activation.

## Not done (later phases)

- Migrating the literal cold chunk/sub-grid broadphase and its chunk cache into
  the package; this phase implements a portable multi-resolution hot index
  instead.
- Remote players, NPCs, projectiles, weapons, props, ragdolls, triggers.
- Entity-vs-entity collision (mask `COLLISION_MASK_ENTITY` is reserved).
- Performance timing capture and the old/old trace comparison.
- Removing the per-sphere `HotCollisionKernel` fallback and the dead cold
  adapters/timing probes (Section 8) — deferred until parity is proven.
