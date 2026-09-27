// 2026-09-27T20:35:40Z
/* purpose
* record the investigation-only audit of the current collision architecture
* separate current runtime evidence from the proposed migration
* does NOT change collision behavior or claim human gameplay acceptance
*/

# Task

- Summary: Audit current MiMITA collision ownership, the limb/world path, shape
  usage, moving-object readiness, and the smallest migration toward one common
  contact contract.
- Status: INVESTIGATION_COMPLETE / CODE_UNCHANGED / HUMAN_REVIEW_REQUIRED
- Scope: `C:\mimita-v9` working tree, including pre-existing dirty and untracked
  actor-triangle work.

# Findings

- The active local GLB path is a staged pipeline, not one universal solver:
  body/weapon contacts, root capsule sweep/slide, root depenetration, floor
  recovery, emergency escape, then movement feedback.
- `MovementContact` is already a useful downstream movement fact with point,
  normal, surface velocity, penetration, source, and entity identity fields.
  `Contact` and `RecoveryContact` are narrower parallel representations.
- Actor body collision is player-local. `doGLBTriangleCollisions` skips the
  body/weapon phase for NPCs; server player movement uses a separate sampled
  headless capsule-style resolver; ragdolls use `RigidBody` capsule collision.
- The current limb failure is architectural rather than evidence that capsules
  are intrinsically broken: body contacts can be detected, but correction and
  velocity response are owned by separate staged paths, visible limb transforms
  can differ from proxies, and later/root-only passes do not make a body-part
  contact a first-class body-to-body physical result.
- Moving entities, support identity, support velocity, and jump inheritance are
  not implemented in the active gameplay path. The contact vocabulary has
  `MovingWorld`/`surfaceVelocity` fields, but no active producer establishes a
  moving support reference frame.
- Triangle-vs-triangle is useful for selected articulated body meshes, but the
  repository specification explicitly permits analytic sphere/capsule/box and
  mesh shapes. A universal primitive-pair detector producing one generalized
  contact is the safer target than forcing every object into triangles.

# Evidence

- `mimita.exe --collision-selftest`: PASS, including moving-limb bounce,
  embedded-limb minimum push, mesh floor contact, swept thin-wall crossing, and
  leaving an old wall contact.
- `mimita.exe --actor-triangle-spike`: PASS; six body parts and a weapon mesh
  were extracted and transformed CPU-only without a GL context.
- No human visual, multiplayer, moving-platform, or server/client parity
  acceptance was performed.

# Next step

The smallest coherent implementation slice is a read-only/diagnostic contract
adapter around the existing `RecoveryContact` producers: preserve the root
capsule and primitive paths, add stable owner/entity/shape/material/support
metadata at the convergence point, and make one response consumer record both
position correction and velocity response. Prove it first for local player
root-plus-body contacts, then extend the same adapter to NPC/server and
ragdoll paths. Do not delete capsules or activate full actor-triangle collision
for every actor until that parity is demonstrated.

# Human review required

- Confirm the exact desired authority boundary for server player collision
  before implementation.
- Manually verify the current limb/slope/wall behavior and the cost of mesh
  collision with `bodyMeshCollision` enabled.
- Approve the narrow contact-adapter slice before code changes.
