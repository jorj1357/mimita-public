# issue-4409099b6172: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (needs-action)

### 2026-11-22T02:22:55.440Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer (review of wait occurrence, no code edits)

VERDICT: needs-action (record is complete; wait-budget policy produces false positives and duplicate issues).

1) This occurrence was a NORMAL build, not a stall.
- Dev-loop journal logs/2026-11-22/20261122_011031/dev-loop/events.jsonl:128 (dev-loop.wait.over_budget) and :127 (dev-loop.wait.reviewer-wake) captured wait_id game-dll-build:2026-11-22T01:38:41.090Z, stage game_build, reason manifest_snapshot started 01:39:07.819Z (23.797s into the attempt), elapsed 60.827s vs budget 60.0s, request_ids [auto-6, 2026-11-22T01:33:15.324Z-b394e61b].
- The build attempt game-dll-20261122T013844014631Z-d09614 SUCCEEDED: generation 440, revision 7e7b5f25b5b9038a9df66a27c961754310d0877221dc1d2decbf7624d4a20fbd, published 2026-11-22T01:47:28.442Z (.dev/build-timings/game-dll-20261122T013844014631Z-d09614.json success=true).
- Measured phase costs: total 524.297s, worker 263.515s, changed_source_preflight 111.719s, compile 70.766s wall (708.320s summed over 10 jobs), link 55.281s, object_cache_scan 19.922s. Slowest TU npc.cpp = 110.879s.
=> changed_source_preflight alone (111.7s) and npc.cpp compile alone (110.9s) each exceed the 60s budget. Every real DLL build that touches npc.cpp or runs changed-source preflight will trip this wait by construction. The wait was ~7.5 minutes of normal build before a successful publish, not a hang.

2) The noisy-wait-policy duplicate-issue risk is realized.
- record_wait_incident (devscripts/build_friction.py:131-146) keys uniqueness on wait_id, and dev-loop.py:3525 derives wait_id as game-dll-build:<started_utc>, i.e. one NEW issue per build attempt. There are currently about 50 open build-friction issues with the identical title Wait over budget: game-dll-build at game_build (e.g. issue-44d82d8626ad, issue-a1eebc340521, issue-585ca55b5bb8, issue-9306252422c4, issue-00d61cbc12a2, plus this one). Each also woke a reviewer.
- This is by design per the build_friction.py docstring (a new wait gets another issue even when stage and reason are identical) and is the documented risk on this issue.

3) Acceptance check for this occurrence: PARTIAL.
- wait_id, stage, reason, elapsed, evidence, and reviewer wake: PASS (wake queued exactly once via dev-loop.py:3538-3548 reported_wait_ids guard; issue uniqueness by wait_id at build_friction.py:164-169).
- build/run IDs: PARTIAL. evidence.build_id is the last successful EXE build id (2026-11-21T02:26:50.306117Z), NOT the active game-DLL attempt id. The real attempt id game-dll-20261122T013844014631Z-d09614 appears only inside the free-text reason. generation recorded as 439 while the attempt published generation 440.
- Recommendation: add first-class build_attempt_id (and final generation) to the wait evidence, not only inside reason.

4) Suggested fix (policy; not applied here).
- Make the wait budget stage- and workload-aware instead of a flat 60s (baseline plus expected compile/link/preflight for the changed set), and treat a no_op build differently from a real rebuild.
- Cluster repeat waits by kind and stage within a bounded window and append an occurrence instead of minting a new issue per attempt; keep one reviewer wake per cluster/window.
- Track this under canonical issue-9a4503f6ed68 (owner wake-proof-wait) and coordinate with issue-7d3dcfd7433b; do not create another duplicate wait issue.

Confidence: high (journal event plus build-timings summary directly observed). No source was edited; this review is evidence only.
