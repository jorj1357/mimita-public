# issue-ca900e60ed4b: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (reviewed-duplicate-wait-policy)

### 2026-11-21T16:19:47.614Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed occurrence: wait_id game-dll-build:2026-11-21T15:52:23.943Z (build_attempt game-dll-20261121T155224262249Z-d0908c, generation 420, source_generation 1, request auto-1).

Evidence
- Issue evidence: budget_seconds=60.0, elapsed_seconds=60.05, stage=game_build, started_utc=2026-11-21T15:52:23.943Z.
- .dev/build-timings/game-dll-20261121T155224262249Z-d0908c.json: started 15:52:24.289Z, total_seconds=122.986, success=false, failed_phase=changed_source_preflight.
- Timeline .jsonl: at the 60s budget the build was inside manifest_snapshot (15:52:33.9 -> 15:53:05.3, 31.3s) transitioning to source_snapshot (15:53:05.5 -> 15:53:33.4, 27.9s). The reason field's '[GAME_STAGE] source_snapshot started ... elapsed_ms 41194' is active progression, not a stall. Pre-compile fixed overhead (codegen ~9s + manifest_snapshot 31.3s + source_snapshot 27.9s) already exceeds the 60s budget before a single TU compiles.
- Build then failed at 122.99s on a real (now-removed) source error: src/actor/player-ai.cpp:1010 `'maybeRequestFallback' was not declared in this scope`. Current source has no `maybeRequestFallback` references (rg across src/, zero matches), so that compile failure is stale/transient and not the wait cause.
- Wake: .dev/agent-wake/requests/wake-e96c02d2d614.json created 2026-11-21T15:53:24.986Z, issue=issue-ca900e60ed4b, status=launched. Exactly one reviewer wake for this wait_id; dedupe by wait_id works.

Finding (first divergence)
The wait policy is the fault, not the build. devscripts/dev-loop.py:108-109 sets one global WAIT_ISSUE_SECONDS=60; check_wait_budget() at devscripts/dev-loop.py:3391-3407 flags game-dll-build only when total elapsed since game_build_started_utc >= that budget, with no per-stage budget and no progress/stall test. Every non-trivial game-DLL build whose pre-compile stages exceed 60s trips it. This is the same root cause already documented on issue-450cfc80a0b9 (verdict reviewed-needs-policy-fix) and belongs to parent issue-e044a931c54b (owner build-runtime-subagents) plus measuring issue-7d3dcfd7433b. This occurrence is a duplicate false positive; do not open a new fix here.

Field-fidelity gap
The evidence bundle's build_id is the latest EXE build '2026-11-21T02:26:50.306117Z' and generation=null, while the actual DLL attempt id (game-dll-...-d0908c, generation 420) appears only inside the free-text reason. Acceptance requires build/run IDs, so recommend adding a structured build_attempt_id (and the DLL generation) to the wait payload at dev-loop.py:3391-3407.

Recommendation for owner (report only; reviewer edited nothing)
1. Replace the flat 60s total-stage budget with stage-aware budgets AND progress detection (flag only when the stage detail/timeline has not advanced for N seconds).
2. Deduplicate by (kind, stage, active build attempt/generation) and append occurrences to the parent instead of one issue per wait-start timestamp.
3. Include structured build_attempt_id/generation in the wait evidence.

Verdict: reviewed-duplicate-wait-policy. Valid occurrence, false-positive wait (build actively progressing), same root cause as issue-450cfc80a0b9; no new code change required for this issue.

## Scheduled independent review

### 2026-11-26T19:05:43.252Z — rocket-league-match

A bounded independent review was queued as wake wake-03c30d93364a after 789 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (reviewed-evidence-decayed-root-cause-persists)

### 2026-11-26T19:09:04.086Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent review (issue-review-sweeper, 2026-11-26) of issue-ca900e60ed4b. Read-only: no source edited, no build/test started, no issue closed, no fix claimed.

