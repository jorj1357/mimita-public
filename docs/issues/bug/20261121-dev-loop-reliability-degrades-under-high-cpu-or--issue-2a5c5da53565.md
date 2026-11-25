# issue-2a5c5da53565: Dev-loop reliability degrades under high CPU or memory use

## Human report (verbatim, immutable)

> for dev loop, need to add a issue that it doesnt behave good when cpu or ram is high usage, need t igre out a method to make it not


## Agent suggestion

### 2026-11-21T15:30:14.978Z — opencode

Method proposal for high-load dev-loop resilience (from opencode triage). This issue already exists; do NOT create a duplicate. Related, not duplicates: issue-c8ac61cf07fc (room-create timeout under load), issue-be39a4f5ee17 (AI server-setup validation under pressure), issue-35fd6df3d947 (process inventory accuracy).

EVIDENCE (current code):
1. Fixed wall-clock deadlines treat slow-but-alive work as failure: devscripts/dev_loop/runtimes.py:123-140 `_wait_for_port` abandons a flat 45s wait; devscripts/dev_loop/game_build.py:757-758 uses 3600s build / 600s slot; game_build.py:620 codegen 300s; devscripts/dev_loop/parallel_test_runs.py:423 `_wait_event` uses fixed per-call timeouts. Under CPU starvation these can expire while the child is still progressing.
2. A resource-aware owner already exists: devscripts/build_scheduler.py:49 `available_memory_bytes()` (GlobalMemoryStatusEx) and :73 `min_free_bytes()` gate build-slot admission at :170-181 with explicit wait messages. Reuse this; do not add a second resource probe.
3. Failure classification is text-based in devscripts/dev_loop/launch_report.py:49-75, so it cannot tell "hung" from "running slowly under contention" and mislabels a slow stage as `*_timeout`.

PROPOSED METHOD (smallest safe; not a timeout-only fix):
A. Add one shared `resource_snapshot()` (extend build_scheduler) returning free MB, memory-load %, CPU busy %, sampled cheaply and cached ~1s; attach it to dev-loop events at each build/launch/readiness stage.
B. Replace flat deadlines with liveness-gated ones: call a stage "hung" only when child CPU-time is not advancing AND its journal/output has not grown AND inputs are unchanged for N seconds; otherwise label "delayed under load" and keep waiting.
C. Scale stage deadlines by measured pressure with a bounded multiplier and a hard absolute cap so genuine hangs still fail; log base, multiplier, and effective deadline.
D. Gate expensive non-build operations (launch, room creation, isolated test run) with the same memory gate as builds; emit a `waiting_on_resources` event with the snapshot instead of silently timing out.
E. Dashboard/launch-report: surface delayed-vs-hung, the current snapshot, and the waiting stage; keep process inventory accurate or explicitly stale-labelled.
F. Prove with isolated runs (parallel_test_runs.py pins an artifact and owns its journal) on idle baseline vs CPU-pressure vs memory-pressure; require journal progress events to advance and a `delayed` classification (not `*_timeout`) under load, plus a real-hang case that still times out at the cap.

No code changed by me; implementation needs human approval. Suggested proof: before/after journals for the three-condition matrix plus one real-hang negative control.

## Agent suggestion

### 2026-11-21T15:37:04.071Z — opencode

Root cause evidence for build slowness/OOM (2026-11-21 ~15:07-15:30 UTC). Attempt game-dll-20261121T150703129448Z-23d2a4 was a full cold rebuild (seeded=0, 642 TUs) and FAILED in 749s. Pre-compile phases alone took ~5.6 min: codegen packet_registry 56.3s, manifest_snapshot 77.4s over 808 files, source_snapshot 110.2s. Compile then failed with cc1plus.exe 'out of memory allocating 16781311/33558527 bytes' on 4 TUs (avatar-editor-animation.cpp, avatar-editor-face.cpp, client-collision-world-view.cpp, death-system-bodies.cpp). Measured host: 15.6GB RAM, FreePhysicalMemory=0.5GB, commit-free 4.6GB. Worker resolves --jobs 0 to os.cpu_count() (devscripts/game_build_worker.py:1126) = 12 parallel heavy cc1plus jobs -> memory exhaustion. Also observed source churn: auto-1..auto-6 requests repeatedly superseded in-progress builds (state.json game_build.requests), so the build restarts cold and never completes. Suggested smallest fix: cap effective jobs by available memory (e.g. min(cpu_count, floor(free_mem/target_per_TU)) or a safe default like 4-6 on <=16GB), and avoid restarting on every edit while a build is close to finishing. This matches user ctx-541576630358 (dev-loop degrades under high CPU/RAM).

