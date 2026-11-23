# issue-b0ba8698acd7: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (evidence-confirmed)

### 2026-11-20T17:20:18.693Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded wake review (wake-f62ce3ee3cb6). This wake was requested to review issue-b0ba8698acd7 and was itself an instance of issue-62ab3daa615d: the opencode session received only the first line of the prompt, so the task was reconstructed from MIMITA_WAKE_ID/MIMITA_WAKE_ISSUE and the on-disk prompt file.

Wait occurrence
- wait_id: game-dll-build:2026-11-20T16:38:24.042Z
- kind/stage: game-dll-build / game_build
- elapsed: 60.024s (budget 60.0s)
- generation: 405, source_generation: 21
- build attempt: game-dll-20261120T163826804025Z-9b84b5

Evidence (read-only, .dev/build-timings/game-dll-20261120T163826804025Z-9b84b5.json)
- success=false, failed_phase=compile, total_seconds=1466.52
- worker_error: "compile failed: 1 file(s) in 336.59s"
- compile error: src/network/server-gamemode.cpp:3949 'const struct GroundState' has no member named 'groundNormal' (also 3950-3951 and a JSON push_back at 3936)
- changed_paths for the attempt: src/actor/player-ai.cpp, src/mapeditor/map-editor-projects.cpp
- worker metrics: compile_seconds=1360.694, object_scan_seconds=500.072, dependency_check_ms=121770.77

Finding
The over-budget wait was not an idle or stale control path: the game-DLL stage legitimately ran far past 60s and then failed with a real compile error in src/network/server-gamemode.cpp:3949 (GroundState::groundNormal does not exist). The compile error is the actionable defect; the 60s wait issue is its symptom. This issue needs no separate code fix beyond dedupe/wait-policy, but the compiler failure needs its own owner/issue.

Duplication note
Adjacent wait issues cover the same build window (issue-bb273ac1386c game-dll-build 17:02:55, issue-40dfaa2a803c game-dll-build 16:21:27). Recommend recording recurrences as occurrences rather than new issues.

Confidence: high for the compile failure and wait timing; medium for the wait-policy dedupe recommendation. No source files were edited.

## Scheduled independent review

### 2026-11-23T12:47:38.741Z — mode3-latency-goal

A bounded independent review was queued as wake wake-2540a9bffc1b after 404 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (evidence-confirmed)

### 2026-11-23T13:02:29.286Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review 2026-11-23 (wake-2540a9bffc1b, agent issue-review-sweeper). Read-only: no source edit, no build, no close, no claim. Prior text preserved; this is append-only.

