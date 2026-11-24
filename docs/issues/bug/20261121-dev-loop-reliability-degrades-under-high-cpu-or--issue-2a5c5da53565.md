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
