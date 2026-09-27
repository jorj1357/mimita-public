// 2026-09-27
/* purpose
* list every collision owner the actor-triangle migration intends to remove or
* consolidate, with the exact removal condition and current consumers
* mirror the in-code TODO-DELETE markers so a future cleanup can be planned
* does NOT delete or change any code
* does NOT authorise removal; human gameplay acceptance is required first
* does NOT cover the legacy block world, which is intentionally kept
*/

# Actor-triangle deletion candidates

Date: 2026-09-27
Status: annotated in code with `TODO-DELETE`; nothing removed
Replacement owner: `src/physics/movement/actor-triangle-solver.cpp`
(`solveActorTriangleCollision`, `runActorTriangleCollisionStep`) plus
`src/physics/movement/actor-collision-mesh.cpp` (`collectActorCollisionMeshes`,
`collectActorMeshContacts`).

## Global gate before any removal

Every gameplay-path removal below is blocked on the same proof:

1. Human gameplay testing with `"actorTriangleSolver": true` in
   `config/collision.json`, compared against `false`, in **Dust 3 Siberia,
   Trainkinda, and Chain of Judgement**.
2. No confirmed regression in movement feel, grounding, limbs, weapons, or
   momentum.
3. The spec (`docs/specs/movement/movement.md` §13) updated to make the triangle
   solver the single gameplay collision authority.

Dead code and efficiency-only items are exempt from gameplay proof but still
need a grep/build to confirm no references remain.

## Candidates

| Owner | File | Class | Blocked on |
|---|---|---|---|
| `doGLBSweepSlide` | `physics-collision-glb-sweep-slide.cpp` | gameplay | human testing **and** step-up decided (port to triangle owner or spec-drop) |
| `doBodyWeaponCollisionPhase` / `runBodyWeaponPass` | `physics-collision-glb-body.cpp` | gameplay | human testing; weapon capsule/sphere config retired; weapon transform verified in-game |
| Phase-1 body/weapon call | `physics-collision-glb-main.cpp` | gameplay | same as body phase |
| Phase-2 sweep/slide call | `physics-collision-glb-main.cpp` | gameplay | same as `doGLBSweepSlide` |
| Phase-3 batched depenetration | `physics-collision-glb-main.cpp` | gameplay | triangle final-pose depenetration accepted |
| Phase-4 `doFloorRecovery` call | `physics-collision-glb-main.cpp`, `physics-collision-glb-safety.cpp` | gameplay | triangle depenetration + grounding accepted |
| Phase-5 emergency stuck escape | `physics-collision-glb-main.cpp` | gameplay | triangle path never leaves the actor stuck; rate-limited diagnostic added |
| Phase-7 capsule debug draw | `physics-collision-glb-main.cpp`, `physics-collision-dispatch.cpp` | diagnostic | triangle debug draw added |
| `collectCapsuleRecoveryContacts` | `physics-collision-glb-contact.cpp` | conditional | remote geometry safety, ragdoll, and stress tests migrated |
| `collectGLBRecoveryContacts` | `physics-collision-glb-contact.cpp` | legacy-only | legacy pipeline removed |
| `collectBodyWeaponSpheres` | `physics-collision-body.cpp` | gameplay | body phase removed |
| `collectBodyWeaponContacts` | `physics-collision-body.cpp` | gameplay | body phase removed |
| `collectPlayerBodyCollisionSamples` | `physics-collision-body.cpp` | conditional | sweep-slide + debug consumers removed |
| `recomputeWeaponCapsule` | `physics-collision-body.cpp` | conditional | `combat/weapon-swordsword.cpp` migrated; body phase removed |
| `doGroundSnap` | `physics-collision-glb-safety.cpp` | dead | grep/build only |
| `doRotationSafetyPass` | `physics-collision-glb-safety.cpp` | dead | grep/build only |
| `doFinalSafetyPass` | `physics-collision-glb-safety.cpp` | dead | grep/build only |
| `applyPostSnapCorrection` | `physics-collision-glb-safety.cpp` | dead (helper) | `doGroundSnap` removed |
| `gatherGLBTrianglesForSphere` | `physics-collision-glb.cpp` | dead | extern declarations removed; grep/build |
| value-returning `gatherGLBTriangles` | `physics-collision-glb-setup.cpp` | efficiency | callers use the scratch-buffer overload |
| capsule penetration check in stress cases | `physics-collision-stress.cpp` | test | triangle-based verification added |
| `actorTriangleSpike` | `actor-triangle-spike.cpp` | diagnostic | superseded by `actorCollisionMeshSelfTest` |

## Intentionally kept (not deletion candidates)

- Legacy block-world path (`physics-collision-dispatch.cpp`, non-GLB worlds) —
  kept by decision.
- `Player::getCapsule()` — kept as non-authoritative data: camera height, player
  dimensions, spawn placement, and the headless server body template.
- `recoverInvalidPlayerCollisionState` — NaN/inf guard, not a gameplay muter.
- `resolveCapsuleVsCapsule` (player vs player) — separate concern, out of scope.
- Remote-body/ragdoll capsule use — owners in their own files; migrate later.
- `collectBodyMeshContacts` / `collectActorMeshContacts` — the shared triangle
  contact path, i.e. keep.
- The `hitfx.json` and body-contact spark boundary — must be preserved.
