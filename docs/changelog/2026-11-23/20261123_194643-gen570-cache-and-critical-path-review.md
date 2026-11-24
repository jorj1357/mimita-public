# Gen570 cache and latency critical-path review

- Task: Continue the Mode 3 edit-to-live latency goal; identify why untouched translation units compile and separate cache scan from compile cost.
- Session: work-52015ec94f32; owner: hot-reload-latency-root.
- Status: In progress. No source/build/runtime changes in this session. The <=10s multi-file live target remains unmet.
- Evidence issue: `issue-136d4553ff92`.

## Findings

The gen570 attempt `game-dll-20261123T191346937240Z-35f07c` took 420.870s total. Four edited `.cpp` units compiled in the 61.311s changed-source preflight. Two edited headers legitimately invalidated 62 dependent units; total was 67 compiled and 576 skipped. The downstream compile phase took 323.760s. The dependency-index cache scan took 4.050s (prepass2.510s, dependency stat prewarm0.507s, freshness checks0.978s). Link took7.642s and publish0.567s.

The source-only cache marker is gated on a precise source-only change report, fresh changed objects, no force/sources-file request, no identity-header change, a complete source/object mapping, and a valid published marker. Since gen570 changed `gamemode.h` and `post-fx.h`, it correctly used dependency scanning. The marker could reduce scan overhead on a `.cpp`-only edit; it cannot avoid compiling changed TUs or header dependents.

Automatic resource telemetry failed for gen570: its sidecar has only start and completion rows, `sample_count=0`, `status=partial`, and no errors. The current checked-in sampler source starts a periodic thread and attempts a final sample if the count is zero; a separate Python3.14 smoke run emitted host rows. The exact root-attempt lifecycle discrepancy is unresolved. The native host-only provider also cannot report process-level CPU/RAM for MiMITA, compiler/linker workers, or competing processes. No resource-pressure causation is established.

The latest `20261123_192224` run is server-only and its current journal reports Bloodtest, zero NPCs and zero players. It cannot serve as the required Juggernaut Horde client/server acceptance workload.

## Next measurement

Close both telemetry gaps first: prove periodic/final samples from the actual root build process and provide per-process metrics when `psutil` is unavailable. Then measure a 15–20 `.cpp`-only batch with unchanged headers under a journal-verified Juggernaut Horde pair. Preserve per-TU compile times, scan marker outcome, link/publish durations, host and process samples, exact DLL generation activation, and a behavior event. A Mode 3 batch reduces repeated/overlapping builds; it does not by itself make each required compile faster.

## File changes

- Appended one evidence section to `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md`.
- No C++, Python source, compiler, game configuration, runtime, or build was changed or started by this session.
- `git diff --check` is the planned documentation validation; runtime and build proof are not claimed.

The existing goal-linked agent gap loop also worked here: agent-coord sync surfaced missing_build_resource_measurement, and the root/resource agents investigated it. The fix is not yet complete because the root attempt still emitted zero samples.


## Matched Juggernaut baseline (no build)

Started isolated run `20261123_195307` with root dev-loop, pinned to game DLL generation570. Server PID22184/client PID32832, actual Juggernaut Horde on dust2cyberiav4, server journal 67 NPC/1 player and module_generation570. Matched 29.655s sample `.dev/perf-captures/jugger-horde-pair-20261123T1957Z-cpu.csv`: 12 logical CPUs, host CPU mean99.36%/max100%, available RAM mean659MB/min526.6MB, server9.55% and client17.76% whole-host CPU; no compiler/build was active. RobloxPlayerBeta alone averaged24.27% host CPU. All 1,862 sampled client frames exceeded4.167ms. The opt-in 1/64 cycle probe estimated server `updateOneNpc` at22.9–42.5% of server process CPU across windows and `doCollisions` at up to12.3%; estimates overlap and do not identify instruction-level hot lines. Full resource and journal details are in issue `issue-ed3ac775283d`; build-latency interpretation is in `issue-136d4553ff92` at line2807 onward.

No DLL build was requested. The matched window had only526.6MB minimum free RAM, and the current game-DLL worker uses fixed jobs=10 with no discovered memory guard. The shared Mode3 batch is pending NPC-owned source edits and active leases, so it is not ready for the latency build. The pinned test pair proves a workload baseline, not hot activation of a future DLL generation. A subsequent trial needs an activation-capable root runtime and must wait for source-owner readiness and safe resource headroom; automatic build sidecar lifecycle/process metrics also remain unresolved.

## Correction from aligned tick-window journal

CPU issue `issue-ed3ac775283d` corrected the matched run `20261123_195307` baseline: 30 server tick-window rows from19:58:30.110–19:59:02.530Z show tick indices advancing800 over32.420s (24.68Hz), mean tick average43.09ms, mean NPC time40.19ms, maximum retained tick value175.53ms. Corrected available RAM was mean629MB/minimum502MB. This was a no-build interval, so it proves the existing Juggernaut test runtime was already degraded under the host load; it does not show that a build caused the degradation. The latency build remains held.
