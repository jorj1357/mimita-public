# RMB camera-aligned right-arm pointing

Time: 2026-10-02T07:56:49Z
Branch: `afad20a-rebuild`

## Result

Added an all-weapons normal-play right-arm pointing mode. With the active
`config/aimbody.json` setting `right_arm_pointing: "rmb"`, holding the right
mouse button steers the physical right arm toward the full camera-forward
direction. Releasing RMB returns control to the existing weapon and animation
pose. This is not aim-down-sights and does not change firing or weapon rules.

## Ownership and implementation

- `src/entities/aimbody-config.{h,cpp}` owns the hot-reloadable
  `right_arm_pointing` mode and defaults missing values to `off`.
- `src/sim/simulate-tick.cpp` forwards the held RMB state into the fixed 60 Hz
  aim-body update.
- `src/ragdoll/ragdoll-mode.{h,cpp}` owns the shared right-arm motor. It reuses
  the ragdoll arm-axis mapping and `ragdoll.json` physical aim tuning, applies
  angular velocity/torque rather than snapping transforms, and runs after the
  hybrid animation-follow correction. The held arm is exempted from the old
  hybrid sway and arm-range corrections while active.
- `config/aimbody.json` enables the mode for the current hybrid aim-body setup.

No weapon-specific branches or weapon offsets were added.

## Validation

- Focused source search confirmed one updated `updateAim` declaration,
  definition, and call site, plus the new config/motor references.
- `git diff --check` passed for the files changed by this feature. Existing
  unrelated worktree warnings remain outside this change.
- `python build_agent.py` returned `Status: SUCCESS` and `Return Code: 0`.
  The incremental build reported `Nothing changed`; the generated objects for
  `ragdoll-mode.cpp`, `aimbody-config.cpp`, and `simulate-tick.cpp` are newer
  than their sources.

## Human review still required

Launch the current build and hold RMB with at least a revolver, shotgun, and
rocket launcher. Check camera left/right and up/down, movement, weapon firing,
release behavior, and that the arm does not visually snap or point sideways.
Build/source evidence is not visual acceptance.
