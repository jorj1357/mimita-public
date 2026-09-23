# Phase 4: server journal taxonomy + process exit-cause classification

Date (UTC): 2026-09-23T20:34:36Z
Status: implemented; build and selftests verified; dedicated-server journal observed

## Scope

Phase 4 of the hot server/networking migration (proposal §8/§9): give the server
process its own authoritative, correlated event record and classify how the
server process ended, so a silent restart is never used before the original
cause is observable.

## Changes

1. **Journal correlation fields** (`live-journal.h`/`.cpp`, additive): added
   `clientTick`, `connectionId`, `requestId`, `entityId`, `serverGeneration`,
   `hotGeneration`, `serverHash`, `hotHash`. Zero numeric ids are omitted so
   existing lines stay compact and every existing reader is unchanged. The
   writer already flushed every line, so critical records are durable.

2. **Server lifecycle records** (`server.cpp`): `server.started` (with the loaded
   hot generation/hash), `server.shutdown.requested`, and
   `server.shutdown.completed` (with `cause`/`exit_code`). The completed record is
   written only when execution reaches the server's own shutdown path; a missing
   completed record identifies a crash/transport failure/termination.

3. **Hot-generation records** (`server.cpp`): `server.hot_generation_announced`
   and `server.hot_generation_activated` with generation/tick.

4. **Player spawn/respawn correlation** (`server-players.cpp`): live-journal
   `server.player_spawned` / `server.player_respawned` mirrors with entity id,
   connection id, spawn generation, transform epoch, health, and the active hot
   generation/hash.

5. **Exit-cause classifier** (`live-code/server-exit-cause.h`, new): pure
   `classifyServerExit(childReportedClean, exitCode, externallyTerminated)` ->
   ExternalTermination / CleanShutdown / NonZeroExit / Unknown. The launcher
   (`gui-main.cpp`) marks explicit `TerminateProcess` as external termination and
   prints the classified cause once. It never assumes a zero exit is clean.

6. **Self-test** (`--server-journal-selftest`, new): correlation fields emitted,
   zero ids omitted, and the full classifier matrix (including "zero exit without
   a clean record is not assumed clean").

## Evidence

- Source changes: `live-journal.h`, `live-journal.cpp`, `server.cpp`,
  `server-players.cpp`, `server-exit-cause.h`, `gui-main.cpp`,
  `server-journal-selftest.{h,cpp}`, `game-cli.cpp`.
- Build (source/build evidence):
  - Cold EXE: `python build_agent.py` -> `Status: SUCCESS`
    (`mimita-20260923T163310.exe`). A running `mimita.exe` was not touched.
- Automated tests (test evidence):
  - `--server-journal-selftest` -> PASS (17 checks).
  - `--live-code-selftest`, `--actor-lifecycle-selftest`,
    `--lagcomp-history-selftest`, `--transport-generation-selftest`,
    `--capability-selftest`, `--generation-bootstrap-selftest`,
    `--packet-codec-selftest` -> PASS.
- Runtime evidence (observed): launched the dedicated server and terminated it
  externally; the server's OWN journal
  (`logs/features/live-code/2026-09-23/live_events_20260923_203406.jsonl`)
  contains `process:"server"`, `server.started` with `hot_generation`/`hot_hash`,
  and NO `server.shutdown.completed` — the intended crash/termination signal.
- Human acceptance: pending.

## Not done

- Building the parent-side "saw clean shutdown" fact from the child's journal (the
  launcher currently relies on external-termination + exit code; the server
  journal is the authority).
- The remaining proposal event set (packet_received/sent at scale, lag_compensation_decision,
  reliable_event_queued/acknowledged, generation_mismatch) is only partially
  emitted today.
