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
