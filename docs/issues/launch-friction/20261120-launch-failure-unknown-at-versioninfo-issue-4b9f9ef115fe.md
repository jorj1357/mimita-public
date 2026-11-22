# issue-4b9f9ef115fe: Launch failure: unknown at versioninfo

## Human report (verbatim, immutable)

> versioninfo failed: Command '['C:\\mimita-v9\\.dev\\current\\mimita.exe', '--versioninfo']' timed out after 20 seconds


## Triage

### 2026-11-20T07:31:06.890Z — launch-latest-stable-fix

Summary: Repeated launch preflight timed out because --versioninfo initializes the full structured logger before a server can start; remove the scan from this lightweight identity probe and prove the exact launch path on the current build.
Owner: launch-latest-stable-fix
Priority: high

## Scheduled independent review

### 2026-11-22T13:39:07.426Z — cpp-latency-digest-cache

A bounded independent review was queued as wake wake-22c5037b7631 after 292 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (evidence-review)

### 2026-11-22T13:58:35.508Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review by issue-review-sweeper (wake-22c5037b7631) at 2026-11-22. Read-only: this reviewer made no source, build, or runtime launch changes.

OBSERVED FACTS
1. Issue/task state: issue-4b9f9ef115fe is state=triaged, owner=launch-latest-stable-fix, kind=launch-friction. Its single task versioninfo-preflight-fast is state=open with latest_proof=null. Readiness score 2/8; missing reproduction, expected_behavior, acceptance, paths, proof_expectation, risk.
2. Original failure evidence: .dev/launch-reports/2026-11-20T07-25-21.469Z records failure_stage=versioninfo, failure_code=unknown, detail "timed out after 20 seconds", versioninfo stage_elapsed_ms=23687 (build_id 2026-11-20T05:38:11.772319Z:06e0eafc...). A second identical report .dev/launch-reports/2026-11-20T07-27-21.600Z records versioninfo stage_elapsed_ms=22031.
3. Source cause/fix: git commit a065471f (2026-11-20 07:56:07Z) removed StructuredLogger::instance().init(), writeEvent(), and shutdown() from runVersionInfoCli(). Current src/game/game-cli.cpp:90-133 reads MIMITA_EVENTS_FILE/MIMITA_RUN_ID/MIMITA_PROCESS_ROLE directly and prints VERSIONINFO_BEGIN, EXECUTABLE_PATH, PID, PROCESS_ROLE, WORKING_DIRECTORY, EVENTS_JSONL_PATH, RUN_ID, VERSIONINFO_JSON, VERSIONINFO_END. src/main.cpp:557 calls handleGameCLI(argc,argv) before launch parsing/crash-handler setup. The added comment attributes the timeout to logger journal reconcile/prune.
4. Post-fix live evidence: of 25 launch reports containing a versioninfo stage, 23 succeeded and only the two pre-fix reports (2026-11-20T07:25/07:27) failed. Post-fix examples: 2026-11-21T13-52-48 2756.6ms; 2026-11-21T14-43-50 540.5ms; 2026-11-21T21-52-37 755.7ms; 2026-11-21T22-40-58 515.0ms; 2026-11-22T01-10-31 68.8ms.
5. Full launch: .dev/launch-reports/2026-11-22T01-10-31.858Z completed successfully (launch_mode=45, mode_name="fun counter strike", map=dust2cyberiav4, complete=succeeded); versioninfo succeeded in 68.8ms using launch_executable C:\mimita-v9\.dev\current\mimita.exe.
6. Direct probe: C:\mimita-v9\.dev\current\mimita.exe --versioninfo returned the complete VERSIONINFO_BEGIN..VERSIONINFO_END block with all keys, EXIT=0, ELAPSED_MS=93.
7. Automation gap: devscripts/dev-loop.py:4713-4726 runs [launch_exe,"--versioninfo"] with timeout=20 and marks the stage succeeded solely on returncode==0. It does not assert BEGIN/END markers or JSON keys, and no task proof record was written.

INFERENCE / UNCERTAINTY
- The fix appears causal, not incidental: the diff removes the only logger/journal-tree work from the preflight path and post-fix probes are consistently far below the 20s budget.
- "No journal-tree traversal" is supported by code inspection, not by a large-tree timing experiment; timing alone cannot prove it.
- stage_elapsed variance (51ms-2756ms) is host/load dependent, still far under budget.
- generation=0 in these launch reports is a launch-side bookkeeping field, not evidence that no build existed.

CONTRARY EVIDENCE
- Later launch failures exist (2026-11-22T01-10-06 client_reconciliation "MiMITA process(es) still alive"; 2026-11-21T21-52-37 mode_start_timeout). None have failure_stage=versioninfo, so they do not contradict the fix.
- The launcher's "complete output" acceptance is not verified automatically; only exit code plus this manual capture currently cover it.

NEXT DISCRIMINATING CHECK
- Owner should record a task proof for versioninfo-preflight-fast pinned to a launch_id/run_id/build_id whose versioninfo stage succeeded, with captured stdout markers. Stronger: make the dev-loop versioninfo stage assert VERSIONINFO_BEGIN/END and VERSIONINFO_JSON/EXECUTABLE_PATH (fail the stage if absent), confirm one fresh launch passes, and confirm a proof record results. If that passes, the task can move to runtime_proven. This reviewer recorded no proof and did not claim the fix.

## Context

### 2026-11-22T14:00:48.081Z — issue-review-sweeper

reproduction: Recorded reproduction: with a pre-existing logs/ journal tree present, the dev-loop launch preflight runs <artifact> --versioninfo with a 20s timeout; before commit a065471f this called StructuredLogger::init() (journal reconcile/prune) before printing, exceeding the budget. Archived reports .dev/launch-reports/2026-11-20T07-25-21.469Z and 2026-11-20T07-27-21.600Z show versioninfo stage_elapsed_ms 23687 and 22031. This reviewer did not rebuild or re-create the pre-fix binary.
expected_behavior: A published EXE returns the complete VERSIONINFO_BEGIN..VERSIONINFO_END block (executable, PID, process role, cwd, events path, run id, JSON) and exits 0 promptly, independent of journal-tree size; the dev-loop records versioninfo success and proceeds to server/client readiness.
acceptance: A fresh published EXE returns complete VERSIONINFO output and exits promptly without traversing/pruning the journal tree; a live dev-loop launch completes server/client readiness from the exact built artifact; the launch journal attributes the result accurately; and the task carries a proof record (or reasoned exemption) pinned to that launch/build/run.
paths: ['src/game/game-cli.cpp', 'devscripts/dev-loop.py', '.dev/launch-reports']
risk: Low regression surface: only the --versioninfo diagnostic is affected; reintroducing logger init would re-block all dev-loop launches at preflight (observed twice on 2026-11-20).
proof_expectation: Pinned launch_id/run_id/build_id with versioninfo stage=succeeded and stage_elapsed_ms well under 20000; optionally captured stdout BEGIN/END markers; task proof status runtime_proven referencing those IDs; no subsequent launch report with failure_stage=versioninfo.
