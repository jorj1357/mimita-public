# Gen571 build pressure and post-build multi-process evidence

- Session ID: work-83982f978167
- Owner: resource-efficiency-goal
- Focus: matched CPU/resource evidence, attribution quality, build and header fanout coordination
- Status: partial; no optimization change made
- Date: 2026-11-23

## Build window

Root dev-loop build attempt `game-dll-20261123T200924195125Z-7401c3` published generation571. It ran 20:09:24–20:23:13 UTC (829.795s), with 642 source units, 271 compiled and 372 skipped. Changed inputs were seven `.cpp` files and `src/config.h`. Worker stage wall times: compile754.586s, changed-source preflight46.412s, LLD link12.657s, object-cache scan2.052s, publish1.009s. Slowest changed/compiled units included `src/audio/audio.cpp`89.075s, `src/network/server-gamemode.cpp`85.368s, `src/mapeditor/map-editor.cpp`63.222s, and `src/ragdoll/ragdoll-mode.cpp`60.994s. This was a long compile on a heavily loaded machine; it does not prove resource contention caused the duration.

Sidecar `.dev/build-timings/game-dll-20261123T200924195125Z-7401c3.resource.jsonl` contains613 samples from20:09:25–20:23:13Z. Host CPU mean99.986%, max100%; RAM used mean92.6%, peak99%; available RAM mean1,107MiB and minimum651,264 bytes. At20:20:27Z a sample recorded 649MiB available and pagefile35.64/38.89GiB used/total. Sampler thread CPU totaled2.078s over about13m50s. Crucial gap: provider was `windows_kernel32_host_only`; tracked/top processes were empty on every row because process discovery was stale by about17.5h. The sidecar explicitly marks tracked process metrics unavailable, so compiler-vs-game-vs-other CPU attribution is unknown. Exact run journal `logs/2026-11-23/20261123_192224/server-01/events.jsonl` shows a Bloodtest server with zero NPC/players and no client during this build. The latency owner was notified; they are handling the native per-process sampler fallback.

## Post-build process sample

The bounded sample was initially named `gen571-idle-server`; inspection found other game PIDs alive, so it was renamed without content changes to `.dev/perf-captures/gen571-postbuild-process-sample-20261123T2029Z.csv`. It covers20:29:04.631–20:29:19.232Z,15.09s,28 rows,12 logical CPUs, after the build and with no compiler process observed. Host CPU mean95.13% (80.4–99.5), available RAM mean4,443MiB/minimum4,210MiB, RAM load72.28%. Sampler overhead averaged4.126% of one core (~0.344% host); snapshot mean29.85ms and sample work mean34.07ms.

This was not a server-only sample. The requested target list named root server PID10164, but per-process rows revealed two other live MiMITA PIDs that had been labeled `other`: server22184 and client32832 from run `20261123_195307`. PID22184 averaged19.43% whole-host CPU (233.16% one core), RSS223.4MiB; PID32832 averaged31.59% host (379.08% one core), RSS283.0MiB. Their pair was51.02% host and about506.4MiB combined RSS. Root server10164 added3.62% host (43.44% one core), RSS23.2MiB. All three averaged54.64% host CPU. Other notable process means: SNAPOS64 PID7384 8.92% host, pythonw16864 5.21%, OpenCode30728 2.56%, Code12760 2.28%, ChatGPT2636 2.08%. This is direct process attribution for the sampled interval after PID/journal matching; it is not proof of what caused build latency.

No exact journal events were present for the20:29:04–20:29:19 sample interval, so exact in-window workload, module generation, frame/tick values remain unknown. Nearest after-sample journal events around20:30:08–20:30:11 show Juggernaut Horde, `dust2cyberiav4`,67 NPC/1 player, host game generation571 but `npc.phase-window.module_generation=570`. They are corroboration only, not exact interval alignment. A nearby `npc.server-phase-window` had29 ticks and721.55ms total, with591.97ms AI; `npc.phase-window` had2,202 updates and elapsed fields physics236.02ms, collision176.72ms, decision92.83ms, path42.74ms, sense28.93ms. These accumulated elapsed scopes may overlap and are not CPU percentages. The nearby authoritative tick window reported40 ticks, mean25.781ms, p95 46.747ms, max72.862ms; nearby client windows were about37.5–38.9FPS, all frames over4.167ms. Both acceptance targets were missed in those nearby records.

## Callsite attribution and next action

Earlier matched gen570 probe rows in the same run estimated `updateOneNpc` at22.9–42.5% of server-thread CPU and collision dispatch6.5–12.3%, based on 1/64 `QueryThreadCycleTime` scopes. This is useful ranking but not exact instruction hotspots; the broad scopes can overlap. Current source has the outer marker at `src/npc/npc.cpp:1579-1580`, elapsed phase aggregation at `src/npc/npc.cpp:4322-4399`, and server NPC aggregate emission at `src/network/server-npcs.cpp:3351-3373`. Next measurement should use non-overlapping thread-cycle subphase scopes or a verified CPU.light stack profile, with aligned journals and a same-PID before/after change. The profiler agent and NPC owner were asked to coordinate scope ownership. No additional build, runtime restart, WPR capture, or game-source edit was made in this session.

## Shared records

- CPU findings/corrections and process discovery gaps appended to `issue-ed3ac775283d`.
- Build stage/resource sample and telemetry gap sent to the root latency owner and appended to `issue-136d4553ff92`.
- Header fanout plan remains in `issue-196c3045b4cb`.
- Root latency owner confirmed the existing isolated pair should be preserved; this session did not stop or restart it.
