# Part-aware real actor GLB → hot PoseState on the generic render path

Date: 2026-09-15 13:00 EST (UTC 2026-09-15T17:00:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW`

## 1. Real actor GLB structure audit

The actor GLB is **rigid multipart**, not weight-based glTF skinning:
- each body part is a named node (`head`, `torso`, `leftArm`, `rightArm`,
  `leftLeg`, `rightLeg`) with its own mesh primitive(s) and a local bind
  transform;
- `player-loader.cpp` already relies on this (`isPlayerBodyPart`, `nodeMatrix`,
  per-part `bodyPartMeshes`), and `Player::renderCurrentPose` draws rigidly per
  part.
So the correct generic model is per-part transforms, not a new weighted system.

## 2/3. Part-aware GLB load (generic)

`loadGlbMesh` now:
- validates the container, then walks the scene node hierarchy accumulating world
  bind matrices;
- merges vertices/indices into shared GPU buffers;
- records one `GpuMesh::Part { bone=gameHash(node.name), indexOffset, indexCount,
  bind }` per meshed node;
- falls through to the previous merged **static** parse when no parts are found
  (ordinary GLBs unchanged).

No `NpcGlbLoader`/`ActorSkinLoader`; it stays generic mesh loading.

## 4. Node name → bone id

`gameHash(node.name)` is the part id, matching `SkeletonInstances` bone keys and
the hot pose `gameHash("leftArm")` etc. No NPC-specific table.

## 5. Bind/rest transforms + matrix order

Each part stores its node **world bind** matrix. Draw computes
`entityModel * boneWorld * bind` (cold renderer composes; hot only supplies the
pose). `boneWorld` is the hot pose offset for that part; a zero pose reduces to
`entityModel * bind` (bind placement).

## 6/8. Hierarchy + fallback

Parent transforms are accumulated during the walk, so child parts get correct
world binds. A missing skeleton instance / non-articulated mesh / malformed GLB
falls back to a single static draw.

## 7. Real `mesh.actor`

`loadGlbMesh` is the loader for `mesh.actor`; with a GL context it now yields a
part-aware `GpuMesh`. Headless `inspectGlbParts` confirms the real asset parses
into named parts (`torso`, `leftArm`, ...).

## 9. One draw owner

For the migrated remote NPC the typed `renderNetworkPlayer` remains skipped;
`render.mesh` owns the model. No hidden second copy.

## 10. Real pose effect

`PoseState` → `skeleton.apply` → `SkeletonInstances[EntityId]` → per-part
`render.mesh` matrices. Part matrix changes are driven through this path.

## 11. Live visual proof

Not run (no visible client). LIVE VISUAL PROVEN = no.

## 12-14. Attack/jump, blend, resource generations

Not reached (gated behind visible confirmation).

## 15/16. Failure safety + tests

Malformed GLB rejected (last-good); missing instance → static fallback; unknown
part → bind transform; zero-index parts skipped. Added: "real actor GLB parses
into named body parts", "generic render.mesh consumes SkeletonInstances by
EntityId", "missing skeleton instance falls back to a static draw". Full suite
PASS. No Player/NPC/Monster-specific mesh API.

## Evidence

- `python build_agent.py` -> `Status: SUCCESS`.
- `--hot-combat-selftest` -> PASS (part parse + skinned consumption + fallback).
- Full suite (9 selftests) -> PASS.

## Classification

- SELFTEST PROVEN: real actor GLB part parse (named parts); EntityId → skeleton
  instance consumption; static fallback.
- COMPILED INTEGRATION: part-aware `loadGlbMesh`; per-part `submitMesh` draw with
  `entityModel * boneWorld * bind`.
- LIVE MULTIPLAYER PROVEN: no.
- LIVE VISUAL PROVEN: no.
- HUMAN VERIFICATION NEEDED: on-screen deformation of the real remote NPC; matrix
  order eyeball; live pose edit; then attack/jump + blend.

## Files changed

`src/render/presentation-render.{h,cpp}`, `src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.

## Next cold owner selected

Visually confirm the real remote NPC follows hot `PoseState`; then add jump/attack
pose policy, then a minimal blend model, then generation-aware skeleton/clip
resources.
