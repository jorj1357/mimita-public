// 2026-09-27T21:42:23Z
/* purpose
* record the actor-triangle collision migration Phases 1-7: inventory, spikes,
* generic actor-triangle input, single solver, safe-to-desired sweep + unified
* manifold, momentum/dedup, and wiring the solver into the active path behind a
* toggle
* separate source, build, and self-test evidence from human review still needed
* does NOT claim the toggle path was played/accepted
* does NOT modify the spark or hitfx configuration
*/

# Task

- Summary: Build the single actor-triangle collision owner alongside the legacy
  pipeline through Phase 6, then wire it into the active GLB path behind a
  hot-reloadable toggle (Phase 7). The toggle defaults off.
- Status: CODE_COMPLETE / DEV_BUILD_VERIFIED / DETERMINISTIC_TEST_PASS /
  HUMAN_REVIEW_REQUIRED
- Date, time, timezone: 2026-09-27T21:42:23Z, ISO 8601 UTC
- Branch: `afad20a-rebuild`

# Scope

- Phases 0–6 additive.
- Phase 7 adds an opt-in active-path branch. Toggle off (default) keeps the
  legacy player collision exactly as before; NPCs always keep the legacy path.
- Out of scope: deleting the legacy owners (Phase 9), NPC/server headless body
  triangles, spec edit, manual gameplay acceptance.

# Pre-existing / concurrent changes (not mine)

- `config/analytics.json`, `config/accounts/default.json` modified by others.
- Concurrent commits `635b4758`, `c1fa36d8` absorbed parts of this work.
- Concurrent audit changelog `20260927_203540-collision-architecture-audit.md`
  recommends a primitive-pair adapter; the user chose triangle-only, so this
  session follows the user decision and records the disagreement.

# Source changes

## Added

- `docs/architecture/collision/actor-triangle-owner-inventory.md` (Phases 1–7).
- `docs/architecture/collision/actor-triangle-phase0-spike.md`.
- `src/physics/movement/actor-triangle-spike.{h,cpp}`.
- `src/physics/movement/actor-collision-mesh.{h,cpp}`.
- `src/physics/movement/actor-triangle-solver.{h,cpp}`.

## Modified

- `src/config/collision-config.{h,cpp}`, `config/collision.json` — added
  `actorTriangleSolver` (default false, hot-reloadable).
- `src/physics/movement/physics-collision-glb-main.cpp` — toggle branch calling
  `runActorTriangleCollisionStep` for non-NPC actors; fallback when the actor has
  no body triangles.
- `src/physics/movement/actor-collision-mesh.{h,cpp}` —
  `ensureActorWeaponColliderMeshFromEquipped` resolves the equipped weapon model
  path and guards against an unset (identity) weapon transform.
- `src/entities/player.h`, `src/physics/movement/physics-collision-shared.h`,
  `src/physics/movement/physics-collision-mesh.cpp`,
  `src/entities/player-loader.cpp` (Phases 1–6).
- `src/game/game-cli.cpp` — spike + two self-tests.

# Phase 7 behavior

- Toggle ON: the local player's GLB collision is handled solely by the triangle
  solver (body + weapon), bypassing body/weapon, sweep-slide, batched
  depenetration, floor recovery, and emergency stuck. Grounding is triangle-only.
  The body-contact spark is preserved and fed the solver's final contact point.
- Toggle OFF: identical legacy behavior.
- Fallback: if the world has no triangles or the player has no body triangles,
  the legacy pipeline runs instead of freezing the actor.
- Not yet handled by the toggle path: step-up (owned by legacy sweep-slide),
  in-game weapon-transform verification, NPC/server headless body triangles.

# Validation

## Build

- `python build_agent.py` — `BUILD SUCCESS` / `Status: SUCCESS`.

## Deterministic self-tests

- `--actor-triangle-solve-selftest` — PASS, 25 checks, including the 200 m/s
  momentum case and the active-path wrapper (falling actor lands and grounds
  through `runActorTriangleCollisionStep`, does not sink).
- `--collision-selftest` — PASS (legacy limb cases unchanged).
- `--actor-collision-mesh-selftest` — PASS.
- `--actor-triangle-spike` — PASS.
- `--collision-subgrid-selftest` — PASS.

# Human review still needed

- Enable `"actorTriangleSolver": true` in `config/collision.json` and play in the
  three maps; A/B against the legacy path. This is the first behavior-changing
  step and has NOT been played.
- Verify in-game weapon collision (transform correctness) under the toggle.
- Decide step-up handling and NPC/server triangle wiring before Phase 9 removal.

# Unchanged and protected

- `config/hitfx.json` and `src/effects/hit-effects*` untouched.
- Legacy owners not deleted; toggle off restores the old path.
