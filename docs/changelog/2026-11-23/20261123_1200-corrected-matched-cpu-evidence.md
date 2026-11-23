# Corrected matched CPU and build-resource evidence

## Summary

Reconciled the 2026-11-23 live runtime and DLL-build samples against rotated canonical journals and raw CSVs. Added the corrected measured packet and client-scope addendum to CPU issue `issue-ed3ac775283d`; sent the normalization correction and findings to the Mode 3 collaborator. No code, runtime, build, or WPR changes were made.

## Findings

The matched active run `20261123_113457_mode3-latency-run` was Juggernaut Horde on `dust2cyberiav4`, with 67 server NPCs and one player. Across 11:36:56.868–11:38:01.006 UTC, 65 samples on 12 logical CPUs showed mean host CPU 54.88%, mean used RAM 58.38%, and 6.51 GiB mean / 6.42 GiB minimum available RAM. Server PID 6944 averaged 130.10% of one logical CPU (10.84% of host capacity); client PID 15328 averaged 225.09% (18.76% host). Combined pair CPU was 29.60% of host. Mean working sets were 0.32 GiB server and 0.77 GiB client; private bytes 0.32 GiB and 1.04 GiB respectively. This is active 67-NPC gameplay, not idle/background evidence.

The rotated server journals recorded 63 tick windows: mean tick time 10.55 ms, mean window p95 17.46 ms, worst tick 201.82 ms, and mean NPC stage 9.76 ms per tick. The client recorded 9,149 of 9,926 frames over the 4.167 ms budget (92.18%), with 154.4 mean one-second-window FPS. Client `performance.frame-slice` records report mean frame 6.58 ms, mean window p95 9.97 ms, and maximum frame 72.95 ms. The server's 67 NPC count conflicts with client `npcs_active=0`, so the client counter is not valid evidence that remote NPC workload was absent.

Per-window NPC diagnostic fields averaged 139.9 ms collision and 176.3 ms physics across each approximately one-second aggregation window, plus 36.1 ms path and 24.6 ms sensing. These are elapsed-scope values, not thread CPU. They identify likely areas for CPU stack attribution but do not state a percentage of process CPU.

The client scope logger reported 315 frame-slice records during the same interval. Selected values include `EngineTick::Simulate` 19.775 s elapsed / 2,783,295 allocations / 993.7 MiB, `Networking` 10.650 s / 1,338,512 / 724.6 MiB, `Network::MpTick` 9.573 s / 1,219,618 / 711.4 MiB, and `Network::RemoteEntities` 5.911 s / 180,143 / 43.8 MiB. These nested scope counters are not exclusive CPU measurements and their allocation bytes do not mean retained memory; do not sum nested scope durations or allocation bytes as independent work.

The build-associated CSV has 17 rows from 11:42:05.695–11:42:21.880 UTC. Generation 557 completed at 11:42:19.992 UTC. Across all 17 rows, host CPU averaged 87.49%; restricting to the 14 rows before build completion gives 89.66%. In those 14 rows, server PID 6944 averaged 12.05% host CPU, client PID 15328 20.75%, and pythonw PID 16460 1.92%. Available RAM averaged 6.30 GiB during that strict overlap. The CSV did not track compiler/linker PIDs or top competing processes. Build duration was 15.098 s, including 7.045 s changed-source preflight, 0.333 s compile, 2.575 s link, and 0.366 s publish. This shows concurrent host load, not proof that the build worker caused it. The 86.51% value previously reported does not match the raw CSV row selection and was flagged to the collaborator for reconciliation.

## Evidence and limits

- Runtime resource CSV: `.dev/perf-captures/mode3-jobs10-live-20261123T1136Z.csv`
- Build resource CSV: `.dev/perf-captures/mode3-jobs10-build-20261123T1142Z.csv`
- Runtime identity: `logs/2026-11-23/20261123_113457_mode3-latency-run/run.json`
- Runtime journals: `logs/2026-11-23/20261123_113457_mode3-latency-run/{server-01,client-01}/events*.jsonl` (include numbered rotation chunks)
- Build timing: `.dev/build-timings/game-dll-20261123T114204890604Z-28389a.jsonl`
- Diagnostic source locations: `src/network/server.cpp:524`, `src/network/server-npcs.cpp:3359`, `src/npc/npc.cpp:4203`, `src/game/server-runtime-module.cpp:454`, `src/engine/engine-tick.cpp:108`, `src/network/multiplayer-tick.cpp:1273,3565`, and `src/network/multiplayer-interpolation.cpp:2162`.

The earlier statement that the live workload was unknown was corrected after scanning `events-*.jsonl`; rotation had moved server events out of the active `events.jsonl`. The captured scopes are useful owner-level leads, not CPU function attribution. The prior sampled-stack ETL lacked profile samples, so exact per-function CPU percentages remain unavailable. No function-level CPU reduction or behavior proof is claimed.

## Next step

Obtain a bounded stack-sampled CPU trace with profiling interval verified and exact active game DLL/symbols, track MiMITA PIDs/threads and build worker PIDs, and record concurrent process usage. Start with the NPC fixed-tick collision/physics path and client simulation/network paths identified above. Then change one evidenced owner at a time and repeat matched active and idle measurements; preserve fixed 60 Hz authority and separately measure client frame-time against 4.167 ms and gameplay correctness. Cross-link build throughput work to `issue-136d4553ff92`.

## Changelog file

New file; no prior text existed. This file records this session's issue updates and evidence review. No source files changed and no build, synthetic test, or WPR capture was run.
