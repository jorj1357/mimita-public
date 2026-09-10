# Ragdoll: configurable aim axes, anti-jitter knobs, quaternion free look

- Task ID: ragdoll-aim-axes-jitter-camera-quat
- Summary: Add per-part `aim` front/up axes so the correct side faces the camera;
  replace the oscillating look motor with a damped controller and expose the
  solver knobs; use a quaternion camera while ragdolled so look up/down never
  reverses past the poles.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: `2026-09-10T19:00:49Z` (2026-09-10 15:00:49 EDT)
- Branch: `8292026stash`
- Base commit: working tree; no commit created
- Final commit: none (uncommitted)

## Pre-existing changes

- Not created by this session: the FFA/kill-event/countdown network and gamemode
  changes already in the working tree, and the other `docs/changelog/2026-09-10/`
  files. Not claimed here.
- Earlier ragdoll work: `20260910_155338`, `163323`, `163852`, `172838`,
  `174658`, `180706`, `183703`.
- The human's `config/ragdoll.json` tuning (attachments offsets, strengths,
  friction, etc.) was preserved; new keys were added alongside it.

## Root causes

1. Wrong side faces forward: `lookRotation` assumes local +Y = forward / +Z = up,
   but the model's mesh forward in the canonical body frame is a different axis.
2. Jitter: the look motor added angular velocity every tick with no damping (pure
   P control, overshoots), and the solver constants were hardcoded. Also
   `joint_stiffness`/`joint_damping` are parsed but unused by ragdoll mode, so
   tuning them had little/no effect.
3. Camera flip: the Euler camera reverses the horizontal basis once `cos(pitch)`
   goes negative, so mouse-up looks down past the poles.

## Implementation changes

### Aim axes
- New `RagdollModeAimConfig` (`frontAxis`, `upAxis`) and `config/ragdoll.json`
  top-level `"aim"` section (not folded into attachments).
- `initParts` precomputes a per-part `aimOffset` that maps the configured
  front/up axes onto `lookRotation`'s (+Y forward, +Z up) convention.
- `applyControls` aims head and torso at `lookRotation(camera.front, worldUp) *
  part.aimOffset`. Uses world up for stability (falls back to `camera.up` when
  the front is near vertical).
- Default `front_axis` is `[1,0,0]`, undoing the current "left faces forward".

### Anti-jitter
- The look motor is now a damped velocity controller: compute the desired
  angular velocity, then blend the current toward it with `look_damping`
  (`body_smoothing` is separate and render-only).
- Exposed hardcoded constants in `ragdoll.json`:
  `joint_position_beta`, `limit_position_beta`, `self_collision_iterations`,
  `self_collision_beta`, `max_angular_speed`, `look_damping`, `body_smoothing`.
- `collideBodies` gained a `correctionBeta` parameter, used by self-collision.
- Optional `body_smoothing` exponentially smooths the rendered part transforms
  (physics and gameplay keep the raw body state).
- `joint_stiffness`/`joint_damping` remain in config but are documented unused
  for ragdoll mode.

### Quaternion free-look camera
- `Camera` gained `freeLookOrientation` + `freeLookInit`.
- While `freePitch` is set (engine sets it from ragdoll active), `updateMouse`
  applies yaw about world up and pitch about the camera's right, accumulating in
  a quaternion, so look up/down never reverses.
- `updateVectors` derives front/up/right from the quaternion and keeps
  `yaw`/`pitch` updated (pitch clamped in that representation) so movement,
  `input.lookYaw/lookPitch`, and client packets keep working. A future change can
  send the quaternion directly.

## Validation

- Build: `python build_agent.py` -> `Status: SUCCESS`, return code 0, duration
  133.79s; `mimita.exe` relinked 2026-09-10 15:00:38. No errors or warnings.
- Config: `config/ragdoll.json` parses; `aim` and all new keys verified present.
- Runtime: not performed this session.

## Regression review

- No new append-only entry.

## Human acceptance

- Set `aim.head.front_axis` / `aim.torso.front_axis` until the forward side faces
  the camera.
- Tune `look_damping` (and `joint_position_beta` / `limit_position_beta` /
  `self_collision_*`) to remove jitter; use `body_smoothing` for a visual assist.
- Confirm the camera keeps looking up/down past the poles without reversing, and
  returns to the normal clamped camera after exiting ragdoll.
