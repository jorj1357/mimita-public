# issue-6e2d7b91e7d9: Root dev-loop drops runtime journal events: wait and watcher evidence never written

## Human report (verbatim, immutable)

> unable to append journal event dev-loop.watcher.source-change-deferred: [Errno 2] No such file or directory: 'C:\mimita-v9\logs\2026-11-22\20261122_011031\dev-loop\events.jsonl'


## Agent suggestion

### 2026-11-22T06:22:48.257Z — build-wait-reviewer

Timeline note: a prior build-wait review (changelog 20261122_044500, issue-ce4c245a8a70) reports reading logs/2026-11-22/20261122_011031/dev-loop/events.jsonl successfully at ~04:45Z. That same directory is now absent at 06:17Z and the watcher is failing to append every ~5s, so the journal was not merely never created; it was lost or the in-memory events_path went stale after the earlier read. Fix should mkdir the parent before append and avoid deleting/replacing an active run's dev-loop journal.

## Occurrence

### 2026-11-24T08:39:19.766Z — build-wait-reviewer

Fresh occurrence on wait game-dll-build:2026-11-24T07:37:44.064Z (issue-8bdc83a5708d): the required dev-loop.wait.over_budget and dev-loop.wait.reviewer-wake events are not present in any logs/**/dev-loop/events.jsonl, and no dev-loop journal contains any runtime event with wall_time in 07:30-07:39Z. .dev/dev-loop-console.log shows repeated 'unable to record launch evidence: [Errno 2] ... logs\2026-11-24\20261124_053435\dev-loop\events.jsonl'; that run dir has client-01/server-01/run.json but no dev-loop directory. The console log's last write is 07:34:11Z, so the exact 07:38 loss is inference, not a directly captured error line. The daemon restarted as run 20261124_075114 (07:51:14Z) and does record wait events (issue-816c05804889 wait 2026-11-24T07:53:14.620Z has both over_budget and reviewer-wake rows), so the missing event is confined to the pre-restart window. Issue record plus the single reviewer wake (wake-f89090687c35) exist; only the journal half of the proof is missing.

Evidence: {"affected_issue": "issue-8bdc83a5708d", "console_last_write_utc": "2026-11-24T07:34:11Z", "events_found": false, "inference": true, "stale_events_path": "C:\\mimita-v9\\logs\\2026-11-24\\20261124_053435\\dev-loop\\events.jsonl", "stale_path_dir_exists": false, "valid_postrestart_run": "20261124_075114", "wait_id": "game-dll-build:2026-11-24T07:37:44.064Z"}

## Occurrence

### 2026-11-24T09:50:21.338Z — build-wait-reviewer

Fresh occurrence on wait exe-build:0:2026-11-24T07:49:35.810Z (issue-128ca34b21c6): dev-loop.wait.over_budget and dev-loop.wait.reviewer-wake events are absent from every logs/**/dev-loop/events.jsonl; the wait_id appears only in .dev/agent-wake/requests/wake-ebca4f95b51c.json. The wait fired 07:50:35-36Z in the pre-restart window (daemon restarted ~07:48:51); retained run journal 20261124_075114 begins after and records only the later 07:53:14 game-dll wait. The issue record and single wake wake-ebca4f95b51c exist. Build attempt exe-build-20261124T024935-5bf13f69 was actively progressing (presentation_dll at the 60 s trigger) and completed successfully at 93.735 s, so this is journal loss, not a stalled build.

Evidence: {"affected_issue": "issue-128ca34b21c6", "build_attempt_id": "exe-build-20261124T024935-5bf13f69", "build_outcome": "success", "build_total_seconds": 93.735, "builder_run_restart_utc": "2026-11-24T07:48:51.328Z", "events_found": false, "inference": true, "inference_note": "Pre-restart attribution is inferred from daemon restart time and absent rows; the exact dropped write was not captured in a retained log.", "only_retained_hit": ".dev/agent-wake/requests/wake-ebca4f95b51c.json", "phase_at_60s": "presentation_dll", "valid_postrestart_run": "20261124_075114", "wait_id": "exe-build:0:2026-11-24T07:49:35.810Z"}

## Occurrence

### 2026-11-24T20:48:12.198Z — build-wait-reviewer

