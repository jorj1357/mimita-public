# Mode 3 build timing and resource evidence

- Task ID: Continue edit-to-live latency goal toward 10 seconds under pressure
- Session ID: session-b15b74c5cedd
- Owner: hot-reload-latency-root
- Focus: Measure a completed Mode 3 DLL build, explain changed-header fanout, verify automatic resource telemetry and inspect the dormant module DLL path
- Summary: The root dev-loop published DLL generation 570 in 420.870 seconds for a six-path batch. It compiled 67 translation units and skipped 576. Four changed C++ sources took 50.202–61.162 seconds in preflight; 62 additional objects were invalidated by two changed headers. The automatic resource sidecar recorded zero samples over 420.835 seconds, leaving CPU/RAM causation unknown. The coordination brain surfaced the attempt as a missing-resource-measurement gap. Historical NPC module artifacts are not an active runtime path because the current host resolves module exports from the monolithic DLL and skips external polling.
- Status: In progress; 10-second target not met
- Date, time, timezone: 2026-11-23T19:32:03Z
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 5e08e1054f8293ba0db96983d482e3bf56938d08
- Final commit: none

## Pre-existing changes

The shared worktree already contained extensive modifications from other agents. This session made no source, build, or game-configuration edits. It appended evidence to the existing latency issue and wrote this changelog; coordination state was also updated through agent-coord.

## Requested behavior and specification alignment

Continue reducing edit-to-live C++ latency toward 10 seconds for a 15–20-file Mode 3 batch while a live Juggernaut Horde pair runs. Every build should report total time and its stage contributors, with host/process resource evidence. This investigation follows `docs/specs/dev-loop/build-time-observability.md`, `docs/specs/dev-loop/change-to-evidence-latency.md`, and `docs/specs/dev-loop/agent-coordination.md`. It distinguishes a published DLL from runtime activation and from workload proof.

## Evidence recorded

Issue: [issue-136d4553ff92](../issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md).

- Lines 2701 onward: module-host path finding. `ModuleHost::loadLocalModules()` resolves module exports from `HostRuntime::activeGameModule()`; `poll()` returns for `mLocalModules`. `config/modules/modules.json` is absent. Existing `npc-behavior` artifact records from 2026-11-11 report 1 compiled TU and 1 reused TU but do not prove live use. The worker source requested by `requestBuild()` is absent from the current tracked tree.
- Lines 2739 onward: partial build snapshot and header invalidation counts.
- Lines 2759 onward: completed build timing and zero-sample resource telemetry.
- Lines 2765 onward: exact changed-source preflight timings.
- Lines 2771 onward: longest source compilation and queue-wait records.

Attempt `game-dll-20261123T191346937240Z-35f07c`, generation 570, source generation 4, run `20261123_184017`, completed 2026-11-23T19:20:47.807Z:

- Total: 420.870s.
- Changed-source preflight: 61.311s; four changed sources compiled concurrently. `gamemode-runtime-resolver.cpp` 50.202s, `camera-config.cpp` 56.804s, `post-fx.cpp` 57.163s, `gamemode.cpp` 61.162s.
- Object-cache scan: 4.050s.
- Main compile wall: 323.760s; link: 7.642s; publish: 0.567s; source snapshot: 1.609s; packet-registry codegen: 2.266s; manifest snapshot: 9.216s.
- 643 total units: 67 compiled, 576 skipped. 62 invalidations were dependent translation units: 43 cite `src/gamemode/gamemode.h`, 14 cite `src/render/post-fx.h`, and five use absolute-path spellings. These are legitimate dependency invalidations; skipping them without narrowing the changed interface would risk stale code.
- Slowest observed compile: `src/network/server-gamemode.cpp` 157.506s, after 161.615s queue wait, invalidated by `gamemode.h`. Other slow units and queue waits are listed in the issue.
- Resource sidecar `.dev/build-timings/game-dll-20261123T191346937240Z-35f07c.resource.jsonl` contains only capture-started and capture-completed rows. Summary status is `partial`, `sample_count=0`, `sampler_thread_cpu_ms=0`, provider `windows_kernel32_host_only`, with no errors. No CPU/RAM causal claim is supported.
- Root runtime PIDs 32088/25772 remained online, but root state reported Bloodtest and zero NPCs during this run. This was not the Juggernaut Horde performance acceptance workload, and no 15–20-file latency claim is made.
- A subsequent `agent-coord sync --json` surfaced `build-resource-sampling:game-dll-20261123T191346937240Z-35f07c` as `missing_build_resource_measurement`. Gap discovery worked; sampling did not.
- Independent smoke check by the CPU/resource agent used the same Python 3.14 host-only sampler for3.252s and recorded3 rows, mean host CPU30.556%, peak33.852%, minimum available RAM5,449,228,288 bytes, no errors (.dev/perf-captures/host-sampler-smoke-20261123/.dev/build-timings/python314-host-fallback-smoke.resource.jsonl). This confirms the standalone fallback works; it does not explain the completed build's zero rows. The agent also cautioned that sampler_thread_cpu_ms=0 is not proof the thread never ran because 	ime.thread_time() was coarse in a Python3.14 busy-loop check.

## Architecture lead

Preserve the monolithic MinGW game DLL as fallback. A small candidate NPC behavior DLL may be worth prototyping because the old artifact shows one compiled TU, but it needs a root-dev-loop-owned candidate build path and per-module local/external loading. The current runtime always resolves folded APIs from the monolithic DLL, external catalog file is absent, and arbitrary object-layout migration remains unsolved. First establish whether the NPC behavior ABI stays narrow and host-owned state remains stable; then test one function before generalizing. This path cannot be claimed to accelerate 15–20 arbitrary C++ files without a matched measurement.

## Validation and handoff

No test or build was requested by this session. The observed DLL build was initiated by the root dev-loop under another task and published generation 570; no additional game instance, build coordinator, runtime restart, or manual sampler was started. The active pair was preserved. The CPU/resource agent was informed of the exact attempt metrics and the module path evidence for coordinated follow-up.

Next steps: diagnose why the automatic sampler emitted zero samples and give it an observable heartbeat/failure state; then wait for the runtime owner and CPU agent to release a matched Juggernaut Horde window. The next performance test must track build request through activation, 15–20 changed C++ files, host and process CPU/RAM, exact mode/NPC/player count, compiled/skipped units, DLL generation, and a runtime behavior event. Do not mark the latency goal complete from this run.

## Exact issue append

Before this session, issue-136d4553ff92 had no completed-attempt record for this build. The append-only evidence now includes: Completed attempt result for the previously partial observation: game-dll-20261123T191346937240Z-35f07c succeeded and published generation570. Exact wall duration 420.870s... (issue line 2759). Other issue additions are identified by heading and line in the section above; no existing issue text was replaced.
