// 2026-09-27
/* purpose
* record the Phase 0 go/no-go evidence for the actor-triangle migration
* keep the probe command and its exact output next to the decision it justifies
* separate build evidence from runtime/self-test evidence
* does NOT change collision behavior
* does NOT claim human gameplay acceptance
* does NOT replace the owner inventory or the migration plan
*/

# Actor-triangle migration — Phase 0 spike results

Date: 2026-09-27
Branch: `afad20a-rebuild`
Probe commit: `cb8f6a3a` + working tree (spike files not yet committed)
Command: `.\mimita.exe --actor-triangle-spike` from `C:\mimita-v9`

## Purpose

Before touching the active collision pipeline, prove three assumptions the
migration depends on:

1. Body-part collision triangles can be built **CPU-only, without a GL context**
   (needed for NPCs, the headless authoritative server, and replay).
2. Weapon **render-mesh** triangles are available and can be transformed into
   world space the way the unified solver will consume them.
3. An actor with no full render/GL model can still supply triangles.

## Probe implementation (non-active path)

- `src/physics/movement/actor-triangle-spike.h` / `.cpp`: a self-contained
  tinygltf probe. It parses the default character GLB and a shipped weapon GLB
  with no GL calls, walks the node hierarchy exactly like
  `server-body-template.cpp`, extracts per-part triangles, and applies a
  synthetic `weaponModelTransform` (translate + rotate + scale).
- `src/game/game-cli.cpp`: adds the `--actor-triangle-spike` command next to
  `--collision-selftest`. It runs before window/GL creation.
- Nothing in the active collision pipeline was modified.

## Evidence

### Build

- `python build_agent.py` — `Status: SUCCESS`, `Compiled: 1`, `Skipped: 483`,
  10.66 s, linked `mimita.exe`.

### Runtime self-test (`.\mimita.exe --actor-triangle-spike`)

```text
[SPIKE] headless(no GL context)=yes
[SPIKE][BODY] part=head     node=0 localTriangles=12 ...
[SPIKE][BODY] part=torso    node=5 localTriangles=12 ...
[SPIKE][BODY] part=leftArm  node=1 localTriangles=12 ...
[SPIKE][BODY] part=rightArm node=3 localTriangles=12 ...
[SPIKE][BODY] part=leftLeg  node=2 localTriangles=12 ...
[SPIKE][BODY] part=rightLeg node=4 localTriangles=12 ...
[SPIKE][BODY] loaded=1 parts=6/6 partsWithTriangles=6/6 totalTriangles=72
[SPIKE][BODY] PASS: body triangles extract CPU-only via tinygltf
[SPIKE][WEAPON] loaded=1 path=assets/objects/weapons/mimita-revolver-v1.glb localTriangles=304
[SPIKE][WEAPON] worldAabb=(1.257 -0.413 -0.187)-(1.696 0.653 1.969) finite=1 nonDegenerate=1
[SPIKE][WEAPON] PASS: render-mesh triangles transform to world space CPU-only
[SPIKE] RESULT: PASS (body + weapon triangles CPU-only, GL-free)
[ACTOR TRIANGLE SPIKE] PASS
```

## Decisions

| Spike | Result | Consequence for the migration |
|-------|--------|-------------------------------|
| 1. Headless body triangles | **GO** | Phase 2 can build a shared CPU-only body-triangle loader for client, NPC, server, replay. |
| 2. Weapon render-mesh triangles | **GO** | The weapon can join the same triangle representation; the render mesh is the collider source. |
| 3. Headless actor + weapon transform | **GO** | Triangles can be produced and world-transformed with no GL context. |

The user decision "derive weapon triangles from the render mesh" and "require
triangles for everyone, block on NPC bodies" are therefore **unblocked**.

## Caveats carried forward

- The probe reimplements tinygltf accessors. Phase 2 must fold this into the
  single shared loader rather than keep two parsers (delete the probe's private
  helpers once the loader exists).
- The probe uses the revolver mesh only; Phase 2 must source the correct mesh
  per weapon from `WeaponModelCache`/`WeaponConfig` and per-actor attachment.
- The probe proves geometry extraction, not collision quality. Broadphase reuse,
  per-part budgets, and world-space parity with `weaponModelTransform` still
  need the Phase 2–5 solver and Phase 9 tests.
- Server hit validation still uses `ServerPlayerBodyPartTemplate` (AABBs); its
  unification or explicit boundary remains an open item.

## Not proven

- No gameplay, visual, multiplayer, or performance acceptance.
- No claim that the active capsule path can be removed yet.
