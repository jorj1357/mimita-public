# Bridge hot rocket policy into the authoritative server path

- EST timestamp: 2026-09-12 11:51:08 EDT (UTC 2026-09-12T15:51:08Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS_WITH_HUMAN_REVIEW`

## Problem

Editing `src/hot-reload/modules/rocket-behavior.cpp` changed only the local
predicted rocket path. The authoritative server built rockets directly from
`weapons.json` in `src/network/server-projectiles.cpp` and
`src/network/server-npcs.cpp`, so NPC/player authoritative damage stayed near
the JSON value while local prediction used the hot value.

## Fix (source)

- `src/network/server-projectiles.cpp`: after `projectileConfigFromDefinition`
  resolves the JSON base, the authoritative projectile now calls
  `LiveGameplay::rocketFlight` (speed, lifetime, gravity, drag, up bias) and
  `LiveGameplay::explosion` (splash radius, splash exponent, damage, knockback,
  self-damage multiplier). The returned values are stored on the authoritative
  `ServerProjectile` and used by `explodeProjectile` for damage.
- `src/network/server-npcs.cpp`: the same two policy calls for NPC-fired
  authoritative rockets.
- `src/network/multiplayer-projectiles.cpp`: client prediction uses the same
  policy for speed and predicted blast damage so prediction matches authority.
- `src/live-code/live-gameplay.h/.cpp`: added `journalPolicy`, recording side,
  active generation and code hash, base/out speed, base/out damage, projectile
  id, and target entity id.
- `src/live-code/live-code-selftest.cpp`: replaced exact-value gameplay
  assertions (`speedScale == 1.0`, `baseDamage == 100`) with invariant checks
  (call succeeds, finite, non-negative). Exact tuned values must never be pinned.
- `src/hot-reload/hot-modules.json`: added the bridge files to the `cold` watch
  list.

## Layering preserved

```text
weapons.json          base configuration (unchanged)
rocket-behavior.cpp   live C++ policy override
combined result       authoritative server projectile simulation
```

The server computes the policy from server-resolved values; the client never
chooses authoritative damage, and packet validation, ownership, replay, and
authority rules are untouched.

## Cold-build requirement

Installing the bridge requires one COLD BUILD because it changes EXE-owned call
sites. At fix time **two `mimita.exe` instances were running**, so
`python build_agent.py` refused with `HOT_RELOAD_BOUNDARY_VIOLATION` (exit 3).
The invariant forbids killing, closing, or relinking the running game, so the
cold build was not forced. After that cold build, all further
`rocket-behavior.cpp` edits activate through the live DLL path only.

## Evidence

### Source / build evidence
- `python build_game_dll.py` -> success, 4 hot sources; DLL publishes the
  `gameplay` module (verified by a direct `GetGameAPI` probe: 4 modules,
  `gameplay` structSize 24).
- Bridge translation units syntax-compile cleanly with the project toolchain
  (`-fsyntax-only`, project flags): `server-projectiles.cpp`, `server-npcs.cpp`,
  `multiplayer-projectiles.cpp`, `live-gameplay.cpp`.
- Full EXE relink: NOT performed (blocked by the invariant; see above).

### DLL load / activation evidence
- Live journal `logs/features/live-code/2026-09-12/live_events_20260912_152827.jsonl`:
  `compile_started` -> `compile_finished` -> `code_activation` for generation 2
  (`5eeaa6e8...`) and generation 3 (`5f85fcdc...`) when `rocket-behavior.cpp`
  changed, with `notification_emitted` "new version active".
- `python devscripts/live-build.py` emitted
  `build/hotreload/mimita-live-g000002.dll` without writing `mimita.exe`.
- `python devscripts/test-live-build-invariant.py` -> PASS (exe unchanged).
- Direct DLL probe: `adjustRocketFlight` returns `speedScale = 1.300` and
  `explosionParameters` returns `baseDamage * 12.50` (the current proof edits).

### Authoritative server runtime evidence
- NOT YET OBTAINED. The running executables are the pre-bridge cold build, so
  their authoritative path does not call the hot module yet. Requires the cold
  build above, then an in-game rocket test.

### Visual damage-number evidence
- NOT YET OBTAINED (same dependency).

### Human in-game acceptance
- NOT YET OBTAINED.

## Required human proof after the cold build

1. Cold build once with the game not running: `python build_agent.py`.
2. Start `mimita.exe`, enter the player-vs-NPC rocket 1v1; record PID, session,
   player/NPC entity ids, active generation.
3. Fire a rocket; record authoritative NPC/player health change.
4. Edit `rocket-behavior.cpp` (`out->baseDamage = base->baseDamage * 12.50f;`
   and/or `out->speedScale`), save; wait for activation.
5. Fire again; confirm authoritative health reflects the hot multiplier and
   speed changes; confirm PID/session/entity ids/world unchanged.
6. Break the compile; confirm the old generation stays active.
7. Revert; confirm a new generation activates.
8. Edit a `cold` file (e.g. `weapon-rocket-launcher.cpp`); confirm
   `HOT_RELOAD_BOUNDARY_VIOLATION`, no relink, no gameplay interruption.
9. Inspect the journal for `gameplay_policy` events with side=server,
   generation/hash, base/out damage, base/out speed, and projectile id.

## Focused skills

- `docs/skills/spec-behavior-review-v1.md`: PASS. JSON remains the base; the hot
  policy modifies resolved values; server authority is preserved.
- `docs/skills/logging-checker-v1.md`: PASS. `gameplay_policy` is event-driven
  (once per projectile spawn), includes generation/hash and comparable numbers.

## Files

Changed: `src/network/server-projectiles.cpp`, `src/network/server-npcs.cpp`,
`src/network/multiplayer-projectiles.cpp`, `src/live-code/live-gameplay.h`,
`src/live-code/live-gameplay.cpp`, `src/live-code/live-code-selftest.cpp`,
`src/hot-reload/hot-modules.json`, `docs/regressions/regressions-v1.md`.

## Pre-existing edits preserved

Unrelated working-tree changes, including the developer's proof edits in
`src/hot-reload/modules/rocket-behavior.cpp`, were not reverted or claimed.
