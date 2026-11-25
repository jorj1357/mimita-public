# Gen627 CPU and system-resource evidence

## Goal and result

Continue the CPU optimization loop with a falsifiable, matched runtime experiment. This activation gathered process-level resource data and reconciled the profiler path. It did not make a runtime code change or request a build; generation 627 was produced by another agent's gameplay edits and is not a CPU optimization A/B.

## Build-window sample

Attempt `game-dll-20261124T222855538088Z-1d20e7` published game DLL generation 627 at 2026-11-24 22:33:56.113 UTC. It changed `src/main-systems.cpp` and `src/physics/physical-entity.cpp`, compiled three units including the generated identity provider, and took 300.575 seconds. The two changed translation units took 186.582 seconds (`main-systems.cpp`) and 167.413 seconds (`physical-entity.cpp`); LLD link took about 65.82 seconds. These changes belong to other work and do not count as this goal's CPU experiment.

Sidecar `.dev/build-timings/game-dll-20261124T222855538088Z-1d20e7.resource.jsonl` contains 199 samples over 22:29:00.738–22:33:56.104 UTC on 12 logical CPUs. Actual mean interval was 1.51 seconds. Host CPU averaged 99.977% (maximum 100%); memory use averaged 97.834%; available RAM averaged 265.9 MiB and reached 2.18 MiB; pagefile use averaged 48.05 GiB.

Four tracked MiMITA processes averaged 36.923% of whole-host CPU. Their averages were:

| PID | Role and run | CPU of host | Working set | Private bytes |
|---:|---|---:|---:|---:|
| 27372 | server, run `20261124_221014` | 9.25% | 46.8 MiB | 340.8 MiB |
| 26492 | client, run `20261124_221014` | 14.92% | 69.8 MiB | 620.5 MiB |
| 37004 | server, run `20261124_221823` | 4.56% | 17.2 MiB | 106.9 MiB |
| 19396 | client, run `20261124_221823` | 8.39% | 62.5 MiB | 508.8 MiB |

The first pair averaged about 24.17% host CPU; the second averaged about 12.95%. Their combined average working set was 196.3 MiB and combined private bytes 1,577 MiB. Private bytes are committed private memory, not resident physical RAM. This was two active pairs under pressure, not the one-pair idle/background target.

Tracked compiler and linker processes averaged 0.872% of whole-host CPU across the same samples. The two `cc1plus` processes averaged 1.197% and 1.007% host CPU, with maxima 2.164% and 2.187%. This shows MiMITA was a material CPU consumer during this build window; it does not prove that closing the game alone caused the perceived PC improvement, or that compiler contention caused the long translation-unit times. Other processes and paging were also present.

## Workload and behavior evidence

The sidecar's journal alignment matched server PID 27372 and client PID 26492 to run `20261124_221014`, but client mode was unverified. The live server journal reports `funcounterstrike`, 19 NPCs and one player. A separate `bloodtest` server/client pair was also live; its client mode and NPC count are not confirmed here.

After publication, the server reported host DLL generation 627 while `npc.phase-window` still reported module generation 625. Recent server tick windows included 35 ticks averaging 33.13 ms with 175.50 ms p95 and 382.13 ms max, and 44 ticks averaging 18.62 ms with 63.37 ms p95 and 238.67 ms max. These do not prove consistent 60 Hz. Client FPS windows ranged from about 46.8 to 91.5 FPS, with every sampled frame above the 4.167 ms target; p95 ranged from about 13.96 to 23.82 ms and the maximum reached 252.11 ms. The client mode remains unverified, and this is not Juggernaut acceptance evidence.

The complete server journal contains no `performance.cpu-callsite-window` rows and no `npc_cpu_probe` command record. `game.hotness.summary.probe=true` is not proof that the function CPU probe ran: `src/network/server.cpp:203-207` sets it from `HostRuntime::probeEnabled()`, which tracks the DLL-versus-resident dispatch probe. The bounded callsite event is emitted by `src/perf/perf-frame.cpp:318-322` only when sampled calls or samples exist.

The only current `PerfNpcCpuProbe` sites found are `NpcSystem::updateOneNpc` at `src/npc/npc.cpp:1576-1580` and `doCollisions` at `src/physics/movement/physics-collision-dispatch.cpp:170-181`. There is no valid per-function CPU percentage ranking for the current 19-NPC workload yet.

## Coordination and next experiment

The active run owner is `mode3-live-latency-investigation`; I reported the exact process/resource findings and requested a generation-aligned, connected Juggernaut window. I own no C++ paths. The agent self-improvement spec already requires falsifiable single-variable experiments and one measured code change in every build requested for an iterative optimization goal (`docs/specs/dev-loop/agent-self-improvement.md:58-82`); it also says to list up to three distinct useful hypotheses when a problem is resolved or progress plateaus (`:69-75`). No duplicate policy text was added.

After the owner releases a connected, same-mode, generation-aligned 67-NPC Juggernaut pair, capture a short host/process baseline and run the bounded callsite probe. Require actual event rows. A planned selection rule is to choose a repeatable site with at least 10% of server process CPU and at least 100 samples; then make one source change predicting at least a 20% reduction in that site's CPU milliseconds. Compare the same workload before and after, including whole-host and per-process CPU, frame times, 60 Hz ticks, and gameplay correctness. If the profile misses the sample/share bar, improve the measurement rather than guessing at a hotspot.

Issue records: CPU and system-resource evidence is linked to `issue-ed3ac775283d`; build/activation context is cross-linked to `issue-136d4553ff92`. No runtime code, build request, process restart, or live command was performed in this activation.
