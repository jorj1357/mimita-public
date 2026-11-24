# Repeated process pressure in failed gen618 preflight

**Date:** 2026-11-24 UTC  
**Issue:** `issue-ed3ac775283d`  
**Change type:** Automatic resource evidence; no optimization code changed.

## Build and host sample

Attempt `game-dll-20261124T204256895649Z-e6e8af` ended at 20:46:02.835Z
after 185.920 seconds. It failed during changed-source preflight on
`src/main-systems.cpp` after 173.268 seconds. It did not produce an
optimization A/B. The automatic resource sidecar recorded 133 samples across
12 logical CPUs, with host CPU averaging 99.981%, peaking at 100%, and minimum
available RAM of 17,514,496 bytes (16.7 MiB).

## MiMITA process CPU

Nine `mimita.exe` processes averaged 57.32% of whole-host CPU combined
(687.8% of one logical CPU, or about 6.88 logical-core equivalents):

| Role / PID | Whole-host CPU | One-logical-CPU equivalent |
|---|---:|---:|
| Client 35812 | 11.51% | 138.1% |
| Client 28996 | 9.00% | 108.0% |
| Server 4372 | 8.74% | 104.8% |
| Client 22968 | 6.58% | 79.0% |
| Server 13680 | 6.18% | 74.2% |
| Server 16440 | 4.80% | 57.5% |
| Server 10696 | 3.60% | 43.2% |
| MiMITA 33328, role/run unknown | 4.03% | 48.4% |
| MiMITA 25764, role/run unknown | 2.88% | 34.6% |
| **Combined** | **57.32%** | **687.8%** |

The two unclassified PIDs were absent in the 20:47:22Z process inventory, so
their exact run and role remain unknown. The sidecar aligned run
`20261124_202802` to server PID 10696 and client PID 22968. Server journal
evidence identifies Bloodtest, one NPC and one player; client mode was
unverified. Other process averages included `cc1plus` PID 23608 at 1.30% host,
`cpptools` PID 25648 at 3.88%, and `pythonw` PID 16864 at 2.98%.

This repeated measurement confirms large combined MiMITA process CPU during
near-total host saturation. It does not prove MiMITA alone caused saturation,
and it does not attribute cost to a source function. The sample lacks a
matched no-build baseline and exact client mode proof.

Artifacts:

- `.dev/build-timings/game-dll-20261124T204256895649Z-e6e8af.resource.jsonl`
- `.dev/build-timings/game-dll-20261124T204256895649Z-e6e8af.resource.json`

Full evidence was appended to CPU issue `issue-ed3ac775283d` and linked to the
build-latency owner. The next CPU optimization still requires a named code
change selected from function-level evidence, followed by same-workload
before/after runtime proof.
