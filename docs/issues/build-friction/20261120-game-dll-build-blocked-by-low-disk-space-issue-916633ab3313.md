# issue-916633ab3313: Game DLL build blocked by low disk space

## Human report (verbatim, immutable)

> 


## Assignment

### 2026-11-21T15:21:59.057Z — root

Owner: root

## Assignment

### 2026-11-21T16:27:53.438Z — root

Owner: build-coordinator

## Occurrence

### 2026-11-25T01:41:43.828Z — build-wait-reviewer

Additional disk_low occurrences during dev-loop run 20261124_221014, after the 01:05 worker_timeout in the same build chain. Direct observation from logs/2026-11-24/20261124_221014/dev-loop/events.jsonl: 01:26:02.335Z attempt game-dll-20261125T012601768949Z-9473ce: 'disk_low: 2033 MB free, need 2048 MB after auto-prune' (0.469s); 01:28:49.395Z attempt game-dll-20261125T012849001874Z-ccdc3f: 'disk_low: 1437 MB free, need 2048 MB after auto-prune' (0.189s). The next attempt at 01:29:27 (game-dll-20261125T012923599004Z-51b848) succeeded and published generation 632 without a disk error. Reported here because this issue already tracks the same disk_low gate; storage retention owner is issue-3f776ce60511. No files or paths were removed by this reviewer. Related wait: issue-e6c253b2fa9f.

Evidence: {"dev_loop_run_id": "20261124_221014", "issue": "issue-916633ab3313", "journal": "logs/2026-11-24/20261124_221014/dev-loop/events.jsonl", "occurrences": [{"attempt": "game-dll-20261125T012601768949Z-9473ce", "duration_seconds": 0.469, "error": "disk_low: 2033 MB free, need 2048 MB after auto-prune", "time_utc": "2026-11-25T01:26:02.335Z"}, {"attempt": "game-dll-20261125T012849001874Z-ccdc3f", "duration_seconds": 0.189, "error": "disk_low: 1437 MB free, need 2048 MB after auto-prune", "time_utc": "2026-11-25T01:28:49.395Z"}], "recovered_attempt": {"attempt": "game-dll-20261125T012923599004Z-51b848", "duration_seconds": 23.127, "published_generation": 632}, "related_wait_issue": "issue-e6c253b2fa9f", "reviewer": "build-wait-reviewer", "storage_issue": "issue-3f776ce60511"}
