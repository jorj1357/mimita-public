# Ragdoll: revert quaternion camera to normal clamped euler

- Task ID: ragdoll-revert-quaternion-camera
- Summary: Remove the quaternion free-look camera and restore the normal
  yaw/pitch camera. Mouse up always looks up regardless of ragdoll orientation
  because the camera stays world-referenced and pitch is clamped.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: `2026-09-10T19:04:21Z` (2026-09-10 15:04:21 EDT)
- Branch: `8292026stash`
- Base commit: working tree; no commit created
- Final commit: none (uncommitted)

## Pre-existing changes

- Not created by this session: the FFA/kill-event/countdown network and gamemode
  changes already in the working tree, and the other `docs/changelog/2026-09-10/`
  files. Not claimed here.
- Earlier ragdoll work: `20260910_155338`, `163323`, `163852`, `172838`,
  `174658`, `180706`, `183703`, `190049`.

## Requested behavior

- Revert the quaternion camera rotation and use the normal camera only.
- Do not let mouse-up look down when the view is flipped; the camera must behave
  the same regardless of the ragdoll orientation.

## Exact implementation changes

- `src/camera.h`: removed `freePitch`, `freeLookOrientation`, `freeLookInit`,
  and the added `<glm/gtc/quaternion.hpp>` include, restoring the plain camera.
- `src/camera.cpp`:
  - `updateMouse` restored to the euler path: `yaw -= xoff; pitch += yoff;` with
    the +/-89.9 pitch clamp and `front` from yaw/pitch.
  - `updateVectors` restored to the clamped-euler basis with
    `right = cross(front, worldUp)` and `up = cross(right, front)`.
- `src/engine/engine-tick-camera.cpp`: removed the per-frame
  `camera.freePitch = ...` assignment.

The ragdoll head/torso aim (configurable `aim` axes) and all solver/jitter knobs
from the previous pass are unchanged.

## Validation

- Build: `python build_agent.py` -> `Status: SUCCESS`, return code 0, duration
  37.19s; `mimita.exe` relinked. No errors or warnings.
- Grep: no remaining `freePitch` / `freeLookOrientation` / `freeLookInit`.
- Runtime: not performed this session.

## Regression review

- No new append-only entry.

## Human acceptance

- Confirm the camera behaves like normal gameplay while ragdolled (clamped pitch)
  and that mouse-up never looks down regardless of body orientation.
