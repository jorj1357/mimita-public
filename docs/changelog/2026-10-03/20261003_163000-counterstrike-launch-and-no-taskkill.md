# Counter Strike launch mode and non-destructive build agent

Time (UTC): `2026-10-03T16:30:00Z`

## Result

Development launch mode `5` now starts a fresh Counter Strike server on
`dust2cyberiav3`, with no NPCs, and the client joins the room created by that
server. The server selects the `counterstrike` community mode and its matching
gamemode, so the round lifecycle starts in the configured intermission phase.

The development build agent no longer runs `taskkill /f /im mimita.exe`.
Existing MiMITA processes are left alone.

## Changes

- `devscripts/dev-launch-modes.json`
  - Changed mode `5` from the crate-testing placeholder to `counter_strike`.
  - Added `--mode counterstrike` and `--gamemode counterstrike`.
  - Kept `--no-npcs`.
  - Removed the `crate_spawn` client startup command.
  - Set the map to `dust2cyberiav3`.
- `config/gamemode-good-maps.json`
  - Added `dust2cyberiav3` so the dev-loop does not replace the Counter Strike
    map with the first general-purpose development map.
- `build_agent.py`
  - Removed the global forced process termination before building.

## Why both mode flags are present

The dev-loop already supplies the server's base `--mode` argument. The launch
mode arguments are appended afterward, so mode `5` explicitly selects the
Counter Strike community mode. `--gamemode counterstrike` keeps the gameplay
gamemode identity explicit for launch paths that inspect that option.

Counter Strike's server startup resolves its configured gamemode and sets the
initial phase to intermission unless a skip-intermission path is requested.

## Validation

- Parsed `devscripts/dev-launch-modes.json` successfully.
- Confirmed mode `5` contains the Counter Strike flags, no NPCs, no crate
  command, and `dust2cyberiav3`.
- Parsed `config/gamemode-good-maps.json` and confirmed the required map is in
  the allowed pool.
- `python -m py_compile build_agent.py devscripts/dev-loop.py` passed.
- Confirmed `build_agent.py` contains no `taskkill` call.
- `git diff --check` passed for the changed files.

## Not yet proven

No C++ build or live server/client run was performed in this documentation-only
validation pass. The next runtime check is to start the dev-loop with mode `5`
and confirm the client joins `dust2cyberiav3` while the server reports the
Counter Strike intermission state.

Removing `taskkill` means a build will no longer close a running executable. If
the root `C:\mimita-v9\mimita.exe` itself is open, Windows may still prevent the
linker from replacing that exact file; that is a normal file-lock failure, not
a process-kill path. The dev-loop's numbered `.dev\\builds\\####` copies remain
separate launch artifacts.
