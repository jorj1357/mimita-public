# Server burst and low-Hz diagnostics

- Task: investigate the alternating packet-burst/one-second-pause behavior in
  `C:\mimita-v9\logs\2026-10-11\20261011_004504\events-000001.jsonl` and
  compare it with the recent NPC performance regression history.
- Date, time, timezone: 2026-10-10T20:49:30-04:00, America/New_York.
- Branch/commit context: existing working tree at `c59876fc`; unrelated
  pre-existing edits were preserved.

## Investigation result

The supplied journal is sufficient to identify the broad cause. The server is
not maintaining a 60 Hz wall-clock schedule once 67 NPCs become active. It is
healthy at `59.46--60.16 Hz` before activation, then reports windows including
`27.57`, `0.77`, `23.83`, `13.22`, `4.79`, and `7.61 Hz`. The corresponding
fixed-tick windows identify NPC simulation as the recurring cost, including
`1,132.3 ms` average and `3,950.0 ms` maximum NPC time in one window, and
later `126.9 ms` average / `713.7 ms` maximum.

The client-side snapshot evidence matches a server backlog rather than random
packet loss: snapshot inter-arrival gaps reach `3,922 ms`, `3,459 ms`,
`1,327 ms`, and about `1,000 ms`, while the applied snapshot stream reports
`snapshots_missed=0` and `tick_gap=1`. In simple terms, the server stops
producing snapshots on time, then catches up and the client sees a burst.

The supplied run is not a test of the attempted AimBody fix. Its runtime
identity is `.dev\\builds\\1964\\mimita.exe`, built at `19:32:15`; the current
source fix was built later at `20:48:24`. The old binary is therefore expected
to retain the pre-fix behavior.

## Recent-change correlation

- `20261008_150000-human-ai-runtime-investigation-loop.md` records that the
  coordinator wait was moved off the authoritative loop, while rare NPC-stage
  spikes remained open.
- `20261010_204037-server-npc-regression-correlation.md` links the later
  sustained NPC regression to the Oct 8 actor/NPC change that added the
  per-NPC `RagdollModeSystem::updateNpcAim` path.
- `20261010_204433-server-npc-empty-aimbody-fix.md` records removal of that
  unused server-side AimBody path after the journal showed repeated empty-body
  initialization.
- The supplied Oct 10/11 journal predates that fix, so the correct next step
  is a matched run using the new published executable before selecting another
  NPC behavior change.

## Source change

In `src/network/server.cpp::reportServerPerf`, added a bounded
`network.server-io-window` StructuredLogger event once per server performance
window. It reports:

- packet-in/out totals and per-window deltas;
- receive attempts, would-blocks, errors, malformed packets, protocol
  mismatches, and unknown packet types;
- hello, join, reconnect, and input packet deltas.

The old behavior emitted only human-readable cumulative `[SERVER STATUS]`
transport counters. The new event keeps the same counters in the canonical
JSONL journal and aligns them with the already-existing loop/tick window.
No packet handling, scheduling, or gameplay behavior was changed.

## Validation

- `git diff --check`: passed; only the repository's existing line-ending
  warning was reported.
- Canonical build: SUCCESS via `python build_agent.py`; `src/network/server.cpp`
  compiled and linked at `20:48:24`. A second forced metadata build compiled
  `src/game/game-cli.cpp` and linked again at `20:49:21` after the first
  `--versioninfo` check exposed a stale skipped CLI object.
- `mimita.exe --versioninfo` executed successfully and wrote
  `logs/10-10-2026/20261010_204922/events-000001.jsonl`. Its embedded
  `build_time` still reports `19:32:03`, so the executable's compile-stamp
  field is stale despite the fresh server object/link. The build result and
  filesystem timestamp are therefore recorded separately from that metadata.
- Runtime: the supplied journal was inspected; it came from build 1964 before
  this logging change and before the AimBody fix, so it is diagnostic evidence
  only. A fresh run is still required to prove the new event is present and
  to measure the fix.
- Human/gameplay acceptance: not performed.

## Focused documents and skills

- `docs/ROUTER.md`
- `docs/specs/networking/networking.md`
- `docs/specs/performance/performance.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/skills/logging-checker-v1.md`
- `docs/skills/efficiency-checker-v1.md`
- `docs/regressions/regressions-v1.md`
