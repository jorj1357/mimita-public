# Boomer arm pivot correction

Date: 2026-10-11 (EST)
Status: SOURCE_AND_ASSET_COMPLETE_WITH_RUNTIME_REVIEW_PENDING

## Request

Make `mimita-boomer-v11.glb` arm movement behave like the other avatars: arms
should swing from the shoulder origin instead of twisting around the middle of
the mesh.

## Finding

The runtime animation path is shared and applies each procedural pose after the
part's authored rest transform. The working girl model places its arm mesh
away from the arm node, so the node is the shoulder pivot. Boomer v11 placed
the arm nodes at the shoulder positions but centered both arm meshes around
their nodes. Its arm POSITION bounds were approximately `z=[-1.235,+1.235]`.

Therefore the renderer was correctly rotating the Boomer mesh around its
authored node; the authored mesh origin was the wrong pivot for a shoulder
swing. This is an asset-authorship issue, not a second animation owner or a
global rotation-axis bug.

## Change

- Created `assets/entity/player/monsters/boomer/mimita-boomer-v12.glb` from
  v11.
- Translated only the local POSITION data of `leftArm` and `rightArm` so each
  mesh's upper endpoint is at local `z=0`, the existing arm-node origin.
- Updated `assets/avatars/monstertest1/avatar.json` to select v12 while keeping
  `body_transform_source=model`.
- Kept v11 unchanged and removed the one-time conversion helper after use.

The corrected arm bounds are approximately `z=[-2.470,0]` for both arms.
No runtime animation, ragdoll, or generic GLB-loader workaround was added.

## Routed review

- `docs/ROUTER.md`
- `docs/specs/avatar/avatar-editor.md`
- `docs/operations/asset-management/asset-management.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/asset-checker-v1.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/gold/2026-10-06-avatar-bind-anchors-and-jsonl-evidence.md`
- `docs/changelog/2026-10-11/20261011_171500-avatar-editor-transform-contract.md`

## Validation evidence

- v11 and v12 both parse as valid GLB files.
- The v12 node names remain `plrOrigin`, `head`, `torso`, `leftArm`,
  `rightArm`, `leftLeg`, and `rightLeg`.
- The v12 arm POSITION accessor bounds confirm the intended pivot translation.
- `monstertest1/avatar.json` parses and selects v12 with model-authored
  transforms.
- No source build was required for this asset-only change.
- The existing dev loop is currently running build 2179 in AtDM mode; it has
  not exercised the changed avatar, so no runtime journal or visual acceptance
  is claimed.

## Human review still required

Load `monstertest1` in the real game and verify both arms swing forward/back
from the shoulder in normal and hybrid/ragdoll presentation. Confirm the arm
mesh, weapon hand, collision shape, and shoulder attachment agree visually.
