# issue-f645dfd6c879: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-22T12:08:38.303Z — build-wait-reviewer

related_build: game-dll-20261122T110831838766Z-61b222

## Review (pass_with_findings)

### 2026-11-22T12:11:59.543Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed build/test wait issue-f645dfd6c879 (game-dll-build at game_build, wait_id game-dll-build:2026-11-22T11:08:31.753Z, elapsed 60.488s).

Verdict: pass_with_findings. The wait policy worked for this occurrence: one issue for this wait_id, one reviewer wake queued. The underlying build is a legitimate long build, not a queue/worker stall. No code fix is claimed for this occurrence; build latency is owned by issue-136d4553ff92 and wait-policy calibration by issue-9a4503f6ed68.

Exact-attempt evidence (.dev/build-timings/game-dll-20261122T110831838766Z-61b222.json):
- success=true, generation=488 (see finding 1), started_utc=2026-11-22T11:08:31.838Z, completed_utc=2026-11-22T11:10:19.033Z, total_seconds=107.156.
- worker_metrics: compiled=2, skipped=641, compile_seconds=42.751, link_seconds=20.875, object_scan_seconds=2.844; slowest compiles src/actor/player-ai.cpp=40.714s and module-identity-provider=1.407s.
- phase_durations_ms: changed_source_preflight=41282, compile=1500, link=21781, object_cache_scan=2875, publish=469.
- reason sources=642 is total source count; only 2 objects compiled and 641 skipped, so this was an incremental build, not a full rebuild.

Timeline evidence (.dev/build-timings/game-dll-20261122T110831838766Z-61b222.jsonl):
- build.started 11:08:31.838; manifest/source snapshot stages consume about 36s; player-ai.cpp translation unit starts 11:09:09.703 and completes 11:09:50.453 (40.714s).
- The 60.488s wait fired about 11:09:32 while player-ai.cpp compile was in flight; lld link ran 11:09:56-11:10:17 (20.875s); build.completed exit_code=0 at 11:10:19.029.
- No stall: the fixed 60s budget is simply shorter than manifest/snapshot plus heavy TU compile plus link for this attempt.

Build note on this attempt f6dbf83aee10: agent cpp-latency-linker-investigation, "Restore the temporary comment-only player-ai.cpp compile canary after measuring generation 487 activation", src/actor/player-ai.cpp. This is the intentional canary family; adjacent issue-d5a7e874a0db is the gen487 half of the same canary.

Wake evidence (.dev/agent-wake/events.jsonl, requests/wake-60247911e4e6.json):
- Exactly one wake.queued for this issue: wake-60247911e4e6 at 11:09:34.999Z (WAKE_d611d248d30c40ce9fac2101b4737057).
- codex claimed at 11:09:39.820 and launched pid 28400 at 11:09:39.877, then its process exited; retry to opencode logged at 11:17:01.981. opencode claimed and launched at 11:49:12.249/.291 (pid 21976).
- Acceptance "reviewer wake queued exactly once" is satisfied. The roughly 39.6 min queued-to-launch delay is wake-delivery latency (adapter exit plus fallback scheduling), a separate class from this build wait.

Duplicate/noise check:
- 108 game-dll-build wait issues exist, with 108 distinct wait_id values and 0 duplicate wait_id values. Dedupe by exact wait occurrence is working and this issue has no duplicate.
- However, the same intentional canary produced two issues minutes apart (issue-d5a7e874a0db gen487 wait 11:04:12; this one gen488 wait 11:08:31). Per policy each wait occurrence gets an issue, so this is not a dedupe defect, but canary/benchmark builds may warrant temporary suppression or a higher budget.

Findings (no source fix claimed):
1. Evidence generation is stale by one: issue evidence records generation=487, but build attempt 61b222 published generation 488. Cause: check_wait_budget reads self.game_build_generation at devscripts/dev-loop.py:3544 while that value is only set on publish at devscripts/dev-loop.py:2976. Diagnostic clarity only; the in-flight attempt id would be less ambiguous.
2. Fixed 60s WAIT_ISSUE_SECONDS is below a normal heavy-TU incremental build on this machine (~105-107s for this canary pair), producing near-routine issues. Tracked by issue-9a4503f6ed68 (wait hook policy) and issue-136d4553ff92 (build stages/latency).
3. Wake fallback latency of roughly 40 min queued-to-launch is wake-delivery work, not build-queue friction.

Readiness: ready; wait_id, stage, budget, elapsed, request IDs, reason, reproduction, expected behavior, acceptance, paths, and proof expectation are all present. related_build is pinned to game-dll-20261122T110831838766Z-61b222.

Acceptance result: satisfied for this occurrence - one issue for this wait_id, one reviewer wake queued exactly once, and required wait fields present.
