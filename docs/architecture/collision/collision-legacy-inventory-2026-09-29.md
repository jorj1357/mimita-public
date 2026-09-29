# Collision and Movement Legacy Inventory

Time: `2026-09-29T15:54:28Z`

Status: INVENTORY ONLY — NOTHING DELETED

Purpose: identify every collision/movement owner or migration bridge that can
make the active implementation difficult to reason about. This document does
not decide deletion by itself. Each item needs caller proof, runtime proof,
replacement coverage, and the acceptance gates listed below.

## Executive finding

The v9 local-player path is not one completely clean implementation yet. With
`config/collision.json` setting `actorTriangleSolver` to `true`,
`doGLBTriangleCollisions()` attempts `runActorTriangleCollisionStep()` for a
non-NPC player and returns early when that step succeeds. The older GLB
pipeline remains directly below that branch and is still real code for:

- NPCs;
- the toggle-off path;
- actors with no body triangles, because the triangle solver returns `false`;
- remote-body geometry safety, ragdolls, physical entities, and tests;
- legacy debug/emergency reporting.

That means the old code is mostly bypassed for the local player, but it is not
dead as a subsystem. The current slope/edge issue is therefore primarily in
the active actor-triangle path, while the legacy code still makes ownership,
tests, and fallback behavior harder to prove.

## Active dispatch boundary

| Owner | Location | Current status | Inventory decision |
|---|---|---|---|
| Collision dispatcher | `src/physics/movement/physics-collision-dispatch.cpp:115` | Calls `doGLBTriangleCollisions()` | Keep as orchestration owner; make the selected collision owner explicit in runtime events |
| Actor-triangle path | `src/physics/movement/physics-collision-glb-main.cpp:137-150`, `src/physics/movement/actor-triangle-solver.cpp:507` | Active for local players when enabled and geometry exists | Intended production owner; continue investigation here |
| Legacy GLB path | `src/physics/movement/physics-collision-glb-main.cpp:152-430` | Active for NPCs, toggle-off, and solver fallback | Keep temporarily as a quarantined compatibility owner; do not silently delete |
| Collision toggle | `src/config/collision.json:13` and `src/config/collision-config.cpp:50` | `actorTriangleSolver=true` | Record the value in every bookmark/version report |

## TODO-DELETE inventory

### 1. Legacy GLB phase block

File: `src/physics/movement/physics-collision-glb-main.cpp`

- `:33-35` — `gatherGLBTrianglesForSphere` extern used only by the old
  emergency/debug path. Candidate after those declarations and consumers are
  removed.
- `:173-190` — Phase 1 `doBodyWeaponCollisionPhase`; replaced for local-player
  actors by `collectActorCollisionMeshes` plus the actor-triangle solver.
- `:192-202` — Phase 2 `doGLBSweepSlide`; owns the old root-capsule sweep and
  the only current step-up behavior.
- `:204-252` — Phase 3 root-capsule batched depenetration; still depends on
  `collectCapsuleRecoveryContacts`.
- `:254-263` — Phase 4 `doFloorRecovery`; only reached by the legacy path when
  the actor-triangle toggle is off.
- `:265-414` — Phase 5 emergency stuck escape and related recovery logic; can
  teleport the actor and zero velocity, so it is a possible source of behavior
  that looks like a collision snag if the legacy path is reached.
- `:416-430` — Phase 7 legacy capsule/body-sample debug drawing; it observes a
  different collision vocabulary than the actor-triangle JSONL events.

No Phase 6 label was found in this block; the numbering itself is migration
debt and should be normalized when the pipeline is consolidated.

### 2. Legacy body/weapon approximation

Files: `physics-collision-glb-body.h/.cpp`,
`physics-collision-shared.h`

- `physics-collision-glb-body.cpp:37-232` — `runBodyWeaponPass` and
  `doBodyWeaponCollisionPhase`; legacy body-part spheres, weapon capsule, and
  capsule recovery. Called only by the legacy phase in the main GLB pipeline.
- `physics-collision-body.cpp:37-75` — `recomputeWeaponCapsule`; still not
  deletable because `combat/weapon-swordsword.cpp` calls it.
- `physics-collision-body.cpp:106-112` — `collectBodyWeaponSpheres`; legacy
  AABB/body/weapon sphere approximation.
- `physics-collision-body.cpp:218-224` — `collectBodyWeaponContacts`; legacy
  sphere-triangle response.
- `physics-collision-body.cpp:341-347` — `collectPlayerBodyCollisionSamples`;
  still used by the old sweep-slide and legacy debug branch.
- `physics-collision-shared.h:391-397` — public declaration group for the
  whole legacy body/weapon sphere and capsule family.

Consolidation target: one actor mesh contact collector and one response owner,
with sword/weapon callers migrated to an explicit weapon-contact contract.

### 3. Legacy safety, grounding, and recovery

Files: `physics-collision-glb-safety.h/.cpp`

- `applyPostSnapCorrection` (`:32-52`) — helper tied to the old ground-snap
  family.
- `doGroundSnap` (`:55-133`) — documented as no active caller; candidate dead
  code, but retain until slope/seam hover acceptance is complete.
