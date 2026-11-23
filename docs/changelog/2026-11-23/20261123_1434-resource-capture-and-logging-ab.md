# MiMITA resource capture and logging A/B

## Scope

- Continued the resource investigation on registered runtime `mode3-latency-run`, run `20261123_135745_mode3-latency-run`.
- No C++ source was changed and no build or WPR capture ran.
- Preserved the live server PID 24692 and client PID 23980 throughout the config experiment.
- Temporary changes to `config/debuglogger.json` were restored byte-for-byte; SHA-256 is `2161a60e71309ce0b0e34c58b478e87a7e62f8ef0b557d5e2e2838a4ef0e5152`.

## Matched live workload

At 2026-11-23 14:10:44.323–14:11:45.454 UTC, the runtime journals confirmed `jugger_horde`, map `dust2cyberiav4`, 67 NPCs and 1 player on a 12-logical-CPU host.

- Host CPU averaged 45.16%; memory load averaged 59.40%, with 6.353 GiB available.
- Server PID 24692 used 119.19% of one logical CPU, or 9.93% of whole-host capacity; client PID 23980 used 218.00% of one CPU, or 18.17% of host capacity. Together they used 28.10% of host capacity.
- The server had 15 threads and client 37. Multiple threads consumed CPU in each process; the simple claim that each executable claims exactly one core is false. This does not mean processes are pinned to one core.
- Client rendered 10,379 frames; 8,159 (78.61%) exceeded the 4.167 ms 240-FPS budget. Mean window FPS was 169.58, mean window p95 frame time 8.740 ms, and the maximum frame was 52.219 ms.
- Server completed 3,668 ticks across 60 windows (about 60 ticks/s). Nineteen windows had p95 tick time above 16.667 ms; the maximum retained tick was 42.789 ms. No tick exceeded 100 ms in the retained samples.
- NPC phase totals averaged 334.25 ms/s: physics 151.37 ms/s (including collision at 119.80 ms/s), decision 69.11, path 30.46, and sensing 22.95. Collision is nested in physics and must not be added to it.
- The paired processes wrote 61.47 MB. Their journals recorded 40,300 server rows / 30.87 MiB and 11,169 client rows / 13.37 MiB. This is a strong I/O volume lead, not proof of CPU attribution.
- NVIDIA pmon sampled the client at 18–32% SM, mean 24.3%. Top competing processes and per-process network counters were not collected.

## Logger source trace

- `config/debuglogger.json` enables hot reload and sets several event-heavy categories to `important`.
- `src/debug/structured-log.cpp:1275` opens the events stream unbuffered (`_IONBF`). `writeJsonLine` serializes each row, takes writer and journal locks, reserves the append, and calls `fwrite` for each row around lines 1723–1781.
- Ordinary `writeEvent` requests `fflush` for IMPORTANT rows around lines 1579–1581. Because the stream is unbuffered, removing only this flush is unlikely to remove the immediate per-row write and is a low-value change without evidence.
- `tryWriteEvent` uses a non-flushing path and `StructuredLogger::tick` asks for a bounded flush at most every 100 ms (around lines 1622–1625 and 1942–1955).
- The generation 560 source manifest marks this file as `resident`, not a game-DLL target. A source change would require a root dev-loop EXE build and restart, so no such build was attempted.

## Same-PID config A/B

Selected high-volume categories were temporarily lowered from `important` to `errors`: NPC movement/combat, collision, weapons, avatar, grenade launcher, rendering, and network. Performance and general categories stayed `important`.

| Window (UTC) | Host CPU | Server CPU (core / host) | Client CPU (core / host) | Available RAM | Process write bytes (server + client) |
|---|---:|---:|---:|---:|---:|
| A, 14:26:22–14:26:46 | 67.20% | 128.24% / 10.69% | 222.97% / 18.58% | 5.35 GiB | 10.55 + 7.37 MB |
| B, 14:27:40–14:28:05 | 73.32% | 124.31% / 10.36% | 220.04% / 18.34% | 4.52 GiB | 0.92 + 8.29 MB |
| C, restored, 14:32:08–14:32:32 | 74.29% | 149.70% / 12.47% | 240.27% / 20.02% | 2.36 GiB | 14.38 + 10.30 MB |

