# Independent review: storage inventory and bounded log retention (issue-3f776ce60511)

**Date:** 2026-11-24 UTC
**Agent:** issue-review-sweeper
**Wake:** `wake-ffc09301fa95` (scheduled issue review, 470 completed sessions)
**Issue:** `issue-3f776ce60511` - "Measure storage usage and keep log retention bounded"

## Result

Read-only bounded review; no source edits, build, run, lease, close, or owner
claim. Appended review id 308 (verdict `needs-evidence`). Caused the issue to
move from `triaged` to `in_review`; prior discussion text preserved.
One process extension was appended to `issue-91e3384b219b`.

## Findings

- Proof/readiness: `ready=true`, one task `storage-audit-v1` still `open` with
  empty evidence, no `task_proofs` row, and zero prior DB review records. The
  acceptance (repeatable all-byte inventory) has no recorded artifact.
- Read-only snapshot: logs 15,670.2 MiB / 463 files; build 9,143.4 MiB /
  48,784 files; `.dev` 7,622.1 MiB / 38,279 files. Top owners include
  `build/game/objects` 4,656.8 MiB, `.dev/ccache` 2,341.4 MiB,
  `.dev/perf-captures` 2,342.2 MiB, `logs/2026-11-23` 6,373.9 MiB, and
  `logs/2026-11-22` 5,530.1 MiB.
- `prune-builds.py --logs-only --dry-run` kept 2026-11-22/23/24, removed 0,
  freed 0. `_prune_logs` (`game_build.py:897-908`) uses the same recognized-date
  logic and discards its removed count; neither path protects active runs or
  open-issue evidence.
- `_looks_like_date` accepts only `YYYY-MM-DD`, but current source still writes
  `logs/<MM-DD-YYYY>/` (`log-manager.cpp:25-56`, `structured-log.cpp:909`
  fallback, replay/terminal owners). `logs/11-23-2026` grew 3,198.4 -> 3,270.5
  MiB during the review with a newest mtime of 2026-11-23T19:19:24 local, so
  ~3.8 GiB of live legacy logs is never a prune candidate. Removed
  LogManager rotation leaves that family without an owner-side cleanup.
- Non-closed issues' structured evidence names 12 run dirs; 11 are missing. The
  one survivor, `logs/2026-11-22/20261122_210058` (open
  `issue-cf0cb7818822`), is in the oldest kept root and would be deleted by the
  next prune once a 2026-11-25 root exists.
- No bulky tracker was introduced: coordination DB plus WAL is about 20 MiB.

## Process review

No new mechanism was created. Evidence that three retention proposals
(2026-11-22 08:38/12:02/22:03Z) never became a task or decision was appended as
a measurable extension to the existing review-to-execution proposal on
`issue-91e3384b219b`, with a 48-hour proposal-to-task target and the retention
case as its first instance.

## References

- `docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md`
- `docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md`
- `devscripts/prune-builds.py`
- `devscripts/dev_loop/game_build.py:897-908,1304,1312`
- `src/debug/log-manager.cpp:25-56`, `src/debug/structured-log.cpp:909`
- `logs/2026-11-22/20261122_210058/`
