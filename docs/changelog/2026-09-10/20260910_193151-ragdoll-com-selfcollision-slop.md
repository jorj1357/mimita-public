# Ragdoll: editable per-part center of mass, self-collision skin/slop

- Task ID: ragdoll-com-selfcollision-slop
- Summary: Put the physics center of mass at the capsule center (editable per
  part) so limbs hang from their attachment instead of behaving like inverted
  pendulums; give self-collision a skin/slop and a capped per-pass correction so
  touching limbs separate calmly instead of jittering.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: `2026-09-10T19:31:51Z` (2026-09-10 15:31:51 EDT)
- Branch: `8292026stash`
- Base commit: working tree; no commit created
- Final commit: none (uncommitted)

## Pre-existing changes

- Not created by this session: the FFA/kill-event/countdown network and gamemode
  changes, and an unrelated in-progress edit to `src/gui/hud/chat-window.cpp`
  (another session). A concurrent partial write briefly broke that file during
  one build; the file is syntactically valid in the working tree and the final
  build linked successfully. Not claimed here.
- Earlier ragdoll work: `20260910_155338`, `163323`, `163852`, `172838`,
  `174658`, `180706`, `183703`, `190049`, `190421`.
- The human's `config/ragdoll.json` tuning was preserved; new keys added.

## Diagnosis

1. **Limbs behaved like inverted pendulums.** `RigidBody.position` was the mesh
   node (hip/shoulder) while the capsule center (the visual mass) was offset
   below it. The attachment anchor is the capsule end nearest the parent, so the
   COM sat ~0.26 m *above* the anchor; gravity then tipped the limb upward.
2. **Self-collision jitter.** `collideBodies` used direct position teleport with
   no slop and ran 4 times/tick around the joint pass, so self-collision and the
   joints fought every tick, and limbs that merely touched (legs are radius 0.3
   and ~0.6 apart) chattered forever.

## Implementation changes

### Editable center of mass
- `RagdollModeCapsuleConfig.centerOfMass` (part/canonical frame), parsed from
  `capsules.<part>.center_of_mass`.
- `initParts`:
  - capsule center = mesh collider center (rotated to canonical) + config
    capsule `offset`.
  - COM = node position + canonical rotation * (capsuleCenter + centerOfMass).
  - `body.position` = COM; `body.capsuleCenter` = `-centerOfMass` so the capsule
    stays where configured while the COM moves.
  - `meshLocal` is now a full transform (translation + rotation) from the COM
    body frame to the mesh node.
- Default COM is the capsule center, so limbs hang downward from the attachment.
  Moving `center_of_mass` moves the mass relative to the capsule.

### Self-collision skin/slop
- `self_collision_skin` (default 0.015) and `self_collision_max_correction`
  (default 0.05) added to config.
- `collideBodies` gained `slop` and `maxCorrection`: overlap inside the skin is
  ignored, and the per-pass positional correction is capped.
- `selfCollision` passes `selfCollisionBeta`, `selfCollisionSkin`, and
  `selfCollisionMaxCorrection`.
- Tick order: one self-collision pre-pass before the joints, then
  `self_collision_iterations` post-passes (previously 2+2).

## Validation

- Build: `python build_agent.py` -> `Status: SUCCESS`, return code 0; final link
  pass 3.59s, `mimita.exe` at 2026-09-10 15:31:38. `physical-body.cpp`,
  `ragdoll-mode.cpp`, `ragdoll-mode-config.cpp` compiled with the new code. No
  errors or warnings in the final pass.
- Note: one earlier build failed on the unrelated concurrent `chat-window.cpp`
  edit; a clean re-link succeeded after that file settled.
- Config: `config/ragdoll.json` parses; new keys verified.
- Runtime: not performed this session.

## Regression review

- No new append-only entry.

## Human acceptance

- Confirm limbs now hang from their attachments and settle.
- Tune `capsules.<part>.center_of_mass` to place each limb's mass.
- Tune `self_collision_skin`, `self_collision_max_correction`,
  `self_collision_beta`, `self_collision_iterations` until touching limbs
  separate with a small calm push and no jitter.
