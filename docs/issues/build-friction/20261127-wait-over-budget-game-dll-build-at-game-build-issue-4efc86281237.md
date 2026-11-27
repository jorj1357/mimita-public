# issue-4efc86281237: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-27T04:46:10.180Z — build-wait-reviewer

related_build: game-dll-20261127T035546740239Z-9b7c2f
related_run: 20261127_032430

## Review (confirmed-build-wait)

### 2026-11-27T04:46:32.223Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer (wake-83be339836a9, session session-7a38174adf54). Read-only: inspected the issue, dev-loop journal run 20261127_032430, and the build timing/resource summaries; no source edits, no builds, no leases.

VERDICT: confirmed-build-wait. This is a TRUE over-budget wait, not a false positive. The waited-on build was progressing normally the whole time; it was simply a full cold rebuild that could not finish within 60s.

WHAT THE WAIT WAS
- wait_id game-dll-build:2026-11-27T03:55:46.569Z, kind game-dll-build, stage game_build, elapsed 61.713s vs budget 60.0s.
- In-flight attempt game-dll-20261127T035546740239Z-9b7c2f, source_generation 8, generation 764, revision efe84567e08dac95bc42514955a80c6f496278e1ec69e47d2c9e0aec7994dea4.
- Trigger request 2026-11-27T03:55:46.197Z-ae89f3c0:mode3:game (Mode 3 batch sealed generation 8, 6 changed compiler inputs).
- Journal: logs/2026-11-27/20261127_032430/dev-loop/events.jsonl (over_budget lines 224, reviewer-wake line 223, build result lines 835-836).

TIMELINE (UTC, from the run journal)
- 03:55:46.569 wait registered started; 03:55:46.759 batch sealed; 03:55:52.902 build_attempt 9b7c2f started (source_generation 8).
- First TUs submitted 03:55:55.6. TUs completed continuously from 03:56:20 onward; detection happened mid-compile, not at a stall.
- 03:57:16.296 dev-loop.wait.over_budget + dev-loop.wait.reviewer-wake (one each) for this wait_id; issue-4efc86281237 created=true.
- 04:11:23.879 build result terminal: status ok, no_op false, generation 764, compiled 646, skipped 0, object_scan 0.078s, compile 922.593s, link 4.906s, total_seconds 936.625.
- 04:11:28.166 dev-loop.game-generation.ready: generation 764 published, success true, duration_seconds 941.016.

CAUSE (observed)
- This attempt was a FULL rebuild, not an incremental build: compiled 646 / skipped 0. The summary lists cache_invalidations of reason "missing or invalid dependency file" across the tree (32 entries shown, spanning src/ui, src/utils, src/vehicles, src/video, src/world, src/worldgen, src/worldproject), and every TU was recompiled. With 10 jobs, compile_sum 9,129,307 ms applied across 646 TUs, mean host CPU 99.66%.
- It follows the same object-cache/link-list regression family already recorded this hour: attempt 487957 (03:51:42) failed at link with undefined MapEditor::* symbols whose definitions exist in-tree, then a cold-link-trial at 03:52:05 reported removed_objects=20, undefined=0. This wait is downstream of that cache regression, not a new independent cause. See docs/regressions/dev-loop/game-dll-link-loses-translation-units-REG-2026-11-27.md and issues issue-4ad4db42be02 / issue-428f7f3b9dba.
- Progress was healthy at detection: the recorded reason tail shows progress_write_ms 5.037 and record_overhead_ms 5.869 for the last completed TU, i.e. no stalled progress write. The 60s budget was exceeded only because the rebuild needed ~936s.

BUILD OUTCOME (separate from the wait)
- Attempt 9b7c2f succeeded and published generation 764 (build\game\generation-764\game-g764.dll), code_hash 17aa27ae81b2aab0a6c1318c8e25d9c6e9785f2f8e091180ee9407a895a2c31c. This is the clean rebuild that cleared the earlier link failures.

RESOURCE CONTEXT (observed, not proven causal)
- .dev/build-timings/game-dll-20261127T035546740239Z-9b7c2f.resource.json: 666 samples @1s over 940.985s; mean_host_cpu_percent 99.66, peak 100.0, minimum_available_memory_bytes 7,168,000 (~6.8 MiB), 10 jobs; sampler thread CPU 218,796 ms. runtime_alignment unavailable (fresh server/client journal not matched). Near-exhausted available memory and saturated CPU are correlates of the long rebuild; they are not proven to have caused the .d invalidation, and unmatched fields remain unknown, not zero. Resource gap keys live on issue-136d4553ff92.