A/B evidence:

- Server journal rows fell from 12,486 to 319 (97.4%); server process write bytes fell 91%. Pair process writes fell from 17.92 MB to 9.21 MB (49%).
- The client did not apply the category change: its B interval still contained 3,420 rows, including AVATAR, NETWORK, and RENDERING IMPORTANT events. After restoring the original config, server rows returned to 16,503 and client rows to 4,773.
- CPU is inconclusive: A-to-B server CPU fell about 3.1% and client about 1.3%, while host CPU rose 6.12 percentage points and available RAM fell. A/B client mean FPS worsened from 101.17 to 85.72; window p95 frame time rose from 18.29 to 27.714 ms. Server tick averages were 12.469 and 12.295 ms, but both windows included ticks over 100 ms.
- The restore window had substantially higher process CPU and lower available RAM, confirming changing machine pressure. Do not claim the config change reduced gameplay CPU or apply it globally.

## Artifacts and follow-up

- Resource capture: `.dev/perf-captures/mode3-resource-crossdomain-20261123T141044Z.csv`, matching JSON, thread JSON, and GPU pmon text.
- A/B process counters: `.dev/perf-captures/mode3-logger-ab-A-baseline.csv`, `mode3-logger-ab-B-reduced-categories.csv`, and `mode3-logger-ab-C-restored.csv`.
- Exact original config copy: `.dev/perf-captures/mode3-logger-ab-original-debuglogger.json`.
- Runtime journals: `logs/2026-11-23/20261123_135745_mode3-latency-run/{server-01,client-01}`.
- Evidence was appended to resource issue `issue-ed3ac775283d` and storage issue `issue-3f776ce60511`.
- Next: investigate why client config hot reload did not match the server; repeat only under a controlled host-load window, including top competing process attribution. Keep the per-function NPC and collision investigation separate from logging I/O and verify frame/tick behavior after any optimization.

## Mixed-runtime CPU sample before function probe

At 2026-11-23T15:06:38.252Z–15:06:59.489Z, a bounded 16-interval sample covered 21.237 seconds on a 12-logical-CPU host. No root build was active or pending immediately before the sample.

| PID / role | CPU per logical CPU | Whole-host CPU | Working set | Private bytes |
|---|---:|---:|---:|---:|
| 25068 / client, run 20261123_144429 | 200.23% | 16.68% | 561.5 MB | 508.6 MB |
| 27384 / server, run 20261123_144429 | 94.18% | 7.85% | 309.9 MB | 307.4 MB |
| 24692 / server-only, run 20261123_135745_mode3-latency-run | 124.20% | 10.35% | 143.7 MB | 334.4 MB |

The three MiMITA processes totaled 34.88% whole-host CPU. Host CPU averaged 93.35%; available RAM averaged 1,961 MB and bottomed at 1,921 MB. The current process usage is time/workload dependent; this sample does not identify the remaining host consumers. The first client/server pair belongs to Juggernaut Horde on dust2cyberiav4; nearest journal summary after the sample confirms 19 NPCs and 1 player. The older server is a separate 67-NPC, 0-player run according to its latest journal summary. Those workload summaries do not fall inside the exact 21-second interval, so they are nearby corroboration rather than exact-window proof. Artifact: .dev/perf-captures/cpu-preprobe-mixed-runtimes-20261123.csv. Top competing processes, exact in-window FPS/tick metrics, and sampler overhead were not collected.

This sample also disproves the literal “each MiMITA process claims one core” explanation: the client averaged 2.00 logical cores and one server averaged 1.24 logical cores. CPU values are normalized to 12 logical CPUs for host percentages.

## Opt-in multi-callsite CPU probe (source changed; build/runtime proof pending)

The existing single-site NPC sample is being extended into five fixed, opt-in buckets: NpcSystem::updateOneNpc, doCollisions, engineTickSimulate, its engineTickNet networking call, and engineTickRenderFrame. Each site samples one in 64 calls using QueryThreadCycleTime; each one-second report also reads whole-process CPU time with GetProcessTimes. It emits per-site estimated CPU milliseconds and one-core/whole-host percentages, plus exact source path, line and function. Estimates are explicitly marked as estimates because cycle sampling is scaled and a site's scope may nest inside another. The command starts the local process probe and forwards the bounded request to the remote server using the existing command route. The default remains disabled.