Fresh occurrence on wait game-dll-build:2026-11-24T20:00:45.963Z (issue-a4eea8fe2246): the required dev-loop.wait.over_budget and dev-loop.wait.reviewer-wake events are absent. No logs/**/dev-loop/events.jsonl contains this wait_id, the issue id, or any runtime event in the 20:00-20:03Z window; a repo-wide search for the wait_id returns only .dev/agent-wake/requests/wake-ad0bab437950.json. Distinct from the prior stale-path/ENOENT occurrences: the daemon (pid 30304) restarted at 2026-11-24T19:55:25.909Z and never launched a game run, so .dev/state.json has run_id=null and events_path=null, and dev-loop --status shows run=NOT RUNNING, events=(none). devscripts/dev-loop.py record_runtime_event() returns early when events_path is None (lines 2333-2335), silently dropping the check_wait_budget() events at lines 3915/3921. The issue record and the single reviewer wake (wake-ad0bab437950) exist; only the journal half of the proof is missing. The waited-on attempt game-dll-20261124T200045986936Z-3f16b3 was real work (compile still running at 60.8s; build ended 134.4s) and failed at compile in src/entities/player-render.cpp:634, so this is journal loss, not a stalled build.

Evidence: {"affected_issue": "issue-a4eea8fe2246", "build_attempt_id": "game-dll-20261124T200045986936Z-3f16b3", "build_failed_phase": "compile", "build_outcome": "failed", "build_total_seconds": 134.394, "code_path": "devscripts/dev-loop.py:2333-2335 record_runtime_event early-return; dev-loop.py:3915/3921 check_wait_budget events", "daemon_pid": 30304, "daemon_started_utc": "2026-11-24T19:55:25.909Z", "events_found": false, "inference": true, "inference_note": "events_path=None read from current daemon state with no active run; the exact dropped write was not captured in a retained log", "only_retained_hit": ".dev/agent-wake/requests/wake-ad0bab437950.json", "run_status": "NOT RUNNING", "state_events_path": null, "state_run_id": null, "variant": "events_path_None (daemon restart without launch run), not stale_path/ENOENT", "wait_id": "game-dll-build:2026-11-24T20:00:45.963Z", "wake": "wake-ad0bab437950"}

## Agent discussion and suggestions

### 2026-11-24T20:52:58.432Z — build-wait-reviewer

Message: `msg-b7a8eb4c9a63` (message)

Subject: Wait events dropped when dev-loop has no active run (events_path=None)

Fresh evidence for issue-6e2d7b91e7d9: wait game-dll-build:2026-11-24T20:00:45.963Z (issue-a4eea8fe2246) produced no dev-loop.wait.over_budget or dev-loop.wait.reviewer-wake in any logs/**/dev-loop/events.jsonl. Daemon pid 30304 restarted 2026-11-24T19:55:25.909Z and never launched a run, so .dev/state.json has run_id=null/events_path=null and dev-loop --status shows run=NOT RUNNING, events=(none). record_runtime_event() returns early when events_path is None (dev-loop.py:2333-2335), so check_wait_budget()'s events (3915/3921) were silently dropped. This is the events_path=None variant, distinct from prior stale-path/ENOENT occurrences. Issue record and single wake wake-ad0bab437950 exist; only the journal half of the proof is missing. Smallest next check: when events_path is None, write wait events to a fallback journal (create_shared_events_path or a dedicated .dev wait journal), or initialize one at daemon start. Full review on issue-a4eea8fe2246. No source edit or build by this reviewer.

## Occurrence

### 2026-11-24T21:23:54.160Z — build-wait-reviewer

Missing dev-loop wait events for over-budget wait game-dll-build:2026-11-24T20:24:20.106Z (issue-74f939fb99fc). The wait issue and exactly one reviewer wake exist (wake.queued 2026-11-24T20:25:22.352Z in .dev/agent-wake/events.jsonl), but no retained journal contains dev-loop.wait.over_budget, dev-loop.wait.reviewer-wake, or the wait_id/issue id: a scan of logs/2026-11-24/**/dev-loop/events.jsonl found zero hits, and a .dev/**/*.jsonl scan for the wait id found zero hits. Current .dev/state.json events_path points at run 20261124_205956, far later than 20:24-20:27, so the journal path active at wait time cannot be recovered from current state. This removes the dev-loop-journal half of the proof expectation for this occurrence. Unknown: whether the root dev-loop had events_path=None between runs (the prior a4ee occurrence variant) or wrote to a rotated/now-absent run journal; no ENOENT error string was observed for this occurrence. Reviewer: build-wait-reviewer.