ACCEPTANCE CHECK (verbatim: "The issue carries wait_id, stage, reason, elapsed, build/run IDs, and evidence; a reviewer wake is queued exactly once for this wait occurrence.")
- wait_id: PASS (game-dll-build:2026-11-27T03:55:46.569Z).
- stage: PASS (game_build).
- elapsed: PASS (61.713s vs 60.0 budget).
- evidence: PASS (present on the issue).
- reviewer wake exactly once: PASS. One dev-loop.wait.over_budget and one dev-loop.wait.reviewer-wake for this wait_id (03:57:16.296), wake-83be339836a9; request .dev/agent-wake/requests/wake-83be339836a9.json.
- reason: FAIL. evidence.reason is a raw JSON tail ("\":60079.0,\"build_attempt_id\":..."), not a readable stage/file/elapsed summary. It happens to embed the correct build_attempt_id but is still a slice. Tracked by issue-6e08d89b55e3.
- build/run IDs: FAIL. evidence.build_id is "2026-11-27T01:43:27.658897Z" (an earlier published build), generation is null, and no run_id is recorded, while the actual in-flight attempt was game-dll-20261127T035546740239Z-9b7c2f / generation 764 / run 20261127_032430. Same stale-identity defect as issue-3878b275e5d9 and issue-f36679f4ba95.
- Net: record + wake uniqueness passes; readable reason and correct build/run identity still fail. Acceptance is therefore NOT fully met yet.

RECOMMENDATIONS (report-only; nothing changed or claimed here)
1. Keep the 60s budget for incremental builds, but do not fire the generic over-budget friction for a known full/cold rebuild (skipped==0 or a large cross-tree "missing or invalid dependency file" invalidation set). Either use a per-stage/per-mode budget (full-rebuild budget >> 60s) or suppress when progress is advancing; the sibling issue-3878b false positive shows the same wall-clock-only policy.
2. Complete the cache regression remediation first (human-authorized object-cache quarantine + full rebuild, per AGENTS.md and REG-2026-11-27); the 646-TU invalidation is the dominant avoidable cost and the reason this wait existed at all.
3. Fix the wait snapshot (issue-6e08d89b55e3): store a readable stage/file/elapsed reason and the in-flight attempt id/generation/run_id so the build/run-ID acceptance item is actually met.
4. Consolidate repeated game-dll-build waits under the existing trackers instead of one high-priority issue per wait_id.

STATUS: acceptance fields present for wait_id/stage/elapsed/evidence and exactly one reviewer wake for this wait occurrence; substantive causes (wall-clock-only policy on a full rebuild + stale identity fields + reason slice) remain, and are already tracked. No new umbrella issue created. No runtime/behavior proof claimed (this is a build-friction review only).

## Agent discussion and suggestions

### 2026-11-27T04:47:12.998Z — build-wait-reviewer

Message: `msg-a3ecf9aeca20` (question)

Subject: issue-4efc86281237: full-rebuild wait mode + stale identity

Follow-up to msg-b62be3c079d5 (same wait family, different failure mode). In issue-4efc86281237 the game-dll-build wait fired at 61.713s at the start of a FULL cold rebuild, not a borderline single heavy TU: attempt game-dll-20261127T035546740239Z-9b7c2f compiled 646/skipped 0, compile 922.593s, total 936.625s, and still published generation 764 ok at 04:11:28. A progress-gate alone would NOT have suppressed this wait (progress_write_ms ~5ms, no stall) - it needs a per-mode/per-stage budget or a full-rebuild exception (skipped==0 / large 'missing or invalid dependency file' invalidation set). Stale identity is also present: wait build_id=2026-11-27T01:43:27 (prior build), generation=null, no run_id, while the in-flight attempt was 9b7c2f/gen 764/run 20261127_032430. Smallest useful next check: decide between (a) dynamic budget keyed on incremental-vs-full rebuild + progress and (b) keep 60s but suppress/annotate a known cold rebuild; and confirm issue-6e08d89b55e3 covers populating wait generation/build_id/run_id from game-build.started. Evidence and journal paths are in the issue review. I have not claimed or edited anything.
