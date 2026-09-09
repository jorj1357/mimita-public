2026-09-09T17:38:25Z

# Camera sway — realistic-ish landing response

## Purpose ################################

Make hard landings produce a small, smooth, readable camera response similar to GMod. Keep camera sway configuration separate from weapon recoil, explosion shake, and other camera-impact effects.

## Desired behavior ################################

- Soft landings create little or no sway.
- Hard landings move the camera smoothly down and back toward the intended view.
- Optional sideways movement is camera roll, not unintended yaw.
- The response feels spring-like and settles without snapping.
- `config/camconfig.json` owns hot-reloadable presentation settings.
- `cameraSway.enabled = false` disables sway.
- `cameraSway.amount = 1.0` is the default; `0.01` is nearly invisible; `100.0` is excessive but bounded.
- Sway does not change gameplay aim, firing direction, body pose, or collision.
- Sway return settings do not change weapon-recoil recovery.
- Weapon recoil remains owned by weapon configuration, preferably per weapon in `config/weapons.json`.

## Current behavior ################################

The implementation now detects a landing from fixed-tick pre-collision downward speed and feeds a dedicated pitch/roll spring. The existing shared punch path remains independent for weapon and explosion effects.

Confirmed code problems:

- The former coupling is retained as the regression record; the corrected path calls `camera.decayPunch(dt)` and uses `Camera::sway` for presentation-only pitch/roll.

Evidence:

- `src/engine/engine-tick-camera.cpp:223-236`
- `src/camera.cpp:64-78`
- `src/combat/weapon-fire-effects.cpp:20-49`
- `src/combat/weapon-rocket-launcher.cpp:78-81`
- `src/pobjects/persistent-physics.cpp:373-374`

## Current status, from working best to not working at all ################################

Implemented in source as of `2026-09-09`; canonical build evidence is pending completion. Runtime visual acceptance is still pending, so the regression is not solved yet.

## Decisions ################################

- Camera sway is presentation-only.
- Camera sway does not alter authoritative aim, firing, body pose, or collision.
- A dedicated damped spring is preferred over shared linear punch decay.
- Impact strength uses pre-collision downward speed captured in the fixed 60 Hz movement step.
- `NEEDS_SPEC_DECISION`: choose per-weapon recoil-return fields in `config/weapons.json` versus a separate weapon-presentation configuration.
- `NEEDS_SPEC_DECISION`: define exact peak time, maximum pitch/roll, settle time, and whether overshoot is allowed.
- `NEEDS_SPEC_DECISION`: decide whether replay regenerates sway from landing events or records final camera presentation.

## Ownership

- Primary code owner: `src/camera.cpp`, `src/camera.h`, `src/engine/engine-tick-camera.cpp`
- Configuration owner: `src/config/camera-config.cpp`, `src/config/camera-config.h`, `config/camconfig.json`
- Runtime/event owner: fixed-tick landing state in `src/physics/movement/movement-step.cpp`
- Network owner: none for presentation-only sway
- Animation/physics owner: movement collision/landing state; camera presentation must not modify physical collision

## Related authoritative documents

- Specifications: `docs/specs/movement/movement.md`, `docs/specs/performance/performance.md`, `docs/specs/debug-logging/debug-logging.md`, `docs/specs/weapons/weapons.md`
- Architecture: `docs/architecture/json-configuration/json-configuration.md`, `docs/architecture/collision/collision.md`
- Workflow: `docs/workflows/fix-repeated-bug.md`
- Focused review skills: `docs/skills/spec-behavior-review-v1.md`, `docs/skills/documentation-checker-v1.md`
- Regression system: `docs/regressions/README.md`
- Regression index: `docs/regressions/regressions-v1.md`
- Specific regression record: `docs/regressions/2026-09-09/camera-sway-recoil-coupling-REG.md`

## Relevant files

- Config: `config/camconfig.json`, `config/weapons.json`
- Camera: `src/camera.cpp`, `src/camera.h`, `src/config/camera-config.cpp`, `src/config/camera-config.h`
- Camera orchestration: `src/engine/engine-tick-camera.cpp`
- Landing state: `src/physics/movement/movement-step.cpp`, `src/physics/movement/movement-conversion.cpp`
- Recoil/impact callers: `src/combat/weapon-fire-effects.cpp`, `src/combat/weapon-viewmodel.cpp`, `src/combat/weapon-rocket-launcher.cpp`, `src/pobjects/persistent-physics.cpp`

## Tests and evidence

- Automated tests: spring state is finite and bounded; dedicated unit coverage remains to be added for exact impulse/reload/recoil invariants.
- Runtime commands: use the canonical executable with bounded timeouts; no dedicated camera-sway command exists yet.
- Logs: centralized `[CAM SWAY]` diagnostics should report landing tick, measured impact, amount, and applied impulse without per-frame spam.
- Build evidence: current canonical build run is recorded in `build/changelog.txt` when complete.
- Human playtest: pending hard/soft landings, amount changes, disabled sway, recoil recovery, and roll verification.

## Acceptance criteria

- Disabled sway produces no landing camera offset.
- Amounts `0.01`, `1.0`, and `100.0` produce nearly invisible, default, and excessive-but-bounded responses.
- Hard landing moves the view smoothly down and back toward neutral.
- Soft landing is weaker than hard landing.
- Landing roll changes camera roll, not yaw.
- Camera-sway return settings do not change weapon recoil recovery.
- Sway does not change firing direction or body/weapon collision.
- Landing detection and impact measurement remain at fixed 60 Hz.
- Invalid JSON keeps the last valid configuration and logs the rejected field.
- Human visual acceptance is required before marking solved.

## Changelog and regression links

- Changelog: `docs/changelog/2026-09-09/20260909_182000-camsway-recoil-separation.md`
- Regression record: `docs/regressions/2026-09-09/camera-sway-recoil-coupling-REG.md`
- Do not copy full regression or changelog history into this feature record.

## Original notes ################################

> end goal: when hitting the world like landing super hard = the camera sways a little
>
> current behavior: when hitting the world landing hard it jerks and then returns to normal, not good
>
> editing `config/camconfig.json` return rate affects recoil return to normal view rate for all weapons. This should be per-weapon specific in `config/weapons.json` at the very least.
>
> camera config should not mess with weapon recoil values at all
>
> desired behavior: when hitting the world it smoothly goes down then up, like GMod hitting the world
