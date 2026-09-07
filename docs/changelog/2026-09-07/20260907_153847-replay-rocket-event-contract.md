// 2026-09-07T15:38:47-04:00 (2026-09-07T19:38:47Z)

# Replay rocket event contract

## Scope and result

Implemented the smallest safe portion of the approved replay rocket plan. Rocket-launcher replay events now carry their weapon identity, and replay no longer renders a rocket gunshot as a hitscan tracer. Full collision-aware replay projectile parity is deliberately not claimed.

## Repository state

- Branch: `8292026stash`
- HEAD before this session: `f94b563`
- Pre-existing worktree changes: unrelated edits in `config/playervisuals.json`, `src/config/player-visuals-config.cpp`, `src/config/player-visuals-config.h`, and `website/server/db.js`; these were preserved and not claimed.
- This session changed only the two source files and two documentation files listed below.

## Exact source changes

### `src/combat/weapon-rocket-launcher.cpp`

Old event construction omitted `assetId` for both `projectile_spawn` and `gunshot`.

New content:

```cpp
projEvent.assetId = def.id;
gunshotEvent.assetId = def.id;
```

This preserves the authoritative weapon definition ID (`rocket_launcher`) in the recorded event.

### `src/engine/engine-tick-camera.cpp`

Old behavior unconditionally called `spawnTracer(...)` for every replay `gunshot` event.

New behavior resolves `effect.assetId` through `WeaponRegistry`, checks `WeaponDefinition::hitscan`, spawns a tracer only for hitscan weapons, and emits a replay diagnostic when a projectile weapon suppresses the tracer.

## Why

The replay spec and effects spec require replay to reproduce recorded weapon behavior and distinguish hitscan tracers from rocket projectile presentation. The old generic dispatch had no weapon identity and therefore could not make that distinction.

## Validation

- `git diff --check`: passed (line-ending warnings only).
- `python build_agent.py`: `Status: SUCCESS`; canonical `C:\mimita-priv-v8\mimita.exe` relinked; five objects compiled because pre-existing edits were also pending.
- `mimita.exe --replay-export-selftest --timeout 60 --no-coordinator`: `26/26 passed, 0 failed`.

## Focused review

- `docs/skills/spec-behavior-review-v1.md`: PASS for the implemented event-contract/tracer behavior; remaining shared projectile parity is an explicit unresolved finding.
- Relevant specifications: `docs/specs/replays/replay-editor-and-export-v2.md` and `docs/specs/effects/effects.md`.

## Remaining human review and follow-up

Human live export must verify that the rocket gunshot no longer shows a tracer. The current replay `projectile_spawn` branch still creates `EffectPart` with `replayType = "replay_rocket"`; it does not call the live `WeaponRocketLauncher::update` path, so rocket travel speed, world collision, explosion, smoke, and exact timing remain unresolved. The first/second/third export camera-startup recurrence and left-leg transform issue also remain open.