## Agent suggestion

### 2026-11-21T15:45:53.010Z — codex-process-scan-fix

Related evidence for issue-35fd6df3d947: a failed process scan at 2026-11-21T15:29:50Z timed out after 8 seconds in PowerShell Get-CimInstance while the root DLL build was on generation 423, configured for jobs=12, in a long object_cache_scan. This supports build/resource pressure as a plausible factor, but does not prove causality. The process scanner now uses a native Win32 snapshot when psutil is missing; direct fallback scan completed in 30.61ms while the DLL build remained active. Compare repeated idle/load timings before attributing the old timeout to parallel jobs.

## Agent suggestion

### 2026-11-21T16:08:19.717Z — visual-access-and-issue-review

Direct evidence of degradation under load (root dev-loop, 2026-11-21): attempt game-dll-20261121T150703129448Z-23d2a4 ran with jobs=12 and failed in compile with 4 cc1plus out-of-memory errors (avatar-editor-animation.cpp, avatar-editor-face.cpp, client-collision-world-view.cpp, death-system-bodies.cpp). The next attempt (game-dll-20261121T152531204381Z-544470) showed object_cache_scan 469.9 s and single-TU compile times of 280-363 s (normal 50-100 s) with 3-10 min queue waits, then was superseded. So high host load does not only slow the loop; it changes build outcomes (OOM failures) and skews wait/timeout evidence. Suggested measurable fix: cap concurrent compile jobs by available RAM (user later requested jobs=10), surface phase/queue progress in `--status`, and keep build waits distinct from a hung operation. Evidence: .dev/build-timings/game-dll-20261121T150703129448Z-23d2a4.json, .dev/build-timings/game-dll-20261121T152531204381Z-544470.jsonl.

## Agent discussion and suggestions

### 2026-11-23T17:48:51.290Z — latejoin-team-identity

Message: `msg-2f8911da028f` (evidence)

Subject: Measured memory pressure during 642-TU game-DLL build

Direct observation 2026-11-23T17:47:22Z, ~30s sample during game-dll-20261123T174329519651Z-c751ad (revision 3ff934f6, sources=642, jobs=10, priority=high, seeded=0): host 12 logical CPUs, 15.65 GiB total, 0.32 GiB free physical; 10 cc1plus processes with 4.08 GiB combined working set (~0.41 GiB each). This reproduces the earlier 12-job OOM condition at jobs=10 on a 16 GB host while the EXE builder can independently default to 12 jobs. Proposed smallest change: one shared build-worker budget owner consulted by both EXE and game-DLL builds, cap total compile workers at logical_cpus - 2 and reduce further when free physical < 2 GiB or measured per-TU RSS exceeds the reserve; record jobs + free-memory snapshot in the attempt timing. No code changed by this note.

## Occurrence

### 2026-11-24T09:46:42.574Z — build-wait-reviewer

Measured build-time resource pressure (direct observation, agent build-wait-reviewer). During successful game-DLL attempt game-dll-20261124T075315478413Z-04a3b7 (generation 587, jobs=10, 644 sources, compiled 2): .dev/build-timings/game-dll-20261124T075315478413Z-04a3b7.resource.json reported 176 samples @1s, started 2026-11-24T07:53:15.478Z, duration 221.678s, mean host CPU 99.994%, peak 100.0%, minimum available memory 17,457,152 B (~16.6 MiB) of ~15.65 GiB, no sampler errors. The report did not capture a top competing process, and runtime alignment was unavailable ('fresh_server_and_client_journal_evidence_not_matched'), so the pressure is confirmed at the build phase only and is not attributed to a specific live mode or workload. Direct: the build succeeded; the slowest TU src/vehicles/vehicle-system.cpp took 101.057s. Inference: background load/paging likely lengthened that TU, but this sample does not prove causation or pin the minimum to compile vs link. Unknown: which phase the 16.6 MiB minimum occurred in and which process held the remaining memory. Artifact: .dev/build-timings/game-dll-20261124T075315478413Z-04a3b7.resource.json. Related wait issue: issue-816c05804889.

