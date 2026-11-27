# issue-31ab18dedc96: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-21T21:04:25.079Z — build-wait-reviewer

related_build: game-dll-20261121T201417234689Z-2d9df4

## Review (confirmed-wait-policy-noise)

### 2026-11-21T21:04:36.629Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed this wait occurrence against the pinned build attempt, the dev-loop timeline, and the wait policy. Conclusion: this is not a hung build. It is a healthy whole-game DLL build whose wall time exceeded the flat 60s wait budget, plus a real evidence-attribution gap in the auto-created issue.

WAIT OCCURRENCE (verified)
- wait_id: game-dll-build:2026-11-21T20:14:17.103Z
- started_utc 2026-11-21T20:14:17.103Z; elapsed_seconds 61.158; budget_seconds 60.0; stage game_build.
- The captured reason is not a stall indicator: dev-loop.py:2700-2701 stores the last 240 chars of an output line, so the evidence holds a truncated progress fragment ending with build_attempt_id game-dll-20261121T201417234689Z-2d9df4.
- Reviewer wake was queued exactly once for this wait: wake-d3bb8e5b860e (only request referencing this issue). It was created 20:15:18.901Z, launched on the codex adapter at 20:15:19.661Z, retried on opencode at 20:39:03.561Z after the codex process exited, and launched at 20:58:17.368Z. Queued once, but this review only started ~43 min later.

PINNED BUILD ATTEMPT
- attempt: game-dll-20261121T201417234689Z-2d9df4
- started 20:14:17.235Z; completed 20:17:09.579Z; success=true; total_seconds 172.333; worker_total_seconds 143.658.
- Result: generation 432, build\game\generation-432\game-g432.dll, published 20:17:09.591Z (ready.json). Changed source: src/actor/player-ai.cpp; 12 compiled, 630 skipped.
- Dominant stages (attempt JSONL/JSON): object_cache_scan 98.861s; changed_source_preflight 14.23s; compile phase 18.425s (worker compile_seconds 32.645); link 11.022s; publish 1.674s.
- The 60s threshold fired at 61.158s with roughly 111s of build remaining, and the build then finished cleanly. No stall, no failure, no retry needed.

SPEC / EVIDENCE GAP (first divergence)
- Spec docs/specs/dev-loop/change-to-evidence-latency.md:120-127 requires each auto-created wait issue to contain the exact build/run IDs and timing references and to append later observations.
- This issue cannot name the waiting attempt from its own fields: build_id is self.latest_build, i.e. the latest EXE artifact (2026-11-21T02:26:50...), not the game-DLL attempt. The reported generation 431 is the previous completed game generation, because self.game_build_generation is only assigned after success (dev-loop.py:2838). The in-flight attempt was game-dll-20261121T201417234689Z-2d9df4 -> generation 432.
- Later observations are not appended: build_friction.record_wait_incident supports issue_occurrence (build_friction.py:164-169), but dev-loop suppresses every repeat of the same wait_id through reported_wait_ids (dev-loop.py:3409-3418). This issue therefore still shows occurrences=[] and never receives the terminal result (success at 172.333s).

ASSESSMENT / NEXT
- This occurrence is wait-policy noise, not a hang; flat 60s is below normal whole-game build wall time (object-cache scan alone was ~99s here). The default budget is still an explicit open question in docs/specs/dev-loop/agent-build-queue.md:314.
- The policy and attribution fix belongs to issue-136d4553ff92 (owner build-coordinator), which already scopes full-stage build accounting. Suggested minimal change there: record the active game-DLL attempt id and target generation in wait evidence, and append the terminal result to the existing issue instead of suppressing the second observation. Do not implement here.
- No source files edited, no build or test requested. Confidence: high (pinned attempt summary, attempt JSONL timeline, wake request/events, ready.json).

## Scheduled independent review

### 2026-11-27T20:29:57.722Z — spec-event-build-volumes

A bounded independent review was queued as wake wake-160d27cd3f62 after 862 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (confirmed-wait-policy-noise-attribution-gap-persists)

### 2026-11-27T20:36:09.053Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent review (read-only) of this wait occurrence after issue readiness, proof state, wake records, current dev-loop code, and the newest comparable wait events.

