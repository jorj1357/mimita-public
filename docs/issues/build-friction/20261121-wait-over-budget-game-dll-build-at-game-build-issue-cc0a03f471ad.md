# issue-cc0a03f471ad: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-recurring-systemic-wait)

### 2026-11-21T22:54:47.036Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded build-wait review by build-wait-reviewer (wake-30dd1065d383). Read-only: no source edited, no build/test requested by this reviewer, no fix or close claimed.

EXACT WAIT
- wait_id game-dll-build:2026-11-21T22:21:50.217Z; attempt game-dll-20261121T222152398558Z-ad2cd0; request_ids [auto-17], source_generation 17.
- Dev-loop run journal logs/2026-11-21/20261121_215237/dev-loop/events.jsonl:
  - line 142 dev-loop.wait.reviewer-wake at 22:22:54.853Z: queue+launch wake-30dd1065d383 for this wait_id, adapter codex.
  - line 143 dev-loop.wait.over_budget at 22:22:54.854Z: elapsed 60.956s, budget 60.0s, issue-cc0a03f471ad, created=true.
- Exactly one issue and one reviewer wake were produced for this exact wait_id, so the acceptance clause ("one visible build-friction issue and one reviewer wake per wait occurrence") is satisfied. Issue record is complete: readiness ready=true, missing=[], and it carries wait_id, stage, reason, elapsed, build/run IDs, paths, reproduction, expected behavior, acceptance, proof expectation.

ATTRIBUTION DEFECT (confirmed, same as sibling issue-a1eebc340521)
- The recorded evidence build_id is the EXE artifact 2026-11-21T02:26:50.306117Z and generation 435 (the game generation published at 22:21:50.168Z, immediately before this wait began). It does not carry the in-flight attempt id ad2cd0 nor its target generation.
- The attempt that actually ran over budget was ad2cd0 and it succeeded as generation 436: .dev/build-timings/game-dll-20261121T222152398558Z-ad2cd0.json reports generation=436, success=true, worker_total_seconds=267.409. Published per build/ready.json at 22:36:56.026Z.
- This matches the defect at devscripts/dev-loop.py:3403-3406, which records self.latest_build / self.game_build_generation instead of the in-flight attempt/generation. Already tracked under issue-136d4553ff92; reported here as cross-link evidence only.

ROOT CAUSE = SYSTEMIC BUDGET MISMATCH (duplicate symptom, not a distinct defect)
- Flat WAIT_ISSUE_SECONDS=60 (devscripts/dev-loop.py:108-109) is applied to a whole-game DLL build (dev-loop.py:3391-3407). Normal whole-game builds take ~267-886s, so the budget must fire on essentially every build attempt.
- wait_id is derived from game_build_started_utc (dev-loop.py:3396) and build_friction.record_wait_incident (devscripts/build_friction.py:143-202) opens a new issue per wait_id, so each build attempt over 60s yields a new "Wait over budget: game-dll-build at game_build" issue plus a reviewer wake.
- Measured noise: 66 open build-friction issues, of which ~40+ share this exact title and were filed 2026-11-20..2026-11-21. This issue is one member of that recurring family; the systemic owner is issue-136d4553ff92 (owner build-coordinator).
- Recommendation (for the systemic owner, not implemented here): stage-aware or no-stage-progress-for-N-seconds budgeting; coalesce per build attempt (one rolling issue per kind+stage, append occurrences) instead of a new high-priority issue per 60s crossing; carry attempt id + target generation in wait evidence.

THIS OCCURRENCE WAS NOT A HUNG WAIT
- At the instant the 60s budget crossed (22:22:50) the build was in manifest_snapshot, not stalled. Per journal line 145 that phase ran 22:22:27.829Z -> 22:29:20.409Z, duration_ms 409414.6 (~409s), source_count 1008. The slow phase is genuine friction, not a deadlock; then source_snapshot took another 111.6s (line 148/149) and the worker 267.4s.

CONCLUSION
- Acceptance for this issue record is met (one issue + one wake, complete fields). The wait is a confirmed recurring systemic budget-mismatch symptom, already owned by issue-136d4553ff92; no duplicate fix issue filed. The evidence attribution defect (generation 435 vs actual 436/attempt ad2cd0) is separately owned by issue-136d4553ff92.
- CONFIDENCE: high for the wait/attribution facts (journal lines 137-150 plus the per-attempt summary .dev/build-timings/game-dll-20261121T222152398558Z-ad2cd0.json and build/ready.json generation 436).