- `doFloorRecovery` (`:138-200`) — active only through the legacy pipeline.
- `doRotationSafetyPass` (`:202-259`) — documented as no caller; duplicate
  capsule depenetration candidate.
- `doFinalSafetyPass` (`:262-303`) — documented as no caller; another duplicate
  final capsule depenetration candidate.

The header exports all four safety functions even though two are documented as
uncalled. That public surface makes deadness less obvious and should be
verified with a symbol/caller check before deletion.

### 4. Legacy contact producers and broadphase wrappers

- `physics-collision-glb-contact.cpp:237-286` and
  `physics-collision-shared.h:314-321` — `collectCapsuleRecoveryContacts`.
  It is superseded for actor contacts but still has live consumers in the
  legacy player path, remote-geometry safety, ragdoll/physical-body code, and
  stress tests.
- `physics-collision-glb-contact.cpp:288-339` — `collectGLBRecoveryContacts`.
  Used by the old emergency/debug search and therefore still appears in the
  active legacy pipeline.
- `physics-collision-glb.cpp:23-42` — `gatherGLBTrianglesForSphere`.
  The source documents no callers beyond extern declarations. This is the
  clearest deletion candidate, pending a clean reference search and build.
- `physics-collision-glb-setup.cpp:277-286` — value-returning
  `gatherGLBTriangles` overload. It allocates/copies a vector and remains for
  the legacy pipeline and stress tests; the scratch-buffer overload is the
  intended hot-path owner.
- `physics-collision-dispatch.cpp:118-130` — legacy capsule/body-sample debug
  report. It is diagnostic, not gameplay, but can make old contacts look like
  active-authority contacts.

### 5. Feasibility and test leftovers

- `actor-triangle-spike.cpp:47-54` — Phase 0 feasibility probe. It is not dead:
  `game/game-cli.cpp:218` calls it. Keep until the CLI/self-test contract is
  deliberately replaced.
- `physics-collision-stress.cpp:156-164` — penetration test still verifies the
  legacy root capsule. It can pass while the live actor-triangle path differs.
  Add actor-triangle authority coverage before retiring the capsule assertion.
- `actor-triangle-solver.cpp:266-270` — per-iteration copy of transformed
  actor meshes. This is a performance consolidation item, not a legacy owner;
  it must wait for profiling and scratch-cache safety proof.

### 6. Physical-entity migration bridge

Files: `src/physics/physical-entity.h:163-168` and
`src/physics/physical-entity.cpp:715-720`

`collectActorEntityContacts` temporarily allocates a `World` and copies entity
triangles so it can reuse the actor-triangle narrowphase. This is not the old
player capsule path, but it is another parallel representation and allocation
boundary that can confuse ownership and affect collision results for dynamic
entities. The documented next step is direct cached entity-shape querying.

## Additional legacy/parallel state found

These are not all marked `TODO-DELETE`, but they add to the same ownership
confusion:

- `Player::syncLegacyStateToLayers()` / `syncLayersToLegacyState()` in
  `src/entities/player.cpp` and callers in animation, rendering, networking,
  reconcile, and collision code. The actor-triangle solver reads modern body
  parts, while compatibility state is still synchronized around the player.
- `src/physics/movement/physics-collision-core.cpp:54` explicitly synchronizes
  legacy state before collision work.
- `src/physics/movement/physics-walk.cpp`, `physics-jump.cpp`,
  `physics-dash.cpp`, `physics-down-dash.cpp`, and `physics-freeze.cpp` are
  Player-specific wrappers around shared movement helpers. They are not
  necessarily wrong, but they create multiple entry points and should be
  treated as adapters until one fixed-tick movement owner is proven.
- `src/physics/movement/movement-step.cpp:1414` contains an explicit
  “legacy behavior” override mode, so movement policy still has a compatibility
  branch even when collision is on the actor-triangle owner.
- `actor-collision-mesh.cpp:206` retains render-mesh triangles as a fallback;
  `actor-triangle-solver.cpp:515-518` returns `false` when body triangles are
  absent, intentionally reopening the legacy pipeline.

## Why collision bugs can still happen

The code is confusing for an AI and for a human because “collision owner” is
not one global answer. It depends on actor kind, config, available body mesh,
remote/ragdoll/entity context, and whether a debug/emergency phase is reached.
The active local-player actor-triangle solver can still have a real bug, while
the legacy code remains relevant enough that a fallback or secondary consumer
can alter the observed result in another scenario.

The supplied JSONL evidence already showed deep repeated penetration on actor
triangles. That points first at the active actor-triangle contact/query or
response path, not automatically at the old capsule code. The inventory does,
however, explain why a code search can produce several plausible answers.

## Safe consolidation order

1. Add runtime fields to every collision summary: actor kind, selected owner,
   solver fallback reason, pipeline phase, and config value.
2. Complete live acceptance for local-player actor-triangle movement on the
   required maps, including slopes, seams, edges, step-ups, and deep starts.
3. Migrate NPCs, remote geometry safety, ragdolls, physical entities, and
   sword/weapon callers to explicit owners rather than deleting shared helpers.
4. Update stress tests so they exercise the same owner as live gameplay.
5. Delete only after a reference search, build, self-tests, and human gameplay
   review agree that the old owner has no remaining contract.

No code was deleted or behavior changed by this inventory.