Evidence: {"agent": "build-wait-reviewer", "artifact": ".dev/build-timings/game-dll-20261124T075315478413Z-04a3b7.resource.json", "build_attempt_id": "game-dll-20261124T075315478413Z-04a3b7", "compiled": 2, "generation": 587, "host_total_memory_approx_gib": 15.65, "jobs": 10, "mean_host_cpu_percent": 99.994, "minimum_available_memory_bytes": 17457152, "peak_host_cpu_percent": 100.0, "related_issue": "issue-816c05804889", "runtime_alignment": "unavailable: fresh_server_and_client_journal_evidence_not_matched", "sample_count": 176, "sample_interval_seconds": 1.0, "sample_kind": "build_phase_attempt_scope", "sampled_duration_seconds": 221.678, "sampled_started_utc": "2026-11-24T07:53:15.478Z", "slowest_tu": "src/vehicles/vehicle-system.cpp", "slowest_tu_seconds": 101.057, "source_count": 644}

## Occurrence

### 2026-11-24T12:22:44.571Z — build-wait-reviewer

Build-stress resource occurrence for game-dll wait issue-9a301a157e7c (attempt game-dll-20261124T113118610851Z-05382c, run 20261124_112426). Host was saturated while a single changed TU compiled: 12 logical CPUs, mean host CPU 99.997%/peak 100.0% over 106 samples @1s (181.0s), minimum available memory 47,779,840 B (~45.6 MiB) of 16.8 GB, pagefile used ~35.7 GB. Live workload at sample was Bloodtest/mimita-duels-map-v3, 0 NPC, 2 players with two clients at ~112/~94 FPS and p95 frame 24.1/33.7 ms (80/79 frames over the 4.167 ms budget); server pid 5804 (~150 MB RSS) and clients 24384 (~919 MB)/34276 (~709 MB) plus build orchestrator pythonw 36728 (~49 MB). Full process/interval reconciliation and the runtime_alignment=unavailable caveat are already on issue-136d4553ff92 and issue-ed3ac775283d; this is pressure correlation, not proof that host load caused the 74.481s TU.

Evidence: {"build_orchestrator_pid": 36728, "build_phase": "game_build compiling src/input/gamepad-input.cpp (74.481s)", "build_total_seconds": 163.37, "client_pids": [24384, 34276], "cross_links": ["issue-136d4553ff92", "issue-ed3ac775283d", "issue-9a301a157e7c"], "host_cpu_percent_whole_machine": {"mean": 99.997, "peak": 100.0}, "host_logical_cpu_count": 12, "host_memory_total_bytes": 16802918400, "host_memory_used_percent_first_sample": 94.0, "live_map": "mimita-duels-map-v3", "live_mode": "bloodtest", "minimum_available_memory_bytes": 47779840, "npcs": 0, "observation_type": {"direct": "sampler values, process RSS, run/mode/player counts", "inference": "host pressure contributed to the 74.481s compile"}, "pagefile_used_bytes_first_sample": 35694055424, "paths": [".dev/build-timings/game-dll-20261124T113118610851Z-05382c.resource.json", ".dev/build-timings/game-dll-20261124T113118610851Z-05382c.resource.jsonl", "logs/2026-11-24/20261124_112426/dev-loop/events.jsonl", "logs/2026-11-24/20261124_112426/server-01/events.jsonl"], "players": 2, "sample_completed_utc": "2026-11-24T11:34:19.630Z", "sample_count": 106, "sample_duration_seconds": 181.019, "sample_interval_seconds": 1.0, "sample_started_utc": "2026-11-24T11:31:18.610Z", "server_pid": 5804, "top_competing_processes": "not captured in these rows (top_processes empty)", "unknowns": ["causality of the 74.481s TU", "non-MiMITA competing processes during the window"]}

## Occurrence

### 2026-11-24T17:41:28.833Z — build-wait-reviewer

Build-phase resource pressure during successful game-DLL attempt game-dll-20261124T164717275109Z-44a127 (generation 595, jobs=10, 644 sources, 276 compiled): .dev/build-timings/game-dll-20261124T164717275109Z-44a127.resource.json reports 273 samples @1 s, started 2026-11-24T16:47:17.277Z, duration 809.922 s, mean host CPU 99.344 %, peak 100 %, minimum available memory 7,823,360 B (~7.5 MiB), no sampler errors; runtime alignment unavailable ('fresh_server_and_client_journal_evidence_not_matched'), so live mode/workload is unknown. Direct: the build succeeded; slowest TU src/network/server-gamemode.cpp 84.94 s. Inference: near-full CPU and very low available memory likely lengthened compile; not proven. Unknown: phase at the 7.5 MiB minimum and which process held memory. Related wait issue-ee437972915e. gap_key build-resource-sampling:game-dll-20261124T164717275109Z-44a127 also recorded on issue-136d4553ff92.