The five directly edited DLL translation units are game-owned per the current source-ownership manifest. The dedicated probe header is included by six translation units, including the existing server command parser in src/network/server-packet-chat.cpp, so six TUs are expected to compile from header fanout. Build note 4b69fedaff24 is queued. The source was saved before that note was queued, which is a coordination-procedure miss; record it honestly and keep this note attached to the upcoming build. No build has been sealed and no candidate has been compiled or activated yet. Runtime proof still must show the probe disabled path emits no rows, then show both client and server performance.cpu-callsite-window events for the same bounded interval.

## Fresh pre-build sample correction and generation 561 status

The actual sampled rows in `.dev/perf-captures/cpu-current-preprobe-20261123T1520Z.csv` span 2026-11-23 15:20:21.986–15:20:39.568 UTC (18 samples; 12 logical CPUs), not the script's wider start/end interval. Host CPU averaged 84.15%; available RAM averaged 2292 MB (minimum 2241 MB). PID20424 (client) averaged 194.52% of one logical CPU / 16.21% host; PID27384 (registered server) 97.26% / 8.11% host; PID3640 another MiMITA process 97.26% / 8.11% host. The intended pair was 24.32% host, all three were 32.43% host. PID3640 is an unregistered server-like process parented by replaced client PID25068. It was configured for slaughterhouse1/team_deathmatch/one NPC, and its post-sample journal event at15:21:32 reports 9 NPC/0 players. Its exact workload during the sample is unknown; it was not the stopped 67-NPC server PID24692. Server27384's nearest journal summaries (15:19:46 and15:22:16) report Juggernaut Horde, dust2cyberiav4,19 NPC/1 player, but no workload event fell inside the sample. This correction was appended to CPU issue `issue-ed3ac775283d` before handing off the build hold.

The coordinated root build `game-dll-20261123T152455729876Z-62f5f6` published game DLL generation561 (source generation10). It compiled 7 units and skipped636; changed-source preflight50.972s, aggregate TU compile time81.883s, LLD link9.646s, publish0.415s, total107.9998s. The seven compiled units were the six expected probe/header-fanout TUs plus `generated/module-identity-provider.cpp`. Server PID27384 logged activation of generation561 at15:26:43.984Z. Client activation and probe-command delivery remain unverified: the command `perf_callsite_cpu on 15` was submitted once and dev-loop status became `command_queued`, but the only client journal (`client-01/events.jsonl`) is capped at99,999,780 bytes and last wrote at15:21:32Z; no `client-02` journal exists. Do not repeat the command or claim probe success until the Mode3 command queue/result is inspected and the current client/server logs provide proof. No post-activation resource sample or callsite rows have been confirmed yet.

## Post-generation 561 resource sample and probe rejection

The one probe request was delivered to server PID27384 but rejected at15:28:43.252Z: active mode `funcounterstrike`, player2, `is_host=false`, admin disabled/ungranted, status `rejected: no admin access`. Do not repeat the command or bypass the existing admin gate. This shows the launch-mode label alone does not prove current mode. Latest near-window server hotness at15:34:33.324Z reports generation561, FCS,19 NPC/1 player, but no workload event landed inside the subsequent sample. PID3640's last available journal at15:16:08.616Z reports 9 NPC/1 player, not its exact workload during the sample. The client journal is capped/stale; client FPS and server tick evidence are unavailable.

Post-generation no-build sample: `.dev/perf-captures/cpu-postgen561-20261123T1535Z.csv`, 25 samples from15:35:00.404–15:35:14.778Z UTC,12 logical CPUs. Host CPU averaged99.91%; RAM used89.12%; available RAM averaged1,828MB (minimum1,758MB). Client PID20424 averaged361.92% of one core /30.16% host with653.7MiB mean RSS; server PID27384 averaged173.24% /14.44% host with138.7MiB RSS; registered pair44.60% host. Extra MiMITA server-like PID3640 averaged133.38% /11.12% host with184.8MiB RSS; all three totaled55.71% host and about977MiB RSS. SnapOS64 PID7384 was the largest observed competitor at9.64% host. The sampler used4.42% of one core (~0.37% host) and took73.88ms per process snapshot on average; these costs and host saturation limit precision. Evidence was appended to CPU issue `issue-ed3ac775283d`; no callsite CPU attribution was obtained.

