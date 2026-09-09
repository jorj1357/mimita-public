# Smooth aimbody and camera sway

## Session

- Branch: not changed; existing checkout branch preserved.
- Commit: not created; existing working tree preserved.
- Timestamp: 2026-09-08T16:06:52Z.
- Pre-existing changes: the checkout was already dirty before this work, including account/config changes, gamemode GUI changes, networking/server changes, documentation changes, and an existing changelog `20260908_101349-gamemode-gui-hot-reload.md`. Those changes were not claimed, reverted, or rewritten.
- Regression file: no append was made. This work implements a newly requested behavior and no prior working-state regression was confirmed during this session.

## Requested behavior

Smooth aimbody mode treats the camera direction as a desired direction. The body and configured aim limbs should approach it continuously in yaw and pitch, with world Z as vertical and horizontal body rotation around world Z. Default mode must retain immediate behavior. Landing camera sway should be configurable through `config/camconfig.json`, presentation-only, and hot-reloadable.

## Files changed

- `config/aimbody.json`
  - Old content had `comment`, `enabled`, and `limbs` only.
  - New content retains the existing limb gains and adds `mode: "default"` and `smoothingFactor: 1.0`.
  - The comment now documents default versus smooth behavior, world-Z orientation, factor semantics, no snapping, and the separate camera-sway owner.
- `src/entities/aimbody-config.h`
  - Added mode/factor accessors and shared scalar/shortest-angle smoothing helpers.
- `src/entities/aimbody-config.cpp`
  - Added parsing, validation, save output, and exponential time-based smoothing.
  - Invalid or non-positive smoothing factors fall back to `1.0`; unknown modes fall back to `default`.
- `src/entities/player.h`
  - Added per-player `aimBodyPitch` state and landing airborne-duration state.
- `src/entities/player-animation.cpp`
  - Aimbody look pitch now uses the smoothed per-player value in smooth mode before applying limb rotations.
- `src/physics/physics-mini.cpp`
  - Local visual body yaw now uses the aimbody shortest-angle smoother instead of assigning camera yaw directly.
- `src/network/server-players.cpp`
  - Dedicated-server player yaw now uses the same smooth shortest-angle aimbody rule in smooth mode before movement-state/snapshot processing; default mode remains immediate.
- `src/physics/movement/movement-types.h`
  - Added fixed-tick landing airborne-duration state.
- `src/physics/movement/movement-step.cpp`
  - Captures airborne duration when a landing event is produced.
- `src/physics/movement/movement-conversion.cpp`
  - Carries the landing duration between `Player` and shared movement state.
- `src/config/camera-config.h`
  - Added camera-sway enable, amount, landing threshold, pitch, roll, and return-rate settings.
- `src/config/camera-config.cpp`
  - Added hot-reload parsing, range validation, and last-valid replacement behavior for camera sway.
- `config/camconfig.json`
  - Added the active `cameraSway` configuration with default amount `1.0`.
- `src/camera.h` and `src/camera.cpp`
  - Made punch decay rate configurable while preserving the old default of `12.0`.
- `src/engine/engine-tick-camera.cpp`
  - Applies one presentation-only landing punch per movement tick when the captured airborne duration meets the configured threshold.
  - Uses configured return rate and rate-limited centralized logging.

## Behavior and implementation notes

- Smooth aim uses exponential convergence with a response baseline of `0.25 * smoothingFactor` seconds and a minimum response interval of `0.0025` seconds.
- Yaw uses the shortest signed angular difference, so wraparound does not rotate the long way.
- Camera input remains immediate. Smooth aim affects the player body/animation pose, not raw camera input.
- Camera sway reuses the existing camera punch/decay mechanism and does not alter gameplay aim or collision input.
- Landing sway currently scales from captured airborne duration because the current landing event does not retain pre-collision vertical impact speed. Exact impact-velocity scaling remains a follow-up refinement.
- The existing physical collision path consumes animated body-part world transforms; the local smoothed pose therefore reaches the local body/weapon collision transform update path. Full server-side replicated limb-pose authority remains a separate networking scope and is not claimed as completed here.

## Validation

- `config/aimbody.json`: parsed successfully with PowerShell JSON validation.
- `config/camconfig.json`: parsed successfully with PowerShell JSON validation.
- `git diff --check`: run. It reported trailing whitespace in pre-existing modified documentation files outside the focused implementation; no focused source/config whitespace failure was introduced.
- Canonical build: `python build_agent.py` completed after the shared PID `5448` build released its lock. `build/changelog.txt` reports `Status: SUCCESS` at `2026-09-08 12:06:52`; the canonical `mimita.exe` was relinked and the server-player object was compiled.
- Runtime/human acceptance: not completed. Required checks include rapid 180-degree yaw, rapid up/down pitch, arm/weapon near walls, smooth-mode hot reload, default-mode compatibility, hard/soft landing sway, disabled sway, and extreme amounts.

## Focused review

- `docs/skills/spec-behavior-review-v1.md`: applied. The implementation follows the approved default/smooth and presentation-only sway decisions. Remaining acceptance warning: the exact pre-impact velocity signal and full remote/server limb-pose replication still need runtime/network review.
- `docs/skills/logging-checker-v1.md`: applied by using the existing centralized, rate-limited `Debug::logThrottled` camera-sway diagnostic. No loose debug file was added.
- `docs/skills/efficiency-checker-v1.md`: applied by using scalar state, no per-frame allocations, and the existing camera punch path. Collision broadphase code was not changed.

## Remaining human review

1. Run a fresh canonical build after the shared build lock is released and confirm `Status: SUCCESS`.
2. Playtest smooth mode with fast left/right and up/down mouse turns around arm and weapon colliders.
3. Confirm default mode remains visually and physically immediate.
4. Confirm camera sway is visible on hard landings, nearly absent at `amount: 0.01`, excessive at `amount: 100.0`, and absent when disabled.
5. Confirm camera sway does not change firing direction or body/weapon collision.
6. Decide whether landing sway should eventually use exact pre-collision vertical speed instead of airborne-duration intensity.
7. Separately extend the incomplete ragdoll-retrograd specification before ragdoll implementation.
