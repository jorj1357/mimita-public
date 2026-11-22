# issue-a59a0058df76: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (not-wedged-policy-miscalibrated)

### 2026-11-22T03:54:34.166Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Review of wait game-dll-build:2026-11-22T03:22:47.155Z (elapsed 60.412s, budget 60.0s).

VERDICT: not-wedged / policy miscalibrated. The build was actively progressing the entire time; the trip is a healthy game-DLL build exceeding a flat 60s budget, not a stalled wait. Same root cause already recorded on sibling issue-6bcfb6252dfa; this entry adds this occurrence's exact evidence rather than a new issue.

EVIDENCE (run 20261122_011031, logs/2026-11-22/20261122_011031/dev-loop/events.jsonl):
- 03:23:50.898 dev-loop.wait.over_budget recorded; elapsed 60.412s >= 60.0s; wait_id game-dll-build:2026-11-22T03:22:47.155Z; request_ids auto-6 + 2026-11-22T03:21:19.164Z-45d0a631.
- Started_utc 03:22:47.155Z equals the instant the PRIOR attempt game-dll-20261122T031729161388Z-2185e7 failed (read-only lastHeardAge/snapshotAge compile error); the new attempt game-dll-20261122T032247328342Z-79f775 began at 03:22:47.328Z. So the clock here is just this build's elapsed time.
- Attempt 79f775 phase durations (.dev/build-timings/game-dll-20261122T032247328342Z-79f775.json): changed_source_preflight 74.96s, compile 110.79s, link 126.98s, publish 9.68s, object_cache_scan 6.58s; total 360.40s. The wait fired at 60s inside the 74.96s preflight, with compile and link still ahead of it.
- Outcome: success. Generation 446 published 03:28:47.747Z (dev-loop.game-generation.ready logged 03:45:09 for the later 447; 446 ready.json shows status=ready).

CAUSE OF THE RECURRING FRICTION: dev-loop.py WAIT_ISSUE_SECONDS = 60 (dev-loop.py:109-110) is applied to game-dll-build at dev-loop.py:3520-3536. Measured whole-game DLL builds are 317-626s with single stages over 60s, so every healthy build emits an identical build-friction issue and reviewer wake. This is one of ~40 such open issues. Spec change-to-evidence-latency.md:122-127 mandates exactly this behavior; agent-build-queue.md:314 still lists the default wait budget as an open question.

MINOR EVIDENCE ACCURACY ISSUE: the recorded evidence has generation=444 but the actual wait was produced by attempt 79f775 (generation 446). self.game_build_generation lags the active attempt because it retains the previous failed attempt's generation at trip time; source_generation=6 is correct for the request.

RECOMMENDATION (human approval needed; no source edited here): give game-dll-build its own budget derived from measured p95, or gate issue creation on absence of forward progress (no new stage/file event within N seconds) instead of flat elapsed. Natural owner: issue-136d4553ff92 (account for every DLL build stage). No files claimed.

## Context

### 2026-11-22T04:03:58.680Z — build-wait-reviewer

related_build: game-dll-20261122T032247328342Z-79f775