Evidence: {"agent": "build-wait-reviewer", "agent_wake_journal": ".dev/agent-wake/events.jsonl", "build_attempt_id": "game-dll-20261124T202420131204Z-8cc10c", "build_outcome": "success", "build_total_seconds": 128.646, "cause": "unknown (events_path None between runs vs rotated/absent journal)", "current_events_path": "logs/2026-11-24/20261124_205956/dev-loop/events.jsonl", "dev_jsonl_scan": ".dev/**/*.jsonl: 0 hits for wait id", "dev_loop_journal_scan": "logs/2026-11-24/**/dev-loop/events.jsonl: 0 hits", "issue": "issue-74f939fb99fc", "missing_events": ["dev-loop.wait.over_budget", "dev-loop.wait.reviewer-wake"], "wait_id": "game-dll-build:2026-11-24T20:24:20.106Z", "wake": "wake-5a26878e0275", "wake_queued_utc": "2026-11-24T20:25:22.352Z"}

## Occurrence

### 2026-11-24T21:26:24.896Z — build-wait-reviewer

Fresh occurrence on wait game-dll-build:2026-11-24T20:35:35.515Z (issue issue-21e4b1fbc77e): no dev-loop.wait.over_budget or dev-loop.wait.reviewer-wake row for the wait_id or issue id exists in any logs/2026-11-24/*/dev-loop/events.jsonl; the only repo hit is .dev/agent-wake/requests/wake-477d42954674.json. No retained dev-loop run covers the 20:36:36Z trigger (run 20261124_202802 journal ends 20:29:30Z; next root run starts 20:55:41Z; run 20261124_203142_solver-proof-3 has no dev-loop journal). Variant events_path=None / no active run, same as the 20:00:45 occurrence; record_runtime_event returns early at dev-loop.py:2333-2335. The issue record and the single reviewer wake wake-477d42954674 exist. Inference, not a captured error: the exact dropped write was not retained. The waited-on attempt game-dll-20261124T203535650414Z-718e6f succeeded at 228.997s (generation 618), so this is journal loss, not a stalled build.

Evidence: {"build_attempt_id": "game-dll-20261124T203535650414Z-718e6f", "build_outcome": "success", "build_total_seconds": 228.997, "code_path": "devscripts/dev-loop.py:2333-2335", "events_found": false, "inference": true, "issue": "issue-21e4b1fbc77e", "only_retained_hit": ".dev/agent-wake/requests/wake-477d42954674.json", "variant": "events_path_None_no_active_run", "wait_id": "game-dll-build:2026-11-24T20:35:35.515Z", "wake": "wake-477d42954674"}

## Occurrence

### 2026-11-24T21:29:39.337Z — build-wait-reviewer

Recurrence 2026-11-24T20:19:59.371Z wait (issue-7ef5043ce95c, attempt game-dll-20261124T201959398000Z-aa0b47): no dev-loop.wait.over_budget or dev-loop.wait.reviewer-wake event exists in any logs/**/dev-loop/events.jsonl. A repo-wide search for the attempt id returns nothing under logs/ and the wait_id appears only in .dev/agent-wake/requests/wake-c29456cabf35.json. The next over-budget wait (20:55:04.680Z, issue-98a885a0bb81) WAS journaled in logs/2026-11-24/20261124_205541_01/dev-loop/events.jsonl, so the drop is per-instance (events_path None or non-existent), not universal. The issue proof expectation is met 2 of 3 (issue record + wake request; dev-loop wait event missing).

Evidence: {"build_attempt_id": "game-dll-20261124T201959398000Z-aa0b47", "issue": "issue-7ef5043ce95c", "wait_id": "game-dll-build:2026-11-24T20:19:59.371Z"}

## Agent suggestion

### 2026-11-25T06:11:06.326Z — build-wait-reviewer

Occurrence 2026-11-25T05:46:30.064Z (issue-9c1fc3e3e668, attempt game-dll-20261125T054630085917Z-48e192): no dev-loop.wait.over_budget or dev-loop.wait.reviewer-wake row exists in any retained 2026-11-25 dev-loop journal; those run journals contain only isolated-run runtime events. .dev/state.json shows events_path=null (root daemon restarted 2026-11-25T06:03:22.911Z). The issue and wake records exist, but the dev-loop wait-event half of proof is missing. Same defect as the 2026-11-22 report; duplicate record issue-0498cbef4ef3. Reviewer build-wait-reviewer.

## Agent suggestion

### 2026-11-25T06:18:55.578Z — build-wait-reviewer

Occurrence 2026-11-25T05:52:56.306Z wait (issue-8af9b97c9a2a): no dev-loop.wait.over_budget or dev-loop.wait.reviewer-wake row exists in any retained 2026-11-25 dev-loop journal; the run 20261125_054920 dev-loop/events.jsonl holds only isolated-run runtime events, and .dev/state.json has an empty events_path (daemon_pid 4044). The issue record and one queued wake (wake-9c7b2299476c) exist, so the journal half of the proof expectation is still missing for this wait. Occurrence added by build-wait-reviewer.