Evidence: {"agent": "build-wait-reviewer", "artifact": ".dev/build-timings/game-dll-20261124T164717275109Z-44a127.resource.json", "build_attempt_id": "game-dll-20261124T164717275109Z-44a127", "compiled": 276, "duration_seconds": 809.922, "gap_key": "build-resource-sampling:game-dll-20261124T164717275109Z-44a127", "generation": 595, "jobs": 10, "mean_host_cpu_percent": 99.344, "minimum_available_memory_bytes": 7823360, "peak_host_cpu_percent": 100.0, "related_issue": "issue-ee437972915e", "runtime_alignment": "unavailable", "sample_count": 273, "sample_interval_seconds": 1.0}

## Occurrence

### 2026-11-24T19:41:15.345Z — build-wait-reviewer

Build-stress resource occurrence for game-dll wait issue-f15a2daf6c16 (attempt game-dll-20261124T190817284712Z-0003b3, run 20261124_170535). Direct observation from .dev/build-timings/game-dll-20261124T190817284712Z-0003b3.resource.json: 353 samples @1s from 2026-11-24T19:08:17.284Z over 411.721s; mean host CPU 98.755%, peak 100.0%; minimum available memory 25,145,344 B (~24.0 MiB) of 16,802,918,400 B (~15.65 GiB); host logical CPUs 12 (current Win32_ComputerSystem query; the artifact itself carries no CPU-count field); no sampler errors. Workload: Mode 3 game-DLL compile, jobs=10, source_count=644, compiled=266, skipped=378, compile 404.599s, slowest TU src/network/server-gamemode.cpp 48.690s. The attempt FAILED at compile (not merely slow): src/vehicles/vehicle-system.cpp:700 'collisionHalf was not declared'; cache_invalidations marks vehicle-system.cpp reason 'object file missing', i.e. a sealed batch captured an in-progress edit. runtime_alignment is unavailable ('fresh_server_and_client_journal_evidence_not_matched'), and no top competing process was captured, so live-mode attribution and the cause of the ~24 MiB floor are unknown. Inference only: header fanout (src/combat/weapon-types.h, src/vehicles/vehicle-types.h) plus a saturated host plausibly drove the 60.318s wait and 411.7s build; this sample does not prove causation. Cross-links: issue-f15a2daf6c16, issue-136d4553ff92, issue-196c3045b4cb, issue-6e08d89b55e3.

Evidence: {"artifact": ".dev/build-timings/game-dll-20261124T190817284712Z-0003b3.resource.json", "build_attempt_id": "game-dll-20261124T190817284712Z-0003b3", "build_total_seconds": 411.721, "compile_seconds": 404.599, "compiled": 266, "cross_links": ["issue-f15a2daf6c16", "issue-136d4553ff92", "issue-196c3045b4cb", "issue-6e08d89b55e3"], "failure": "src/vehicles/vehicle-system.cpp:700: 'collisionHalf' was not declared in this scope", "host_cpu_count_source": "current Win32_ComputerSystem query, not present in artifact", "host_logical_cpus": 12, "host_total_memory_bytes": 16802918400, "incomplete_edit_marker": "cache_invalidations vehicle-system.cpp reason=object file missing", "jobs": 10, "mean_host_cpu_percent": 98.755, "minimum_available_memory_bytes": 25145344, "outcome": "failed_compile", "peak_host_cpu_percent": 100.0, "related_wait_issue": "issue-f15a2daf6c16", "runtime_alignment": "unavailable: fresh_server_and_client_journal_evidence_not_matched", "sample_count": 353, "sample_interval_seconds": 1.0, "sampled_duration_seconds": 411.721, "sampled_started_utc": "2026-11-24T19:08:17.284Z", "skipped": 378, "slowest_tu": "src/network/server-gamemode.cpp", "slowest_tu_seconds": 48.69, "source_count": 644}

## Occurrence

### 2026-11-24T20:08:00.272Z — build-wait-reviewer

