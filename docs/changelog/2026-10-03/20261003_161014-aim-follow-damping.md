# Aim follow damping controls

Time: 2026-10-03T16:10:14Z

## Requested result

The RMB right-arm aim now exposes separate controls for how calmly the physical arm catches up to a camera-direction change. These controls affect response speed only; they do not restrict the target's upward or downward aiming range.

## Configuration

`config/aimbody.json` now supports:

```json
"follow_damping": 1.0,
"max_follow_speed": 30.0
```

`follow_damping` is a response-time multiplier: increasing it makes the arm less twitchy and more gradual. `max_follow_speed` is the arm correction's angular-speed ceiling in radians per second. Both values hot-reload with the existing aim-body config.

## Evidence

- `AimBodyConfig` parses, stores, and saves both settings.
- `RagdollModeSystem::applyRightArmPointMotor` uses them only for response rate and angular speed, leaving the camera target and pitch range unchanged.
- The current JSON value `aim_strength: 999.0` is accepted by the file parser but clamped by code to its existing safe maximum of `100.0`.
- Aim-body, ragdoll, simulation, and dependent camera/config sources compiled successfully; build result was `SUCCESS` with 8 objects compiled and 509 skipped.
- No runtime gameplay or human acceptance was performed in this turn.
