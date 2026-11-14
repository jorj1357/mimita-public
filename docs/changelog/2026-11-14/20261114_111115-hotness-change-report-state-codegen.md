# Hotness instrumentation, live change report, state codegen foundation

Date: 2026-11-14

## Source changes

- `src/network/server.cpp`: added `HotnessState` and 1 Hz
  `game.transport.handled` / `game.hotness.summary` events plus
  `game.hotness.violation` when a migrated packet reaches the resident path
  while the probe is active. Both dedicated and listen receive loops now report
  `path=dll|resident`, packet type, player, and active generation.
- `src/game/server-command-dispatch-module.cpp`: migrated handlers now
  reject-and-log when they fail under strict mode (`MIMITA_HOTNESS_STRICT`,
  default on) instead of silently returning to the resident handler.
- `src/host/host-runtime.h` / `.cpp`: exposed `hotnessStrictMode()`; reused the
  existing `activeGeneration()`.

## Tooling changes

- `devscripts/live.py`: writes `snapshots/<revision>/change-report.json` and
  passes it to the worker with `--change-report`; `ready.json` and `--status`
  carry the diff summary.
- `devscripts/game_build_worker.py`: accepts `--change-report`, rejects a
  revision mismatch, and attaches the report to `current.json` and
  `generation.json`.
- `config/hot-reload/state-schema.json`: eight initial state domains.
- `devscripts/resident_state_codegen.py`: generates
  `src/host/generated/resident-state-v1.h` and
  `config/hot-reload/state-ledger.json`; `--check` detects drift and
  unchanged files are not rewritten, so the watcher cannot loop.
- `devscripts/live.py` runs the codegen before each cycle (or `--check` in
  dry-run) and watches `config/hot-reload/state-schema.json`.

## Evidence

- Dev-loop build generation 511 published successfully
  (`2026-11-14T15:57:20Z`) after the hotness changes.
- `python -m py_compile` passed for the three changed Python files.
- `python devscripts/resident_state_codegen.py` wrote 8 domains / 47 fields;
  `--check` passed immediately afterwards.
- `python devscripts/live.py --once --dry-run` wrote an initial snapshot and
  change report without requesting a build.

## Not proven

- No live join/chat runtime proof of the new events yet.
- Server simulation and unmigrated inbound packets are still resident.
- Generated domain structs are not yet authoritative; no adapter uses them.