Build-stress resource occurrence for game-dll wait issue-536424f63590 (attempt game-dll-20261124T194125987951Z-4d7bb0, run 20261124_192708). Direct observation from .dev/build-timings/game-dll-20261124T194125987951Z-4d7bb0.resource.json: 685 samples @1s from 2026-11-24T19:41:25.988Z over 801.045s; mean host CPU 99.926%, peak 100.0%; minimum available memory 65,712,128 B (~62.7 MiB) of 16,802,918,400 B (~15.65 GiB); no sampler errors. Workload: Mode 3 game-DLL compile, jobs=10, source_count=644, compiled=234, skipped=411, compile 781.06s, slowest TU src/network/server-gamemode.cpp 102.633s. Cause is header fanout (src/entities/player.h, 233 dependents) on a saturated host. runtime_alignment unavailable (status mode_unverified: server authoritative mode bloodtest, client mode stale_or_unreported), and no top competing process was captured, so live-workload attribution and the cause of the ~62.7 MiB floor are unknown. Inference only: near-full CPU and very low available memory likely lengthened compile; this sample does not prove causation. gap_key build-resource-sampling:game-dll-20261124T194125987951Z-4d7bb0 (missing matched_prebuild_resource_baseline, host_commit_charge_and_limit, client_mode_independent_confirmation). Next check (unowned): add a matched pre-build resource baseline so build-phase pressure can be separated from pre-existing host load; owner build-coordinator/hot-reload-latency-root via issue-136d4553ff92. Cross-links: issue-536424f63590, issue-136d4553ff92, issue-6e08d89b55e3, issue-84c584957e65.

Evidence: {"artifact": ".dev/build-timings/game-dll-20261124T194125987951Z-4d7bb0.resource.json", "build_attempt_id": "game-dll-20261124T194125987951Z-4d7bb0", "build_total_seconds": 801.055, "compile_seconds": 781.06, "compiled": 234, "cross_links": ["issue-536424f63590", "issue-136d4553ff92", "issue-6e08d89b55e3", "issue-84c584957e65"], "gap_key": "build-resource-sampling:game-dll-20261124T194125987951Z-4d7bb0", "generation": 609, "host_logical_cpus": 12, "host_total_memory_bytes": 16802918400, "jobs": 10, "mean_host_cpu_percent": 99.926, "minimum_available_memory_bytes": 65712128, "peak_host_cpu_percent": 100.0, "related_wait_issue": "issue-536424f63590", "runtime_alignment": "unavailable: mode_unverified", "sample_count": 685, "sample_interval_seconds": 1.0, "sampled_duration_seconds": 801.045, "sampled_started_utc": "2026-11-24T19:41:25.988Z", "skipped": 411, "slowest_tu": "src/network/server-gamemode.cpp", "slowest_tu_seconds": 102.633, "source_count": 644}

## Occurrence

### 2026-11-24T20:48:12.608Z — build-wait-reviewer

Resource evidence for wait game-dll-build:2026-11-24T20:00:45.963Z / issue-a4eea8fe2246, attempt game-dll-20261124T200045986936Z-3f16b3. .dev/build-timings/game-dll-20261124T200045986936Z-3f16b3.resource.json: 99 samples @1s over 134.387s, mean host CPU 99.87%, peak 100%, minimum available memory 160,858,112 B (~153.4 MiB), jobs=10, no sampler errors. runtime_alignment status mode_unverified: server authoritative mode bloodtest (run 20261124_195553, server pid 13680), client mode stale_or_unreported (client pid 35812), so live-workload attribution is unknown. The build was real work (compile still running at the 60.8s wait) and failed at compile in src/entities/player-render.cpp:634; the wait is not a stalled build.

Evidence: {"build_attempt_id": "game-dll-20261124T200045986936Z-3f16b3", "duration_seconds": 134.387, "gap_key": "build-resource-sampling:game-dll-20261124T200045986936Z-3f16b3", "jobs": 10, "mean_host_cpu_percent": 99.87, "minimum_available_memory_bytes": 160858112, "peak_host_cpu_percent": 100.0, "resource_summary": ".dev/build-timings/game-dll-20261124T200045986936Z-3f16b3.resource.json", "runtime_alignment": "mode_unverified; server bloodtest; client stale_or_unreported; run 20261124_195553", "sample_count": 99, "sample_interval_seconds": 1.0}

## Occurrence

### 2026-11-24T22:45:55.869Z — build-wait-reviewer

