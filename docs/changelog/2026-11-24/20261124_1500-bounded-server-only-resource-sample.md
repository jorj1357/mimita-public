# Bounded server-only resource sample

## Work

- Recorded the 14:54 UTC passive sample result for resource investigation.
- Coordinated the exact sample with the Mode 3 latency owner through the CPU issue discussion. The owner appended the sample while holding the issue lease; after the lease was released, I appended the later point-in-time memory gate under my own lease.
- No source, build, or runtime changes were made.

## Evidence

- Artifact: `.dev/perf-captures/mode3-server-only-resource-20261124T1450Z.csv`.
- Interval: 2026-11-24T14:54:22.783Z–14:54:36.781Z; 15 rows; 12 logical CPUs.
- Host CPU mean 36.22% (range 25.7–45%); available RAM mean 2.71 GiB, minimum 2.69 GiB (82.69% used).
- Bloodtest/slaughterhouse1 server PID 25840: 105.0% of one logical CPU, or 8.75% of whole-host capacity; RSS 27.61 MB.
- Root dev-loop daemon PID 19372: 5.83% of one logical CPU, or 0.486% of whole-host capacity; RSS 49.99 MB.
- Sample collection averaged 0.789 ms and peaked at 2.653 ms. Reported sampler CPU of 0.0 ms is timer-quantized and does not prove zero overhead.
- Journal-aligned server data: 14 tick windows/848 ticks; 1 NPC/0 players; mean reported tick-window average 0.971 ms; worst-window p95 6.619 ms; highest individual tick 12.765 ms. Host DLL generation 594; NPC module generation 593.
- No client or build was active. Top competing-process attribution and function-level CPU attribution are unknown.

## Interpretation and next check

This is a server-only Bloodtest/one-NPC resource-gate sample. It does not identify a hot function, establish a Juggernaut client/server baseline, or explain host CPU contention. The measured server load is not enough evidence to select a code optimization. Next, after the generation mismatch and runtime ownership are resolved, capture one connected, generation-aligned target workload and run the existing bounded callsite probe; use those results to select one falsifiable code change and compare matched before/after samples while checking 60 Hz server ticks and client frame-time separately.


## Later memory gate

- 2026-11-24T15:02:40.133Z: 0.79 GiB available of 15.649 GiB (95.0% used).
- 2026-11-24T15:03:23.743Z: 0.721 GiB available (95.4% used); dev-loop still showed server PID 25840 only, Bloodtest/1 NPC, no client, `published_stale`.
- One working-set snapshot: Windows Memory Compression PID 3932 1,377.6 MiB; Firefox PIDs 27644/26976/33816 668.5/513.9/380.7 MiB; ChatGPT PID 2636 548.3 MiB; OpenCode PID 31060 546.0 MiB; MsMpEng PID 6696 443.4 MiB; SystemSettings PID 7296 441.4 MiB; OpenCode CLI PID 16776 381.4 MiB; pythonw PID 16864 337.0 MiB. These are working sets and cannot be summed as unique physical memory; Memory Compression is an OS process. No CPU rate was measured and no process was stopped.
