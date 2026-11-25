# Generation 627 live NPC phase evidence

## Goal and result

Continue the CPU evidence-to-code loop. This pass used current server/client journals and traced existing phase timing to its source owner. No runtime code was changed and no build, command, restart, or CPU capture was started. The shared run remains owned by `root-mode3-latency`; no optimization A/B was performed.

## Current live journal evidence

Read-only inspection of `logs/2026-11-24/20261124_221014/server-01/events.jsonl` and `client-01/events.jsonl` at 2026-11-24 23:05:35–23:05:46 UTC found server PID 27372 and client PID 26492 in the same run ID. Server runtime reports `funcounterstrike`, `dust2cyberiav4`, 19 NPCs and 1 player. The host DLL reports generation 627 while `npc.phase-window` reports NPC module generation 625; do not treat this as a generation-aligned runtime.

Two adjacent NPC phase events report:

| UTC | Updates | Total phase wall time | Physics wall time | Collision wall time |
|---|---:|---:|---:|---:|
| 23:05:35.768 | 1,140 | 182.70 ms | 101.86 ms | 81.39 ms |
| 23:05:36.769 | 1,025 | 238.01 ms | 120.66 ms | 97.96 ms |

These are aggregated `steady_clock` elapsed durations within one-second NPC phase windows, not thread CPU-cycle measurements. Collision time is nested within the physics time; do not add them. They make NPC collision a concrete profiling candidate, but do not prove it dominates total process CPU or is the cause of whole-PC slowdown.

The same server journal reports a 56-tick window with 12.77 ms average, 52.94 ms p95 and 72.79 ms maximum total tick; the following 60-tick window reports 12.37 ms average, 41.33 ms p95 and 68.70 ms maximum. These windows show an observed 60-tick count in one interval, but the tail latencies exceed the 16.67 ms budget, so consistent 60 Hz behavior is not established. Recent client windows were roughly 54–92 FPS, with p95 frame times around 25–48 ms and many frames over 4.167 ms; 240 FPS is not established.

No `performance.cpu-callsite-window` rows were present in the inspected server tail. `game.hotness.summary.probe=true` is the HostRuntime DLL-dispatch probe (`src/network/server.cpp:203-207`), not the NPC callsite CPU probe. The latter emits only sampled rows from `src/perf/perf-frame.cpp:318-322`.

## Source path and next experiment

`NpcSystem::updateOneNpc` is the phase aggregator at `src/npc/npc.cpp:1576-1580` and `4376-4438`. It copies `PhysicsStageCost::collisionMs` at line 3636. The collision timer surrounds the loop invoking `doCollisions` in `src/physics/physics-mini.cpp:397-404`; the callsite probe is at `src/physics/movement/physics-collision-dispatch.cpp:170-181`. Because the phase numbers are wall time and the current NPC module is stale relative to the host DLL, change selection still requires a clean matched profile.

I sent this exact evidence to `root-resource-loop` and `root-mode3-latency` with `issue-ed3ac775283d` attached. Wait for an owner-released connected Juggernaut Horde pair whose host DLL and NPC module generations match. Then take a bounded host/process CPU and memory sample and run the existing bounded callsite probe; require actual callsite rows. If `doCollisions` is a repeatable large share of server thread CPU, make one collision-focused source change, request one CPU-focused DLL build, and compare the same workload's whole-host/process CPU, callsite cycles, frame times, tick rate/latency, and gameplay. If the function probe remains unavailable, fix its supported command path before optimizing by guesswork.

Related issue: `issue-ed3ac775283d`. Build-latency context remains cross-linked in `issue-136d4553ff92`. No current resource sample is aligned to these 23:05 phase events, so prior generation627 build-window CPU/RAM figures must not be combined as if simultaneous.

## Follow-up: two-pair host/process sample

