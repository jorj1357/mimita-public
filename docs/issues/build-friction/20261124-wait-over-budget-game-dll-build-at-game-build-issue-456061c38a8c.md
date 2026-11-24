# issue-456061c38a8c: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (verified-tooling-correct)

### 2026-11-24T04:16:47.197Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed build/test wait issue-456061c38a8c (game-dll-build at stage game_build).

FINDINGS (facts from journal logs/2026-11-24/20261124_022440_01/dev-loop/events.jsonl):
1. The wait was real and legitimate, not wedged: build_attempt_id game-dll-20261124T033057531530Z-a2c134 started 03:30:57.098Z (rev e6c8a891..., source_generation 20) and ended dev-loop.game-build.failed at 03:32:02.444Z with duration_seconds=64.037. The attempt FAILED at changed_source_preflight: 'src/ragdoll/ragdoll-mode.cpp in 44.953s' (game-build.error line 88; file status=failed seconds=35.438 line 83). So the >60s wait reflects a genuinely slow build that also failed, not a stuck wait.
2. Tooling acceptance is met for this occurrence:
   - wait_id game-dll-build:2026-11-24T03:30:57.098Z, stage game_build, elapsed=60.796s, budget=60.0s, build_id=2026-11-24T03:30:52.372066Z, source_generation=20, request_ids=[2026-11-24T03:29:40.150Z-4e3952c7:mode3:game] all present in the issue evidence.
   - Exactly one issue created (dev-loop.wait.over_budget line 90 created=true).
   - Exactly one reviewer wake queued: dev-loop.wait.reviewer-wake line 89 @03:32:00.423Z; agent-wake wake.queued (wake-3e8165012607, adapter codex) at 03:31:59.032Z. Only one wake.queued and one over_budget event exist for this wait_id. no duplicate.
3. Defect (already owned, NOT new): evidence.reason is not human-readable. It is a mid-JSON tail of a [GAME_FILE] worker row: reason='CFG cleanup","raw":"tree CFG cleanup : 0.24 ... ggc... truncated:true},progress_write_ms:4.341,...'. Root cause dev-loop.py:3892 reason=self.game_build_detail (last 240 chars of raw worker output). Same defect tracked with two prior occurrences in issue-6e08d89b55e3 (tech-debt). Appending this occurrence there.

ROOT-CAUSE OWNERSHIP: The recurring slow game-dll-build root cause (slow compile units this attempt: ragdoll-mode.cpp 35.4s, server-gamemode.cpp etc.) is owned by issue-136d4553ff92 (build-coordinator) and issue-e044a931c54b (build-runtime-subagents). The wake/one-issue-per-occurrence mechanism is owned/proven by issue-9a4503f6ed68 (wake-proof-wait).

BOARD NOISE: 147 open issues titled 'Wait over budget: game-dll-build at game_build' exist (129 in_review, 17 new, 1 assigned, none done/cancelled). This is the risk stated on the issue; surfacing to issue-9a4503f6ed68 owner rather than filing another duplicate.

VERDICT: verified-tooling-correct for this occurrence. No code change made in this review. Recurrence is a repeat of the known slow-build + noisy-reason pattern, already owned; no new duplicate issue created.
