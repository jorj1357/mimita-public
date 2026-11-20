# Isolated TDM logger startup stall

## Findings
- Continued the live same-EXE TDM startup investigation under umbrella issue `issue-cf9cd77eb056` and isolated-run journal issue `issue-4e3ef676ba20`.
- Reviewed the server's per-process legacy log for run `20261120_192229` (PID 25904). `StructuredLogger::init()` reached `journal_opened` at 8,512 ms, then emitted no `run.started` or `logger.started` event. The test harness readiness timeout terminated the server before client launch; therefore no transport or connection attempt occurred in this run.
- In `src/debug/structured-log.cpp`, the first event append takes the process-wide writer mutex, then the per-journal named mutex, and reserves quota under a named quota mutex. The init scope still owns the quota mutex, but Windows mutex ownership is recursive for the same thread, so this nested acquisition alone does not establish the stall's cause.
- The isolated run contract specifies a run-level `dev-loop/events.jsonl`; source creates its parent in `create_run_layout()` and `_append_event()` now also creates the parent. Historical and current run folders still lack this journal after early failures. The previous mkdir change is retained, but it has not resolved or explained persistence.

## Source change
- Added parent-directory creation before `_append_event()` opens the run-level journal in `devscripts/dev_loop/parallel_test_runs.py`.
- `git diff --check` was clean when the edit was made. No build or runtime validation of this Python change is claimed.

## Evidence and remaining work
- Server startup trace: `logs/11-20-2026/Server_log_142230.txt`.
- Isolated run manifest: `logs/2026-11-20/20261120_192229/run.json`.
- Next step is to instrument or capture the exact blocking phase inside the first JSONL append (writer mutex, per-journal mutex, quota reservation, or file append) and resolve why the isolated run journal is absent. Then repeat the pinned TDM run through the root dev-loop and continue client connect/reconnect lifecycle proof.
- An evidence proposal was appended to `issue-cf9cd77eb056`: correlate manifests and journals with startup-stage output and synchronized screenshots; explicitly record evidence-capture failures.

## Proof status
- Source edit: present.
- Build: not performed.
- Runtime: startup stall reproduced; no client or network proof.
- Human visual review: not performed.
