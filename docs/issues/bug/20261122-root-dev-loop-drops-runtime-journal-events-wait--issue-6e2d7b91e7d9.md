# issue-6e2d7b91e7d9: Root dev-loop drops runtime journal events: wait and watcher evidence never written

## Human report (verbatim, immutable)

> unable to append journal event dev-loop.watcher.source-change-deferred: [Errno 2] No such file or directory: 'C:\mimita-v9\logs\2026-11-22\20261122_011031\dev-loop\events.jsonl'


## Agent suggestion

### 2026-11-22T06:22:48.257Z — build-wait-reviewer

Timeline note: a prior build-wait review (changelog 20261122_044500, issue-ce4c245a8a70) reports reading logs/2026-11-22/20261122_011031/dev-loop/events.jsonl successfully at ~04:45Z. That same directory is now absent at 06:17Z and the watcher is failing to append every ~5s, so the journal was not merely never created; it was lost or the in-memory events_path went stale after the earlier read. Fix should mkdir the parent before append and avoid deleting/replacing an active run's dev-loop journal.
