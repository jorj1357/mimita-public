# issue-63ec094b8859: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-25T22:03:29.154Z — build-wait-reviewer

related_build: game-dll-20261125T214558738291Z-806cc2
related_run: 20261125_212905
evidence: {'reviewer': 'build-wait-reviewer', 'reviewed_utc': '2026-11-25T22:02:00Z', 'wait_id': 'game-dll-build:2026-11-25T21:45:58.563Z', 'dev_loop_run_id': '20261125_212905', 'journal': 'logs/2026-11-25/20261125_212905/dev-loop/events.jsonl', 'original_wait_evidence': {'budget_seconds': 60.0, 'build_id': '2026-11-25T20:35:21.601166Z', 'elapsed_seconds': 60.471, 'generation': None, 'kind': 'game-dll-build', 'reason': ':57765.0,"build_attempt_id":"game-dll-20261125T214558738291Z-806cc2","source_generation":25,"generation":650,"revision":"5d18bf5a0f85f49f3cfddc34b2f69a35e11c60fdfd8be5dae1786096abc3b3d8","progress_write_ms":6.234,"record_overhead_ms":6.828}', 'request_ids': ['2026-11-25T21:45:58.346Z-672050eb:mode3:game'], 'source_generation': 25, 'stage': 'game_build', 'started_utc': '2026-11-25T21:45:58.563Z'}, 'journal_events': {'dev-loop.wait.over_budget': {'count': 1, 'wall_time': '2026-11-25T21:47:27.733Z', 'created': True, 'issue': 'issue-63ec094b8859'}, 'dev-loop.wait.reviewer-wake': {'count': 1, 'wall_time': '2026-11-25T21:47:27.269Z', 'queued': True, 'adapter': 'codex'}, 'dev-loop.wait.issue-failed': {'count': 0}}, 'wake': {'id': 'wake-52852b584468', 'queued_count': 1, 'queued_utc': '2026-11-25T21:47:09.435Z', 'first_launch': {'adapter': 'codex', 'pid': 28780, 'claimed_utc': '2026-11-25T21:47:26.894Z', 'launched_utc': '2026-11-25T21:47:26.990Z'}, 'retry': {'adapter': 'opencode', 'reason': 'previous adapter process exited; trying fallback', 'wall_time': '2026-11-25T21:54:45.068Z'}, 'working_launch': {'adapter': 'opencode', 'pid': 19440, 'claimed_utc': '2026-11-25T21:55:03.001Z', 'launched_utc': '2026-11-25T21:55:03.022Z'}}, 'in_flight_build': {'build_attempt_id': 'game-dll-20261125T214558738291Z-806cc2', 'started_utc': '2026-11-25T21:45:58.738Z', 'completed_utc': '2026-11-25T21:51:08.089Z', 'total_seconds': 309.344, 'worker_error': 'worker timed out after 300s', 'failed_phase': 'worker_timeout', 'success': False, 'target_generation': 648, 'source_generation': 25, 'timing_summary': '.dev/build-timings/game-dll-20261125T214558738291Z-806cc2.json', 'resource': {'mean_host_cpu_percent': 98.592, 'peak_host_cpu_percent': 100.0, 'minimum_available_memory_bytes': 48332800, 'sample_duration_seconds': 309.329, 'process_discovery_wall_ms_total': 139537.13, 'runtime_alignment': 'unavailable: fresh_server_and_client_journal_evidence_not_matched'}}, 'subsequent_build': {'build_attempt_id': 'game-dll-20261125T215331554351Z-19bd08', 'completed_utc': '2026-11-25T21:54:56.370Z', 'total_seconds': 84.812, 'success': True, 'published_generation': 651}, 'identity_gaps': ['recorded build_id 2026-11-25T20:35:21.601166Z is the prior latest_build, not the in-flight attempt game-dll-20261125T214558738291Z-806cc2 (dev-loop.py wait arm ~3896-3897)', 'recorded generation is null while the in-flight target generation was 648', 'reason is a mid-JSON tail from game_build_detail (dev-loop.py ~3892) instead of a readable stage summary'], 'root_cause_note': "The 60s over-budget signal was a true build-friction event, not a stalled worker: attempt 806cc2 was compiling a cold object cache under host CPU ~98.6% and ~48MB available memory while run 20261125_212905 was active, and it was killed by the running daemon's 300s game-build worker timeout. The next attempt reused the cache and published generation 651 in 84.812s.", 'temp_limit_state': {'path': 'devscripts/dev_loop/game_build.py:1862', 'working_tree_diff': 'MIMITA_GAME_BUILD_TIMEOUT default 300.0 -> 9999.0 (uncommitted)', 'running_daemon_still_300s': True, 'effective_cap': 'MIMITA_GAME_ATTEMPT_TIMEOUT default 1080.0s (18 min) still bounds a 9999s worker timeout', 'user_context': "ctx-81875ddb4419: 'continue, but put a temp limit raise to 9999 sec'"}, 'cross_links': ['issue-a6890271d414', 'issue-6e08d89b55e3', 'issue-7d3dcfd7433b', 'issue-9a4503f6ed68', 'issue-84c584957e65', 'issue-136d4553ff92', 'issue-bb51d96a1200', 'issue-8b7208f291a8', 'issue-16e2cf31162b'], 'acceptance_check': {'wait_id': 'pass', 'stage': 'pass', 'elapsed': 'pass', 'evidence': 'pass', 'reason_readable': 'fail (tracked issue-6e08d89b55e3)', 'build_run_identity': 'partial (stale build_id, generation null; tracked issue-7d3dcfd7433b)', 'reviewer_wake_once': 'pass (one wake.queued record; one retry under the same wake id)'}}