I took one passive 15-row sample at 2026-11-24 23:19:44.796–23:20:00.815 UTC using known PIDs only (no process enumeration per row, no game command, restart, or build). Artifact: `.dev/perf-captures/cpu-live-four-mimita-20261124T2313Z.csv`. The machine had 12 logical CPUs. Host CPU averaged 99.89% and peaked at 100%; RAM load averaged 95.67% and available RAM averaged 621.4 MiB. The root dev-loop status before/after the sample showed no active build; its latest game-DLL attempt had failed changed-source preflight before the sample.

| PID | Role and run | CPU of host | CPU of one logical CPU | Working set | Private bytes |
|---:|---|---:|---:|---:|---:|
| 27372 | root server, `20261124_221014`, FCS/19 NPC/1 player | 12.99% | 155.88% | 85.5 MiB | 395.8 MiB |
| 26492 | root client, same run; current effective client mode not verified | 19.09% | 229.08% | 122.0 MiB | 891.2 MiB |
| 36224 | isolated server, `20261124_224314`, Bloodtest/1 NPC/1 player | 4.44% | 53.28% | 23.3 MiB | 107.3 MiB |
| 26536 | isolated client, same run; current effective client mode not independently verified | 8.88% | 106.56% | 105.9 MiB | 498.5 MiB |

MiMITA averaged 45.40% of whole-host CPU across the four known game PIDs: 32.08% for the root pair and 13.32% for the isolated pair. Combined sampled MiMITA working set averaged 336.7 MiB; combined private bytes averaged 1,892.8 MiB. Private bytes are committed memory, not resident RAM. The PowerShell sampler itself (PID 29788) averaged 2.12% host CPU, which is material at this short interval and must be counted; the remaining host CPU was not attributed to specific competing processes. This is direct evidence that the game processes were a substantial concurrent CPU load during this saturated, memory-pressured sample. It does not isolate their causal effect on subjective responsiveness or prove a single hot function.

The root server journal was fresh through 23:20:36 UTC and still showed host DLL g627 versus NPC module g625, 19 NPC/1 player, one 61-tick window with 12.26 ms mean total, 37.71 ms p95, and 79.87 ms max; adjacent NPC phase time showed 89.87 ms collision, 113.61 ms physics, 211.61 ms total over its approximately one-second window. The second server journal at 23:12:38 UTC showed Bloodtest/1 NPC/1 player and host/NPC module g627; it is a different, light workload. Client frame journals were not confirmed fresh/aligned to the 23:19–23:20 sample, so do not use earlier FPS rows as same-window proof.

The isolated run's `run.json` identifies `parallel_agent_test` but has empty `agent` and `source_validation.requested_agent`; its launcher parent PID 36984 had exited while server PID 36224 and client PID 26536 remained alive. Root dev-loop did not list this pair. This confirms an ownership/registration gap, not who started the run. I replied to `hot-reload-latency-root` and cross-linked the issue discussion on `issue-b9edd402e658`; I did not send commands to or stop the unowned processes. This extra pair makes a one-pair CPU baseline invalid until it is owner-attributed or safely released.

Root build attempt `game-dll-20261124T230934953855Z-9ace92` failed at 23:11:10.526 UTC after 95.572 seconds; the latest dev-loop status reports changed-source preflight failure for `src/engine/engine-tick-camera.cpp` after 77.702 seconds and retains generation 627. It included no CPU-focused change and is not an optimization iteration. No function-level CPU probe rows were collected in the 23:19–23:20 sample.

## 00:28 UTC liveness recheck

At 2026-11-25 00:28 UTC, root dev-loop still reported no active build, the same preflight failure, and only the registered root pair (PIDs27372/26492) for run `20261124_221014`. The four MiMITA PIDs were nevertheless still alive: root server/client plus unregistered PIDs36224/26536 from `20261124_224314`. A point-in-time `Get-Process` snapshot showed combined working sets of about347MiB and combined private bytes about1.91GiB; these are memory snapshots, not a new CPU-rate sample. Root run remains FCS with 19NPC/1 player; the extra run's server journal last directly confirmed Bloodtest/1NPC/1player and g627 host/NPC module. No newer client mode or frame/tick alignment was established. Root-mode3-latency asked for CPU file ownership; I confirmed I own no C++/build paths and will not alter or command either pair.
