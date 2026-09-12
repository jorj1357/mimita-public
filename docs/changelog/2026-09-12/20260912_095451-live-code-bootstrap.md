# Live-code bootstrap: architecture, journal, and v3 hot-module pipeline

- EST timestamp: 2026-09-12 05:54:51 EDT (UTC 2026-09-12T09:54:51Z)
- Branch: `8292026stash`
- Commits: none (work left uncommitted for human review)
- Result: `PASS_WITH_HUMAN_REVIEW`

## Task

Implement bootstrap phases 0, 1, and 2 of the live-code development plan:
authoritative documentation, live UTC/millisecond event evidence, notification
and sound lifecycle, a general `GameAPI v3` contract, and a non-blocking
background compile/validate/activate/rollback pipeline. The game thread must
never block on compilation, and a failed candidate must never replace the active
version.

Decisions confirmed with the human before implementation:

- scope: Phase 0-2 only;
- one coarse replaceable DLL with internal module tables;
- runtime owns the JSONL journal as the single writer, worker writes a result
  file;
- actor unification is incremental at the hot boundary.

## Documents

- New authoritative spec / feature record:
  `docs/features/live-code-development/live-code-development.md`
- Router route added: `docs/ROUTER.md` common-routes table, new row
  `Live code, hot reload, or replaceable game modules`.
- Read for the task: `docs/skills/spec-behavior-review-v1.md`,
  `docs/skills/logging-checker-v1.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`,
  `docs/specs/debug-logging/debug-logging.md`,
  `docs/architecture/player-npc-systems/player-npc-systems.md`.

## Exact files and changes

### Phase 0 - documentation

- Added `docs/features/live-code-development/live-code-development.md` with the
  required contract block, ownership, envelopes, migrations, fixed-tick
  activation, hashes/generations, compile-failure, rollback, deterministic
  validation, live evidence, and the future multiplayer READY/switch-tick
  protocol.
- `docs/ROUTER.md`: added one route row. No architecture was added to
  `AGENTS.md`.

### Phase 1 - time, journal, notifications

- Added `src/utils/time-format.h` and `src/utils/time-format.cpp`:
  `utcIso8601Millis`, `utcIso8601Seconds`, `utcDateFolder`, `utcCompactStamp`,
  `monotonicMillis`.
- Deleted five duplicated UTC helpers and routed them to the shared utility:
  - `src/analytics/analytics-events-impl.cpp` (removed `isoNow`)
  - `src/terminal/player-commands.cpp` (removed `chatUtcNow`, `chatNowMs`)
  - `src/gui/hud/chat-window.cpp` (removed `chatUtcNow`)
  - `src/gui/ui-text-input.cpp` (removed `chatUtcNow`)
  - `src/network/multiplayer-tick.cpp` (removed `chatUtcNow`)
- Added `src/live-code/live-journal.h/.cpp`: thread-safe append-only JSONL,
  flush per line, path
  `logs/features/live-code/<yyyy-mm-dd>/live_events_<yyyymmdd_hhmmss>.jsonl`,
  fields `ts_utc`, `mono_ms`, `type`, `tick`, `generation`, `code_hash`, `file`,
  `module`, `actor_id`, `projectile_id`, `packet_id`, `result`, `error`.
- Added `src/live-code/live-code-events.h/.cpp`: lifecycle notifications, sounds,
  and journal records for edit/compile/failure/ready/activation/rollback.
- `src/audio/audio.cpp`: added `live/success` and `live/failure` aliases reusing
  existing assets (no new binary asset).
- `src/main-init.cpp`: `LiveEventJournal::instance().init()` after
  `StructuredLogger` init. `src/main.cpp`: journal `shutdown()` before
  `engine.shutdown()`.

### Phase 2 - GameAPI v3 and pipeline

- `src/hot-reload/game-api.h`: API version 2 -> 3; added `GameStateType`,
  `GameEnvelope`, `GameSelfTestResult`, `GameSelfTestFn`,
  `GameModuleDescriptor`, `GameEffectModuleV1`; `GameAPI` now carries
  `generation`, `codeHash`, `selfTest`, and named `modules[]`.
