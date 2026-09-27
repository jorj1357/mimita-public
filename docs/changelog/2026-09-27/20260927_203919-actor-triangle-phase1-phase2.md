// 2026-09-27T20:39:19Z
/* purpose
* record the actor-triangle collision migration Phase 1 inventory, Phase 0 spikes,
* and Phase 2 generic actor-triangle input
* separate source, build, and self-test evidence from human review still needed
* does NOT claim collision behavior changed or was accepted
* does NOT modify the spark or hitfx configuration
*/

# Task

- Summary: Inventory every active collision owner (Phase 1), verify headless
  triangle feasibility (Phase 0), and add the single generic actor-triangle
  input: `ActorCollisionMesh`, `collectActorCollisionMeshes`, weapon
  render-mesh triangles, and GL-free body triangles for NPC/headless actors
  (Phase 2).
- Status: CODE_COMPLETE / DEV_BUILD_VERIFIED / DETERMINISTIC_TEST_PASS /
  HUMAN_REVIEW_REQUIRED
- Date, time, timezone: 2026-09-27T20:39:19Z, ISO 8601 UTC
- Branch: `afad20a-rebuild`
- Base commit: `cb8f6a3a` ("pre big colision htings fro like better colisions")

# Scope

- Phase 1: read-only inventory.
- Phase 0: additive feasibility probe.
- Phase 2: additive actor-triangle input. The active collision path is still
  unchanged; `collectActorCollisionMeshes` is not yet called by `doCollisions`.
- Out of scope: the solver (Phases 3–5), momentum rework (Phase 6), removing
  capsule/emergency owners (Phases 7–8), spec edit, Phase 9 removal.

# Pre-existing / concurrent changes (not mine)

- `config/analytics.json` was already modified before this session; untouched.
- `docs/changelog/2026-09-27/20260927_203540-collision-architecture-audit.md`
  was created by a concurrent audit session; left untouched. It recommends a
  primitive-pair contact adapter instead of triangle-only; the user explicitly
  chose triangle-only for all actors, so this session follows the user decision
  and records the disagreement here.

# Source changes

## Added

- `docs/architecture/collision/actor-triangle-owner-inventory.md` — Phase 1 map
  (owner, input geometry, world geometry, correction, velocity, zeroing, order),
  dead safety passes, block path, weapon sphere/capsule, non-player consumers,
  grounded facts, spark boundary, plus a Phase 2 section.
- `docs/architecture/collision/actor-triangle-phase0-spike.md` — go/no-go record.
- `src/physics/movement/actor-triangle-spike.{h,cpp}` — Phase 0 probe, now using
  the shared loader (no duplicated parsing).
- `src/physics/movement/actor-collision-mesh.{h,cpp}` — Phase 2 owner:
  - `ActorCollisionMesh { label; localTriangles; previousTransform;
    desiredTransform; affectsMovement }`.
  - `collectActorCollisionMeshes(Player&)` (body parts then weapon) and
    `commitActorCollisionMeshes(Player&)`.
  - GL-free `loadActorBodyMeshParts`, `loadActorWeaponTriangles`,
    `ensureActorWeaponColliderMesh`, `ensureActorBodyCollisionMesh`.
  - `actorCollisionMeshSelfTest`.

## Modified

- `src/entities/player.h`
  - Added `bool loadModelColliders(const char* path);`.
  - Added `weaponColliderMesh`, `weaponColliderMeshPath`,
    `previousWeaponModelTransform`.
- `src/entities/player-loader.cpp`
  - Extracted `buildNodeHierarchyFromModel`, `syncPoseSkeletonFromRestLocal`,
    `buildBodyPartCollidersFromModel(model, player, buildRenderMeshes)`.
  - `Player::loadModel` now calls the extracted helpers
    (`buildRenderMeshes=true`); behavior preserved.
  - Added `Player::loadModelColliders`: CPU-only skeleton + colliders, no GL, no
    render mesh, no cache write (`buildRenderMeshes=false`).
- `src/game/game-cli.cpp`
  - Added `--actor-triangle-spike` and `--actor-collision-mesh-selftest`.

# Root cause / design notes

- The migration needs triangles for actors that never load a render body
  (`npc.body` has no `physicalBody.parts`) and for headless runs. Phase 0 proved
  CPU-only extraction works; Phase 2 turned it into a shared GL-free loader.
- `appendNodeRenderMesh` resolves a GL texture (`getOpaqueWhiteTexture`), so the
  headless path must skip render-mesh building; the first attempt crashed with
  an access violation and was fixed by the `buildRenderMeshes` parameter.
- Weapon triangles are built by baking the node hierarchy exactly like the
  render mesh does (`walkGLBScene` semantics), without calling the GL texture
  helper, so the collider and renderer share model-local space.

# Documents and skills

- `AGENTS.md`, `docs/ROUTER.md`, `docs/architecture/collision/collision.md`
- `docs/specs/movement/movement.md` (spec update deferred to the migration work)
- `docs/operations/build-and-exe/build-and-exe.md`,
  `docs/operations/task-completion/task-completion.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`
- Skills: `docs/skills/spec-behavior-review-v1.md` (no behavior changed yet).

# Validation

## Source

- Phase 1 inventory and Phase 2 additions reviewed against current code.

## Build

- `python build_agent.py` — `Status: SUCCESS`, `Compiled: 188`, `Skipped: 297`
  (player.h change forced a wide rebuild), `Compiled: 1` on the follow-up fix
  build. See `build/changelog.txt`.

## Deterministic self-test

- `.\mimita.exe --actor-triangle-spike` — PASS (headless): body 6/6 parts, 72
  triangles; weapon 304 triangles, world AABB finite + non-degenerate.
- `.\mimita.exe --actor-collision-mesh-selftest` — PASS:
  - headless actor loads 6 body parts and weapon triangles,
  - collector returns 6 body + 1 weapon mesh, every mesh has triangles,
  - body and weapon desired transforms differ from their sweep start.
- `.\mimita.exe --collision-selftest` — PASS (existing limb cases unchanged).

# Human review still needed

- No gameplay behavior changed this session. Human gameplay acceptance is
  required only once the solver replaces the active path (Phases 7–9).
- Confirm the concurrent audit's requested server authority boundary before
  Phase 7 wires triangles for server/NPC actors.

# Unchanged and protected

- `config/hitfx.json` and `src/effects/hit-effects*` untouched.
- No collision owner deleted or rerouted; `doCollisions` still uses the old path.
