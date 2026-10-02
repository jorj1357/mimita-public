# RMB centered physical right arm

Time: 2026-10-02T13:15:28Z
Branch: `afad20a-rebuild`

## Result

Refined RMB aiming to match the requested centered-Doom/Quake-style physical
arm behavior. The right arm is no longer only rotated from its normal shoulder
offset. While RMB is held, its physical center is driven toward a configurable
player-local target near the player center and shoulder height, while its
weapon barrel is aimed along the camera ray.

## Configuration

`config/aimbody.json` now owns the live presentation target:

- `right_arm_pointing.mode`: `rmb` or `off`;
- `right_arm_pointing.center_offset`: player-local `[forward, lateral, up]`.

`config/ragdoll.json` owns the physical response:

- `right_arm_pointing_position_force` controls how strongly the arm returns to
  the center target;
- `right_arm_pointing_position_damping` controls motion settling;
- `right_arm_pointing_max_stretch` controls how far the shoulder link may
  stretch while aiming.

Lower force/damping leaves more movement momentum in the arm. There is no random
spread added by this feature; motion error comes from the physical body,
movement, collisions, and the configured response.

## Implementation

- `AimBodyConfig` parses the object form while retaining the old string form.
- `RagdollModeSystem` applies a spring-like linear acceleration and the existing
  torque motor before fixed-tick integration.
- Hybrid animation-following skips the actively pointed right arm instead of
  immediately pulling it back to the ordinary weapon pose.
- The existing ragdoll joint solver provides the temporary arm stretch and
  collision response.

## Validation

- Focused `git diff --check` passed.
- `python build_agent.py` returned `Status: SUCCESS` and return code `0`.
- Generated objects for the changed ragdoll and aim-body translation units are
  newer than their source files.

## Human review still required

Hold RMB while standing and moving, then tune `center_offset`, force, damping,
and max stretch. Verify left/right and up/down camera motion, multiple weapons,
release recovery, collisions, and that movement produces physical drift rather
than artificial random inaccuracy.
