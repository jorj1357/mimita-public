# issue-8b5c92a76d97: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-observability-defect)

### 2026-11-24T11:16:13.653Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Review of wait occurrence `game-dll-build:2026-11-24T09:46:54.861Z` (agent build-wait-reviewer, wake-0e8ffa7e4b48).

VERDICT: confirmed observability defect. Issue data is complete and exactly one reviewer wake was queued for this occurrence, but the dev-loop journal does NOT contain the proof event the issue requires; it records a misleading `dev-loop.wait.issue-failed` instead.

ACCEPTANCE CHECK
- Data completeness: PASS. Issue carries wait_id, stage=game_build, reason, elapsed=60.072s, budget=60.0s, build_id=2026-11-24T07:48:35.684015Z, generation=587, source_generation=47, request_ids=["2026-11-24T09:46:54.020Z-1b3a3fb7:mode3:game"], started_utc=2026-11-24T09:46:54.861Z.
- One issue per occurrence: PASS. Exactly one build-friction issue carries this wait_id (issue-8b5c92a76d97); no duplicate occurrence was created.
- Reviewer wake queued exactly once: PASS. One request file `.dev/agent-wake/requests/wake-0e8ffa7e4b48.json`; one `wake.queued` event at 2026-11-24T09:47:57.451Z (`.dev/agent-wake/events.jsonl:2645`).
- Proof expectation (journal over_budget event + issue + wake record): FAIL. See below.

EVIDENCE (journal `logs/2026-11-24/20261124_084513/dev-loop/events.jsonl`)
- 09:47:55.150Z issue-8b5c92a76d97 created (dev-loop caught elapsed crossing 60s).
- 09:47:57.425Z wake-0e8ffa7e4b48 queued.
- 09:48:27.872Z line 224 `dev-loop.wait.issue-failed` for this wait_id, error: "Command '[pythonw.exe, .../agent-wake.py, run, --once]' timed out after 30 seconds".
- There is NO `dev-loop.wait.over_budget` and NO `dev-loop.wait.reviewer-wake` event for this wait in the run journal.
- Same pattern for the sibling waits in this run: line 110 (09:33:39.270Z) and line 274 (10:36:51.643Z); 3 `issue-failed`, 0 `over_budget`, 0 `reviewer-wake` total.

ROOT CAUSE (observed code path)
- dev-loop.py:3839-3930 `check_wait_budget` calls `build_friction.record_wait_incident` (succeeds, issue created), then dev-loop.py:3909 `self.reported_wait_ids.add(wait_id)`, then `build_friction.wake_reviewer`.
- build_friction.py:205-249 `wake_reviewer` queues the wake (`agent-wake.py add`, succeeded) and then runs `agent-wake.py run --once` inline with `timeout=30` (build_friction.py:244-246). A cold adapter start exceeds 30s, raising `subprocess.TimeoutExpired`.
- That exception propagates to dev-loop.py:3926, which logs `dev-loop.wait.issue-failed` and skips both the `dev-loop.wait.reviewer-wake` and `dev-loop.wait.over_budget` events. So a successful issue + queued wake is mislabeled as "could not record over-budget wait".
- Because `reported_wait_ids` was already updated, the wait is not retried through this path; the persisted wake request was later drained by the scheduler.
- Inference (not directly measured): the inline 30s launch timeout is shorter than a cold adapter start; the failure is in the launch side-effect, not in issue recording.

WAKE TIMELINE (`.dev/agent-wake/events.jsonl`, log `2026-11-24T09-47-57.425Z-build-wait-reviewer.log`)
- 09:47:57.451Z wake.queued -> 10:20:26.775Z wake.claimed (codex, pid 2676) -> logged `model_not_found: 'gpt-6-luna' model is not supported` -> 10:54:27.013Z wake.retry (opencode) -> 10:54:31.471Z wake.claimed/launched (pid 27948). Dispatch delay ~66 min; this late-dispatch/adapter-health behavior is already tracked by issue-84c584957e65 (and earlier wake-adapter issue-36579a10979a), so it is not re-filed here.

SECONDARY EVIDENCE-QUALITY (inference)
- Issue `evidence.generation=587`, but the corresponding journal attempt `game-dll-20261124T094655760219Z-f5a080` records `generation: 589` and `source_generation: 47`. `check_wait_budget` reads `self.game_build_generation` (dev-loop.py:3897), which is only updated on completion (dev-loop.py:3259), so an in-flight build can report the previous generation.
- Issue `build_id` is the EXE build id (2026-11-24T07:48:35.684015Z), not the DLL attempt id; the actual attempt id exists only in the journal.

BUILD CONTEXT (observed)
- The flagged attempt f5a080 was not a stall: it failed compile at 09:50:04.719Z after 146.03s (`dev-loop.game-build.error`, paths engine-tick-ui.cpp, engine-tick-ui-game-hud.cpp, engine-tick.cpp), revision 2ec4e144... (source_generation 47). This matches earlier evidence that a flat 60s total-elapsed budget flags healthy/long incremental game-DLL builds rather than hangs.

SCOPE / OWNERSHIP
- No code edited or committed (requires human approval per wake rules).
- Observability/mislabel fix belongs with issue-136d4553ff92 (owner build-coordinator, "eliminate unmeasured gaps in live per-attempt logs"); evidence appended there.
- Late wake dispatch belongs with issue-84c584957e65. Build-budget tuning (60s flat threshold) remains a separate policy decision.

SMALLEST USEFUL NEXT CHECK
- The defect is deterministic whenever the inline `agent-wake.py run --once` exceeds 30s, so no multi-run repro is needed. Suggested safe fix (for the owner to approve): make the inline launch side-effect non-fatal to `check_wait_budget`, emit `dev-loop.wait.over_budget`/`reviewer-wake` based on `record_wait_incident` + queued outcome (and reserve `issue-failed` for genuine record failures), move `reported_wait_ids.add` after the wake is queued, and record the in-flight attempt generation from the build attempt id rather than the stale last-completed value.

CONFIDENCE: high for the proof-event defect and one-wake/one-issue facts (direct journal evidence); medium for the exact 30s-vs-cold-start cause (inference from the timeout message and timings).