## Review (reviewed-needs-separate-fix)

### 2026-11-25T22:04:03.728Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed occurrence: wait_id game-dll-build:2026-11-25T21:45:58.563Z (dev-loop run 20261125_212905, in-flight build attempt game-dll-20261125T214558738291Z-806cc2, source_generation 25, target generation 648). Reviewer made no source edit and claimed no file.

Occurrence and record
- Issue evidence has wait_id, kind, stage=game_build, elapsed=60.471s, budget_seconds=60.0, started_utc, source_generation=25, request_ids=[2026-11-25T21:45:58.346Z-672050eb:mode3:game], and the build attempt id inside reason.
- Journal logs/2026-11-25/20261125_212905/dev-loop/events.jsonl has exactly one dev-loop.wait.over_budget (21:47:27.733Z, created=true, issue-63ec094b8859) and one dev-loop.wait.reviewer-wake (21:47:27.269Z, queued=true) for this wait_id.
- Wake identity is correct: exactly one wake.queued (wake-52852b584468, 21:47:09.435Z), codex claimed/launched at 21:47:26.894/990Z (pid 28780). The codex adapter exited; the same wake was retried under opencode at 21:54:45.068Z and launched 21:55:03.022Z (pid 19440). That is one wake record with a fallback launch, not a duplicate wake.

First divergence
- The waited-on attempt did not stall: .dev/build-timings/game-dll-20261125T214558738291Z-806cc2.json shows started 21:45:58.738Z, completed 21:51:08.089Z, total_seconds=309.344, success=false, failed_phase=worker_timeout, worker_error="worker timed out after 300s". At the 60s signal the worker was still compiling a cold object cache (mean host CPU 98.592%, peak 100%, minimum available memory 48,332,800 bytes, 309.329s sample) while run 20261125_212905 was live.
- The next attempt (game-dll-20261125T215331554351Z-19bd08) reused the cache and published generation 651 in 84.812s, so the 300s worker cap stopped a recoverable cold build. A warm build of ~85s still exceeds the 60s wait budget (see sibling wait issue-bb51d96a1200, same successful attempt), so the single 60s game_build budget remains below normal build duration.
- Record-quality gaps are already tracked, not new: reason is a mid-JSON tail from game_build_detail (issue-6e08d89b55e3; dev-loop.py ~3892); recorded build_id is the prior latest_build 2026-11-25T20:35:21.601166Z rather than the in-flight attempt and generation is null (issue-7d3dcfd7433b; wait arm dev-loop.py ~3896-3897).

Temp limit raise (user context ctx-81875ddb4419)
- The requested "temp limit raise to 9999 sec" is already staged uncommitted in the working tree: devscripts/dev_loop/game_build.py:1862 changes MIMITA_GAME_BUILD_TIMEOUT default 300.0 -> 9999.0. The running daemon predates that edit and still enforces 300s, which is why attempt 806cc2 was killed. A coordinated root-daemon reload is required before it takes effect, and MIMITA_GAME_ATTEMPT_TIMEOUT (default 1080s, 18 min) still caps an attempt, so 9999s only widens the worker cap up to that 18-minute ceiling.

Acceptance
- wait_id/stage/elapsed/evidence: pass. reviewer wake queued exactly once: pass. reason readable: fail (tracked issue-6e08d89b55e3). build/run identity: partial (tracked issue-7d3dcfd7433b).
- This occurrence is distinct from siblings issue-8b7208f291a8 (wait 21:39:39.056Z, attempt df3f49, 316.015s, worker_timeout, and its reviewer wake failed once with agent-wake.py run --once timing out after 30s) and issue-bb51d96a1200 (wait 21:53:31.363Z, attempt 19bd08, succeeded in 84.812s). No duplicate issue is needed for this wait_id; duplicate-issue policy and wake-dispatch latency are tracked by issue-9a4503f6ed68, issue-84c584957e65, issue-7d3dcfd7433b, and build-timeout ownership by issue-a6890271d414.

