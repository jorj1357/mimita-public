# Dev launch mode 3 crate testing

Time created: 2026-09-30T15:37:00Z
Time last updated: 2026-09-30T15:37:00Z

## Request

Add launch mode 3 to `devscripts/dev-launch-modes.json` for a solo
`funworld3` crate test, with no NPCs and automatic `crate_spawn` after the
client has loaded into the room.

## Changes

- `devscripts/dev-launch-modes.json`: added mode `3`, named
  `crate_testing_funworld3`, with map `funworld3`, server argument
  `--no-npcs`, and client arguments `--startup-command crate_spawn`.
- `devscripts/dev-loop.py`: launch-mode map overrides now take precedence over
  the profile's random map selection while still checking the allowed map pool.
- `src/network/net_mode.h`: added repeatable client startup-command storage.
- `src/network/net_mode.cpp`: parses and documents `--startup-command <cmd>`.
- `src/main.cpp`: executes each startup command once after the room is
  connected and the gameplay world is loaded. This keeps client-local commands
  such as `crate_spawn` on the client, where the player and camera exist.

## Reasoning

`crate_spawn` is a client terminal command, not a headless-server command. A
server argument would run in the wrong process and could not use the local
player's camera direction. The startup command is therefore delayed by state,
not by an arbitrary sleep: it runs at the first frame where both multiplayer
connection and world-loaded state are true.

## Documents and focused review

- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/specs/networking/networking.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
- `docs/regressions/README.md`
- Memory registry entry for `dev-launch-modes.json` and `dev-loop.py`.

## Validation

- `python -m json.tool devscripts/dev-launch-modes.json`: passed.
- Launch-mode 3 validation loaded the JSON and asserted the map, server args,
  client args, and allowed-map selection: passed.
- `python -m py_compile devscripts/dev-loop.py`: passed.
- `python devscripts/dev-loop.py --help`: passed.
- `python build.py build-only`: returned success.
- The resulting `mimita.exe` contains `--startup-command` and `crate_spawn`.
- `git diff --check` reports only pre-existing trailing whitespace in
  `docs/specs/20260930plan.md`; no new whitespace errors were introduced.

## Human review still needed

Launch mode 3 has not been visually accepted in a live game during this
session. Human review should confirm `funworld3`, no NPCs, one player, and one
crate spawned after loading.
