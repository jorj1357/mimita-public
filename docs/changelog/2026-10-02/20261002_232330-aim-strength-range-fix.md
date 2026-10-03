# Aim strength no-range-limit fix

Time: 2026-10-03T03:23:30Z

## Requested result

`aim_strength` in `config/aimbody.json` now affects how quickly the physical right arm corrects toward the camera target. It no longer scales the arm-centering positional spring, so large values cannot shove the arm into a competing shoulder/joint response. The camera-derived target and its full vertical range remain unchanged.

## Source change

In `src/ragdoll/ragdoll-mode.cpp`, the right-arm motor now:

- keeps the centering spring independent of `aim_strength`;
- keeps the look-mode angular target and speed cap independent of `aim_strength`;
- uses `aim_strength` only as the look-controller response rate;
- retains stronger PD correction for the explicit physical damping mode without changing its target or configured range.

## Evidence

- Source inspection identified the prior coupling in `applyRightArmPointMotor`.
- Incremental build completed with `SUCCESS` / `Nothing changed` after the affected object was current.
- `git diff --check` remains clean apart from existing line-ending warnings.
- No executable launch or human gameplay acceptance was performed in this turn.
