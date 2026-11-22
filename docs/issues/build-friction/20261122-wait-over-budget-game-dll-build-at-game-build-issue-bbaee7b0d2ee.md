# issue-bbaee7b0d2ee: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-with-evidence-gaps)

### 2026-11-22T13:36:13.759Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Independent review of issue-bbaee7b0d2ee (build-wait-reviewer, wake-70d850300ee2).

VERDICT: confirmed wait, real build friction; issue content has evidence gaps. No code edited, no files claimed, no duplicate issue created.

WAIT AND BUILD CORRELATION (proven from build timings)
- wait_id: game-dll-build:2026-11-22T12:51:33.244Z, stage game_build, elapsed 60.121s, budget 60.0s, request 2026-11-22T12:51:32.959Z-148766b1.
- Matching attempt: game-dll-20261122T125133309168Z-46b1e5 (started 12:51:33.309Z, completed 12:52:42.192Z, success, total_seconds 68.864, generation 491).
- Phase breakdown (ms): changed_source_preflight 31016.683 (contains the single-TU compile), link 10371.265, object_cache_scan 2603.975, coherence_validation 1302.377, compile 1601.071, publish 906.270, source_snapshot 14306.265, toolchain_setup 486.776.
- Slowest compile: src/effects/zombie-tower-effects.cpp = 30.657s. Link input = 480,395,963 bytes / 641 objects.
- Verdict on cause: this is genuine wall-clock build duration, not a stalled or idle wait. At the 60.121s mark the attempt was mid-link after a 30.657s compile of one edited TU; total 68.864s. Next attempt 687e4d (gen 492) was also over budget at 61.407s (preflight/compile 26.5s + link 8.1s), so this is a recurring class, not a one-off reporter glitch.

WAIT POLICY BEHAVIOR (proven)
- One issue per exact wait_id: record_wait_incident dedups by evidence.wait_id (devscripts/build_friction.py:131-169). No duplicate issue exists for this wait_id.
- One reviewer wake queued for this occurrence: .dev/agent-wake/events.jsonl has exactly one wake.queued for wake-70d850300ee2 at 12:52:36.299Z (adapter codex), then a fallback opencode relaunch after the codex adapter process exited. Queued once; launch retried, which is consistent with acceptance.
- issue-f200660c97fe is a DIFFERENT occurrence (wait_id game-dll-build:2026-11-22T13:00:15.598Z, attempt 687e4d, gen 492), not a duplicate of this issue.

EVIDENCE GAPS / DEFECTS (reported, not fixed)
1. reason is malformed. Stored reason begins 55770.586,build_attempt_id:..., a raw truncated worker-output tail, not a readable reason. Owner: devscripts/dev-loop.py:2833 _record_game_build_output sets game_build_detail = line.rstrip()[-240:] (line 2863); wait dict uses reason = self.game_build_detail or self.last_message (line 3539).
2. generation is stale. Evidence says generation 490, but the attempt actually built generation 491. self.game_build_generation is only assigned at result parsing (dev-loop.py:2976), so during the build it retains the prior value (dev-loop.py:3544).
3. build_id is not the in-progress attempt. dev-loop.py:3543 records self.latest_build (prior published build 2026-11-22T08:00:31.376619Z), not build_attempt_id game-dll-20261122T125133309168Z-46b1e5; the attempt id is only buried inside the malformed reason fragment.
4. no run_id in the wait evidence (runtime run 20261122_011031). Acceptance asks for build/run IDs.
5. Dev-loop wait event is missing. record_runtime_event (dev-loop.py:2275-2300) appends to logs/2026-11-22/20261122_011031/dev-loop/events.jsonl, but that directory does not exist (run dir has only server-01 and diagnostics), and the OSError is swallowed. The proof expectation dev-loop wait event cannot be satisfied from the journal; only the issue record and agent-wake record exist.

INTERPRETATION AND NEXT
- The friction is real and rooted in whole-game build cost for a heavy single-TU edit (zombie-tower-effects.cpp) plus source snapshot and link; this overlaps issue-136d4553ff92 (build coordinator, in_review). Any budget-policy change is a human decision.
- Recommended follow-ups (report only): make the wait reason stable and human-readable; record the in-progress build_attempt_id, generation, and run id; ensure record_runtime_event creates/repairs its journal parent (or surfaces the write failure); consider stage- or kind-specific budget for whole-game DLL builds.
- Files to touch if authorized: devscripts/dev-loop.py (game_build_detail/reason, wait evidence fields, runtime journal path), devscripts/build_friction.py (evidence shape). src/effects/zombie-tower-effects.cpp is the build-cost owner but needs no change for the reporter.

CONFIDENCE: high on the 68.864s/61.407s timings and the single-wake/single-issue behavior; high on defects 1-4 from source and stored evidence; medium-high on defect 5 (empirical: no dev-loop/events.jsonl exists for run 20261122_011031).

## Context

### 2026-11-22T13:36:46.653Z — build-wait-reviewer

related_build: game-dll-20261122T125133309168Z-46b1e5
related_run: 20261122_011031
