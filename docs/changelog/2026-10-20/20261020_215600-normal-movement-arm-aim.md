# Normal-movement arm aim response

## Goal

Implement the normal-movement arm-aim smoothing behavior specified in
`docs/specs/ragdoll-retrograd/ragdoll-retrograd.md`, while leaving ragdoll-mode
controls unchanged.

## Changes

- Added the hot-reloadable `normal_movement_arm_aim_response_multiplier` to
  `config/aimbody.json`, defaulting to `1.5` and clamped to `[1.0, 10.0]`.
  The setting is independent of the existing `smoothingFactor`.
- Generalized the normal-aim target state and post-physics hybrid stabilization
  so both arms follow camera yaw and pitch while RMB is held. The right arm
  retains the existing weapon-barrel-to-camera-ray alignment used by physical
  weapon aim; the left arm receives the same camera-forward target.
- Applied the response multiplier to hybrid stabilization and physical-mode
  arm angular-speed limits. The camera look path and ragdoll-mode input path
  were not modified.
- Extended the existing rate-limited `AIMBODY_LIVE_SAMPLE` with response,
  active-state, per-arm target, and target-error fields.

## Evidence

- Canonical build: `python devscripts/dev-loop.py --request build --wait-for-build`
  published build 2682. The successful build compiled one unit and skipped 558.
- The published executable at `.dev/builds/2682/mimita.exe` contains the new
  configuration and diagnostic strings. Its `--versioninfo` output identified
  `logs/10-20-2026/20261020_175315/events.jsonl` for that short metadata run.
- The dev-loop later reported a live client and server using build 2682. Its
  `AIMBODY_LIVE_SAMPLE` records show hybrid mode and response multiplier 1.5.
  The sampled records have RMB released, so they do not establish active
  two-arm tracking, camera immediacy, crosshair lag, or convergence after a
  quick mouse turn.
- No tests were added or run. No live gameplay input was issued, and the
  running client/server were left untouched.

## Follow-up acceptance

In the live game, hold RMB and make quick left/right and up/down camera turns.
Confirm both arms track the full camera-forward direction, the camera remains
immediate, and the physical crosshair follows the arms until they settle. Try
values 1.0 and 1.5, then confirm ragdoll-mode controls remain unchanged.
