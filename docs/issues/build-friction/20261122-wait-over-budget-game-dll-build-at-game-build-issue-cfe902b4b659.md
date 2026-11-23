# issue-cfe902b4b659: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed)

### 2026-11-22T17:50:05.822Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Review by build-wait-reviewer (wake-53e8678c15f2). Verdict: confirmed, but the immediate cause is a transient uncommitted worker regression, not the 60s budget.

Journal inspected: logs/2026-11-22/20261122_144609_01/dev-loop/events.jsonl

Timeline (UTC):
- 16:18:29.701 game-build attempt game-dll-20261122T161713327419Z-cc05b3 failed: UnboundLocalError `progress` at game_build_worker.py:1348.
- 16:24:45.093 attempt game-dll-20261122T161831011383Z-738c5a failed with the same UnboundLocalError (same line).
- 16:24:45.777 wait game-dll-build:2026-11-22T16:24:45.777Z opened for request auto-8 (this issue). No build phase had started yet.
- 16:24:54 new attempt game-dll-20261122T162454119696Z-a296b3 created.
- 16:25:08.832 manifest_snapshot started (23s after the wait opened).
- 16:26:03.074 manifest_snapshot completed after 36.36s for 1008 sources (snapshot_retries=0).
- 16:26:11.440 dev-loop.wait.over_budget fired at 64.604s (budget 60s), issue-cfe902b4b659 created; reviewer wake queued at 16:26:06.550.
- 16:27:20.105 attempt a296b3 also failed with the same UnboundLocalError. No later UnboundLocalError appears in this run after 16:27:20.

First divergence: game_build_worker.py `build()` called `progress.set(worker_priority=..., compiler_creationflags=...)` at (then) line 1348 before `progress = Progress(...)` was constructed. Because the function assigns `progress` later, Python treated it as local and raised UnboundLocalError. Three consecutive auto-builds crashed, which continuously re-opened build waits.

Current source state:
- HEAD c4f439a6 is clean for this defect (no `progress.set(worker_priority=...)` before construction; probe at HEAD line 1377).
- The current working tree (mtime 2026-11-22T16:27:15Z) already restores the correct order: `progress = Progress(...)` at devscripts/game_build_worker.py:1417, then `progress.set(worker_priority=...)` at :1419; `worker_priority`/`compiler_creationflags` are defined at :1346-1347. `ast.parse` succeeds. No additional code change appears required for this crash.
- The correction is UNCOMMITTED (git shows `M devscripts/game_build_worker.py`; +58/-11). It must be committed and included in the next build by its owner; until then the fix is not durable. I did not edit or commit it.

Acceptance check: the issue carries wait_id, stage, reason, elapsed, build/run IDs and evidence; exactly one `wake.queued` exists for this wait_id (16:26:06.550). PASS. The wait budget was not the defect.

Wake latency observation (separate from this issue): `.dev/agent-wake/events.jsonl` shows wake-53e8678c15f2 queued 16:26:06.550, claimed/launched via codex 16:26:10; codex process exited and fallback retry was recorded 16:34:15.529; the opencode fallback was not claimed until 17:18:37.380 (~44 min later) and relaunched then. This is a distinct wake-delivery/serialization latency, not duplicate issue creation.

Duplicate-issue concern: `record_wait_incident` keys on wait_id (devscripts/build_friction.py:143-200), so each distinct wait occurrence intentionally gets its own issue even with the same title. The cluster of same-titled build-friction issues (issue-3a39801d6420, issue-d7e0bdaf919a, issue-edbac5d40c37, issue-21dba28546bc, issue-125a65704fa8, issue-cfe902b4b659) is per-occurrence by design, not duplicate records for this wait_id.

Recommendations:
1. Owner of the working-tree build changes: commit the game_build_worker.py ordering fix and confirm the next build no longer raises UnboundLocalError.
2. Investigate manifest_snapshot duration (36.36s / 1008 sources) plus the ~23s pre-phase requeue delay; these, not the 60s threshold, drove elapsed time over budget.
3. Investigate the ~44 min codex->opencode reviewer-wake fallback latency if reviewer response time is expected to be bounded.

Evidence confidence: high for the build-worker crash and timeline (direct journal events); medium for attributing all of the wait to requeue vs manifest_snapshot (both measured, contributions inferred); the wake-latency gap is direct from wake events.

## Agent discussion and suggestions

### 2026-11-22T17:58:25.091Z — build-wait-reviewer

Message: `msg-49121ad1911c` (evidence)

Subject: Uncommitted game_build_worker.py priority fix crashed 3 builds

Your priority telemetry change left devscripts/game_build_worker.py uncommitted with progress.set(worker_priority=...) executing before progress = Progress(...) (old line 1348). That raised UnboundLocalError and failed attempts cc05b3 (16:18:29Z), 738c5a (16:24:45Z), and a296b3 (16:27:20Z) in logs/2026-11-22/20261122_144609_01/dev-loop/events.jsonl. The current working tree already has the correct order (progress at :1417, use at :1419; ast.parse ok), but it is still uncommitted and your edit lease expired 16:42Z. Please commit the ordering fix so the next build is durable. No duplicate issue filed. Details: issue-cfe902b4b659.
