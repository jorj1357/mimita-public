# CPU collision-cache A/B: first result

Date: 2026-11-25 UTC
Goal: reduce MiMITA process CPU/resource use through one evidence-selected code change per optimization build while preserving gameplay, 240 FPS frame time, and 60 Hz server behavior.

## Hypothesis and change

Hypothesis: combining the exact-cache and containing-superset cache lookup into one scan in `src/physics/movement/physics-collision-glb-setup.cpp` would reduce server collision time by at least 5% under the same 67-NPC workload, without CPU, frame, tick, or behavior regression.

Only that C++ file changed. Build note `2fd9e43be6ce` was queued before the edit. Root dev-loop attempt `game-dll-20261125T075134481455Z-3013f3` succeeded and published generation 641 in 28.688 s. The build compiled the changed source plus generated identity-provider source and skipped 643 translation units. Attempt summary: `.dev/build-timings/game-dll-20261125T075134481455Z-3013f3.json`.

## Before and after

Pinned run `20261125_061355`: server PID 17988 and client PID 13872, Juggernaut Horde on `dust2cyberiav4`, 67 NPCs and one player. Before sample `.dev/perf-captures/gen640-cache-lookup-before-20261125T073947Z.csv` ran 07:39:49–07:40:04Z (16 samples): host CPU 57.00%; pair 28.81% whole-host (server 10.61%, client 18.20%); available RAM mean 5793 MiB. Server NPC phase collision averaged 152.985 ms/window, physics 192.593 ms; server tick average was 11.850 ms. Client recorded 1328 frames and all 1328 exceeded 4.167 ms.

After sample `.dev/perf-captures/gen641-cache-lookup-after-20261125T075454Z.csv` ran 07:54:56–07:55:11Z (16 samples): host CPU 83.32%, pair 56.73%, available RAM mean 3988 MiB; server/client working sets averaged 1489/766 MiB. Collision averaged 161.249 ms/window, physics 200.277 ms, and server tick average 16.736 ms.

Repeat `.dev/perf-captures/gen641-cache-lookup-after-repeat-20261125T0758Z.csv` was intended as 15 seconds but took 32.372 seconds (16 rows; slow counter calls). From 07:58:32–07:59:04Z: host CPU 85.87%, pair 59.37%, available RAM mean 2587 MiB; server/client working sets averaged 2971/770 MiB. Collision averaged 149.093 ms/window, physics 184.179 ms, and server tick average 19.070 ms. The exact server interval journal is in `logs/2026-11-25/20261125_061355/server-01/events-0052.jsonl`: 67 NPCs/one player and host DLL generation 641. `npc.phase-window` still reports NPC behavior module generation 640. Client frame rows were not found in the post windows.

Automatic build resource sidecar `.dev/build-timings/game-dll-20261125T075134481455Z-3013f3.resource.json` reports 19 samples, host CPU mean 66.489%, and minimum available RAM 4,952,150,016 bytes. It lacks matched runtime alignment. The manual samples used host counters and CPU-time deltas for known PIDs, with no process enumeration; top competing processes were not captured.

## Decision and next step

The measured CPU result is inconclusive, not a win or proven regression. Pair CPU nearly doubled while host CPU and memory pressure changed substantially. Collision time across the two after windows averaged about 155.17 ms/window, about 1.4% above baseline and below the pre-set 5% improvement threshold. Tick performance also worsened in the post windows; this cannot be assigned to the code change. The server host DLL is generation 641, but the NPC behavior module remains generation 640, so exact call-path execution evidence is incomplete.

Do not stack a second optimization on this candidate yet. First resolve which generation executes the collision path and obtain a comparable post sample with fresh server and client frame journals, stable host load, and top competing-process attribution. Then decide keep/revert from the same pre-registered threshold. Evidence was appended to CPU issue `issue-ed3ac775283d` and cross-linked to build-latency issue `issue-136d4553ff92`.
