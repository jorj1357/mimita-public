# Generation 566 incremental build evidence

## Build result

Root Mode 3 attempt `game-dll-20261123T171100081510Z-c4f29b` started at 2026-11-23 17:11:00.084 UTC and completed successfully at 17:15:03.397 UTC. The attempt summary reports 243.303 seconds; the dev-loop summary rounds this to 243.342 seconds. Generation 566 was produced.

The sealed batch named `src/network/server-gamemode.cpp`. The worker scanned 642 project source units: it compiled that one project TU and skipped the other 641, with `jobs=10`. It also compiled one generated TU, so two TUs in total were compiled:

- `src/network/server-gamemode.cpp`: 98.422 seconds.
- Generated `build/game/generation-566/module-identity-provider.cpp`: 37.364 seconds.

The compile phase took 135.969 seconds. Changed-source preflight took 0 seconds. LLD linked 642 objects (484,685,544 input bytes, 666 response-file arguments) in 32.382 seconds; the published DLL is 226,948,096 bytes. Object-cache scan took 2.494 seconds, including a 1.078-second dependency-index prepass, 0.450-second unique-dependency stat prewarm, and 0.939 seconds of object-freshness checks. Coherence validation took 0.841 seconds and publication 0.520 seconds.

The first translation unit started 72.661 seconds after the build record began. Before that point, the timeline shows manifest snapshot, manifest validation, source snapshot, cached resident-state codegen, packet-registry codegen, worker-input validation, initial source-coherence validation, toolchain setup, and cache scan. The stage summaries do not account cleanly for the full interval: manifest snapshot reports 13.519 seconds while its event timestamps span 16.295 seconds; manifest validation reports 0.538 seconds while timestamps span 3.718 seconds; source snapshot reports 0.833 seconds while timestamps span 3.403 seconds. The remaining setup and boundary time is not yet reconciled.

The attempt JSON has an empty `changed_paths` array even though the sealed request named `src/network/server-gamemode.cpp` and the cache invalidation row names that source (`missing or invalid dependency file`). `game_build_worker.py` derives its changed-source `-ftime-report` set from that array, so this attempt did not capture the intended per-source GCC phase report. The attempt also records `notes: []`; note attachment should be reconciled with the request before treating the build-note requirement as satisfied.

## Runtime and resource evidence

`logs/2026-11-23/20261123_165703/server-01/events.jsonl:14974` records `game.generation.activated` for generation 566 at 17:15:03.727 UTC, 0.330 seconds after build completion. A later hotness summary reports generation 566, TDM on map `atdm`, 8 NPCs, and 2 players. This proves DLL activation in that runtime; it is not the required Juggernaut Horde workload or a 15–20-file edit.

No host or process CPU/RAM sample overlapped the build. Therefore these timings do not prove that resource pressure caused the long duration. A later process listing showed the three MiMITA test-run processes still alive, but it was after publication and cannot be used as an in-build sample. Do not compare this run with generation 565 as a resource A/B: source invalidations, link inputs, runtime state, and host resource measurements were not matched.

## Next work

1. Fix or explain changed-path propagation and ensure changed-source compiler timing reports are present.
2. Account for all request-to-first-TU time and reconcile event durations with stage wall timestamps.
3. Profile why the generated identity-provider TU takes 37.364 seconds and why a one-source change triggers a 32.382-second full DLL link.
4. Capture host and per-process CPU/RAM, compiler/linker processes, and competing processes during the next canonical build.
5. Continue toward a 15–20-file edit under a live Juggernaut Horde pair; this attempt does not meet the <=10-second goal.

## Generation 567 follow-up

Two further Mode 3 attempts completed while this investigation was read-only:

- `game-dll-20261123T172558516861Z-1c9c25` failed at packet-registry codegen in 11.044 seconds (17:25:58.516–17:26:09.560 UTC). Its attempt record includes build note `c55776a5bbd7` for `src/network/server-gamemode.cpp` and `src/npc/npc.cpp`.
- `game-dll-20261123T172739696116Z-e431e2` succeeded in 116.147 seconds (17:27:39.696–17:29:35.860 UTC), producing generation 567. It scanned 642 project source units, compiled `src/network/server-gamemode.cpp` (80.058 seconds) and `src/npc/npc.cpp` (45.692 seconds), and skipped 640. The generated identity-provider TU also compiled in 4.194 seconds, so three compile events total were recorded. Project-TU compile time overlaps: summed TU compile time was 129.944 seconds while compile wall time was 84.293 seconds. The link took 9.497 seconds for 642 objects (484,695,341 input bytes, 666 response-file arguments). Object-cache scan took 1.474 seconds, including a 0.380-second dependency prepass. Preflight was 0 seconds, coherence validation 0.289 seconds, and publication 0.365 seconds. The first TU started 19.605 seconds after the build record began.

The successful attempt summary has `changed_paths: []` and `notes: []`, while the prior failed attempt records the note naming both changed C++ files. This needs follow-up: confirm whether the retry should retain the note and repair changed-source attribution so compiler timing details are dependable.

No `game.generation.activated` event for generation 567 was found in runs 144429, 165703, or the new server run 172947. Early hotness summaries in run 172947 report generation 1 with zero active NPCs and players; they do not prove generation 567 activation. Generation 566 remains the latest positively verified activation in run 165703, at 17:15:03.727 UTC. No host/process CPU or RAM sample overlapped either generation 567 attempt. Therefore the 116-second duration is an observed faster build than generation 566, but not a matched experiment and not evidence that resource pressure caused the earlier duration. This was not a Juggernaut Horde runtime or a 15–20-file edit, and does not meet the latency target.
