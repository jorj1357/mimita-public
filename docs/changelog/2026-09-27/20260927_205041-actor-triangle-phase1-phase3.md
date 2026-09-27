// 2026-09-27T20:50:41Z
/* purpose
* record the actor-triangle collision migration Phase 1 inventory, Phase 0 spikes,
* Phase 2 generic actor-triangle input, and Phase 3 single triangle solver
* separate source, build, and self-test evidence from human review still needed
* does NOT claim the active collision path changed or was accepted
* does NOT modify the spark or hitfx configuration
*/

# Task

- Summary: Inventory every active collision owner (Phase 1), verify headless
  triangle feasibility (Phase 0), add the single generic actor-triangle input
  (Phase 2), and build the unified actor-triangle solver (Phase 3). The solver
  runs alongside the legacy pipeline and is not yet called by `doCollisions`.
- Status: CODE_COMPLETE / DEV_BUILD_VERIFIED / DETERMINISTIC_TEST_PASS /
  HUMAN_REVIEW_REQUIRED
- Date, time, timezone: 2026-09-27T20:50:41Z, ISO 8601 UTC
- Branch: `afad20a-rebuild`
- Commits: Phase 1/2 was committed by a concurrent process as `635b4758`
  ("midlde of doing like colision tjings idk"). Phase 3 is in the working tree
  (untracked `actor-triangle-solver.{h,cpp}` plus edits to
  `actor-collision-mesh.{h,cpp}`, `physics-collision-mesh.cpp`,
  `physics-collision-shared.h`, `game-cli.cpp`).

# Scope

- Phase 1: read-only inventory.
- Phase 0: additive feasibility probe.
- Phase 2: additive actor-triangle input and GL-free loaders.
- Phase 3: additive solver built alongside the old path. `doCollisions` is
  unchanged.
- Out of scope: momentum rework (Phase 6), removing capsule/emergency owners
  (Phases 7–8), spec edit, Phase 9 removal.

# Pre-existing / concurrent changes (not mine)

- `config/analytics.json` was already modified before this session; untouched.
- `docs/changelog/2026-09-27/20260927_203540-collision-architecture-audit.md`
  was created by a concurrent audit session; untouched. It recommends a
  primitive-pair contact adapter rather than triangle-only; the user explicitly
  chose triangle-only for all actors, so this session follows the user decision
  and records the disagreement.

# Source changes

## Added

- `docs/architecture/collision/actor-triangle-owner-inventory.md` — Phase 1 map
  plus Phase 2/3 sections.
- `docs/architecture/collision/actor-triangle-phase0-spike.md` — go/no-go record.
- `src/physics/movement/actor-triangle-spike.{h,cpp}` — Phase 0 probe, using the
  shared loader.
- `src/physics/movement/actor-collision-mesh.{h,cpp}` — Phase 2 actor input:
  `ActorCollisionMesh`, `collectActorCollisionMeshes`,
  `collectActorBodyCollisionMeshes`, `commitActorCollisionMeshes`, GL-free
  `loadActorBodyMeshParts` / `loadActorWeaponTriangles`,
  `ensureActorWeaponColliderMesh` / `ensureActorBodyCollisionMesh`,
  `actorCollisionMeshSelfTest`.
- `src/physics/movement/actor-triangle-solver.{h,cpp}` — Phase 3 solver:
  `ActorWorldContact`, `ActorTriangleCollisionResult`,
  `solveActorTriangleCollision`, `actorTriangleSolverSelfTest`.

## Modified

- `src/entities/player.h` — `loadModelColliders`, `weaponColliderMesh`,
  `weaponColliderMeshPath`, `previousWeaponModelTransform`.
- `src/entities/player-loader.cpp` — extracted
  `buildNodeHierarchyFromModel`, `syncPoseSkeletonFromRestLocal`,
  `buildBodyPartCollidersFromModel(model, player, buildRenderMeshes)`; `loadModel`
  uses them (`buildRenderMeshes=true`); added `Player::loadModelColliders`
  (CPU-only, no GL, no cache write).
- `src/physics/movement/physics-collision-shared.h` — declared
  `collectActorMeshContacts` and `makeSweptActorMeshAABB`.
- `src/physics/movement/physics-collision-mesh.cpp` — extracted
  `collectActorMeshContacts`; `collectBodyMeshContacts` now delegates to it.
- `src/game/game-cli.cpp` — added `--actor-triangle-spike`,
  `--actor-collision-mesh-selftest`, `--actor-triangle-solve-selftest`.

# Root causes found and fixed

1. `appendNodeRenderMesh` resolves a GL texture. The headless body loader
   crashed with an access violation until `buildBodyPartCollidersFromModel` was
   given a `buildRenderMeshes` switch (headless passes false).
2. Current-overlap normal orientation used the root position. A root just below
   a floor flipped the floor normal downward. The solver now passes the actor
   pose-box center as the orientation reference.
3. Calling `updateModelWorldTransforms` between correction iterations overwrote
   the safe previous pose, so a floor depenetration was re-read as a fresh
   downward sweep and flipped the normal. The solver now captures the safe pose
   once, accumulates corrections as a translation, and applies them once.
4. Grounding used the capsule feet. It now uses the actor mesh pose-box lowest
   point, so it does not require a capsule.

# Documents and skills

- `AGENTS.md`, `docs/ROUTER.md`, `docs/architecture/collision/collision.md`
- `docs/specs/movement/movement.md` (spec update deferred)
- `docs/operations/build-and-exe/build-and-exe.md`,
  `docs/operations/task-completion/task-completion.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`
- Skills: `docs/skills/spec-behavior-review-v1.md` (no behavior changed yet).

# Validation

## Build

- `python build_agent.py` — `Status: SUCCESS` for each step; `player.h` change
  forced a 188-file rebuild, later steps 1 file.

## Deterministic self-tests (all PASS)

- `--actor-triangle-spike`: headless body 6/6 parts, 72 triangles; weapon 304
  triangles, world AABB finite + non-degenerate.
- `--actor-collision-mesh-selftest`: headless 6 body parts + weapon, collector
  returns 6 body + 1 weapon mesh all with triangles, sweep deltas detected.
- `--actor-triangle-solve-selftest`: floor rest grounded and not sunk;
  high-speed wall stopped with tangential momentum preserved and velocity
  nonzero; old wall contact not glued; floor + wall contact at once.
- `--collision-selftest`: existing limb cases unchanged.

# Human review still needed

- No gameplay behavior changed. Human gameplay acceptance is required only once
  the solver replaces the active path (Phases 7–9).
- Confirm the concurrent audit's requested server authority boundary before
  Phase 7 wires triangles for server/NPC actors.

# Unchanged and protected

- `config/hitfx.json` and `src/effects/hit-effects*` untouched.
- No collision owner deleted or rerouted; `doCollisions` still uses the old path.
