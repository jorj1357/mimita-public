# issue-d5a7e874a0db: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-22T11:32:54.454Z — build-wait-reviewer

related_build: game-dll-20261122T110412831692Z-baa638

## Review (pass_with_findings)

### 2026-11-22T11:34:25.654Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed build/test wait issue-d5a7e874a0db (game-dll-build at game_build, wait_id game-dll-build:2026-11-22T11:04:12.764Z, elapsed 60.17s).

Verdict: legitimate latency wait, not a queue/worker stall. The build succeeded. No code fix is required for this occurrence; the underlying compile/link latency is already owned by issue-136d4553ff92 (build lifecycle / edit-to-active time) and by the cpp-latency-linker-investigation canary recorded on this attempt.

Exact-attempt evidence (.dev/build-timings/game-dll-20261122T110412831692Z-baa638.json):
- success=true, generation=487, started_utc=2026-11-22T11:04:12.831Z, completed_utc=2026-11-22T11:05:58.647Z, total_seconds=105.781.
- worker_metrics: compiled=2, skipped=641, compile_seconds=42.797, link_seconds=21.938, object_scan_seconds=2.875. changed_paths=src/actor/player-ai.cpp.
- The issue reason sources=642 / seeded=0 is the total source count, not a full rebuild: only 2 units compiled and 641 were skipped.

Timeline evidence (.dev/build-timings/game-dll-20261122T110412831692Z-baa638.jsonl):
- The 60.17s wait fired at about 11:05:13Z while real work was in flight; the build then needed about 45s more and completed exit_code=0.
- translation_unit.completed for src/actor/player-ai.cpp = 40.457s; link.gnu_linker = 21.938s; build.completed elapsed_ms=105781. Dominant cost is one heavy TU compile plus an incremental link, not a stall.

Recurrence context (player-ai.cpp across nearby attempts): gen485 38.716s, gen486 27.743s, gen487 40.457s, gen488 40.714s. A ~40s compile is within this TU normal range, so the 60s budget is simply shorter than a normal compile+link for it.

Build notes on this attempt: cd658a118507 (agent cpp-latency-linker-investigation) - temporary comment-only canary to measure one translation-unit compile and changed-manifest codegen invalidation, restored immediately after proof, on src/actor/player-ai.cpp. The adjacent gen488 attempt (107.156s) is the follow-on of the same canary.

Wake evidence (.dev/agent-wake/events.jsonl): exactly one wake.queued for issue-d5a7e874a0db (wake-d0811baf6504, WAKE_f66c0eaa6c4d456090e74243eb0b3555) at 2026-11-22T11:05:16.312Z. Delivery detail: the codex adapter claimed at 11:05:20.885Z but its process exited; the opencode fallback claimed and launched at 11:17:03.206Z / 11:17:03.244Z (about 11.7 min queued-to-launch). That is wake-delivery latency, not build-queue friction.

Issue readiness: ready. wait_id, stage, budget, elapsed, request IDs, build id, reason, reproduction, expected behavior, acceptance, paths, and proof expectation are all present. related_build is now pinned to game-dll-20261122T110412831692Z-baa638.

Acceptance result: satisfied for this occurrence - one issue per wait_id, one reviewer wake queued exactly once, all required wait fields present.

Findings (no duplicate issue created, no source fix claimed):
1. Metadata nuance: issue evidence generation=486 and sources=642 reflect wait-time state and total source count; the completed attempt is generation 487 with 2 compiled and 641 skipped. Not incorrect, but easy to misread as a full rebuild. Diagnostic clarity only.
2. The fixed 60s budget is shorter than a normal heavy-TU compile+link (105.8s here). This is the same systemic false-positive class tracked by issue-9a4503f6ed68 (wait hook policy) and issue-136d4553ff92 (edit-to-active latency); no new issue needed.
3. The wake adapter fallback added about 11.7 min from queued to launched; if it recurs, it belongs to wake-delivery work, not this build wait.
