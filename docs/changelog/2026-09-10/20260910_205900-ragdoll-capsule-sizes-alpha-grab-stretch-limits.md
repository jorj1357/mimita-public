# Ragdoll: locked/editable capsule sizes, alpha, grab radius, arm stretch, strict limits

- Task ID: ragdoll-capsule-sizes-alpha-grab-stretch-limits
- Summary: Expose capsule size/axis/alpha per part without changing current
  sizes; wire `grab_radius`; add a one-sided stretchy shoulder joint and
  configurable arm reach/body pull for climbing; make rotation limits strict.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: `2026-09-10T20:59:00Z` (2026-09-10 16:59:00 EDT)
- Branch: `8292026stash`
- Base commit: working tree; no commit created
- Final commit: none (uncommitted)

## Pre-existing changes

- Not created by this session: the FFA/kill-event/countdown network/gamemode
  work, an unrelated in-progress `src/gui/hud/chat-window.cpp` edit, and the
  other `docs/changelog/2026-09-10/` files. Not claimed here.
- Earlier ragdoll work: `155338`, `163323`, `163852`, `172838`, `174658`,
  `180706`, `183703`, `190049`, `190421`, `193151`.
- Human `config/ragdoll.json` tuning preserved; new keys added.

## Implementation changes

### A. Capsule sizes + alpha
- `config/ragdoll.json` `capsules.<part>` now carries explicit `radius`,
  `half_height`, `axis`, `alpha`, and `center_of_mass`. The values equal the
  previous derived sizes (head r0.30 hh0.14; torso r0.16 hh0.44; arms r0.208
  hh0.592; legs r0.30 hh0.50; axes as before), so behavior is unchanged but the
  sizes are now editable.
- `RagdollModeCapsuleConfig.alpha` added and parsed; `render` uses it for the
  capsule color alpha (1 opaque, 0 invisible).

### B. `grab_radius`
- `raycastWorld` gained a radius parameter; `processGrab` now sweeps a sphere of
  `grab.grab_radius` from the hand along camera-forward, still limited to the
  `grace_distance` from the hand.

### C. Arm stretch + climb
- New config `arms.max_stretch`, `arms.stretch_force`, `arms.body_pull`.
- `RagdollModePart.maxStretch` set for arms. The arm shoulder joint is now a
  one-sided max-distance constraint (`solvePointJointMaxDistance` /
  `...Velocity`): anchors stay coincident up to `restLength + maxStretch` and
  are pulled back only beyond it. Other limbs keep rigid joints.
- `processExtend` keeps the angular aim and adds a linear reach along
  camera-forward while the shoulder is within `restLength + maxStretch`; once
  fully stretched it optionally applies `arms.body_pull` to the torso. With a
  grab the hand is pinned, so the reach pulls the torso toward the wall (climb);
  without a grab the arm telescopes without dragging the body (prevents flying).

### D. Strict rotation limits
- `solveRotationLimits(float betaOverride)`; betas are clamped to [0,1]
  (`joint_position_beta` 1.5 in the human's config could previously overshoot).
- A final `solveRotationLimits(1.0)` runs after all other constraints and just
  before `syncToPlayer`, so per-axis limits are not overridden by self-collision
  or depenetration. The actively-extending arm exception is retained.

## Validation

- Build: `python build_agent.py` -> `Status: SUCCESS`, return code 0, duration
  18.34s; `mimita.exe` relinked. No errors or warnings.
- Config: `config/ragdoll.json` parses; capsule sizes/axis/alpha, `arms`
  stretch keys, and `grab_radius` verified.
- Runtime: not performed this session.

## Regression review

- No new append-only entry.

## Human acceptance

- Confirm capsule sizes match the previous look and that editing
  `capsules.<part>.radius/half_height/axis` and `alpha` takes effect live.
- Confirm grabs use `grab_radius` and remain solid.
- Confirm extending stretches the arm from the shoulder, the torso follows up to
  `max_stretch` when grabbing, and holding arms out cannot fly.
- Confirm leg rotation limits hold (feet do not rotate up into the torso),
  especially when diving.
