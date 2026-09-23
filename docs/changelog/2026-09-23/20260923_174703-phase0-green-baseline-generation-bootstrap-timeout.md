# Phase 0: green baseline + bounded generation-bootstrap failure

Date (UTC): 2026-09-23T17:47:03Z
Status: implemented; build and selftest verified; no live two-process run

## Scope

Phase 0 of the hot server/networking migration
(`docs/architecture/live-development/hot-kernel-next-steps.md`,
`docs/specs/networking/networking.md`): restore a green baseline and turn the
logged persistent client/server live-generation mismatch into a bounded,
explicit failure. No gameplay behavior was changed.

## Changes

1. **Fixed a pre-existing cold EXE compile failure** in
   `src/network/server-players.cpp`. The lifecycle-JSONL patch and the generic
   actor-lifecycle boundary patch both declared a local named `lifecycleEvent`
   in the same scope (`debug::Event` and `ActorSpawnEvent`). Renamed the debug
   journal event to `lifecycleJournalEvent` (declaration + `MIMITA_EVENT`).
   This failure predated this session and blocked the whole cold build.

2. **Bounded late-join generation bootstrap** (`src/hot-reload/generation-bootstrap.h`):
   - added `BootstrapFailure::Timeout = 3` and `bootstrapFailureName`;
   - added `startedMs` (reset by `begin()`);
   - added `tickTimeout(nowMs, timeoutMs)` which turns an active but
     unacquired bootstrap into an explicit `Failed(Timeout)`.

3. **Wired the timeout** in `src/network/multiplayer-tick.cpp` (20 s window,
   once per client tick). On firing it writes a `generation_bootstrap_failed`
   JSONL record with server/target generation and failure reason. The hot
   `net.generation-policy` still owns world participation, so a mismatch stays
   playable; it is now visible instead of wedging in `Acquiring` forever.

4. **Selftest cases** added to
   `src/hot-reload/generation-bootstrap-selftest.cpp`: timeout arms, stays
   acquiring inside the window, fires to an explicit failure, and a completed
   bootstrap is unaffected.

## Root cause recorded from the evidence log

`logs/features/live-code/2026-09-23/live_events_20260923_163837.jsonl` shows
`generation_sync_state result:"mismatch"` with `bootstrap_state:2` (Acquiring)
and `server_generation:1` for the whole session while the client activated
generations 4, 5 and 13. The server advertised its ACTIVE generation
(`phase:3`, `server-packets.cpp:1725-1769`) but the client never completed
artifact acquisition, so `worldParticipationAllowed` stayed false indefinitely.
This is the failure family already recorded in
`docs/regressions/2026-09-20/generation-mismatch-repeats-REG.md` and
`docs/regressions/2026-09-16/2026-09-16-server-client-generation-mismatch-spawn-lock.md`.
The hot `net.generation-policy` was the intended safety valve; the missing piece
was a bounded failure for an unacquirable server generation.

## Evidence

- Source changes:
  - `src/network/server-players.cpp`
  - `src/hot-reload/generation-bootstrap.h`
  - `src/hot-reload/generation-bootstrap-selftest.cpp`
  - `src/network/multiplayer-tick.cpp`
- Build (source/build evidence):
  - Hot DLL: `python build_game_dll.py` -> `DLL build success`
    (`build/mimita-game.dll`); re-run `DLL up to date, skipping`.
  - Cold EXE: `python build.py build-only` -> `BUILD SUCCESS` (exit 0,
    linked `mimita.exe`). The first attempt failed on the pre-existing
    `server-players.cpp` collision before the fix.
- Automated tests (test evidence):
  - `mimita.exe --generation-bootstrap-selftest` -> PASS (includes the 4 new
    timeout checks).
  - `mimita.exe --transport-generation-selftest` -> PASS.
  - `mimita.exe --live-code-selftest` -> PASS.
- Runtime evidence: none. No two-process live session was run.
- Human acceptance: pending.

## Remaining Phase 0 debt

- Dedicated `--server` process journal identity and exit-cause classification
  (proposal §8/§9) are not implemented.
- Cold-parity fixtures for join / packet / rocket / death / respawn / shutdown
  are not captured.
- The two-process live proof (client converges to server generation, or shows
  the new bounded failure) is still required. Until then the mismatch record
  remains an attempted fix.
