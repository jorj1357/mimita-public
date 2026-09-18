# One live `events.jsonl` debug stream

- EST timestamp: 2026-09-17 14:00:00 -04:00
- Branch: `8292026stash`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Goal

Replace the fragmented per-category `.txt` files and the `LogManager` run log with
one append-only JSONL file per process run,
`logs/yyyy-mm-dd/hhmmss/events.jsonl`, readable live by VSCode, `rg`, and
`Get-Content -Wait` while the game runs. Errors, state transitions, and important
events are retained; repeated events are aggregated; categories stay hot
configurable.

## Changes

### A/B — single authoritative stream (`src/debug/structured-log.h/.cpp`)
- Added the generic, subsystem-neutral API: `debug::Level`, `debug::Event`,
  `debug::logEvent`, `debug::flushEvents`, `debug::eventsEnabled`,
  `debug::eventsPath`, and the provenance macro `MIMITA_EVENT`.
- The logger now owns exactly one file: `FILE* mEventsFile`, `mEventsPath`,
  `mSequence`. `init()` creates `logs/<utcDateFolder()>/<utcCompactStamp()>/`,
  opens `events.jsonl` append with `_fsopen(..., _SH_DENYNO)` so VSCode/`rg`/
  `Get-Content -Wait` can read while the game writes, emits `logger.started`,
  and flushes. `shutdown()` flushes pending buckets, emits `logger.stopped`,
  flushes, and closes.
- Removed all per-category `mCategoryFiles`, `openCategoryFile`,
  `writeStartupMetadata`, `writeSummary`, and category directory naming.
- One compact JSON object per line with universal fields (`wall_time` UTC
  ISO-8601 `Z`, `t`, `seq`, `run_id`, `pid`, `process`) plus event fields
  (`level`, `category`, `event`, and optional message/reason/correlation_id/
  frame/tick/server_tick/client_tick/duration_us/source/line/func/result and
  caller fields). Every key is written exactly once; caller fields that collide
  with universal keys are skipped.
- Bounded duplicate aggregation: `RepeatBucket`; ERROR/FATAL bypass; caller
  `aggregationKey` wins, else a key from category + event + stable numeric/string
  identity fields (never timestamps/seq/positions); flush on key change, on
  `repeat_window_seconds`, and on shutdown; summary emits `count`, `first_t`,
  `last_t`, `first_tick`, `last_tick`. Live buckets capped at 512 with
  oldest-first flush.
- `pollConfig()` keeps hot-reload, parses both the new flat keys and the legacy
  nested `categories.<name>.{level,file_output,throttle}`, and a malformed config
  emits an ERROR event instead of stopping logging.
- The legacy `Entry`/`write`/`writeVFormatted`/`assertNear`/`MIMITA_LOG`/`DBG`
  surface is preserved and now emits one JSONL record per message, mapping
  structured levels onto debug levels.

### C — `config/debuglogger.json`
- Added `events_file`, `console_mirror`, `flush_each_event`,
  `repeat_window_seconds`, `default_level: important`, and uppercase-equivalent
  per-category levels (keys stay lowercase as before). Preserved the
  `replay_validation` and `performance` blocks other systems read.

### D — `src/debug/log-manager.*`
- Removed the `.txt` run log, header/footer, summary copy, and rotation. Kept the
  stdout pipe-capture thread; each captured line becomes one `LEGACY` /
  `legacy.stdout` record. `writeConsole` still mirrors to the real console.
  `path()` and `managedFilePath()` now return the events.jsonl path.

### E — `src/devtools/terminal.*`
- `addLog` keeps the scrollback and also emits one `TERMINAL` / `terminal.log`
  record per visual line.

### F — hot path (cold ABI change)
- `game-api.h`: added `GAME_CAP_LOG_EVENT` and the plain-data `GameLogEventV1`
  (level, tick, entity, category/name/message/reason/result) with
  `GameLogEventFn`. This is one generic capability, not a per-feature ABI field.
- `live-behavior.cpp`: registered `capLogEvent` forwarding to `debug::logEvent`,
  and updated `capLog` to emit a `HOT` event.
- `hot-reload-system.cpp`: emits `HOT_RELOAD` / `module.reloaded` on activation
  (`result: success`), rollback (`result: rollback`), and the compile-load,
  switch-rejection, and package-registration failures (Error level, never
  aggregated).
- `npc-combat-log.cpp`: emits `NPC` / `npc.combat` records with the `proc` field
  instead of opening a separate `.txt`.

### G — self-test
- `live-code-selftest.cpp` now verifies: exactly one `events.jsonl`; no other file
  in the run directory; every line independently valid JSON; strictly increasing
  `seq`; UTC `Z` timestamps; 500 identical events collapse to one summary with
  `count: 500`; a key change flushes the prior bucket; ERROR events are never
  aggregated.

## Evidence

- Cold build: `python build_agent.py` → `Status: SUCCESS`,
  `mimita-20260917T205540.exe`.
- Hot build: `python devscripts/live-build.py` → `mimita-live-g000002.dll`
  (never writes `MiMITA.exe`).
- Runtime: `mimita-20260917T205540.exe --live-code-selftest` →
  `[LIVE CODE SELFTEST] PASS` with every logger check `[ok]`.
- Sample stream (`logs/2026-09-18/20260918_005552/events.jsonl`):
  `logger.started` (path/run_id once), `selftest.keychange`, then a single
  `selftest.repeat.summary` with `count: 500`, then a standalone ERROR, then
  `logger.stopped`.
- Package summary shows `providers=9` (was 8) and `collision.main` still
  resolves, confirming the new generic `log.event` provider registered without
  disturbing the collision package.
- `git diff --check` passed (exit 0).

## Human verification still required

- With the game running, confirm live reading:
  `Get-Content logs\<date>\<hhmmss>\events.jsonl -Wait`,
  `rg '"category":"COLLISION"' <file>`,
  `rg '"event":"module.reloaded"' <file>`.
- Confirm the visible game terminal still mirrors and that raw `printf` output
  appears once as `LEGACY` (no duplicate record alongside `Debug::log`).
- Change `config/debuglogger.json` (`"collision": "verbose"`) and confirm new
  collision events appear without restarting.

## Notes / follow-ups

- Collision-category records are still sparse because the collision subsystem
  uses direct logs; converting its important paths to `debug::logEvent` is the
  next migration step and now needs no new logger API.
- `build.py build-only` does not produce a stamped runnable EXE; use
  `build_agent.py` when a runnable EXE is needed for the self-test.
