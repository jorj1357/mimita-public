# CPU loop: build pressure and run identity

**Date:** 2026-11-24 UTC  
**Issue:** [issue-ed3ac775283d](../../issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md)  
**Purpose:** record the latest CPU evidence and why this activation did not make a source optimization.

**Does not:** claim a CPU root cause, an optimization result, a matched active-play baseline, or runtime proof.

## Observed

- Root game-DLL attempt `game-dll-20261124T211225830289Z-dcfa7e` failed after 322.968 s. Its `.resource.jsonl` sidecar contains 186 samples from 2026-11-24 21:12:31.261–21:17:48.693 UTC, with 12 logical CPUs, mean host CPU 99.986%, mean memory use 97.72%, mean free RAM 296,660,860 bytes, minimum free RAM 8,781,824 bytes, and mean pagefile use 40.48 GB.
- Across five tracked MiMITA processes in that build window, the process means sum to about 56.57% of whole-machine CPU (about 678.9% of one logical CPU, or 6.79 logical-core equivalents): server PID 9988 15.40% host; client PID 22208 24.60%; its child server PID 28100 6.84%; separate server PID 11616 2.93%; separate client PID 4796 6.81%. These tracked snapshots are not guaranteed to enumerate every process at every sample.
- The same sidecar reports 4.047 s sampler-thread CPU over 322.953 s (about 1.25% of one core, 0.10% of 12-core host capacity), and 28.14 s cumulative process-discovery wall plus 51.30 s runtime-context-discovery wall. These cumulative discovery times overlap sampling and must not be added to build wall time.
- The root run `20261124_205956` has server PID 9988 in Juggernaut Horde/dust2cyberiav4 with 67 NPCs and zero connected players. Its latest hotness summary is host DLL generation 621 while NPC phase rows still report module generation 620. Client PID 22208 does not provide a same-mode connected-client proof: its journal includes a child server PID 28100 in Bloodtest with one NPC and one player.
- Separate run `20261124_212828` has server PID 8940 and client PID 25432 in Bloodtest/slaughterhouse1 with one NPC and one player. Its `run.json` has blank agent/task ownership. It is not a Juggernaut acceptance workload and was not changed or stopped here.
- Root dev-loop currently has no build or pending batch and reports generation 623 published, but the root EXE is stale relative to source. The latency owner confirmed generation 623 changed `src/physics/physical-entity.cpp`; that unrelated build is not a CPU optimization iteration. No CPU optimization build or matched before/after runtime test was run in this activation.

## Inference and unknowns

- The captured MiMITA processes contribute a substantial share of whole-machine CPU during this saturated window. The sample mixes multiple workloads and overlapping runs, so it does not quantify a single client/server pair or explain why the linker/build failed.
- Prior server journals show high elapsed NPC physics/collision scopes, but elapsed scope time is not function-level CPU time. Prior callsite estimates were from a two-NPC sample and are too sparse to choose a safe code change for a 67-NPC workload.
- Exact top function CPU, the current connected pair’s same-generation behavior, compiler/linker CPU completeness, and causal impact of game processes on build duration remain unknown.

## Decision and next experiment

No source change or build was made: the available runs do not provide a valid matched baseline for an attributable optimization. Issue-linked coordination requests were sent to `root-resource-loop`, `root-resource-profiler`, and `hot-reload-latency-root` to resolve runtime ownership, active captures, and a safe matched window.

Next pass: verify one owner-approved Juggernaut run with the intended NPC count, one connected client, fresh server/client journals, and equal EXE/DLL/module identity; collect one short host/process sample and the existing bounded function CPU probe on that exact run. Select one code change only after the probe ranks a hot owner. Compare the same pinned workload before and after using whole-host CPU, memory, live `events.jsonl`, client frame-time distribution (each frame at or below 4.167 ms for the 240 FPS target), server tick rate/latency (60 Hz), and gameplay checks. Keep, revert, or iterate based on that result.

## Evidence paths

- `.dev/build-timings/game-dll-20261124T211225830289Z-dcfa7e.json`
- `.dev/build-timings/game-dll-20261124T211225830289Z-dcfa7e.resource.jsonl`
- `logs/2026-11-24/20261124_205956/server-01/events.jsonl`
- `logs/2026-11-24/20261124_205956/client-01/events.jsonl`
- `logs/2026-11-24/20261124_212828/server-01/events.jsonl`
- `logs/2026-11-24/20261124_212828/client-01/events.jsonl`