## Why server callsite probe did not start

The rejected request follows the existing admin boundary: `src/network/server-packet-chat.cpp:790-800` applies `serverAdminAccessEnabled` and returns before the `npc_cpu_probe` handler at line804. `src/terminal/debug-commands.cpp:584-610` forwards `perf_callsite_cpu` through the existing server command packet. `admincmds` self-toggle at `server-packet-chat.cpp:451-453` is available only to a host or an already-granted player; grant/revoke at lines501-520 is host-only. The observed sender was player2, `is_host=false`, and ungranted. No admin bypass was introduced. Runtime function-level CPU attribution is still unavailable; next proof requires an already authorized host to grant access for the bounded sample, or a separately designed/reviewed dev-only profiler authorization.

## GPU point sample

At 2026-11-23 15:53:56.762–15:53:59.578 UTC, nvidia-smi queried the NVIDIA GeForce RTX 4050 Laptop GPU: 31% GPU utilization, 3% memory-controller utilization, 1,887/6,141 MiB VRAM, 13.61 W. Two pmon samples listed client PID20424 as C+G but did not provide its SM/MEM percentages; dedicated server PIDs27384 and3640 were not listed in those per-process rows. OBS PID19408 showed SM29%, memory2%, encoder6%. The device-wide 31% cannot be attributed to MiMITA from this capture; per-process MiMITA GPU usage remains unknown. Same live run20261123_144429; exact mode/workload and client FPS during this short sample were not verified. No build overlapped. Values are also appended to CPU issue issue-ed3ac775283d.

## Generation 563 no-player 19-NPC sample with server tick telemetry

The 12-second native process sample `.dev/perf-captures/cpu-gen563-no-player-20261123T1604Z.csv` spans16:04:51.381–16:05:03.139Z with12 logical CPUs. Host CPU averaged99.80%; available RAM1.95GB; memory load88.41%. PID20424 used25.31% whole-host CPU (303.74% one core,279MiB RSS), server PID27384 13.71% host (164.55% one core,129MiB RSS), and extra server-like PID3640 13.37% host (160.49% one core,86.8MiB RSS). The three totaled52.39% host. SnapOS64 PID7384 averaged9.58% host. Sampler overhead averaged4.57% one core (~0.38% host), with63.3ms snapshot work per0.5s interval.

Same-window server journal rows confirm actual mode funcounterstrike, map dust2cyberiav4, game generation563,19 NPCs,0 players. The client process remained alive but was not connected; do not treat this as a connected-pair or active-player run. Twelve server-loop windows averaged58.95Hz (55.18–61.58);9/12 fell below60Hz, with170 overruns and23 capped catch-ups. Twelve server-tick windows covered724 ticks: mean total7.95ms, mean of per-window p95 28.38ms, max p95 47.59ms, max individual tick112.38ms, one tick over100ms. The 10ms NPC phase budget was exceeded on120 ticks across12 windows, with max NPC phase106.49ms. Two npc.tick-cost reports averaged70.11ms total NPC work,67.99ms NPC AI,66.55ms ai_update, and worst actor68.68ms. The related server source owner emits this report in `src/network/server-npcs.cpp:3375`; the sampled callsite probe is placed in `src/npc/npc.cpp:1549-1550` around `NpcSystem::updateOneNpc`. These scope timings identify NPC AI as the leading measured hotspot but are elapsed time, not per-function CPU shares. FPS and client mode are unavailable because the client journal is capped/stale. Evidence is in CPU issue `issue-ed3ac775283d`.

Derived ranking from the exact no-player 19-NPC interval: eight npc.server-phase-window rows summed 2,092.96ms of AI time out of 2,452.53ms total (85.34% of logged NPC phase elapsed time). Target selection was19.62ms (0.80%); recovery51.91ms; pair41.47ms; rebuild53.95ms. This prioritizes per-NPC update/AI for function-level CPU sampling, but is not a process CPU percentage. Keep the bounded callsite probe's estimated per-function CPU separate from these phase-scope measurements.