WHAT I CHECKED (read-only)
- issue brief/readiness: ready=true, score=8, missing=[], state=in_review, tasks=0, proof tasks=0, occurrences=0, reviewers=[build-wait-reviewer].
- linked artifact .dev/build-timings/game-dll-20261120T163826804025Z-9b84b5.json (+ .jsonl timeline).
- wait/wake code: devscripts/build_friction.py, devscripts/dev-loop.py (check_wait_budget).
- wake records: .dev/agent-wake/events.jsonl, .dev/agent-wake/requests/*.json.

OBSERVED FACTS
1. Prior review figures reproduce exactly against the artifact: success=false, failed_phase=compile, total_seconds=1466.52, changed_paths=[src/actor/player-ai.cpp, src/mapeditor/map-editor-projects.cpp], worker_metrics.object_scan_seconds=500.072, worker_metrics.dependency_check_ms=121770.77, compile error at src/network/server-gamemode.cpp:3949 (GroundState has no member groundNormal).
2. Structured wait evidence is present: wait_id=game-dll-build:2026-11-20T16:38:24.042Z, kind=game-dll-build, stage=game_build, reason=[GAME_STAGE] manifest_snapshot started, elapsed_seconds=60.024, budget_seconds=60.0, generation=405, source_generation=21, request_ids=[...068665ac, ...0dec340e, auto-21].
3. Reviewer wake for this occurrence was queued exactly once: wake-f62ce3ee3cb6 (agent build-wait-reviewer) at 2026-11-20T16:39:29.250Z; codex attempt failed, opencode fallback completed the review at 17:20:18. Dedupe is implemented: build_friction._wait_issue keys on evidence.wait_id and records an occurrence instead of a new issue, and dev-loop.py:3772 only calls wake_reviewer when result.created is true. wake-2540a9bffc1b (this session) is the separate scheduled independent review (dedupe_key issue-review-sweep-404), not a duplicate wait wake.

NEW EVIDENCE BEYOND PRIOR REVIEW
4. evidence.build_id points at the wrong build. It is "2026-11-20T07:47:09.981634Z" (the earlier published generation-3 build), not the over-budget attempt game-dll-20261120T163826804025Z-9b84b5 (generation 405). dev-loop.py:3756 sets "build_id": self.latest_build, i.e. the last published build, not the active attempt. The true attempt id survives only inside the free-text "reason" string. A solver following the structured build_id is sent to the wrong artifact. Acceptance clause "carries ... build/run IDs" is therefore only partially met (no run_id exists because this is a build wait, which is acceptable).
5. The referenced artifact carries two contradictory durations: summary total_seconds=1466.52 is corroborated by the timeline build.failed elapsed_ms=1466520, but summary.worker_metrics.total_seconds=2134.073 and summary.worker_total_seconds=2001.281 both exceed the entire started_utc (16:38:26.805) to completed_utc (17:02:53.485) wall clock (~1466.5s). The three fields disagree by ~45%.

INFERENCE / UNCERTAINTY
- Finding 4 is likely a general wait-evidence defect (build_id always = last published generation), not a one-off, inferred from the code line and this single sample. Medium confidence; not yet reproduced live.
- Finding 5 is a real observed inconsistency; root cause is NOT established. It may be a worker-scope monotonic base or an internal worker retry. High uncertainty; do not attribute to a cause without a matched live attempt.
- Acceptance clause "wait_id/stage/reason/elapsed/build/run IDs/evidence" is met except the build_id-target defect in finding 4. Acceptance clause "one reviewer wake per occurrence" is met.

CONTRARY EVIDENCE / CONTEXT
- The immediate trigger was a real compile failure and already has owners: issue-b7216286bdcc and a duplicate issue-b91438ac2972 (identical quote_hash and paths, opened 30s apart at 17:23/17:24). So duplicate friction issues did occur in this window, but through the agent-created path, not the dev-loop wait path; the wait path itself deduped.
- Over-budget waits are not currently reproducing on idle-path builds: current.json generation 559 (2026-11-23T12:25) reports total_seconds=33.127, compile_seconds=15.37. The 60s wait policy is not today's bottleneck.
- Adjacent wait issues referenced by the prior review (issue-bb273ac1386c, issue-40dfaa2a803c) had different wait_ids (17:02:55, 16:21:27), consistent with one issue per distinct wait occurrence.

NEXT DISCRIMINATING CHECK (smallest, cheapest)
A. On the next deliberate/observed over-budget game-dll-build, assert issue.evidence.build_id == the active attempt id (currently expected to FAIL, equalling the last published build). Cheap fix candidate: use the active attempt id (e.g. self.game_build_attempt_id) instead of self.latest_build.
B. On the same run, assert summary.total_seconds == summary.worker_metrics.total_seconds. Both are plain JSON fields; no rebuild of the assertion harness is needed. If unequal, inspect the worker progress started_monotonic base and any retry count in the attempt timeline.

CONFIDENCE
- High: the compile-failure and wait timing facts; one wake per occurrence; finding 4 (build_id mislabel).
- Medium: that finding 4 generalizes.
- High uncertainty: cause of finding 5.
No requirement to change behavior in this task was assumed; no code was modified.
