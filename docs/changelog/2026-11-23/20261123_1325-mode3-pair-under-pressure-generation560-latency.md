# Mode 3 multi-file build under one live pair

## Scope

Coordinated the ready batch through the root dev-loop only. Preserved the live registered runtime `mode3-latency-run` (Juggernaut Horde, `dust2cyberiav4`, server PID 6944 and client PID 15328). One secondary pressure pair had been stopped before this attempt. No second dev-loop or direct compiler invocation was used.

## Build and activation evidence

- Root batch request `2026-11-23T13:25:58.067Z-43378a5f` sealed source generation 27.
- Mode 3 status included both `cold` and `game` targets. The cold target was latched when a temporary `src/host/host-runtime.cpp` timer draft was saved. That file was restored and is Git-clean, but the daemon retained the reverted path in its in-memory pending set. Restarting the daemon would discard the other pending inputs, so the batch was preserved. The host timer change was deferred and its build note moved out of the pending-note directory.
- Cold EXE artifact timestamp: `2026-11-23T13:26:39.822Z`. Request-to-artifact elapsed 41.755 s; this includes queue and work and is not a measured cold-link phase.
- DLL attempt `game-dll-20261123T132647317966Z-638ff7` started at 13:26:47.318Z and published generation 560 at 13:27:21.888Z (34.559 s attempt wall). The DLL worker used 10 jobs and LLD.
- The primary client journal records `game.generation.activated` generation 560 at 13:27:22.188Z. Request-to-client activation was 84.121 s. This is not a clean DLL-only result and save-to-activation remains unknown because the source-save timestamp was not correlated.
- The primary server journal later reports DLL generation 560, 67 NPCs, 1 player at 13:28:01.319Z. Treat this as confirmation that generation 560 was serving, not the first server activation instant.
- The client remained the same process and did not close/reopen. This proves live DLL activation for the active game generation, not every behavior module's generation.

## DLL attempt breakdown

Attempt summary: 20 translation units compiled, 623 skipped; 19 changed `.cpp` paths plus generated `module-identity-provider.cpp` (0.866 s). The configured source count is 642, with the generated provider bringing the checked count to 643. Ten jobs overlapped; individual TU compile durations sum to 110.874 CPU-seconds, while their wall overlap is much shorter. Slowest source units were `server-packet-chat.cpp` 14.615 s, `npc.cpp` 14.549 s, `debug-commands.cpp` 10.849 s, and `perf-frame.cpp` 9.931 s.

| Stage | Wall time |
| --- | ---: |
| Changed-source preflight | 17.735 s |
| Object-cache scan | 1.824 s |
| Vendor compile | 0.009 s |
| Separately labeled compile | 0.894 s |
| Link | 5.431 s |
| Publish | 0.463 s |
| Entire DLL attempt | 34.559 s |

The 17.735-second changed-source preflight contains compilation of the 19 changed source units; the separately labeled 0.894-second compile is only the generated provider. This makes the current phase label misleading. The 15 wrapper files were changed by the marker cleanup in this batch and should now remain stable if left untouched; they were not changed on every generation by this run.

The dependency prepass took 0.609 s across 623 checks (1.273 ms average, 139.421 ms maximum for `src/actor/player-ai.cpp`); 1,495 unique dependencies were seen. Scan parsing was 0.014 s, stat prewarm 0.325 s, object-freshness checks 0.840 s, and total object-cache scan 1.824 s. LLD link took 5.365 s over 642 inputs / 483.5 MB of object files. Publish took 0.463 s, including 0.389 s for rename/hash.

## Resource sample

Raw artifact: `.dev/perf-captures/mode3-latency-batch-20261123T1325Z.csv`, 348 rows over 13:25:34.586–13:28:50.186Z; 12 logical CPUs.

During the latched cold-target interval (13:25:58.067–13:26:47.229Z), host CPU averaged 97.39%, with 1.20 GiB mean available RAM. During the DLL worker (13:26:47.229–13:27:21.888Z), host CPU averaged 99.38%, with 1.65 GiB mean available RAM. The live pair accounted for about 68.10% of host capacity during the DLL worker (server 31.65%, client 36.45%). Build compiler children consumed additional CPU; the sampler raised build process priority to High. After the build, the sample window averaged 84.30% host CPU and 5.48 GiB available RAM. These are resource correlations, not proof that pressure caused a particular stage duration.

## Gaps and next step

- This attempt did not meet the <=10-second target: request-to-client activation was 84.121 s, with 41.755 s to the cold artifact followed by a 34.559-second DLL attempt.
- The cold target was caused by a reverted path remaining latched. Add an owner-controlled Mode 3 reconciliation path that removes reverted inputs while preserving the rest of the pending batch; do not restart the daemon as a workaround.
- After clearing the cold-target issue safely, run a matched game-only test with one changed source and a 15–20 source batch under the same live workload.
- Server NPC phase telemetry still labels `module_generation=556` while host DLL telemetry reports generation 560. Do not claim NPC module migration from game-DLL activation alone.
- The opt-in CPU probe runtime/command-route proof is unverified. Current CLI command routing cannot address this registered isolated runtime; it targets the root client or a parallel test run. Add/use an explicit runtime-aware command route before claiming probe evidence.
- Append detailed build-stage evidence to `issue-136d4553ff92` and resource evidence to `issue-ed3ac775283d`.

## Artifacts

- DLL attempt summary: `.dev/build-timings/game-dll-20261123T132647317966Z-638ff7.json`
- DLL attempt timeline: `.dev/build-timings/game-dll-20261123T132647317966Z-638ff7.jsonl`
- Resource capture: `.dev/perf-captures/mode3-latency-batch-20261123T1325Z.csv`
- Live run journals: `logs/2026-11-23/20261123_113457_mode3-latency-run/{server-01,client-01}/events.jsonl`

## Snapshot correction: why 15 wrappers compiled


## Root-side worker-entry timings

The attempt timeline also decomposes the roughly 6.25 s before the first worker phase. From attempt start at 13:26:47.318Z: `manifest_snapshot` took 1.481 s (1,008 inputs), `manifest_validation` 0.009 s, `source_snapshot` 1.784 s using `junction_overlay`, resident-state codegen was a cache hit, and packet-registry codegen took 0.695 s. Packet codegen ended at 13:26:52.569Z; worker input validation started at 13:26:53.565Z, a 0.996 s stage boundary. Worker validation took 0.012 s. The remaining elapsed time to worker entry includes the 0.450 s before manifest stage start and orchestration between measured stages. These stages are already near or below two seconds each; changed-source compilation and full link dominate the DLL attempt.

