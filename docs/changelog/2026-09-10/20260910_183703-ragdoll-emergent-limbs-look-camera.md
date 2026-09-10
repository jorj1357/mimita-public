# Ragdoll: emergent limbs, self-collision, look strength, free camera pitch

- Task ID: ragdoll-emergent-limbs-look-camera
- Summary: Remove the positional cone that held the legs up; enable
  directly-jointed self-collision with a joint exclusion; give the torso and
  head a tunable camera-look motor; allow the ragdoll camera to pitch
  continuously past straight up/down.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: `2026-09-10T18:37:03Z` (2026-09-10 14:37:03 EDT)
- Branch: `8292026stash`
- Base commit: working tree; no commit created
- Final commit: none (uncommitted)

## Pre-existing changes

- Not created by this session: the FFA/kill-event/countdown network and gamemode
  changes already in the working tree, and the other `docs/changelog/2026-09-10/`
  files. Not claimed here.
- Earlier ragdoll work in this session:
  `20260910_155338`, `20260910_163323`, `20260910_163852`, `20260910_172838`,
  `20260910_174658`, `20260910_180706`.
- The human edited `config/ragdoll.json` during testing (friction, mass,
  torso_look_spring). Their values were preserved except torso_max_angular_step
  and head strength/speed, which were raised for the requested stronger look.

## Root cause of the floating legs

`solveConeLimits` was a positional clamp that moved `child.position` directly.
Its axis `restDirectionLocal` was computed from `child.position - parentAnchor`;
for legs the anchor is the capsule segment end nearest the torso, which lies
below the leg node, so the vector pointed up. The cone therefore actively held
legs up and kept them moving. Editing mass could not help, and per-axis rotation
limits only partly fought it.

## Implementation changes

### Emergent limbs (remove the cone)
- Deleted `solveConeLimits` and its call, and removed `restDirectionLocal` from
  `RagdollModePart`. Limbs are now governed by gravity, the shoulder/hip ball
  joint, world collision, self-collision, and the configurable per-axis
  `rotation_limit_deg` only. `cone_limit_deg` remains parsed for compatibility
  but is unused.

### Self-collision
- `collideBodies` gained an optional `excludePoint` / `excludeRadius`; contacts
  whose closest points fall inside that sphere are ignored.
- `selfCollision` now includes directly-jointed pairs, excluding a sphere of
  radius `childRadius + parentRadius` around their shared joint anchor, so
  limbs can collide with the torso/other limbs but do not fight their own joint.
- Self-collision runs twice before the joint re-convergence and twice after it,
  so the joint pass cannot re-penetrate parts.

### Torso/head camera look
- `applyControls` now runs an angular "aim at camera" motor on both the head and
  the torso. It reuses the existing hot-reloadable keys: `head.rotation_strength`
  / `head.rotation_speed` and `torso_look_spring` / `torso_max_angular_step`.
- Defaults in `config/ragdoll.json` raised (`torso_max_angular_step` 15 -> 90,
  head `rotation_strength` 12 -> 40, `rotation_speed` 18 -> 60) so the body
  clearly faces where the camera looks, especially in third person.

### Free camera pitch while ragdolled
- `Camera` gained `freePitch`. `updateMouse` and `updateVectors` skip the
  +/-89.9 clamp when it is set, and the basis uses a pole-safe fallback
  (cross with X when front is near vertical) so pitch can keep rotating past the
  poles indefinitely.
- `engineTickCamera` sets
  `camera.freePitch = player.ragdollModeActive && RagdollModeSystem::instance().isActive()`
  each frame, so normal clamped camera returns on exit.

## Diagnostics

- Existing `[RAGDOLL SYM]` and structured tick events remain; no `printf` added.

## Validation

- Build: `python build_agent.py` -> `Status: SUCCESS`, return code 0, duration
  115.40s; `mimita.exe` relinked 2026-09-10 14:36:51. No errors or warnings.
- Runtime: not performed this session.

## Regression review

- No new append-only entry. The left-leg entry at `2026-09-10T17:46:58Z`
  remains the relevant record.

## Human acceptance

- Confirm legs no longer float and settle under gravity; tune
  `rotation_limit_deg` and `body_angular_damping` / `stop_*` as needed.
- Confirm no limb enters its own or another limb's geometry.
- Confirm torso/head strongly face the camera look in third person; tune the
  reused strength keys.
- Confirm the camera can keep pitching past straight up/down while ragdolled and
  returns to the normal clamp after exiting.
