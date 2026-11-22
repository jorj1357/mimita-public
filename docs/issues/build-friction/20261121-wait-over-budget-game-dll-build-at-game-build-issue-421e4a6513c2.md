# issue-421e4a6513c2: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (reviewed)

### 2026-11-22T00:16:39.849Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed exact wait occurrence in a bounded activation. No source edits, no file claims, no fix claimed.

Wait identity
- wait_id game-dll-build:2026-11-21T23:53:56.960Z; kind game-dll-build; stage game_build; elapsed 60.391s vs 60.0s budget; request auto-18; source_generation 18.
- Exactly one issue exists for this wait_id (searched all build-friction issues by evidence.wait_id), so wait_id dedupe held; issue opened 2026-11-21T23:54:57.374Z.
- Exactly one reviewer wake wake-6ce039d9862c was queued at 23:54:57.864Z for this issue. The wake log shows one wake.queued, then a wake.claimed/launch, a wake.retry at 00:04:26.259Z when the first adapter process exited, and a fallback wake.claimed/launch. That is one queued request with adapter fallback, not a duplicate wake. Acceptance "reviewer wake queued exactly once" holds.

What the wait was
- In-flight attempt game-dll-20261121T235357368634Z-2a4776 (generation 437, source_generation 18), triggered by a one-line comment change to src/npc/npc.cpp (build note beb7cc126625, agent cpp-latency-pressure-profile). It was not a hang; it completed successfully at 2026-11-21T23:57:40.529Z, duration_seconds 222.06, total_seconds 222.017, worker_total_seconds 91.495.
- Timeline from .dev/build-timings/game-dll-20261121T235357368634Z-2a4776.jsonl: game_build start 23:53:56.960Z; resident_state codegen 23:53:57; packet_registry codegen 23:54:00.9 (elapsed 3.49s); manifest_snapshot started 23:54:34.5 (elapsed 37.1s); manifest_validation 23:55:10.1 (72.7s); source_snapshot 23:55:11.9 (74.5s); saved compiler inputs captured 23:55:54.5; npc.cpp compile 42.911s; remaining compiles to 23:57:02; link 22.474s; publish 0.529s.
- The 60s check fired at 23:54:57 during the fixed prelude (manifest_snapshot, before source snapshot/compile/link). So the build still had roughly 2.7 minutes of work left. The wait is a true positive for the 60s threshold but matches this host's normal non-trivial game-DLL baseline rather than an anomaly.

Attribution defect (evidence-backed, same class as prior reviews)
- The structured evidence fields build_id=2026-11-21T02:26:50.306117Z and generation=436 are the previously published/selected artifact, not the in-flight attempt. The in-flight identity (attempt 2a4776, generation 437, stage manifest_snapshot) appears only inside free-text reason.
- Consequence: structured build_id/generation on the issue cannot be used to correlate to the build that caused the wait; only wait_id and the reason text are reliable.
- Fix candidate (not claimed): while game_build is running, populate structured build_attempt_id/generation/stage from the active build state instead of latest_build/game_build_generation.

Evidence availability (met this time)
- Dev-loop journal exists and is readable: logs/2026-11-21/20261121_224058/dev-loop/events.jsonl. Line 43 dev-loop.wait.reviewer-wake and line 44 dev-loop.wait.over_budget both carry wait_id game-dll-build:2026-11-21T23:53:56.960Z and issue-421e4a6513c2. This satisfies the proof expectation (dev-loop wait event plus issue record plus reviewer wake record). Unlike earlier occurrences, no journal gap applies here.

Recurrence and budget calibration
- The class recurs: another game-dll-build wait fired at 2026-11-22T00:06:30.601Z (elapsed 60.756s) and created issue-d220df93f7cb, a distinct wait_id, so it is policy-conformant rather than a duplicate of this issue.
- Brain currently holds 48 "Wait over budget: game-dll-build at game_build" issues; 27 were created since 2026-11-21T00:00, 16 are still new and 32 in_review. That volume shows the flat 60s game_build budget is below the measured game-DLL build baseline and fires on nearly every build, not that each build is anomalous.
- Recommendation (report only, not claimed): make the wait budget kind- and stage-aware, calibrating game_build against the measured prelude plus compile/link baseline (this occurrence 222s; the 60s check sits inside a 74.5s prelude before compile), and record the active build identity in the structured wait fields. Cross-referenced on wait-policy owner issue-e044a931c54b; final policy decision remains with a human.

Acceptance for this occurrence: met, including the dev-loop journal portion. The recurring cause is the 60s budget calibration, not this wait being abnormal.
