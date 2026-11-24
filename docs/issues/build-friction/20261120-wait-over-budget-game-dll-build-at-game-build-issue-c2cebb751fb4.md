# issue-c2cebb751fb4: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Scheduled independent review

### 2026-11-23T19:34:24.125Z — hot-reload-latency-root

A bounded independent review was queued as wake wake-a33d0b4713e3 after 439 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (confirmed-build-wait)

### 2026-11-23T19:38:49.257Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Reviewer: issue-review-sweeper

Bounded independent review (wake-a33d0b4713e3, coordination session session-fa6fb87f74c7). Read-only: no source edits, no builds, no leases, no close, no owner claim.

Issue completeness: readiness ready=true, missing=[], score 8/8. No prior review existed (reviews=[]); the only earlier artifact was the 2026-11-23 scheduling note. This is the first substantive review.

OBSERVED FACTS (direct evidence)
- Occurrence is valid, not a policy false positive: wait game-dll-build:2026-11-20T18:27:26.113Z, budget 60s, elapsed 61.047s. The in-flight attempt is game-dll-20261120T182726149737Z-df8100 (.dev/build-timings/game-dll-20261120T182726149737Z-df8100.json): started 18:27:26.149Z, success=true, generation 409, source_generation 29, request_ids ['2026-11-20T18:27:25.930Z-92c342cb'], completed 19:04:53.819Z, total 2247.633s (~37.5 min). At the 61s mark the build was actively progressing: ~27s into changed_source_preflight (started 18:28:00.137Z, 89.1s), compiling physics-collision-glb-sweep-slide (53.5s), multiplayer-tick (88.2s), and server-packets (88.5s). No stall at fire time.
- Dominant real cost in this occurrence: compile_seconds 2026.98s (265 TUs compiled, 377 skipped) plus object_cache_scan 131.5s, of which scan_parse_ms 126895.7ms (~126.9s). link 32.9s, publish 14.6s, changed_source_preflight 89.1s. The 60s trigger captured a tiny early slice of an intrinsic ~37.5-min rebuild of 3 changed sources.
- Wake queued exactly once for this wait_id: exactly one wake.queued for wake-2ed65f7d688f (build-wait-reviewer) at 2026-11-20T18:28:28.097Z (.dev/agent-wake/events.jsonl:295). The 2026-11-23 wake-a33d0b4713e3 is the separate scheduled issue-review sweep, not a second wait wake.

CONTRARY EVIDENCE / ACCEPTANCE GAP
- "build/run IDs" is NOT satisfied for the in-flight attempt. The issue evidence carries build_id=2026-11-20T17:17:26.879159Z and generation=405, i.e. self.latest_build / self.game_build_generation (set on completion), a prior build, not attempt df8100/gen 409 (devscripts/dev-loop.py check_wait_budget game-dll branch, ~3878-3879). source_generation 29 and request_ids DO match the attempt, so the request link is correct, but the attempt identity is only inferable from .dev/build-timings. Same defect already documented on issue-9a4503f6ed68.
- The single reviewer wake failed and produced no review: codex launch 18:28:29 -> wake.retry to opencode 19:05:56 -> opencode claimed/launched 19:43:26 (~75 min after queue) -> wake.failed "adapter process exited" 20:19:28 (.dev/agent-wake/events.jsonl:297,298,307,314,315,345; requests/wake-2ed65f7d688f.json status=failed, attempts=2). No review was appended; the issue stayed state=new for ~3 days until this independent sweep. So "queued exactly once" holds, but "wakes one reviewer automatically" delivered 0 reviews for this occurrence.
- Duplicate-signature noise: the issue's own risk ("a noisy wait policy could create duplicate friction issues") is realized. Same-titled "Wait over budget: game-dll-build at game_build" issues already include issue-dc6792df3815 and issue-194c538d7cbc (both reviewed 2026-11-21) and many more; build_friction.record_wait_incident intentionally mints one issue per wait_id (docstring lines 143-148), so identical kind/stage yields a new issue+wake per attempt. This is not a new finding; it is tracked on issue-9a4503f6ed68 and issue-e044a931c54b task wait-instrument.

PROOF-EXPECTATION STATUS
- Issue record: present. Reviewer wake record: present but terminal failed. Dev-loop wait event row: NOT retained - logs/2026-11-20 is absent/empty and no 2026-11-20 dev-loop events.jsonl exists; only the build-timings timeline df8100.jsonl survives. So the stated proof expectation ("Dev-loop wait event plus the issue record and reviewer wake record") is only 2/3 directly available.

INFERENCE / UNCERTAINTY
- Confidence high that (a) the wait was a true positive (build running, then succeeded) and (b) the attempt/gen misattribution and wake failure are real, because both come from direct files. Medium on whether the failed adapter exit shares a root cause with the documented opencode first-line/delivery defects (issue-62ab3daa615d); this failure is recorded only as "adapter process exited" with no prompt-length error, so the link is unproven.

NEXT DISCRIMINATING CHECK
- On the next real game-dll-build over-budget occurrence, inspect in one pass: .dev/build-timings/<attempt>.json (build_attempt_id, generation, completed_utc), the issue evidence (must carry the in-flight attempt id/generation, not self.latest_build), .dev/agent-wake/requests/<wake>.json (terminal status + attempts), and whether a review row was appended within 60 min. Pass = attempt identity matches AND exactly one wake AND a review (or a recorded missing-review task).

PROCESS-REVIEW
- The two process gaps evidenced here (a failed reviewer wake yielding no review for ~3 days; duplicate-signature issues) are already filed on issue-9a4503f6ed68 with measurable targets and smallest experiments (e.g. at most 1 new game-dll-build issue/day while retaining every wait_id; 0 wait issues whose reviewer wake failed with no review after 60 min; a unit test feeding two distinct wait_ids). I am not adding a duplicate proposal; this occurrence is additional confirming evidence for those existing entries. No new process improvement beyond them is supported by this sweep.

Verdict: confirmed-build-wait (true over-budget wait, genuine progress, issue acceptance partially met). Evidence review only; no behavior proof, no fix, no owner assigned. Suggest state in_review.