- `src/effects/effect-part.cpp` (DLL branch): added `gameSelfTest` (deterministic
  one-step effect integration check) and published the `effects` module table.
- `src/live-code/code-hash.h/.cpp`: SHA-256 via BCrypt provider API.
- `src/hot-reload/hot-reload-system.h/.cpp`: replaced the blocking
  `std::system` mtime reload with a non-blocking pipeline:
  - SHA-256 hash manifest from `src/hot-reload/hot-modules.json`;
  - background `std::thread` worker running `build_game_dll.py` in generation
    mode into `build/hotreload/gen<N>/`;
  - candidate ABI check + `selfTest`, `LoadLibrary` of a unique temp copy;
  - activation at the top of the fixed tick (`pollAndAdvance`);
  - two-generation retention (active + previous) so rollback swaps tables and
    the older module is freed one activation later;
  - `rollback()` and a `Status` snapshot.
- `build_game_dll.py`: added `--generation`, `--output`, `--result`,
  `--hot-modules`; reads the manifest, hashes sources+headers, writes
  `build-result.json`, and never overwrites the active DLL. Default invocation
  keeps the legacy skip-if-newer behavior.
- Added `src/hot-reload/hot-modules.json` (effects module + `game-api.h` header
  in change detection).
- `src/engine/engine-tick-setup.cpp`: `reloadGameDLLIfChanged()` ->
  `pollAndAdvance()`.
- `src/main-init.cpp`: `HotReloadSystem::instance().startup()`.
- Added `src/terminal/live-code-commands.h/.cpp` with `hotreload status` and
  `hotreload rollback`; registered in `src/main-systems.cpp`.
- Added `src/live-code/live-code-selftest.h/.cpp` and a `--live-code-selftest`
  CLI path in `src/game/game-cli.cpp`.

## Reasoning

- The existing system already had a `GameAPI`, so it was extended rather than
  duplicated, per the specification.
- The old pipeline blocked the game thread on `std::system("python
  build_game_dll.py")` and used mtimes; both conflict with the required
  non-blocking, hash-based, generation-stamped behavior.
- Retaining exactly two loaded generations makes rollback instant and guarantees
  no call can still be executing retired code before `FreeLibrary`.
- Envelopes and module descriptors are plain data only; no STL or engine objects
  cross the boundary.

## Validation and evidence

- Source build: `python build_agent.py` -> `Status: SUCCESS`
  (`build/changelog.txt`).
- DLL build: `python build_game_dll.py` -> success; generation mode produced
  `build/hotreload/gen1/mimita-game.dll` and `build-result.json`
  (`status: ok`, `code_hash: ef1d37b3...`).
- Runtime (headless, real EXE): `mimita.exe --live-code-selftest` ->
  `[LIVE CODE SELFTEST] PASS`, exit 0, with:
  - `[ok] utc millisecond format`
  - `[ok] sha256 empty string`
  - `[ok] journal active` / `[ok] journal line written` / `[ok] journal utc field`
    / `[ok] journal valid JSON object`
  - `[ok] GameAPI load + ABI + self-test` (`activeGeneration=1`,
    `reloadCount=1`).
- Journal artifact: `logs/features/live-code/2026-09-12/live_events_*.jsonl`.

## Focused skills

- `docs/skills/spec-behavior-review-v1.md`: PASS. The new spec is authoritative;
  code matches it. No spec-code disagreement found. No `NEEDS_SPEC_DECISION`.
- `docs/skills/logging-checker-v1.md`: PASS. New diagnostics are owned by the
  live-code system; journal lines are event-driven, not per-frame; the only
  per-frame work is a throttled (15-frame) hash compare with no allocations on
  the no-change path beyond hashing every 15 frames.

## Human review still required

- `edit -> background compile -> safe-tick activation -> visible behavior change`
  in a live match was not performed (no interactive session); the worker and
  generation build are proven, but full end-to-end activation needs a human.
- Notification popup text and the success sound are implemented and source-proven
  but not visually/audibly accepted.
- The build worker requires `python` on PATH; not yet resolved explicitly.

## Pre-existing edits preserved

The working tree already contained many modified files (audio, combat, config,
gamemode, network, npc, render, etc.) and untracked files. None were reverted or
claimed; only the files listed above were changed for this task.