Verdict: reviewed; occurrence record and single reviewer wake are correct. Root cause is the running daemon's 300s worker timeout, not a wedged worker. Next step is a coordinated root dev-loop daemon reload to load the staged 9999s temp default (bounded by the 1080s attempt cap), then one Mode 3 game batch and verification from the new run's events.jsonl that no worker_timeout occurs and the build publishes. Resource sampling during the live pair (CPU ~99%, ~22-48MB available memory) should stay cross-linked to issue-16e2cf31162b.

## Agent discussion and suggestions

### 2026-11-25T22:04:35.402Z — build-wait-reviewer

Message: `msg-90088b812d44` (question)

Subject: Staged 9999s worker timeout is not live in the running daemon

devscripts/dev_loop/game_build.py:1862 already changes MIMITA_GAME_BUILD_TIMEOUT default 300.0 -> 9999.0 (uncommitted, per user ctx-81875ddb4419), but the live root daemon still enforced 300s: attempt game-dll-20261125T214558738291Z-806cc2 failed worker_timeout at 309.344s in run 20261125_212905, then attempt game-dll-20261125T215331554351Z-19bd08 published generation 651 in 84.812s. MIMITA_GAME_ATTEMPT_TIMEOUT default 1080s still caps the attempt. Who owns the coordinated root-daemon reload (and is it safe for the active run), and will you confirm the next run's events.jsonl has no worker_timeout? Reviewer made no edit.

Sender: build-wait-reviewer
Paths: devscripts/dev_loop/game_build.py
Build attempt: game-dll-20261125T214558738291Z-806cc2
Run: 20261125_212905

This is a cross-agent signal. Please reply with whether this work is yours, the exact owner if known, and the first missing or failing step. Do not assume the sender will stop or overwrite your work.

## Agent discussion and suggestions

### 2026-11-25T22:04:35.856Z — build-wait-reviewer

Message: `msg-175b45ab60e8` (question)

Subject: Staged 9999s worker timeout is not live in the running daemon

devscripts/dev_loop/game_build.py:1862 already changes MIMITA_GAME_BUILD_TIMEOUT default 300.0 -> 9999.0 (uncommitted, per user ctx-81875ddb4419), but the live root daemon still enforced 300s: attempt game-dll-20261125T214558738291Z-806cc2 failed worker_timeout at 309.344s in run 20261125_212905, then attempt game-dll-20261125T215331554351Z-19bd08 published generation 651 in 84.812s. MIMITA_GAME_ATTEMPT_TIMEOUT default 1080s still caps the attempt. Who owns the coordinated root-daemon reload (and is it safe for the active run), and will you confirm the next run's events.jsonl has no worker_timeout? Reviewer made no edit.

Sender: build-wait-reviewer
Paths: devscripts/dev_loop/game_build.py
Build attempt: game-dll-20261125T214558738291Z-806cc2
Run: 20261125_212905

This is a cross-agent signal. Please reply with whether this work is yours, the exact owner if known, and the first missing or failing step. Do not assume the sender will stop or overwrite your work.

## Agent discussion and suggestions

### 2026-11-25T22:04:36.259Z — build-wait-reviewer

Message: `msg-4b1b71a87c8f` (question)

Subject: Staged 9999s worker timeout is not live in the running daemon

devscripts/dev_loop/game_build.py:1862 already changes MIMITA_GAME_BUILD_TIMEOUT default 300.0 -> 9999.0 (uncommitted, per user ctx-81875ddb4419), but the live root daemon still enforced 300s: attempt game-dll-20261125T214558738291Z-806cc2 failed worker_timeout at 309.344s in run 20261125_212905, then attempt game-dll-20261125T215331554351Z-19bd08 published generation 651 in 84.812s. MIMITA_GAME_ATTEMPT_TIMEOUT default 1080s still caps the attempt. Who owns the coordinated root-daemon reload (and is it safe for the active run), and will you confirm the next run's events.jsonl has no worker_timeout? Reviewer made no edit.

Sender: build-wait-reviewer
Paths: devscripts/dev_loop/game_build.py
Build attempt: game-dll-20261125T214558738291Z-806cc2
Run: 20261125_212905

This is a cross-agent signal. Please reply with whether this work is yours, the exact owner if known, and the first missing or failing step. Do not assume the sender will stop or overwrite your work.
