# Repeated Background Work in the Frame Loop

Time created: 2026-09-21T02:00:00Z
Time last updated: 2026-09-21T02:54:48Z

Status: SOLUTION AS OF 2026-09-21T02:54:48Z

Related specification:
`docs/features/live-code-development/live-code-development.md`

Related regression:
`docs/regressions/2026-09-21/frame-timing-policy-repeats-REG.md`

---

## Regression Occurrence 1

### Observed

Baseline:
`logs/2026-09-21/20260921_011056/events.jsonl`

The session contained approximately 23,877 performance spikes, reached a worst frame of
823.778 ms, and grew to approximately 246 MB. The largest repeated contributor was
`Setup::HotReloadDLL`; `Setup::ConfigPolling` was also repeatedly present.

### Expected behavior

Unchanged source should require no expensive hot-reload scan, hash, manifest reload, build,
or activation work. Work should begin because a filesystem event occurred, with only a rare
background reconciliation for missed notifications.

### Actual behavior

`engineTickSetup()` called `HotReloadSystem::pollAndAdvance()` from the frame path. The polling
path periodically performed manifest work, cold-boundary checks, and full source hashing even
when no source had changed. This repeated-work pattern has appeared across multiple performance
and hot-reload investigations.

### Attempted correction

The frame path now consumes watcher state and signals the existing worker. Manifest reload,
cold-boundary checks, source hashing, and build enqueueing are moved to the worker path.

### Required proof

Compare an unchanged five-minute session before and after this correction. The corrected session
must show no repeated generation, no duplicate build for one hash, and idle
`Setup::HotReloadDLL` work below `0.05 ms` without disabling live reload or gameplay.

### Human confirmation

The human reported that this correction resolved the observed frame-time stuttering and that the
game now feels smooth at high FPS during ordinary play. Live-code editing during the same smooth
session has not yet been tested, so that remains a follow-up acceptance case rather than a reason
to reopen this confirmed idle-performance regression.

### Future rule

New systems must use event-driven notifications or explicit low-priority reconciliation. Do not
place expensive unconditional checks in a per-frame loop merely because the check is convenient.
