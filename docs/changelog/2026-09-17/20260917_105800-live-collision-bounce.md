# Live collision bounce policy

- EST timestamp: 2026-09-17 10:58:00 -04:00
- Branch: `8292026stash`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Goal

Move player/world bounce tuning from the inactive JSON loader into the existing
hot collision-policy event so bounce strength and related values can change in
the live DLL without restarting the executable or recreating world state.

## Changes

- `src/hot-reload/hot-movement-policy.h`
  - Extended `CollisionPolicyV1` with `bounceEnabled`, `bounceStrength`,
    `bounceFriction`, `bounceMinSpeed`, `bounceMaxSpeed`, and `bounceCooldown`.
- `src/hot-reload/modules/collision-policy.cpp`
  - Added compiled hot defaults: enabled, strength `0.35`, friction `0.0`,
    minimum speed `0.0`, maximum speed `999999.0`, cooldown `0.001`.
  - These constants are the live edit points for future DLL generations.
- `src/config/collision-config.h/.cpp`
  - Replaced JSON/file polling with `currentCollisionBouncePolicy(tick)`.
  - Dispatches the existing hot collision-policy event once per simulation tick.
  - Clamps returned values and uses a safe disabled fallback if no hot handler
    is available.
- `src/physics/movement/physics-collision-shared.h`
  - Preserved the existing response formula and switched it to the hot policy
    snapshot for both player velocity and external impulse.
- `src/engine/engine-tick-setup.cpp`
  - Removed the obsolete `CollisionConfig` JSON hot-poll call and include.

## Evidence

- Hot build: `python devscripts/live-build.py` succeeded and produced
  `build/hotreload/mimita-live-g000022.dll`; the path does not write
  `MiMITA.exe`.
- Cold build: `python build_agent.py` succeeded and produced
  `C:\mimita-priv-v8\mimita-20260917T105835.exe`.
- The running `mimita-20260917T103243.exe` processes were not killed,
  restarted, or replaced.
- Client collision adapter tests passed: 6 passed, 0 failed.
- `git diff --check` passed.
- No remaining `CollisionConfig` bounce API references remain in source; the
  remaining `WeaponCollisionJsonConfig` references are the separate weapon
  collision system.
- Follow-up live fix: changed `kBounceMinSpeed` from the gentle-contact gate to
  `0.0f`, so any body-part contact with incoming velocity can bounce.
- Live journal proof: client PID `2880` detected the source change, compiled and
  validated the candidate, then activated generation `8` at
  `2026-09-17T15:05:47.339Z` without restarting the process.

## Documents and focused reviews

- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/specs/movement/movement.md`
- `docs/architecture/collision/collision.md`
- `docs/architecture/json-configuration/json-configuration.md`
- `docs/architecture/live-development/live-development.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/efficiency-checker-v1.md`

## Pre-existing unrelated work preserved

- `config/accounts/default.json`
- `config/analytics.json`
- `src/hot-reload/hot-animation-clips.h`
- `src/hot-reload/modules/presentation/animation-policy.cpp`
- `src/render/presentation-entities.cpp`
- Existing animation changelog files under `docs/changelog/2026-09-17/`

## Human review still required

Start the newly built executable or use the normal live-generation activation
path in a controlled session, change `kBounceStrength` in the hot collision
module, and verify a later collision visibly uses the new value while the same
world, players, EntityIds, and session remain alive. This visual/live behavior
was not claimed from source or build evidence alone.
