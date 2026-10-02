# RMB weapon sight alignment

Time: 2026-10-02T08:05:00Z
Branch: `afad20a-rebuild`

## Result

Refined the RMB right-arm mode after visual review showed that the arm was
being aimed generally forward but the weapon was not lining up with the
camera center. RMB now drives the right arm toward a target derived from the
weapon's actual grip-to-muzzle axis.

## Exact change

`RagdollModeSystem::applyRightArmPointMotor` now:

- reads the existing `Player::weaponLocalToArm`, `weaponGripLocal`, and
  `weaponMuzzleLocal` transforms;
- converts each weapon's real barrel direction into the arm frame;
- aims the current muzzle at a distant point on the camera-forward ray so a
  laterally offset right-hand weapon converges toward the camera center;
- preserves the force/torque-driven, smooth fixed-tick behavior; and
- falls back to the existing ragdoll arm-axis target when no valid weapon axis
  is available.

No weapon-specific IDs or offsets were added.

## Validation

- `git diff --check` passed for the focused source files.
- `python build_agent.py` returned `BUILD SUCCESS`, `Status: SUCCESS`, and
  return code `0`.

## Human review still required

Hold RMB with multiple weapons and verify that the weapon sight/barrel points
through the screen center while looking left/right and up/down. Also verify
that releasing RMB restores the normal weapon pose and that firing remains
unchanged. The attached screenshots are evidence that the previous attempt
needed this correction; they are not evidence of the corrected runtime.