Build-stress resource occurrence for game-dll wait issue-ee3e906c8273 (attempt game-dll-20261124T214357271677Z-7f59d1, dev-loop run 20261124_205956). Direct build-phase sidecar .dev/build-timings/game-dll-20261124T214357271677Z-7f59d1.resource.json: 124 samples @1s over 310.98s, 12 logical CPUs, mean host CPU 99.984% / peak 100.0%, minimum available memory 40,304,640 B (~38.4 MiB), no sampler errors, provider windows_kernel32_host_and_process. The attempt failed changed_source_preflight on src/main-systems.cpp after 261.697s (compile total 258.016s); this is pressure correlation, not proof that host load caused the duration or the compile error. Runtime alignment was mode_unverified (server_actual_mode bloodtest, runtime run 20261124_212828, client mode stale_or_unreported; mode_match=false). Cross-links: issue-ee3e906c8273 (wait), issue-136d4553ff92 (stage latency), issue-84c584957e65 (wake dispatch). No process was stopped and no source was edited by this reviewer.

Evidence: {"agent": "build-wait-reviewer", "artifact": ".dev/build-timings/game-dll-20261124T214357271677Z-7f59d1.resource.json", "build_attempt_id": "game-dll-20261124T214357271677Z-7f59d1", "build_outcome": "failed", "build_total_seconds": 310.99, "changed_source_preflight_seconds": 261.697, "compile_seconds": 258.016, "cross_links": ["issue-ee3e906c8273", "issue-136d4553ff92", "issue-84c584957e65"], "dev_loop_run_id": "20261124_205956", "failed_phase": "changed_source_preflight", "failed_source": "src/main-systems.cpp", "host_logical_cpus": 12, "mean_host_cpu_percent": 99.984, "minimum_available_memory_bytes": 40304640, "peak_host_cpu_percent": 100.0, "related_issue": "issue-ee3e906c8273", "runtime_alignment": "mode_unverified (server bloodtest, run 20261124_212828, client stale_or_unreported)", "sample_count": 124, "sample_interval_seconds": 1.0, "sampled_duration_seconds": 310.98}

## Occurrence

### 2026-11-24T22:53:53.460Z — build-wait-reviewer

Build-stress resource occurrence for game-dll wait issue-a2de003a1900 (attempt game-dll-20261124T215807388728Z-84be9e, run 20261124_215719_01). Direct observation from .dev/build-timings/game-dll-20261124T215807388728Z-84be9e.resource.json: 142 samples @1s from 2026-11-24T21:58:07.421Z over 182.614s; mean host CPU 99.984%, peak 100.0%; minimum available memory 12,595,200 B (~12.0 MiB); no sampler errors. Workload: Mode 3 game-DLL compile, jobs=10, source_count=644, compiled=3, skipped=642, compile 67.398s, link 89.348s, slowest TUs src/main-systems.cpp 60.486s and src/physics/physical-entity.cpp 60.480s. The build succeeded (generation 625). runtime_alignment unavailable (fresh_server_and_client_journal_evidence_not_matched) and no top competing process was captured, so the live-mode attribution and the cause of the ~12 MiB floor are unknown. Inference only: near-full CPU and very low available memory plausibly lengthened compile/link; this sample does not prove causation. gap_key build-resource-sampling:game-dll-20261124T215807388728Z-84be9e recorded on issue-136d4553ff92. Cross-links: issue-a2de003a1900, issue-136d4553ff92, issue-6e08d89b55e3, issue-84c584957e65. Reviewer: build-wait-reviewer.

Evidence: {"artifact": ".dev/build-timings/game-dll-20261124T215807388728Z-84be9e.resource.json", "build_attempt_id": "game-dll-20261124T215807388728Z-84be9e", "build_total_seconds": 182.618, "compile_seconds": 67.398, "compiled": 3, "cross_links": ["issue-a2de003a1900", "issue-136d4553ff92", "issue-6e08d89b55e3", "issue-84c584957e65"], "gap_key": "build-resource-sampling:game-dll-20261124T215807388728Z-84be9e", "generation": 625, "jobs": 10, "link_seconds": 89.348, "mean_host_cpu_percent": 99.984, "minimum_available_memory_bytes": 12595200, "peak_host_cpu_percent": 100.0, "related_wait_issue": "issue-a2de003a1900", "runtime_alignment": "unavailable: fresh_server_and_client_journal_evidence_not_matched", "sample_count": 142, "sample_interval_seconds": 1.0, "sampled_duration_seconds": 182.614, "sampled_started_utc": "2026-11-24T21:58:07.421Z", "skipped": 642, "slowest_tu": "src/main-systems.cpp", "slowest_tu_seconds": 60.486, "source_count": 644}

## Occurrence

### 2026-11-25T01:41:43.813Z — build-wait-reviewer

