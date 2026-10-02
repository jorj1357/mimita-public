# RMB aim transition and rotation tuning

Time: 2026-10-02T13:19:48Z
Branch: `afad20a-rebuild`

## Result

RMB right-arm pointing now transitions smoothly in both directions. Pressing
RMB blends the arm into the centered physical target; releasing RMB blends the
center force, shoulder stretch, and normal hybrid pose back over time.

Added hot `config/aimbody.json` controls:

```json
"right_arm_pointing": {
  "mode": "rmb",
  "center_offset": [0.2, 0.0, 0.4],
  "rotation_degrees": [0.0, 0.0, 0.0],
  "blend_rate": 10.0
}
```

`rotation_degrees` is an arm-local goal correction in pitch/yaw/roll. It is
intended to correct avatar bind orientations where the camera-aligned arm goal
appears to point down or up.

## Implementation

- Added a persistent `rightArmPointingBlend` to the normal-play ragdoll body.
- Applied the blend before fixed-tick integration so entering and leaving aim
  remain physical and momentum-sensitive.
- Scaled hybrid pose return, shoulder stretch, arm-range exemption, center
  force, and aim torque from the same transition value.
- Preserved the existing weapon barrel alignment and added the JSON goal
  rotation after that alignment.

## Validation

- Focused `git diff --check` passed.
- `python build_agent.py` returned `Status: SUCCESS` and return code `0`.
- Generated changed translation-unit objects are newer than their sources.

## Human review still required

Tune `rotation_degrees` live until the current avatar aims forward. Verify RMB
press/release, camera pitch, movement momentum, collisions, and multiple
weapons. No visual acceptance is claimed from the build alone.
