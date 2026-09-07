# Replay export local-player rocket recording

- Branch: `8292026stash`
- Timestamp: `2026-09-07T20:48:52Z`
- Purpose: restore local-player rocket and smoke visibility in multiplayer replay exports.
- Pre-existing worktree changes: existing edits to configuration, movement, replay/rocket implementation, and prior documentation were preserved. This session does not claim ownership of those edits.

## Changed files and exact behavior

### `src/network/multiplayer-projectiles.cpp`

Function changed: `MimitaNet::mpPredictProjectileAttack()`.

Old behavior: multiplayer local firing created a predicted `NetworkProjectile`, but no replay `projectile_spawn` event was recorded at this boundary. The later authoritative spawn handler intentionally skipped replay capture for `localOwner` because it assumed prediction had already recorded the event.

New behavior: when the predicted network weapon is `NETWORK_WEAPON_ROCKET_LAUNCHER`, the function records one replay `projectile_spawn` event with the predicted position, velocity, lifetime, weapon asset ID, and local owner ID. This gives replay the source event required to create the existing shared `RocketLauncherState::Rocket`, simulate it, render it, and emit smoke.

The authoritative spawn handler now records a local-owner fallback only when the local prediction was not adopted and no matching predicted projectile already exists. Remote/NPC recording remains unchanged. The new diagnostics identify local prediction, authoritative fallback, or remote authority and include fire serial, owner ID, position, and velocity.

### `src/game/game-cli.cpp`

The replay export self-test sample rocket now includes `sourceActorId = "player"` and asserts that source actor identity survives the replay event contract.

## Documentation and review

- Appended the confirmed regression to `docs/regressions/regressions-v1.md`; the file remains append-only.
- Reviewed `docs/skills/spec-behavior-review-v1.md`: the implementation aligns the missing local-player projectile event with the replay requirement to reproduce local projectiles/effects through shared behavior.
- Reviewed `docs/skills/logging-checker-v1.md`: diagnostics are owned by replay/network projectile handling and are emitted per shot, not per frame.

## Validation

- The repository build lock was owned by another active `build_agent.py` process during this session. No competing build was started and the process was not terminated.
- Previous executable evidence before this change: `Status: SUCCESS`; replay export self-test `28/28 passed, 0 failed`.
- Fresh build and self-test after this change: pending because the shared build had not produced a new result at session completion.
- Human multiplayer MP4 acceptance: pending. Verify local-player rocket spawn, smoke, travel, collision, one explosion, lifetime removal, and no duplicate projectile/effects.

