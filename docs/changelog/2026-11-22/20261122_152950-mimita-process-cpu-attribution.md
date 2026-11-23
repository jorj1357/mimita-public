# MiMITA process CPU attribution

## Scope

Measured the live MiMITA process tree and matched it to runtime diagnostics. No source, build, network, or gameplay behavior was changed. Findings were appended to CPU issue `issue-ed3ac775283d` with the user's exact request linked as context.

## Process sample

Cumulative CPU time was sampled over 2026-11-22T15:21:31.2886754Z–15:21:34.6366740Z (3.348 seconds, 12 logical CPUs). CPU-second values are rounded to hundredths; percentages are approximate and normalized to the whole 12-core machine.

| PID | Role and launch details | CPU seconds | One logical CPU | Whole machine |
|---:|---|---:|---:|---:|
| 30076 | Dev-loop client, `--connect 127.0.0.1:51171` | 5.83 | 174.1% | 14.51% |
| 4540 | Dev-loop server, `--mode jugger_horde --npcs 1`, no players in live journal | 3.89 | 116.2% | 9.68% |
| 28664 | Second server child of PID 30076, `--mode duel --npcs 1`, listening on 1357 | 1.78 | 53.2% | 4.43% |

Total MiMITA share in that sample was about 28.62% of machine capacity, or 3.43 logical-core equivalents. This is process CPU only; no matched whole-host CPU sample was captured. It is consistent with the user's reported process readings, while showing three simultaneous MiMITA processes rather than a single one-NPC server.

## Runtime evidence and source locations

At 2026-11-22T15:22:57Z, the root dev-loop server journal (`logs/2026-11-22/20261122_144609_01/server-01/events.jsonl`, PID 4540) still reports `mode=jugger_horde`, zero players, and 66 NPCs despite its `--npcs 1` startup argument:

- `performance.server-tick-window` at `src/network/server.cpp:524`: 66 NPCs, 0 players, NPC average 62.01 ms/max 195.34 ms; total average 64.91 ms/p95 196.63 ms.
- `performance.server-loop-window` at `src/network/server.cpp:647`: actual 15.13 Hz, loop average 330.08 ms.
- `npc.server-phase-window` at `src/network/server-npcs.cpp:3144`: 776.48 ms AI across 15 ticks; worst actor 124.19 ms.
- `server.world-phase-window` at `src/game/server-runtime-module.cpp:454`: 728.58 ms NPC simulation and 202.10 ms physical contact.

This proves that one server remains CPU-active after its player count reaches zero. The process tree also contains a second Duel server. PID 28664's launch argument says one NPC, but its actual actor count and journal have not been identified.

The client journal's most recent frame profile is older (15:12:57Z; file last write 15:12:57Z while PID 30076 remained alive at 15:21Z). That last `performance.frame-slice` at `src/perf/perf-frame.cpp:493` showed 5 frames in 210.69 ms, 41.69 ms average, 84.97 ms p95/max, all frames over the 4.167 ms budget, and 11,478 allocations / 947,062 bytes. Its top scopes included `RagdollCorpseUpdate` 22.59 ms over 17 calls, `Ragdoll::StepBody` 21.34 ms over 102 calls, `Ragdoll::WorldCollision` 12.75 ms over 102 calls, and `Simulation::Lifecycle` 27.15 ms over 17 calls. The measured code path is `src/sim/simulate-tick.cpp:744-745` into `RagdollModeSystem::updateCorpses` at `src/ragdoll/ragdoll-mode.cpp:2927-3055`, then `stepBody` at `:2849-2881`. The code caps full corpse solves to six per tick by default (`src/ragdoll/ragdoll-mode-config.h:78`). These scoped timers identify expensive owners, but they do not assign every CPU sample to an exact instruction or call stack. The event's `corpses=0` summary conflicts with nonzero ragdoll solver scopes and needs clarification.

Windows Performance Recorder's CPU profile could not start; it returned `0xc5585011`, “Failed to enable the policy to profile system performance.” No ETL was created, so native sampled call stacks are unavailable in this session.

## Next measurement

The largest proven contributor is PID 4540's retained 66-NPC Jugger Horde simulation. Measure again after the old dev-loop server is stopped through its normal control path, and separately measure PID 28664's actual Duel actor count and CPU. Refresh client performance slices in the same interval as process CPU. Native per-call-stack attribution requires an authorized WPR run with system-performance profiling rights.


## User correction and recent-change correlation

The user confirmed the 999 FPS cap has been unchanged throughout; do not attribute the CPU increase to a cap change. `config/video-settings.json` history has no `maxFrames` change since Aug 2026, and commit `9ed266ca` changed only `fullscreen` from 6 to 0 in that file, preserving `maxFrames: 999`.

A stronger runtime regression candidate is commit `70f3c051` (2026-11-22 09:28:40 -05). In `src/npc/npc.cpp`, the pre-change `hasLoggedDash` guard limited `npc.dash` event emission to once per actor life; the commit removed the guard and writes the Important event on every dash (current event source line 3741). It also removed the reset at respawn from `src/network/server-npcs.cpp`. The live server log later recorded 1,002 such events in 60 seconds from 54 actors. That proves a large increase in logging work; the share of process CPU caused by serialization/file logging remains unmeasured. The same commit revised objective-navigation fallback, but the live Jugger Horde run does not establish that objective code path as active.

This is distinct from the independently measured 66-NPC zero-player Jugger Horde simulation, which itself overruns the 60 Hz tick budget. The build-only `d89884f6` change selected 10 DLL jobs with high priority; it can add contention during actual builds, but is not the explanation for runtime load after builds finish. A matched before/after build comparison remains open.


## Logger flush mechanism

The repeated dash event has a synchronous logging path. `StructuredLogger::writeEvent` builds a JSON record and calls `writeJsonLine` with `flush=true` for Important level events (`src/debug/structured-log.cpp:1553-1581`). The adjacent `tryWriteEvent` comment says per-record `fflush` under the cross-process journal mutex can create fixed-tick stalls (`:1622-1625`); `flushEvent` locks that mutex and calls `fflush` (`:1711-1719`). Commit `70f3c051` made each dash in `src/npc/npc.cpp` an Important event with no per-actor guard. At the observed rate of 1,002 events/minute, this is about 16.7 Important record writes per second from NPC dashes alone. That is a strong mechanism for additional CPU, file I/O, and tick delay, but the CPU share has not been isolated from the 66-NPC AI/physics workload.