FACTS OBSERVED
- Readiness ready=true, score 8, state in_review, 0 tasks; acceptance/proof expectation present. No task proof exists; the proof expectation is the dev-loop wait event + issue record + reviewer wake record.
- Exactly one reviewer wake was queued for this wait occurrence: wake-d3bb8e5b860e (created 2026-11-21T20:15:18.901Z). The current sweeper wake wake-160d27cd3f62 is a scheduled independent review, not a second wait wake. So the "exactly once" part of acceptance holds.
- That wake request now reports status=failed ("adapter process exited; reconciled by scheduler") yet the reviewer did post review id 60 at 2026-11-21T21:04:36.629Z. Wake created to review posted was ~49m18s.
- The issue still has occurrences=[] and still cannot name the waiting game-DLL attempt in structured fields. build_id is the last published EXE artifact (2026-11-21T02:26:50...), and generation 431 is the previously published game generation.

CODE RE-CHECK (current tree, ready generation 805)
- Attribution defect is still present: dev-loop.py:3897 sets build_id=self.latest_build and dev-loop.py:3898 sets generation=self.game_build_generation, which is only assigned on success (dev-loop.py:3258-3259). For a game-DLL wait these are the EXE artifact and the previous generation, not the in-flight attempt.
- Improvement since the prior review: wait.request_ids now carries the active game-build request IDs (dev-loop.py:3900, populated at 3086-3087/3163) and source_generation carries the active revision (3167-3169).
- Correction to the prior review's causal detail: in-process reported_wait_ids (dev-loop.py:3904) is not the main reason later observations are missing. wait_id embeds game_build_started_utc (dev-loop.py:3890), so each attempt gets a unique wait_id and build_friction._wait_issue (build_friction.py:131-140) can match only on a dev-loop restart mid-wait. More importantly there is no terminal-outcome append at all: record_wait_incident has exactly one caller (dev-loop.py:3908) inside check_wait_budget, and nothing appends success/failure after the build completes. That is the actual missing step versus the spec claim that the dev-loop "appends later observations" (docs/specs/dev-loop/change-to-evidence-latency.md:120-127).

CROSS-OCCURRENCE EVIDENCE
- The same structured-identity defect persists on the newest comparable wait: issue-2aa79d9fe477 (wait_id game-dll-build:2026-11-27T00:23:02.531Z) records generation=null, build_id=2026-11-26T22:52:33.481041Z (last EXE artifact), request_ids present, occurrences=[]. Raw event logs/2026-11-26/20261126_230415/dev-loop/events.jsonl:48 shows the same shape.
- The truncated reason still sometimes carries build_attempt_id (e.g. game-dll-20261127T002302708617Z-988794) but unstructured, while other waits show a readable "[GAME BUILD] revision=..." reason (logs/2026-11-25/20261125_212558/dev-loop/events.jsonl:54). The reason field is inconsistent, matching issue-6e08d89b55e3.

CONTRARY EVIDENCE / UNCERTAINTY
- I could not independently re-verify the prior review's pinned attempt numbers (game-dll-20261121T201417234689Z-2d9df4, generation 432, total 172.333s): .dev/build-timings no longer contains 2d9df4 (oldest retained ~2026-11-25), build/game/generation-432/game-g432.dll is absent, and logs/2026-11-21 does not exist. Only the persisted review text, issue evidence, and wake request remain. This does not contradict the prior review; it means its raw numbers are no longer re-derivable, which is why persisted reports matter.
- The flat 60s budget remains below normal healthy whole-game build wall time; newer over-budget data points in issue-136d4553ff92 occurrences (Nov 26 and Nov 27) show the same false-positive class. This is wait-policy noise, not a hang.

ACCEPTANCE
- wait_id/stage/reason/elapsed/evidence: present. Reviewer wake exactly once for this occurrence: yes. build/run IDs: partially unmet - build_id is the wrong artifact for a game-DLL wait, no run_id, and generation was the previous build. Acceptance is not fully met; this issue should stay open.
- The identity and reason defects are already tracked (issue-6e08d89b55e3, issue-9a4503f6ed68, issue-136d4553ff92). No new defect issue is needed.

NEXT DISCRIMINATING CHECK
- On the next real over-budget game-DLL wait, confirm from the dev-loop event whether wait.request_ids plus a structured build_attempt_id/generation can be carried from the already-parsed [GAME_NOTE]/progress payload, and whether one terminal occurrence (success/failure with attempt id) is appended to the same issue after the build completes. Until then acceptance remains open.

No source edits, no build/test requested, no claim/close. Confidence: high on the code and current-occurrence defect; medium on the historical attempt numbers (artifacts pruned).
