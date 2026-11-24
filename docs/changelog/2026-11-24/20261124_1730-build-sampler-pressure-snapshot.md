# Build sampler resource pressure snapshot

**Purpose:** Preserve one live resource observation for the MiMITA CPU and DLL latency investigations.

**Audience:** MiMITA performance investigators and build-coordination agents.

**Does not:** Identify a hot function, prove that the build caused the runtime stalls, or establish the 67-NPC acceptance workload.

## Observation

On 2026-11-24, the root build attempt `game-dll-20261124T171210403561Z-968a7e` was still compiling at 17:32:56 UTC. Its automatic resource sidecar held 204 samples from 17:13:19.238 through 17:32:56.581 UTC on a 12-logical-CPU host. Host CPU averaged 99.81% (96.2–100%). Available RAM averaged 748 MB and reached a 27 MB minimum; average memory use was 95.55% and average pagefile use was 6.53 GB. Requested 1-second sampling averaged 4.95 seconds, with an 87.094-second maximum gap.

Across available process samples, MiMITA processes averaged 17.60% of whole-host CPU (31.7% peak) and compiler/linker processes averaged 13.29% (31.87% peak, 176 samples). The root build orchestrator averaged 1.65% and worker processes 0.63%. These are process-level CPU observations, not function attribution. Other active processes also consumed CPU; see the raw sidecar for per-sample leaders.

The joined root run was Bloodtest on slaughterhouse1, with one NPC and one player: server PID 32620 and client PID 29884, run `20261124_170535`. The server NPC module reported generation 595. A recent client frame row at 17:32:54.780 reported 169.7 FPS, p95 11.577 ms, maximum 35.526 ms, and 108 of 170 frames over the 4.167 ms budget. The server reported a three-tick NPC phase window at 17:32:54.201; this is not a full server-tick-rate measurement. The latest full `performance.server-tick-window` found at 17:31:20.932 had only five samples, mean total tick time 246.55 ms, p95/maximum 354.004 ms, with four of five above 100 ms.

The DLL attempt had compiled 216 of 645 units and skipped 370 at the latest sidecar sample. The dev-loop status text identified generation 595 while a timing detail had reported target generation 596 and source generation 17. Preserve this identity mismatch until reconciled.

## Interpretation and limits

This is direct evidence of severe host pressure, near-exhausted physical memory, poor client frame times, and server stalls while a large DLL build was active. It does not prove that MiMITA, the compiler, or the sampler caused the host pressure or the stalls. It is Bloodtest with one NPC, not the Juggernaut 67-NPC acceptance workload. The resource sampler's actual intervals were too irregular for millisecond-level attribution. A 400 KB server-journal tail inspected at 17:31:20 contained no `performance.cpu-callsite-window` events; `probe=true` in the hotness summary only means probe support is available.

No runtime code was changed and no CPU optimization was tested in this pass.

## Next discriminating check

After the current build ends and memory headroom recovers, verify one connected same-generation pair and exercise the existing bounded server callsite command once, requiring command acknowledgement and `performance.cpu-callsite-window` rows in that run's canonical server journal. Then repeat on a controlled 67-NPC workload before choosing a runtime optimization. Keep client frame acceptance and server tick acceptance separate.

## Evidence

- `.dev/build-timings/game-dll-20261124T171210403561Z-968a7e.jsonl`
- `.dev/build-timings/game-dll-20261124T171210403561Z-968a7e.resource.jsonl`
- `logs/2026-11-24/20261124_170535/server-01/events.jsonl`
- `logs/2026-11-24/20261124_170535/client-01/events.jsonl`
- CPU issue `issue-ed3ac775283d`; DLL latency issue `issue-136d4553ff92`
