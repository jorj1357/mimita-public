# Hot logging ABI bootstrap (Phase 0)

Time: 2026-09-24T15:26:22Z

## Scope

Phase 0 of "move all debug logging behind the hot boundary": install the stable
logging ABI and the generic overridable-capability mechanism with one intentional
cold build. This pass does not yet move filtering/fields/destinations into a hot
provider (Phases 2-3) and does not touch raw `printf` or ad-hoc debug files.

## Changes

- `src/hot-reload/game-api.h`
  - Append-only `GameLogEventV1` envelope: `structSize`/`abiVersion`,
    `schemaId`/`schemaVersion`, `fieldCount` + `const GameLogFieldV1* fields`,
    `correlationId`, `parentEventId`, `sessionId`, `processRole`. `structSize == 0`
    means the original V1 layout, so old and new generations coexist. The hot API
    version is intentionally not bumped.
  - Generic hot-safe field payload: `GameLogFieldType`, `GameLogFieldV1`,
    `GameLogFieldListV1` (POD, no STL, no owning pointers).
  - Provider output `GameLogRecordV1` + `GameLogProviderFn`, destination bits, and
    `GAME_CAP_LOG_APPEND` + `GameLogAppendFn`.
- `src/hot-reload/generic-runtime.{h,cpp}`
  - Generic `overridable` flag on kernel capabilities, a defaulted
    `registerKernelCapability(..., bool overridable)`, and
    `overrideCapability` / `overrideProviderGeneration` /
    `kernelCapabilityOverridable`. Non-overridable kernel ids are unaffected.
- `src/live-code/live-behavior.cpp`
  - `log.event` registered overridable. `capLogEvent` consults a package override
    first (provider fills `GameLogRecordV1`), then falls back to the existing cold
    path; a declining override falls through.
  - `makeLogEvent` projects the append-only envelope, including the typed field
    payload when `structSize` covers it.
  - New `capLogAppend` registered as `log.append` for the one safe append
    mechanism.
  - `capLog` no longer writes to stdout directly.
- `src/debug/structured-log.{h,cpp}`
  - `writeLine` now performs one atomic `fwrite` of the whole line (including the
    newline), preventing interleaved malformed lines in a shared `events.jsonl`.
  - New `appendRaw` and `emitProviderRecord`; `buildRecord` accepts a prebuilt
    provider body that replaces the caller field object.
- `src/hot-reload/hot-modules.json`
  - Logging mechanism files classified `cold` (`structured-log.*`,
    `log-manager.*`, `debug-log.*`, `crash-handler.*`, `live-journal.*`);
    `net-hot-log.h` classified as a `bridge`.
- `src/hot-reload/capability-selftest.cpp`
  - New P6 checks: an overridable kernel id resolves to the package provider
    while the kernel entry stays stable; non-overridable ids are never
    overridden; deactivate retires the override and the kernel fallback survives.

## Validation

Cold build (canonical `python build_agent.py`):

```text
Status: SUCCESS
Executable: mimita-20260924T112457.exe
```

Automated tests (test evidence):

```text
--capability-selftest   PASS (incl. new P6 overridable checks)
--live-code-selftest    log.event capability resolves; the 4 "journal" checks
                        FAIL identically on the pre-change build
                        mimita-20260924T100751.exe (pre-existing, not caused here)
```

Build evidence is separate from runtime evidence: the capability and live-code
selftests ran against the new executable and are recorded above. No human/live
acceptance was performed in this session.

## Honest boundary

- The hot logging provider that owns filtering, fields, schemas, destinations,
  throttling, and aggregation is Phase 2; the cold fallback is still authoritative
  when no override is registered.
- Raw `printf` and ad-hoc debug-file writers were deliberately deferred.
- The pre-existing `--live-code-selftest` journal failures are unrelated to this
  change and were reproduced on a prior build.
