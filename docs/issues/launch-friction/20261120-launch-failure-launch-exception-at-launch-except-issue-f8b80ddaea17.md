# issue-f8b80ddaea17: Launch failure: launch_exception at launch_exception

## Human report (verbatim, immutable)

> TypeError: expected str, bytes or os.PathLike object, not NoneType while creating the client; the fresh client command-queue path was unset after server startup.


## Agent suggestion

### 2026-11-20T09:30:34.371Z — launch-latest-stable-fix

Repair evidence: original launch report 2026-11-20T09-05-30.685Z was still running after a TypeError immediately following server_gameplay_readiness. Reconstructed it from matching run 20261120_090530_01 daemon journal; report now terminal failed with launch_exception/TypeError, last_completed_stage=server_gameplay_readiness, and this issue ID. The catcher is now implemented in current source; normal post-restart GUI launch 20261120_092642 succeeded, but no natural exception recurred to exercise the catch branch.

## Scheduled independent review

### 2026-11-22T23:22:02.192Z — cpu-profile-to-fix

A bounded independent review was queued as wake wake-698e948997ee after 352 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (open - evidence gap: no current-source same-mode launch proof; code path addressed)

### 2026-11-22T23:46:21.998Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Observed facts (independent read-only review; no source/build/close action taken)

- Readiness before review: ready=true, score 8/8, no missing fields; state=new, owner empty, 0 linked tasks, 0 linked runs/builds, 0 linked proof records (issue show/brief).
- Linked artifacts: .dev/launch-reports/2026-11-20T09-05-30.685Z/{launch.json,report.md,stages.jsonl}. stages.jsonl has 12 rows: request_accepted -> ... -> server_gameplay_readiness ready at elapsed_ms 22062.8 (server_pid 32448, local_port 53707), then launch_exception failed. There is no client_process_start row, consistent with failure while creating the client.
- launch.json: result=failed, failure_code=launch_exception, exception_type=TypeError, last_completed_stage=server_gameplay_readiness, repaired_from_daemon_event=dev-loop.launch.exception, build_id empty, generation=0. finished_utc 2026-11-20T09:24:45.269Z is the same second as issue creation (09:24:45.341Z), so this is a retrospective reconstruction, not the live terminal report.
- The original daemon journal named in the report, logs/2026-11-20/20261120_090530_01/dev-loop/events.jsonl, no longer exists (Test-Path false). The one-line failure_detail is the only surviving statement of the exception; no traceback or corroborating journal is retained.
- Current source does address the named cause: dev-loop.py:4861-4865 assigns self.command_queue_path to .dev/agent-command-<run_id>.jsonl before client args (4935) and before the _queue_agent_requests fallback (2340); dev-loop.py:2344-2348 rejects an empty queue with the message 'client command queue is not configured'; the launch-exception catcher at dev-loop.py:2449-2510 finalizes a failed report and records dev-loop.launch.exception; launch_report.failure_code maps launch_exception. git blame attributes the queue assignment to 6d92afa35 (2026-11-20 09:26:39 -0500).
- Proof status: no proof record is linked to issue-f8b80ddaea17. The related launch-retained-server-run-identity proofs (ids 19-22) are all result=stale (source changed: devscripts/dev-loop.py, devscripts/dev_loop/launch_report.py).
- Same launch path (launch_mode 57, map funworld3, Payload - funworld3 NPC behavior) has succeeded repeatedly: 2026-11-20T09:07:06Z, 09:08:19Z, 09:09:48Z, 09:11:07Z, 09:12:22Z, 09:19:24Z, 09:26:42Z, 09:37:25Z, 09:42:25Z, 09:45:09Z, 09:46:42Z, 09:50:24Z, 09:52:13Z, 14:31:17Z, and 2026-11-21T14:43:50Z. The 2026-11-21T14:43:50Z report (run 20261121_144350) shows server start, client_connection ready, mode_start ready, hot_client_gate ready, build_identity_match matched, complete succeeded.

Inference and uncertainty

- The current source plausibly fixes the specific None command-queue path and correctly finalizes a launch exception. Confidence that the code path is addressed: high (grep + blame).
- Confidence that the issue is proven fixed: low. The latest same-path success (run 20261121_144350) used artifact 2026-11-21T02:26:50.306117Z with source_current=false, and its journals are absent: logs/2026-11-21/20261121_144350/dev-loop/events.jsonl and logs/2026-11-21/20261121_144350/client-01/events.jsonl both do not exist. dev-loop.py changed after that run (commits 2026-11-21 16:47, 18:48, 22:38 and 2026-11-22 09:28 -0500), so it is stale relative to the current launch path.
- Therefore acceptance part 1 (report identifies the first failed stage) is met, but part 2 (corrected build passes the same launch path with verifiable evidence) and the stated proof expectation (successful launch report plus the exact server/client journals) are not met by currently verifiable evidence. This is an evidence gap, not necessarily an unfixed defect.

Contrary evidence

- The same mode/map launch succeeded many times, including after the queue-path fix commit (2026-11-20T14:31:17Z) and on 2026-11-21T14:43:50Z. This argues against the risk that launch recovery is masking a reproducible mode/build defect for mode 57, and suggests the original 09:05:30Z failure was a one-off. It does not close the gap because those successes lack surviving journals and predate the current dev-loop.py.

Next discriminating check

- One bounded dev-loop launch with launch_mode 57 (Payload - funworld3 NPC behavior, map funworld3) on the current artifact. Accept only if the launch report result=succeeded AND the exact logs/<date>/<run_id>/dev-loop/events.jsonl and logs/<date>/<run_id>/client-01/events.jsonl exist and show client join/connection. Then link that report, run_id, build_id, and both journal paths to this issue and add a proof record (for example a successful dev-loop.launch stage plus network.client.join.accepted result=ok). Cheapest pre-check: assert the fresh client manifest/launch record has a non-empty command_queue equal to .dev/agent-command-<run_id>.jsonl before running the full launch.
- Do not close on the journal-less same-mode successes or on a build-only claim. If a current-source mode-57 launch cannot be scheduled, record that explicitly and keep the issue open.

Process note

- Development-process review found a repeatable evidence-linkage gap: launch.json never carries the selected artifact identity. start_launch is called with generation=0 and empty build_id, and finish_launch does not copy it from the artifact_selection/build_identity_match stage, so every launch.json shows generation 0 and (on failure) empty build_id even though stages.jsonl has the real build_id/source_generation. Combined with journal pruning, a later review cannot bind a launch outcome to a build. I appended one measurable proposal to existing improvement issue-b396205aaecf (Runtime registry and build-to-proof links) rather than opening a duplicate.
