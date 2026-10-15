# Griever specification audit and implementation contract

**Date:** 2026-10-15  
**Scope:** Documentation/specification planning only  
**Status:** Complete for this session; implementation and runtime acceptance remain open

## What changed

Replaced the scratchpad at `docs/specs/griever-monster/griever-monster.md` with an implementation-planning specification covering:

- informal product intent and explicit likes/dislikes;
- current avatar, movement, collision, NPC, and networking ownership;
- the current split roll paths and client-transform authority;
- a target shared fixed-tick roll contract;
- camera-relative WASD force semantics;
- sphere-body versus authored-spike collision choices;
- lifecycle, reconciliation, and server-position requirements;
- implementation slices with exit conditions;
- bounded StructuredLogger evidence and a real runtime scenario;
- historical attempt outcomes and separate source/build/runtime/human acceptance.

## Confirmed findings

1. `assets/avatars/monstergriever/avatar.json` defines a 19-part rig and roll capability data.
2. `src/physics/physics-mini.cpp` and `src/engine/engine-tick-net.cpp` contain separate relevant roll paths.
3. The multiplayer local path currently integrates roll movement from render-frame `dt`, conflicting with the repository fixed-60-Hz gameplay rule.
4. `src/network/server-players.cpp`, `src/network/movement-validation.cpp`, and `src/network/multiplayer-reconcile.cpp` implement a transitional client-transform-authoritative roll path.
5. Current code uses both a large movement body and extra rig triangle participation; the production collision model is not yet an explicit settled choice.
6. The latest historical evidence shows bounded runs with tumbling, grounded settling, and small server position error, but does not prove human visual acceptance, robust multiplayer behavior, or the correct final spike collision model.
7. The router-referenced `docs/doc-review-09-03-2026.md` and `docs/regressions/regressions-v1.md` are absent at the referenced paths in this checkout.

## Validation

- `git diff --check`: passed; existing line-ending warnings only.
- Build: not run; this session changed documentation only.
- Runtime/dev-loop scenario: not run; no gameplay code changed.
- Human visual/multiplayer acceptance: not performed.

## Pre-existing working-tree changes

The following were present before this documentation work and were not claimed or modified:

- `config/accounts/default.json`
- `config/analytics.json`
- `docs/changelog/2026-10-14/20261014_003000-avatar-limb-render-origin.md`
- `src/engine/engine-tick-net.cpp`
- `src/entities/player.cpp`
- `src/entities/player.h`
- `src/physics/movement/physics-collision-shared.h`

## Focused review result

Used `docs/skills/documentation-checker-v1.md` and `docs/skills/spec-behavior-review-v1.md`. The new spec has an audience, purpose, DOES NOT scope, current code evidence, explicit disagreements, ownership rules, implementation slices, and acceptance criteria. The original scratchpad's informal failure history was retained as historical context, not treated as precise behavior.

## Follow-up update from user requirements

The specification was extended to make the actual Griever GLB triangles the
authoritative movement/collision geometry. The documented runtime path is now
`appendNodeCollider` -> `physicalBody.extraParts` ->
`collectActorBodyCollisionMeshes` -> `doGLBTriangleCollisions` ->
`runActorTriangleCollisionStep` -> the shared response owner. A sphere/capsule
is explicitly limited to broadphase or invalid-mesh safety fallback.

The Griever input contract now uses full camera forward including pitch: looking
down plus W applies downward force. The bounce plan requires tracing and reusing
the existing player response through `respondVelocityAgainstNormal`, including
later velocity writers that could create hover or erase bounce.

The spec now separates the grenade-launcher network lifecycle reference from the
`object_spawn` GLB/physical-body reference, records that physical-entity packet
ownership is incomplete, and adds a separate Jason ragdoll/head investigation.
It also adds a bottom attempt log with Attempt 1 containing the user's current
report: avatar loading tentatively okay, server position not updating or large
`server_pos_error`, slow upward hover, full camera-forward force, and player-like
bounce requirements.

Validation remains documentation-only for this session: no build or runtime
scenario was run.

