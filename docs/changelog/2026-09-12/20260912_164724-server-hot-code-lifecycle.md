# Dedicated server hot-code lifecycle: authoritative explosions use the hot policy

- EST timestamp: 2026-09-12 12:47:24 EDT (UTC 2026-09-12T16:47:24Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS_WITH_HUMAN_REVIEW` (fix staged; running-server proof requires one bootstrap cold build)

## Problem

The live dedicated server log showed JSON-only rocket damage
(`[PROJECTILE CONFIG] ... splashDamage=1520.00 source=weapon-definition`,
`[EXPLOSION NPC DAMAGE] ... damage=714`) with no `hot_damage_policy_result`,
even though `--hot-authoritative-selftest` passed and the client hot module was
active.

## Root cause

`src/main.cpp` handles `--server` inside `handleGameCLI` and returns before
`gameInit`. `HotReloadSystem::startup()` and `LiveEventJournal::init()` live in
`gameInit`, so the dedicated server process never started the hot loader, never
called `pollAndAdvance`, and never opened the live journal. Therefore
`LiveModules::findFunctions` found no active generation, `serverResolveDamagePolicy`
always took the fallback, and the journal write was a no-op — exactly matching
the missing evidence.

## Fix (source)

`src/network/server.cpp::runServer`:
- after `StructuredLogger::init()`: `LiveEventJournal::instance().init()` and
  `HotReloadSystem::instance().startup()`, then log
  `[SERVER LIVE CODE] loaded= generation= code_hash=`;
- at the top of each fixed simulation step:
  `HotReloadSystem::instance().pollAndAdvance()` (safe activation boundary);
- on exit: `HotReloadSystem::instance().unloadGameDLL()` and
  `LiveEventJournal::instance().shutdown()`.

`src/hot-reload/hot-reload-system.cpp`: generation output filenames now include
the process id (`mimita-live-p<pid>-g<gen>.dll`) so a server and a client
compiling at the same time cannot collide. The hot-module root now prefers the
executable directory when it contains `src/hot-reload/hot-modules.json` (falling
back to the current directory) so a server process launched with a different
working directory still finds the manifest and build output.

Docs: `docs/architecture/live-development/hot-kernel.md` gained a "Server
lifecycle" section. Regression entry appended to
`docs/regressions/regressions-v1.md`.

## Authoritative data flow after the fix

```text
dedicated server startup -> journal + hot loader + generation log
each fixed step -> pollAndAdvance (activate ready generation)
weapons.json -> base projectile values
explosion -> GAME_EVENT_DAMAGE_POLICY -> rocket-behavior.cpp::onEvent
-> DamagePolicyV1::outDamage -> server authoritative health apply
```

## Evidence

- `src/network/server.cpp` and `src/hot-reload/hot-reload-system.cpp` compile
  (`-fsyntax-only`, project flags).
- `python build_game_dll.py` -> success (4 hot sources).
- `mimita.exe --live-code-selftest` -> PASS.
- `mimita.exe --hot-authoritative-selftest` -> PASS.
- `python build_agent.py` -> refused with `HOT_RELOAD_BOUNDARY_VIOLATION`
  (exit 3) because the client (PID 19960) and dedicated server (PID 39616) were
  running. Not forced; the invariant forbids killing them.

## Not yet proven (requires the bootstrap cold build)

The running-server proof from the task is pending: start the cold-built
server/client, enter the 1v1, set `weapons.json` base damage to `1520`, set the
hot module to `outDamage = 999999` for explosions, save only
`rocket-behavior.cpp`, confirm live generation activation, then confirm the
server log contains `base_damage=1520 out_damage=999999 result=hot` and that
authoritative health reflects it; then change the value, break the file, and
restore, confirming generation retention and rollback behavior.

## One required step

Close the two running `mimita.exe` processes (client PID 19960 and dedicated
server PID 39616), then run `python build_agent.py` once to install the server
bridge. After that, only `rocket-behavior.cpp` edits are needed for live damage
proofs, and `python devscripts/live-build.py` emits generations without touching
`mimita.exe`.

## Files

Changed: `src/network/server.cpp`, `src/hot-reload/hot-reload-system.cpp`,
`docs/architecture/live-development/hot-kernel.md`,
`docs/regressions/regressions-v1.md`.

## Pre-existing edits preserved

Unrelated working-tree changes were not reverted or claimed.
