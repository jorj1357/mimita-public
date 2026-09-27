# render.mesh consumes SkeletonInstances (per-part) by EntityId

Date: 2026-09-15 12:00 EST (UTC 2026-09-15T16:00:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW`

## 1. Existing skinned path audit

The typed player renderer is **rigid per body part**: `Player::renderCurrentPose`
loops `physicalBody.parts` / `physicalBody.partMeshes` and draws each part's
batches with the part transform (`glDrawArrays`). It is not weight-based GPU
skinning with inverse-bind matrices. Reusable cold mechanism: the per-part draw +
the shape of a part transform.

## 2. Generic skinned render input

`GpuMesh` gained an optional `parts` list: `{ bone (gameHash(partName)), index
range/ebo, optional bind matrix }`. `GameRenderMeshCommandV1` is unchanged; the
existing `entity` field is the join key. No `renderNpcSkinned`/`renderPlayerSkinned`.

## 3. EntityId is the join key

`submitMesh` calls `SkeletonInstances::get((EntityId)command.entity)`; a mesh with
parts + a live instance draws per part with `entityModel * boneWorld (* bind)`;
otherwise a single static draw. No Player/Npc pointer lookup.

## 4. Skeleton resource compatibility

Bone identity is `gameHash(partName)`, matching `SkeletonInstances` bone keys.
Unknown parts keep the identity/bind transform. (A real GLB skin/bone table is the
next loader step.)

## 5. Final skinning matrices

Cold code composes part matrices; hot code produces only `PoseState`. Hot never
touches VAO/shader/buffers.

## 6. Generic GPU skinning draw

Wired into the existing generic `render.mesh` path (no parallel NPC renderer).
`skinnedSubmissionCount()` / `staticFallbackCount()` expose evidence.

## 7. Real remote NPC proof

Mechanism proven with a synthetic part mesh keyed by EntityId; the real remote
NPC path already skips the typed draw and drives `SkeletonInstances` from hot
pose. See the honest limitation.

## 8. Static fallback

Documented: mesh with no skinning, missing skeleton, missing pose, or incompatible
resource -> single static draw (never disappearing/crashing).

## 9-11. Attack/jump, blend, resource generations

Not reached (deliberately gated behind visible skinning).

## 12. Live visual proof

Not run (no visible client). LIVE VISUAL PROVEN = no.

## 13. Tests

Added: "generic render.mesh consumes SkeletonInstances by EntityId" and "missing
skeleton instance falls back to a static draw" (with `debugInstallPartMesh` as a
headless install hook). Full suite PASS.

## 14. animation.update

Kept intentionally (local-player compatibility). Not removed.

## Evidence

- `python build_agent.py` -> `Status: SUCCESS`.
- `--hot-combat-selftest` -> PASS (skinned consumption + fallback + prior checks).
- Full suite (9 selftests) -> PASS.

## Honest limitation

The real `mesh.actor` GLB loader still produces a **merged static** mesh with no
bone-tagged `parts`, so the real remote NPC still renders static. Visible
deformation requires a part-aware/skinned GLB load that populates `GpuMesh::parts`
(node name -> bone hash, index range, bind matrix). The consumption mechanism is
SELFTEST PROVEN; the real NPC GPU skin is not yet visible.

## Classification

- SELFTEST PROVEN: EntityId lookup; per-part skinned draw path; static fallback.
- COMPILED INTEGRATION: `GpuMesh::parts`; `submitMesh` skinned branch; counters.
- LIVE MULTIPLAYER PROVEN: no.
- LIVE VISUAL PROVEN: no.
- HUMAN VERIFICATION NEEDED: part-aware GLB load; visible posed NPC; live pose
  edit; then attack/jump + blend.

## Files changed

`src/render/presentation-render.{h,cpp}`, `src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.

## Next cold owner selected

Part-aware/skinned GLB load into `GpuMesh::parts` (bind matrices per node) so hot
`PoseState` visibly deforms the real remote NPC; then attack/jump; then blend;
then generation-aware skeleton/clip resources.
