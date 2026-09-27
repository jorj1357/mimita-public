# Hot logging provider and canonical writer bridge (Phases 2-3)

Time: 2026-09-24T15:43:06Z

## Scope

Phases 2 and 3 of "move all debug logging behind the hot boundary": add the hot
logging provider that owns logging policy, and make the cold logger storage +
mechanism only. Nothing was deleted; cold policy is retained as the fallback and
marked legacy.

## Changes

- `src/hot-reload/hot-logging.h` (new) and
  `src/hot-reload/modules/logging-provider.cpp` (new)
  - Registers the overridable `log.event` provider and a `logging.flush` system.
  - Owns category/level filtering, event naming, field selection,
    schema/version awareness, sampling/throttling, aggregation, destination
    routing, and JSONL body construction.
  - Live-reloads `config/debuglogger.json` (including a new `destinations`
    block); a malformed update keeps the last-good config and emits one
    `logger.config_error` record.
  - Never opens a file: aggregation summaries and config errors go through the
    `log.append` mechanism.
- `src/hot-reload/structured-log.cpp` (cold)
  - `debug::logEvent` now consults the hot provider first and falls back to the
    cold path only when no provider is active or it declines.
  - Added the provider bridge (`fillEnvelopeFromEvent` + `emitViaProvider`) that
    projects a `debug::Event` onto the stable POD envelope with a typed field
    payload, plus `appendBody` (universal prefix + atomic line) used by
    `log.append`.
  - A provider body fragment is honored even when empty (provider chose no
    fields).
- `src/live-code/live-behavior.cpp`
  - `capLogEvent` honors `GAME_LOG_DEST_JSONL` / `GAME_LOG_DEST_TERMINAL` from
    the provider decision (terminal routed through the `terminal.output`
    capability).
  - `capLogAppend` now wraps a body fragment via `appendBody`.
  - `makeLogEvent` carries `correlationId` / `parentEventId` from the envelope.
- `src/debug/structured-log.h` / `src/debug/debug-log.h`
  - `StructuredLogger` config/level/aggregation logic documented as LEGACY cold
    fallback; `debug-log.h` documented as a legacy cold bridge. No code deleted.
- `src/hot-reload/hot-modules.json`
  - `src/hot-reload/hot-logging.h` added to `headers` so runtime change detection
    sees provider edits.
- `src/live-code/live-code-selftest.cpp`
  - New check: the cold `debug::logEvent` API routes through the hot provider
    while the package is active.

## Validation

Cold build (canonical `python build_agent.py`):

```text
Status: SUCCESS
Executable: mimita-20260924T114203.exe
```

Automated tests (test evidence):

```text
--capability-selftest        PASS (incl. P6 overridable checks)
--live-code-selftest         PASS except the 4 pre-existing "journal" checks
                             (reproduced identically on mimita-20260924T100751.exe);
                             new "cold logEvent routed through hot provider" PASS
--gamemode-hot-selftest      PASS
--dynamic-lifecycle-selftest PASS
--production-loop-selftest   PASS
--hot-combat-selftest        only the 25 known pre-existing animation/phase2 FAILs
--hot-authoritative-selftest only the known pre-existing journal-evidence FAIL
```

Startup log confirms the provider is active:

```text
[SYSTEM_REGISTERED] system=logging.flush ...
[CAPABILITY_RESOLVED] provider=logging.provider id=... (log.event)
```

Build evidence is separate from runtime evidence: the above tests ran against
the new executable. No live/human acceptance was performed.

## Honest boundary

- Sampling/throttling decision counters are parsed from config but not yet used
  to drop events (aggregation is active). A follow-up can wire them without an
  ABI change.
- The hot provider always handles `log.event` while active, including during
  `StructuredLogger::init/shutdown`; the cold fallback runs only when no
  provider is registered.
- Player/NPC/projectile `player.state` dynamic field schemas and raw `printf`
  cleanup remain later phases (cleanup deferred by decision).
