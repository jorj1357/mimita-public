# Ragdoll: canonical body frames, folded damping, natural stop

- Task ID: ragdoll-canonical-frames-damping
- Summary: Stop the torso from reading as rotated 90 by making physics body
  frames canonical and moving the model's baked Z-up rotation into a per-part
  mesh offset; fold the unused `linear_damping`/`angular_damping` into the body
  damping; add per-part stop thresholds so limbs settle naturally.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: `2026-09-10T18:07:06Z` (2026-09-10 14:07:06 EDT)
- Branch: `8292026stash`
- Base commit: working tree; no commit created
- Final commit: none (uncommitted)

## Pre-existing changes

- Not created by this session: the FFA/kill-event/countdown network and gamemode
  changes already in the working tree, and the other `docs/changelog/2026-09-10/`
  files. Not claimed here.
- Earlier ragdoll work in this session:
  `20260910_155338-...rigid-body-core`, `20260910_163323-...collision-solid`,
  `20260910_163852-...stiffness-limits-camera`,
  `20260910_172838-...model-frame-unify`,
  `20260910_174658-...capsules-attachments-left-leg`.
- The human's tuning of `config/ragdoll.json` was preserved except where noted
  below (restitution set to 0 because it prevented the requested settling).

## Root cause: torso read as rotated 90 degrees

`initParts` bound the physics frame to the raw mesh node frame
(`body.orientation = quat_cast(nodeWorld)`). The model bakes a Z-up conversion
into every part (`config/bodyparts.json` rotation `[90,0,0]`, applied as
`T * Rx(90)` in `player-loader.cpp:230-237`), so `torso.body.orientation =
yaw * Rx(90)`. The capsule still looked vertical (mesh-long axis local Y times
Rx90 gives world Z), but the body frame, `modelRootRotation`, attachment axes,
and rotation limits all carried the 90-degree rotation.

## Implementation changes

### Canonical body frames
- `RagdollModePart` gained `meshLocal` (transform from the canonical body frame
  to the mesh node frame).
- `initParts` now sets `body.orientation = bindCanonical` (character yaw frame,
  shared by all parts), computes `meshLocal = inverse(bodyBindWorld) * nodeWorld`,
  and expresses the derived capsule center/axis in the canonical frame via
  `meshLocal`'s rotation. Config capsule `offset`/`axis` are applied in the
  canonical frame.
- `syncToPlayer` writes `node.local = inverse(parentWorld) * bodyWorld * meshLocal`
  so the visible mesh stays 1:1 while the physics frame is unrotated.
- `RigidBody` already exposes `capsuleCenter`/`localAxis`; no core change needed.

### Damping folded in
- `ragdoll-mode-config.cpp`: `body_linear_damping` falls back to
  `linear_damping`, and `body_angular_damping` falls back to `angular_damping`.
- Defaults raised to `body_linear_damping = 0.5`, `body_angular_damping = 2.5`
  so motion dies out instead of rolling forever.

### Natural stop (no sleeping)
- Added `stop_linear_speed` (default 0.1) and `stop_angular_speed` (default 0.4)
  to config, applied to each part.
- At the end of the ragdoll tick, a part below those speeds has its linear and
  angular velocity zeroed. The threshold is deliberately below one tick of
  gravity (about 0.163 m/s), so a free part still falls while a nearly still
  limb comes to rest. No global sleep/wake state was added, per the human.

### Config
- `config/ragdoll.json`: added `body_linear_damping: 0.6`,
  `body_angular_damping: 2.5`, `stop_linear_speed: 0.1`,
  `stop_angular_speed: 0.4`; removed the duplicate `body_*` keys; set
  `restitution: 0.0` (was 1.0, which bounced indefinitely). Human values for
  `friction` and `mass.globalMultiplier` were preserved.

## Diagnostics

- Existing `[RAGDOLL SYM]` and structured tick events remain; no `printf` added.

## Validation

- Build: `python build_agent.py` -> `Status: SUCCESS`, return code 0, duration
  19.29s; `mimita.exe` relinked 2026-09-10 14:06:53. No errors or warnings.
- Config: `config/ragdoll.json` parses; damping/stop/rest values verified.
- Runtime: not performed this session.

## Regression review

- No new append-only entry; this continues uncommitted ragdoll work. The
  left-leg entry at `2026-09-10T17:46:58Z` remains the relevant record.

## Human acceptance

- Confirm the torso no longer reads as rotated 90 degrees in the debug axes and
  attachment visuals, and that the mesh still matches the capsules.
- Confirm the left leg matches the right.
- Tune `body_angular_damping` / `stop_angular_speed` until limbs stop rolling but
  still fall naturally; tune `mass.globalMultiplier` for weight feel.
