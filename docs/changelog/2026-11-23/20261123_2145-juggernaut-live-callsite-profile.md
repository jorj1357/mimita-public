# Juggernaut live callsite profile and host sample

- Session: `work-5e0d892f368d`
- Date: 2026-11-23
- Scope: bounded, read-only runtime resource measurement; no source edits, builds, or process restarts.

## Workload and identity

The isolated run `20261123_195307` was live on server PID `22184` and client PID `32832`. The server journal reports Juggernaut Horde on `dust2cyberiav4`, 67 NPCs and one player. The client reported one player, but its frame-slice NPC count was zero, so the client-side NPC count is not independently corroborated. Server hotness reported DLL generation 571 while `npc.phase-window` reported module generation 570. This is not a same-generation proof run.

## CPU and memory samples

The paired baseline files `.dev/perf-captures/runtime-attribution-20261123T2137Z-{host,server,client}.csv` cover 2026-11-23 21:37:28.074–21:37:43.828 UTC. There were 12 logical CPUs. Host CPU averaged 97.63% and peaked at 99.88%. Server averaged 301.99% of one logical CPU (25.17% of total host capacity); client averaged 354.73% (29.56% of host); together they averaged 54.73% of host capacity. Available physical RAM averaged about 3.98 GB and bottomed at 3.91 GB; memory load was 75–76%.

A separate 3.09-second post-probe snapshot is `.dev/perf-captures/runtime-attribution-top-20261123T2143Z.json`. Host CPU was 97.64%. The top measured processes were the client at 25.55% host, server at 22.26%, separate idle Bloodtest server PID 10164 at 3.41%, SNAPOS64 at 8.00%, OpenCode at 4.00%, and pythonw at 3.70%. The pair's working sets totaled about 0.69 GB and private bytes about 3.43 GB. This short top-process snapshot is after the callsite probe and is not build-pressure evidence.

## Bounded callsite probe

The existing probe was accepted at 21:37:51.122 UTC and auto-stopped after 15 one-second windows, ending 21:38:06.094 UTC. Server estimates averaged `updateOneNpc` (`src/npc/npc.cpp:1580`) at 12.40% of server-process sampled cycles, equivalent to 3.04% of whole-host CPU or about 365 estimated CPU-ms per one-second wall window. `doCollisions` (`src/physics/movement/physics-collision-dispatch.cpp:181`) averaged 5.33% of server-process cycles, 1.31% of host or about 156.5 estimated CPU-ms per second.

Client estimates averaged `engineTickRenderFrame` (`src/engine/engine-tick.cpp:242`) at 7.75% of client-process cycles / 2.21% host; `engineTickSimulate` (`src/engine/engine-tick.cpp:112` and `:222`) at 9.85% / 2.81% host and 6.79% / 1.94% host respectively. The run journals contain `performance.cpu-callsite-window` rows for both processes. The probe estimates use 1/64 sampled thread cycles; sites may run on multiple threads and their scopes may overlap. They identify worthwhile owners to inspect, not a complete partition of CPU or proof that one function consumes nearly all host CPU.

The follow-up command `admincmds 0` completed at 21:43:11.229 UTC with `admin_enabled=false`; the bounded probe is off and the capture window has been released to the DLL-latency owner.

## Runtime performance and next step

Adjacent server timing summaries during the profile show roughly 17–30 ms mean tick-stage duration and p95 examples around 38–64 ms, above the 16.67 ms budget for 60 Hz. Client frame slices just after the profile show 13.8–26.8 ms average frame times and 28.5–72 ms p95; these are adjacent samples, not exact probe-overlap proof, and miss the 4.167 ms budget for 240 FPS. Do not call either target satisfied.

The strongest actionable CPU lead is NPC update cost, with collision as a smaller server lead; client render and simulation scopes also contribute. Next, split `updateOneNpc` into non-overlapping sampled sub-scopes, especially decision/navigation, physics/collision, and late behavior work; preserve call counts and host normalization. Then change one hotspot at a time and compare matched same-PID/same-generation runs at the same mode, NPC/player counts, and frame/tick workload. Require 60 Hz server, per-frame 240 FPS, and gameplay proof before accepting a CPU reduction. The largest remaining part of saturated host CPU is outside these measured function scopes and needs separate system/process profiling.

Evidence was appended to CPU issue `issue-ed3ac775283d` and shared with the DLL-latency owner. This is runtime CPU attribution, not build-latency evidence.
