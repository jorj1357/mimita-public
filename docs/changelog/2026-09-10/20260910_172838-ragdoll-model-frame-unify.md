# Ragdoll Phase F: unify physics and mesh frames, attachments, extension, grabs

- Task ID: ragdoll-model-frame-unify
- Summary: Make the visible player model 1:1 with the ragdoll capsules and free
  the torso to tip on all axes; derive capsules/anchors from the mesh;
  make arm extension point accurately along camera-forward without pulling the
  player; stop grabs from snapping the body into geometry.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: `2026-09-10T17:28:38Z` (2026-09-10 13:28:38 EDT)
- Branch: `8292026stash`
- Base commit: working tree; no commit created
- Final commit: none (uncommitted)

## Pre-existing changes

- Not created by this session: the FFA/kill-event/countdown network and gamemode
  changes already in the working tree, and the other files under
  `docs/changelog/2026-09-10/`. Not claimed here.
- Earlier ragdoll work in this session:
  `20260910_155338-ragdoll-mode-rigid-body-core.md`,
  `20260910_163323-ragdoll-collision-solid.md`,
  `20260910_163852-ragdoll-stiffness-limits-camera.md`.
- `config/ragdoll.json` was tuned by the human during testing; their rotation
  limits and arm extension strength were preserved.

## Root cause found

`RagdollModeSystem::syncToPlayer` wrote skeleton local transforms, but
`Player::render` calls `Player::updateModelWorldTransforms`, which re-roots the
whole model at `transformMatrix(movementCapsule.position, movementCapsule.rotation)`
with `movementCapsule.rotation = yawRotation(yaw)` (`src/entities/player.cpp:159-191`).
That is a yaw-only root, so the ragdoll torso orientation was cancelled every
frame (torso read as world-Z locked), and limbs were composed under a different
root than the capsules. Separately, `initParts` placed bodies from torso-relative
config offsets with `orientation = yawRotation(yaw)`, so the physics frames did
not match the mesh node frames (which the loader builds with a `[90,0,0]` Z-up
conversion from `config/bodyparts.json`). That mismatch is why limb axes and
extension looked wrong and limbs appeared to rotate about `plrOrigin`.

## Exact implementation changes

### F1 - model root override
- `src/entities/player.h`: added `modelRootRotationActive` and
  `modelRootRotation`.
- `src/entities/player.cpp`: `syncLegacyStateToLayers` uses the override quat
  instead of `yawRotation(yaw)` when active.
- `RagdollModeSystem` sets the override to the physical torso orientation each
  tick and clears it on exit, so pitch/roll survive and the torso can tip.

### F2 - physics frame = mesh node frame
- `initParts` reads each body part's `perfectPoseSkeleton.nodes[nodeIndex].worldTransform`
  and sets `body.position` / `body.orientation` from it. `activate` first calls
  `player.updateModelWorldTransforms()` so bind frames are current.

### F3 - capsule geometry from the mesh (ragdoll.json override)
- Derives `capsuleCenter`, `localAxis`, `radius`, `halfHeight` from
  `PhysicalBodyPart.collider.localMin/localMax`.
- `RigidBody` gained `capsuleCenter`; `capsuleOf` now offsets the capsule
  center by it.
- `RagdollModeCapsuleConfig` defaults radius/halfHeight to -1 (derive);
  ragdoll.json can override radius/half_height and add a part-frame offset.
  `config/ragdoll.json` capsule entries are empty so derivation is used.

### F4 - attachments rotate about the shoulder/hip
- Anchors are derived from the capsule endpoint nearest the parent body, so the
  child pivots at its shoulder/hip rather than the player origin. `offset` is an
  optional extra parent-frame adjustment (set to zero in config).

### F5 - `syncToPlayer` is a true 1:1 map
- Non-part skeleton ancestors (e.g. `plrOrigin`) are neutralized to identity
  while ragdolled; each part node local is
  `inverse(parentWorld) * bodyWorld`, where the parent is the skeleton-parent
  part if any, otherwise the model root. Restored to rest on exit.
- The authoritative root is anchored to a fixed point in the torso frame
  (`mRootOffsetLocal`), so repeated toggles no longer drift.

### F6 - camera-forward extension
- Extension now derives the hand from the capsule: the endpoint farther from the
  shoulder anchor. It steers the arm's angular velocity so the vector
  shoulder->hand aligns with `camera.front`. It is an internal angular motor, so
  it adds no net linear momentum and cannot pull the player forward; the rigid
  shoulder attachment limits reach to the arm's length.
- Rotation limits are suspended for the extending arm so they cannot block
  reaching camera-forward.

### F7 - grab without snapping or tunneling
- Acquisition only succeeds when the ray hit is within
  `grab.grace_distance + capsuleRadius` of the hand; the previous 2.5 m reach
  could teleport the hand and yank the body into geometry.
- Grab still solves velocity then position at near-rigid stiffness, and the final
  `depenetrateWorld` pass keeps the body out of walls.

## Diagnostics

- No new log sites. Existing `StructuredCategory::Ragdoll` tick events remain.

## Validation

- Build: `python build_agent.py` -> `Status: SUCCESS`, return code 0, duration
  198.11s; `mimita.exe` relinked 2026-09-10 13:28:23. No errors or warnings.
- Config: `config/ragdoll.json` parses; capsules derive, attachments have zero
  offset, human rotation limits and extension strength preserved.
- Runtime: not performed this session. Torso tipping, mesh/capsule 1:1, camera-
  forward extension, and grab solidity still need human play testing.

## Regression review

- No append-only entry added; continues uncommitted ragdoll feature work.

## Human acceptance

- Visual review required: while ragdolled, the visible limbs/torso/head must
  track the debug capsules exactly, and the torso must tip over in 3D.
- Gameplay review required: hold LMB/RMB; the arm must reach toward where you
  look, limited by the shoulder, without moving the player.
- Gameplay review required: grab a wall; the grip must hold without snapping the
  body into the wall.
- Known follow-up: head mesh aim still uses a world-space look rotation and may
  not match the head node's bind convention in third person.

## Spec TODOs observed (not edited)

- `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md:1` and `:7` remain as
  previously noted.