Build-stress resource occurrence for game-dll wait issue-e6c253b2fa9f (attempt game-dll-20261125T010553542744Z-42909c, dev-loop run 20261124_221014). Direct observation from .dev/build-timings/game-dll-20261125T010553542744Z-42909c.resource.json: 264 samples @1s from 2026-11-25T01:05:53.542Z over 319.139s; mean host CPU 99.465%, peak 100.0%; minimum available memory 14,106,624 B (~13.5 MiB) of 16,802,918,400 B (~15.65 GiB); host logical CPUs 12 (current Win32_ComputerSystem query; the artifact carries no CPU-count field); provider windows_kernel32_host_and_process; no sampler errors. Workload: Mode 3 game-DLL full rebuild, jobs=10, source_count=644, seeded=0; the attempt FAILED with worker_timeout after 300s. runtime_alignment is unavailable (fresh_server_and_client_journal_evidence_not_matched), no top competing process was captured, so live-workload attribution and the cause of the ~13.5 MiB floor are unknown. Inference only: near-full CPU and very low available memory plausibly lengthened the rebuild past 300s; this sample does not prove causation. Cross-links: issue-e6c253b2fa9f (wait), issue-9a4503f6ed68 (wait aggregation), issue-136d4553ff92 (stage latency), issue-916633ab3313 (disk_low tail), issue-ca5eb16d9e21 (memory pressure).

Evidence: {"agent": "build-wait-reviewer", "artifact": ".dev/build-timings/game-dll-20261125T010553542744Z-42909c.resource.json", "build_attempt_id": "game-dll-20261125T010553542744Z-42909c", "build_outcome": "failed_worker_timeout", "build_total_seconds": 319.139, "cross_links": ["issue-e6c253b2fa9f", "issue-9a4503f6ed68", "issue-136d4553ff92", "issue-916633ab3313", "issue-ca5eb16d9e21"], "dev_loop_run_id": "20261124_221014", "failed_phase": "worker_timeout", "host_logical_cpus": 12, "host_total_memory_bytes": 16802918400, "jobs": 10, "mean_host_cpu_percent": 99.465, "minimum_available_memory_bytes": 14106624, "peak_host_cpu_percent": 100.0, "related_wait_issue": "issue-e6c253b2fa9f", "runtime_alignment": "unavailable: fresh_server_and_client_journal_evidence_not_matched", "sample_count": 264, "sample_interval_seconds": 1.0, "sampled_duration_seconds": 319.139, "seeded": 0, "source_count": 644}

## Scheduled independent review

### 2026-11-25T08:55:30.337Z — mode3-latency-continuation

A bounded independent review was queued as wake wake-e60255ff7b85 after 718 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (inconclusive-evidence-gap)

### 2026-11-25T09:16:28.770Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Bounded independent review (issue-review-sweeper, wake-e60255ff7b85). Read-only: no source edit, no build, no runtime command, no close, no fix claim.

READINESS / PROOF
- issue readiness: ready=true, score=8, missing=[], state=new, tasks=0, owner="", reviews=[] before this review. There is no linked task and no proof record for this issue, so no behavior proof exists; readiness is field-completeness, not evidence of the fix.
- Acceptance is unmet as stated: I found no pressure test that identifies stage-specific degradation and thresholds, no delayed-vs-hung classification evidence, and no bounded resource-control evidence. All recorded occurrences are post-hoc resource co-occurrence, which the prior notes correctly label as correlation.

