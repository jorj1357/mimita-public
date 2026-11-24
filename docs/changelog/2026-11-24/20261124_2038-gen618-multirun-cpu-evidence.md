# Gen618 multi-run CPU evidence

**Date:** 2026-11-24 UTC  
**Issue:** `issue-ed3ac775283d`  
**Change type:** Automatic resource evidence; no CPU optimization change.

## Build and host sample

Root dev-loop completed attempt `game-dll-20261124T203535650414Z-718e6f`,
generation 618, from 20:35:35.653Z to 20:39:24.640Z. The only declared source
change was `src/vehicles/vehicle-system.cpp`; this was not a CPU-focused
optimization test. Its resource sidecar sampled 176 times over 228.988 seconds
on 12 logical CPUs. Host CPU averaged 99.97% and peaked at 100%. Minimum
available physical memory was 4,861,952 bytes (4.64 MiB). The sampler thread
used 4,328.125 CPU milliseconds, about 1.89% of one logical CPU or 0.16% of
whole-host capacity averaged over the attempt.

## Process attribution

Seven live `mimita.exe` processes were sampled across 174 rows each, spanning
three client/server run folders and one older server-only process. Their mean
whole-host CPU shares were:

| Role / PID | Whole-host CPU | One-logical-CPU equivalent |
|---|---:|---:|
| Client 35812 | 12.02% | 144.2% |
| Server 4372 | 10.33% | 123.9% |
| Client 28996 | 8.91% | 106.9% |
| Server 13680 | 6.75% | 81.0% |
| Client 22968 | 6.03% | 72.4% |
| Server 16440 | 4.87% | 58.4% |
| Server 10696 | 3.20% | 38.4% |
| **Combined** | **52.11%** | **625.3%** |

This is direct process-level evidence that multiple live MiMITA instances were
using a substantial share of the machine during this interval. It does not
identify the expensive functions or establish that MiMITA alone caused host
saturation. Other listed consumers included cpptools at 4.20% host, ChatGPT at
3.21%, pythonw at 3.07%, cc1plus at 1.50%, and ld.lld at 0.34%.

## Workload and limits

The automatic sidecar joined run `20261124_202802`: server PID 10696,
client PID 22968. Fresh server journal evidence identifies Bloodtest,
slaughterhouse1, 1 NPC and 1 player; client mode remained stale/unreported.
At 20:39:22.392Z its frame window showed 20.98 FPS, 22/22 frames over the
4.167 ms budget, p95 78.44 ms and maximum 246.33 ms. Other live MiMITA runs
were present, so this was not a single-pair baseline.

The attempt had no matched no-build baseline, did not modify CPU-focused code,
and has no source-function CPU attribution. The client-mode and workload
identity gaps prevent calling it a complete matched runtime profile. Do not
claim a before/after improvement from these data.

Artifacts:

- `.dev/build-timings/game-dll-20261124T203535650414Z-718e6f.resource.jsonl`
- `.dev/build-timings/game-dll-20261124T203535650414Z-718e6f.resource.json`
- `logs/2026-11-24/20261124_202802/`

The exact measurement and uncertainty were appended to CPU issue
`issue-ed3ac775283d` and cross-linked to build-latency issue
`issue-136d4553ff92`.
