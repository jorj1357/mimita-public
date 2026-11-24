# Matched Juggernaut CPU profile and source leads

- Session ID: work-9a3fea65a1c5
- Owner: resource-efficiency-goal
- Focus: Collect matched process/frame/tick evidence and coordinate function-level follow-up
- Status: partial; no game source optimization made
- Date: 2026-11-23

## Matched no-build sample

Artifact `.dev/perf-captures/mimita-pair-20261123T2051Z.csv` covers 2026-11-23 20:52:05.334–20:52:23.042Z (17.708s,34 rows), with 12 logical CPUs. Root dev-loop reported no active build and game-build idle. Host CPU averaged97.188% (89.4–100); host RAM used averaged79.06%, available RAM averaged3.28GiB and bottomed at3.22GiB. Process totals: server PID22184 averaged24.14% whole-host CPU (289.68% of one logical CPU), RSS678.3MiB; client PID32832 averaged30.44% host (365.28% of one logical CPU), RSS287MiB; pair54.58% host and about965MiB combined working set. Root Bloodtest server PID10164 added3.73% host. The sampler used about0.267% host CPU on average (3.208% of one core), with28.88ms mean collection work per 500ms interval. Other sampled processes included SnapOS64 PID7384 at8.84% host, PowerShell PID31596 at5.42%, and pythonw PID16864 at4.90%; their ownership and activity are unknown.

Exact workload is confirmed within the sample. Run `20261123_195307` is recorded as Juggernaut Horde on `dust2cyberiav4`; in-window `gamemode.actor-life` events name `jugger_horde`. Seventeen server hotness rows show67 NPCs/1 player and host game DLL generation571. Seventeen NPC phase rows still say `module_generation=570`, so the client/server pair is not generation-aligned at the NPC module boundary. The server log segment is `logs/2026-11-23/20261123_195307/server-01/events-0016.jsonl`; the client journal is `logs/2026-11-23/20261123_195307/client-01/events.jsonl`.

## Frame and server-tick results

The server emitted17 tick windows: total tick time averaged20.92ms, per-window p95 averaged43.21ms, p99 averaged63.69ms, and maximum91.25ms; NPC work averaged19.67ms. Server loop actual rate averaged46.71Hz (38.99–57.47) with about9.76 overruns per window. These measurements miss the60Hz target.

The client emitted17 FPS windows, averaging41.67 FPS (29.75–58.3). All718 of718 recorded frames exceeded the4.167ms budget; mean frame time was24.17ms and the average of window p95 values was41.93ms. Twelve resource windows reported main-thread CPU averaging17.14ms. GPU timer queries were disabled in all12 and had0 GPU samples; whole-GPU utilization and VRAM remain unknown. Disk/process-I/O and network byte-rate are not covered by this capture. The active workload is not an idle/background acceptance run, so the <=1% idle CPU and <=500MB idle pair targets were not evaluated.

## Source-resolved leads and limits

Client frame-budget elapsed scopes ranked `Rendering` self time at5.39ms on average (`src/engine/engine-tick.cpp:254`), `Rendering::Actors::NetworkEntities` at4.86ms (`src/engine/engine-tick-render.cpp:659`), and `Network::RemoteEntities` at3.21ms (`src/network/multiplayer-tick.cpp:3565`). These are elapsed frame-scope values, not CPU-time percentages. They identify where to add calibrated thread-cycle attribution, not proof of CPU shares.

Five sampled server `npc.tick-cost` events reported `npc_ai_ms` mean57.09ms/max70.42 and worst single-actor AI time mean12.63ms/max16.63; the emitter is `simulateSharedNpcs` at `src/network/server-npcs.cpp:3421`. The84 sampled behavior decision calls at `src/npc/npc-behavior-module.cpp:219` averaged0.003ms and peaked at0.0351ms each, so that leaf call is not the evident cause of the broad NPC cost.

One thresholded NPC collision outlier at20:52:07.008 logged11.845ms total, with10.125ms in sweep-slide,26 initial candidates and5,465 world triangles. The diagnostic is emitted at `src/physics/movement/physics-collision-glb-main.cpp:177`; the stage calls `doGLBSweepSlide` at:312 and its implementation begins at `src/physics/movement/physics-collision-glb-sweep-slide.cpp:71`. It is one slow sample, not a typical or whole-process CPU share. The source marks this legacy sweep as superseded but explicitly protects step-up and requires human gameplay checks before deletion; no removal is proposed.

## Handoff and next step

The complete measurements, source leads, and generation mismatch were appended to CPU issue `issue-ed3ac775283d` and sent to the active Mode3 latency owner for cross-linking with `issue-136d4553ff92`. The next step is a bounded, non-overlapping thread-cycle profile on a generation-aligned pair, alongside the already-running native process sampler. Then optimize one owner at a time and repeat the same workload with live60Hz server and240FPS client proof. No build, runtime restart, or source change was performed by this session.
