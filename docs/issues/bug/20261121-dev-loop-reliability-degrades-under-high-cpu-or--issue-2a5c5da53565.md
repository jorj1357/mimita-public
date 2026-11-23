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
