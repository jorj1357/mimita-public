# issue-6613d3354c65: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-24T11:11:12.453Z — build-wait-reviewer

related_build: game-dll-20261124T093339904951Z-946024 gen588 srcgen46; exe build 2026-11-24T07:48:35.684015Z
related_run: 20261124_084513

## Review (needs-fix)

### 2026-11-24T11:13:27.453Z — build-wait-reviewer

Reviewer: build-wait-reviewer


Reviewer: build-wait-reviewer (wake wake-7786fb770571)

Scope: verify the issue record and reviewer-wake behavior for over-budget game-dll-build wait game-dll-build:2026-11-24T09:33:39.270Z.

VERDICT: acceptance NOT met. The issue record is unique and complete on most fields, but the wake-delivery path logged a false failure and the reviewer was not delivered for about 80 minutes.

ACCEPTANCE CHECK (issue-6613d3354c65)
- wait_id present and unique: game-dll-build:2026-11-24T09:33:39.270Z. Only one build-friction issue carries this wait_id (verified across the store).
- stage: game_build. reason: [GAME BUILD] revision=0057bf... sources=644 jobs=10 priority=high. elapsed: 61.502s vs budget 60s (WAIT_ISSUE_SECONDS default 60; crossed by 1.502s).
- build/run IDs: PARTIAL. evidence.build_id=2026-11-24T07:48:35.684015Z is the current EXE/runtime build, not the DLL attempt. The attempt actually building at 09:33:39 was game-dll-20261124T093339904951Z-946024 (gen 588, source_generation 46). No run_id was in evidence. Both are now linked via issue context (related_build / related_run=20261124_084513).
- evidence: present.
- wake queued exactly once: YES. agent-wake events contain exactly one wake.queued for wake-7786fb770571 at 09:34:42.740Z; no duplicate issue and no duplicate wake for this wait_id.
- wake delivered promptly: NO.

FIRST DIVERGENCE (dev-loop journal run 20261124_084513)
- 09:34:42.740Z wake.queued (wake-7786fb770571, issue-6613d3354c65).
- 09:35:13.086Z dev-loop.wait.issue-failed {wait_id, error: agent-wake.py run --once timed out after 30 seconds}.
Cause: build_friction.wake_reviewer() runs agent-wake.py run --once synchronously with timeout=30 and returns only after it completes. subprocess.TimeoutExpired escapes dev-loop.check_wait_budget()'s broad except, so the success events dev-loop.wait.reviewer-wake and dev-loop.wait.over_budget are never emitted, and the log reason "could not record over-budget wait" is false: record_wait_incident() and the wake add already persisted the issue and the wake.
- Delivery timeline: queued 09:34:42.740Z; first claim/launch 10:20:26.966Z adapter=codex pid=7856; codex child exited; wake.retry 10:54:27.026Z; claimed/launched 10:54:31.658Z adapter=opencode pid=34404 (this review). Queued to first launch 45m44s; queued to review start about 80m.

RECURRENCE (same defect; not a duplicate issue)
- 09:46:54.861Z wait -> dev-loop.wait.issue-failed 09:48:27.872Z (issue-8b5c92a76d97, state new).
- 10:36:51.643Z wait -> dev-loop.wait.issue-failed 10:38:25.296Z (issue-8576955c4fb5, state new). Its evidence.reason is a truncated JSON fragment because game_build_detail stores the last raw stdout line (240 chars) from _record_game_build_output; reason is therefore unreliable across occurrences.
- 20 build-friction issues exist for 2026-11-24 (11 game-dll-build), i.e. each over-60s build files a new high-priority issue and consumes a reviewer. This realises the issue's own "noisy wait policy" risk.

EVIDENCE-ACCURACY NOTE
- evidence.generation=587 is self.game_build_generation, the last COMPLETED generation, not the in-progress attempt (which was gen 588). The wait record does not identify the in-progress build attempt; only wait_id/started_utc do.

LIKELY CAUSE (inference, not proven)
The synchronous agent-wake.py run --once spawned from dev-loop wait checking competes with the 10-job, 644-source compile and is starved past the hard 30s timeout; with no active wake daemon, the queued request then waits for an external scheduler pass. Direct evidence: three identical timeouts, each during an active compile phase. Unknown: the specific starvation mechanism (interpreter start vs load_requests/pid_alive over 497 request files vs no active daemon) and why the queue was not processed between 09:35 and 10:20.

RECOMMENDED SMALLEST FIX (report only; this reviewer edited no code)
1. Decouple wake launch from issue recording: record dev-loop.wait.over_budget immediately after record_wait_incident() succeeds, then attempt launch best-effort and log a distinct dev-loop.wait.wake-launch-slow/failed event instead of a false issue-failed.
2. Never run the launcher synchronously under compile load: enqueue and let the daemon process it, or use the fire-and-forget Popen that agent-wake.launch() already uses.
3. Populate wait build_attempt_id and run_id and use the in-progress attempt generation; sanitise reason to the [GAME BUILD]/stage summary, not the last raw stdout line.

CONFIDENCE: high on the false-failure event and the one-issue/one-wake facts (direct journal evidence). Medium on the starvation cause. Unknown: scheduler/daemon absence during 09:35-10:20.
PROOF STATUS: mechanism and record accuracy only. No fix applied, so no behaviour proof is claimed.
