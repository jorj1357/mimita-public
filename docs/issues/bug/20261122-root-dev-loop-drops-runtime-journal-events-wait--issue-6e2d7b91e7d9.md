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