Observed facts
- Brief/readiness: ready=true, missing=[], score 8/8, state=in_review, owner unset. One prior review (build-wait-reviewer, 2026-11-21T16:19:47Z, verdict reviewed-duplicate-wait-policy). This issue has occurrences=[] — no recurrence was appended here.
- Acceptance content is present but partly free-text: wait_id game-dll-build:2026-11-21T15:52:23.943Z, stage game_build, elapsed 60.05s, request_ids [auto-1], source_generation 1, generation=null; the real attempt id game-dll-20261121T155224262249Z-d0908c appears only inside the free-text reason.
- Linked-artifact decay: the prior review's primary artifacts are gone. .dev/build-timings/game-dll-20261121T155224262249Z-d0908c.json/.jsonl does not exist (oldest retained game-dll timing is game-dll-20261124T230934953855Z-9ace92), and .dev/agent-wake/logs/2026-11-21T15-53-24.986Z-build-wait-reviewer.log is also missing. So the prior 122.986s / failed_phase=changed_source_preflight / manifest_snapshot->source_snapshot timeline claims can no longer be independently re-verified from retained artifacts.
- Wake-status fidelity gap: .dev/agent-wake/requests/wake-e96c02d2d614.json is status=failed, attempts=2, error "adapter process exited", updated 2026-11-21T16:29:06.625Z — contradicting the prior review's "status=launched". events.jsonl sequence: queued 15:53:25.013Z -> codex claimed/launched 15:53:26.105Z -> wake.retry to opencode 15:55:28.992Z -> opencode claimed/launched 16:07:36.556Z -> wake.failed 16:29:06.626Z. The review body landed at 16:19:47Z while the opencode launch was active. So a reviewer wake was queued exactly once (acceptance satisfied), but its terminal status is "failed" although a review was produced.
- Root cause persists in current code: dev-loop.py:114-115 still defines one global WAIT_ISSUE_SECONDS (default 60); the game-dll arm of check_wait_budget (dev-loop.py:3884-3900) flags purely on utc-age since game_build_started_utc with no per-stage budget and no progress/stall test; reported_wait_ids dedupe (dev-loop.py:3901-3904) is keyed on the start timestamp (:3889); build_friction._wait_issue/record_wait_incident (build_friction.py:131-147) dedupe by exact wait_id only, so each new start timestamp still spawns a new issue. The "duplicate false positive" mechanism is unabated.
- Field fidelity partially improved: the current payload now carries structured generation/source_generation (dev-loop.py:3897-3898), unlike this occurrence's generation=null; but structured build_attempt_id is still absent (only in the free-text reason), so acceptance's "build/run IDs" is only partly structured.

Inference / uncertainty
- Inference (medium confidence): the 2026-11-21 occurrence was an active-progress false positive, not a stall — supported structurally by pre-compile fixed overhead already exceeding 60s and by the reason's stage marker, but NOT re-verifiable now that the timing/timeline artifacts are pruned.
- Uncertain: whether a wake whose adapter produced the expected review should still terminate as failed; the retained records cannot show whether opencode wrote the review then exited non-zero.

Contrary evidence
- Not a universal false positive: retained 2026-11-26 game-dll waits were 60.5-62s and several game-dll builds finish under 60s, so the detector fires intermittently, not on every build.
- Recurrence lives in sibling issues, not this one; this issue itself stayed at a single occurrence.

Next discriminating check
- Convert "plausible active-progress false positive" into proof using a still-retained 2026-11-26 occurrence: for issue-a02e5a6588db (started 17:20:46.773Z, generation 703, build game-dll-20261126T172047133775Z-30f81f) read .dev/build-timings/game-dll-20261126T172047133775Z-30f81f.jsonl and mark whether the 60s point falls inside an advancing phase boundary or a real gap >~15s.
- Separately confirm in devscripts/agent-wake.py whether an adapter that writes the review should flip the request to succeeded; if not, treat status=failed as a reporting defect for issue-84c584957e65.

Verdict: reviewed-evidence-decayed-root-cause-persists. Valid detector firing with a coarse policy; this occurrence's remediation belongs to the parent/measurement issues, so no new code change is specific to this occurrence. Cold-handoff caveat: the prior review's linked artifacts are no longer retained.
