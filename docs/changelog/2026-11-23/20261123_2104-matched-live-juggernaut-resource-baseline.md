# Matched Live Juggernaut Resource Baseline

**Date:** 2026-11-23 21:04 UTC  
**Agent:** hot-reload-latency-root  
**Goal:** Continue the coordinated Mode 3 edit-to-live latency investigation with a workload-matched no-build CPU/RAM baseline.

- Task ID: `work-04185414cbf9`
- Session ID: work-04185414cbf9
- Owner: `hot-reload-latency-root`
- Focus: matched live Juggernaut CPU/RAM baseline and build readiness
- Status: partial; source-owner readiness and a matched build sample remain pending

## Evidence

- Sampled for15.078s using the native Windows host/process provider loaded by the root dev-loop's Python 3.14 environment when `psutil` is unavailable.
- Artifacts: `.dev/build-timings/mode3-prebuild-native-20261123T2102Z.resource.jsonl` and `.dev/build-timings/mode3-prebuild-native-20261123T2102Z.resource.json`.
- Provider `windows_kernel32_host_and_process`;14 samples; no errors; setup31ms; cumulative process-discovery wall128.78ms; sampler-thread CPU78.125ms.
- Host:12 logical CPUs; CPU mean99.734%, peak100%; available RAM mean3262.5MiB, minimum3210.1MiB; pagefile used mean27.98GiB.
- Exact journal-aligned runtime: run `20261123_195307`, Juggernaut Horde on `dust2cyberiav4`, server PID22184/client PID32832, generation571,67 NPCs/1 player. Mean server/client usage22.60%/25.69% of whole-host CPU (48.29% combined); root server PID10164 2.96% host.
- Other sampled CPU leaders: SNAPOS64 PID7384 8.32% host, pythonw PID16864 3.80%, Code PID3304 2.78%, OpenCode PID11528 2.24%. No compiler/linker was present.
- The client logged64 frame-slice events and52 `anim.npc.ticklocked` events while its frame-slice NPC count remained0; the server's exact-window hotness summaries report67 NPCs. Preserve this counter mismatch as a telemetry limitation.
- A prior manual sample using the interactive `psutil` provider and a2s discovery cadence took32.125s for a15s request, including13.625s setup and25.831s cumulative discovery. It was not the root daemon's provider. The native provider above is the relevant baseline for future build comparisons.

## Interpretation and Next Step

This establishes a no-build baseline under a saturated host with the intended live workload; it does not establish that the game pair caused all host CPU use or predict build slowdown. The next root dev-loop build must capture compiler/linker process rows with the same native provider and preserve this runtime workload. The latency target remains unmet.

## Validation

No build, compiler, game restart, or runtime command was issued. The process and runtime journals remained active during collection. The issue tracker contains the detailed direct observations and caveats.