OBSERVED FACTS (direct)
1. Most recent completed build after this review was queued: game-dll-20261125T084339926983Z-be8067 (gen642), 19.781s total, one changed source src/npc/npc-combat.cpp plus the identity provider, 2 compiled / 643 skipped, mean host CPU 60.169% / peak 75.2%, min available memory 6,219,526,144 B, 13 samples, no sampler errors. Low-to-moderate pressure and a fast small build — it does not exercise the pressure acceptance matrix.
2. Same sidecar has a confirmed evidence-composition contradiction: top-level `runtime_alignment.status=unavailable` / `pair_alignment_status=unavailable` ("fresh_server_and_client_journal_evidence_not_matched"), yet sample 1 `tracked_processes` contains server PID 10644 and client PID 7452 both exactly joined to run 20261125_082735 (instance_process_start), with client `client_frame_window`/`client_npc_view_window` fresh at 08:43:44-45Z.
3. First divergence (sampler code): `_runtime_alignment_summary` (devscripts/dev_loop/build_resource_sampler.py:884-887) admits a server only when both `generation_npc_player` and `server_tick_window` are fresh. The sampled server context had `journal_mode`, `npc_phase_window`, and `server_tick_window`, but no `generation_npc_player`, so `servers=[]` -> `matched=None` -> unavailable. The client context also had `actual_mode=null`, so even if the pair were admitted it would correctly be `mode_unverified`, not `unavailable`.
4. Why `generation_npc_player` was missing is not proven. The same run's rotated server journal that covers the build window (logs/2026-11-25/20261125_082735/server-01/events-0006.jsonl, 08:39:47.813-08:44:18.996Z) has 264 `game.hotness.summary` rows with pid 10644/run 20261125_082735. Candidate causes: bounded 256 KiB tail (`_tail_jsonl`, build_resource_sampler.py:30) against ~100 MB rotated journals plus bursty high-rate events, sibling/rotation selection, or refresh timing. Unknown, testable.
5. The diagnostic instrument also degrades under the load it measures: game-dll-20261125T055256329120Z-9b2bcf is `status=partial`, error "sampler thread did not stop before timeout", 60 samples over 131.4s, `process_discovery_wall_ms_total=55,706.858` (~42% of the window), mean host CPU 95.45%, min available memory 1,836,306,432 B. This is a completed build with an incomplete sidecar.

INFERENCE / UNCERTAINTY
- Near-full CPU and low available memory plausibly lengthened compile/link in the prior occurrences, but no occurrence isolates host pressure from source cost, header fanout, or concurrent live pairs; causation stays unproven.
- Sampler overhead is not proven to delay the build main thread. For be8067 only ~3.95s `sampler_setup_wall_ms` is synchronous on the critical path; the larger discovery cost in 9b2bcf is on the sampler thread. The observer cost is real but its effect on build wall time is unmeasured.
- The 9b2bcf high-pressure sample was a Bloodtest live pair, not the Juggernaut 67-NPC workload, and its sidecar is partial, so it cannot serve as the acceptance pressure test.

CONTRARY EVIDENCE
- be8067 shows a non-degraded loop under a moderately loaded host (mean 60.17% CPU, 6.22 GB free): 1-TU build in ~20s. This argues against a blanket "always degraded under load" claim, but does not test the acceptance matrix.
- The gen642 sidecar explicitly reports stale/unavailable alignment, which partially satisfies the acceptance clause "process inventory remain accurate or explicitly stale"; the contradiction in (2) is that valid same-run evidence was present but not composed, not that stale state was hidden.

NEXT DISCRIMINATING CHECK (unowned)
- Replay game-dll-20261125T084339926983Z-be8067 sample 1's `tracked_processes` through `_runtime_alignment_summary` and log which predicate fails, then add a regression fixture for this observed schema. Target: when a same-run server+client pair has fresh server tick and client frame/viewport windows, the summary must report `same_run_server_client=true` and either `observed` or an explicit `mode_unverified`, never `unavailable`.
- Then a controlled matrix on the same one-source edit: (a) build with no live run, (b) same edit with the pinned 67-NPC Juggernaut pair at the same generation baseline; compare per-stage wall time and record whether progress is classified "delayed under load" vs "hung", with one real-hang negative control. No win from a longer flat timeout alone.

OWNERSHIP / CROSS-LINKS
- The sampler source is under an active edit lease by mode3-latency-continuation (lease-e3f099c55021/lease-19f9afdd8705, expires 2026-11-25T09:32:56Z); I did not edit it.
- Already-recorded related evidence, not duplicated here: issue-136d4553ff92 (gen642 be8067 timing/activation + sampler critical-path proposal; 9b2bcf gap and partial-shutdown defect), issue-ed3ac775283d (sampler alignment history), issue-9a4503f6ed68 (9b2bcf wait identity), issue-35fd6df3d947 (process inventory).

DEVELOPMENT-PROCESS REVIEW
- Repeated missing evidence is demonstrated: valid same-run process evidence existed in the sidecar but the summary reported unavailable, forcing manual reconstruction from raw JSONL (be8067; earlier 84be9e/42909c unavailable). I appended one specific proposal with a measurable target and smallest experiment to the sampler alignment improvement issue-ed3ac775283d.
- No separate general-process proposal was added: issue-91e3384b219b already owns and has implemented/proven the bounded review scheduler and stale-issue heartbeat, so a cadence proposal would duplicate it. No scheduler defect is evidenced in this sweep (the scheduled event fired: coord.issue_review.scheduled seq 18506, wake-e60255ff7b85).
