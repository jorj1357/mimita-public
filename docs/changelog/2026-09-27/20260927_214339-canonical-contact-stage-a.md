// 2026-09-27T21:43:39Z
/* purpose
* record Stage A of the unified collision/contact migration: promote
* MovementContact to the one canonical contact vocabulary, add adapter
* conversions from RecoveryContact/SweepHit, enrich the body/weapon producer,
* and wire the single CPU-only weapon loader for the local player
* separate build, deterministic-test, and runtime evidence from human review
* does NOT claim the active local collision path changed or was accepted
* does NOT implement limb/world response unification or moving entities
*/

# Task

- Summary: Stage A of the unified collision/contact migration. `MovementContact`
  is now the canonical contact; producer-specific results convert through
  adapters; the body/weapon producer emits canonical metadata; the local
  player's weapon render-mesh triangles are exposed through the one shared
  CPU-only loader. Default `doCollisions` behavior is unchanged.
- Status: CODE_COMPLETE / DEV_BUILD_VERIFIED / DETERMINISTIC_TEST_PASS /
  HUMAN_REVIEW_REQUIRED
- Date, time, timezone: 2026-09-27T21:43:39Z, ISO 8601 UTC
- Branch: `afad20a-rebuild`
- Commits: none from this session; changes are in the working tree.

# Pre-existing / concurrent changes (not mine)

- A concurrent session is active in the same working tree. It already
  committed `635b4758` and `c1fa36d8` and, during this session, edited
  `physics-collision-shared.h`, `physics-collision-mesh.cpp`,
  `actor-triangle-solver.{h,cpp}`, `actor-collision-mesh.{h,cpp}`,
  `physics-collision-glb-main.cpp`, `player.h`, and
  `config/collision-config.{h,cpp}` (Phase 6/7 work). None of those edits were
  reverted or claimed here.
- The concurrent session added the opt-in `actorTriangleSolver` config switch
  (`collision-config`, default false) and `runActorTriangleCollisionStep`, which
  bypasses the legacy pipeline only when explicitly enabled. Stage A leaves the
  default (legacy) path in place.
- `config/accounts/default.json` and `config/analytics.json` were modified by
  others; untouched.
- The unrelated deletions (`.opencode.disabled/`, `.opencode/plans/`,
  `.github/workflows/sign-release.yml`, `.signpath/...`) were preserved.

# Source changes

## Modified

- `src/physics/movement/movement-types.h` — added `MovementShapeKind` and
  `MovementSubshape`; `MovementContact` gained `shapeKind`, `subshape`,
  `targetEntityId`, `materialId`, and `sweepVelocity`. Contact identity helpers
  were not changed, so dedup behavior is identical.
- `src/physics/movement/physics-collision.h` — includes `movement-types.h`;
  declares `movementSubshapeFromLabel`, `movementContactFromRecoveryContact`,
  `movementContactFromSweepHit`, and `canonicalContactSelfTest`.
- `src/physics/movement/physics-collision-core.cpp` — implements the adapters;
  added `<cstring>`.
- `src/physics/movement/physics-collision-glb-body.cpp` — the body/weapon contact
  loop now builds the canonical `MovementContact` through the adapter (same
  kind/source/point/normal/penetration/surfaceId; adds shape/subshape/sweep) and
  logs canonical fields when `COLLISION_VERBOSE`.
- `src/physics/movement/physics-collision-stress.cpp` — added
  `canonicalContactSelfTest`.
- `src/game/game-cli.cpp` — added `--canonical-contact-selftest`.
- `src/combat/weapon-viewmodel.cpp` — after `loadModel(modelPath)`, calls
  `ensureActorWeaponColliderMesh(player, modelPath)` when
  `CollisionConfig::bodyMeshCollision()` is on.

## Added

- None. `MovementContact` was promoted rather than introducing a second
  `PhysicalContact` type, per the recorded user decision.

# Specification alignment

- `docs/specs/movement/movement.md` requires one collision meaning and the
  existing local collision feel preserved. Stage A adds contact vocabulary and
  metadata without changing positions, velocities, grounding, or ordering.
- `docs/architecture/collision/collision.md` "one concept = one owner": the one
  contact owner is now `MovementContact`, with adapters at the producer boundary.
- `docs/architecture/collision/actor-triangle-owner-inventory.md` gained a
  "Stage A" section.

# Exact implementation changes

- Canonical fields default to Unknown/0/zero, so every existing caller is
  byte-for-byte behaviorally equivalent.
- `movementContactFromRecoveryContact` maps `triangleIndex` to `surfaceId`
  (`index + 1`, `0` for `-1`), `label` to `subshape`, and `sweepDelta` to
  `sweepVelocity`; `penetration` becomes both `penetrationDepth` and `strength`
  (same convention as the existing static-world factory).
- `movementContactFromSweepHit` additionally carries `surfaceVelocity`.
- The body/weapon producer's dedup identity is unchanged because
  `movementContactsMatchTickLocalIdentity`/`MatchStableIdentity` ignore the new
  fields.

# Diagnostics

- `[CANONICAL CONTACT]` (only when `DebugConfig::COLLISION_VERBOSE`) logs actor
  part, shape, subshape, triangle, penetration, sweep velocity, and tick. No new
  debug files; existing categorized logging only.

# Validation

## Build

- `python build_agent.py` — `Status: SUCCESS` (exit 0), `mimita.exe` linked.
- `python build.py build-only` — `Nothing changed` (all objects up to date).

## Deterministic self-tests (all PASS)

- `--canonical-contact-selftest` — 19 checks: recovery metadata survival;
  sphere/mesh canonical-field equivalence with distinct shape kind; sweep-hit
  sweep and surface velocity; label mapping; arm-on-slope keeps subshape and
  point; new metadata does not change dedup identity.
- `--collision-selftest` — PASS (legacy stress + limb cases unchanged; identical
  final positions/velocities to baseline).
- `--collision-subgrid-selftest` — PASS.
- `--actor-triangle-spike` — PASS.
- `--actor-collision-mesh-selftest` — PASS.
- `--actor-triangle-solve-selftest` — PASS.

# Human review still needed

- No user-visible behavior changed by default. Human review is required once the
  opt-in `actorTriangleSolver` path is enabled and once Stage B unifies
  correction/response.
- Confirm in-game that the equipped weapon triangles loaded by
  `ensureActorWeaponColliderMesh` match the rendered weapon before the solver is
  enabled for real play.

# Unchanged and protected

- `doCollisions` default ordering, `respondVelocityAgainstNormal`,
  `solveBatchedCorrection`, `applyCollisionContact`, the spark boundary, and all
  capsule/emergency owners are behaviorally unchanged.
- `config/hitfx.json` and `src/effects/hit-effects*` untouched.

# Next stage

- Stage B (separate changelog): converge body/weapon/root correction and
  velocity response onto one canonical manifold and reorder `glb-main` so root
  capsule is fallback. Stage C: `PhysicalEntity` + dynamic triangle provider and
  the deterministic moving-crate carry/inheritance proof.
