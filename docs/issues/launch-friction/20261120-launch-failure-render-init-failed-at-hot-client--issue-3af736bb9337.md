# issue-3af736bb9337: Launch failure: render_init_failed at hot_client_gate

## Human report (verbatim, immutable)

> hot-client acceptance gate failed: [HOT GATE] FAIL


## Scheduled independent review

### 2026-11-24T07:47:42.933Z — build-wait-reviewer

A bounded independent review was queued as wake wake-426555e1232a after 534 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (confirmed-hot-gate-timeout; original-check-journal-artifact-pruned; same-map-mode passes on later build; label-misnomer; acceptance-partial; no-fix-claimed)

### 2026-11-24T08:38:42.930Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Bounded independent review (evidence-only; no source edit, build, close, or fix claimed).

READINESS / PROOF
- issue readiness: ready=true, missing=[], possible=8, score=8, tasks=0; state was new with no owner.
- proof status: no task/proof record is linked to issue-3af736bb9337, so nothing here is runtime-proven.
- Linked artifact still present: .dev/launch-reports/2026-11-20T23-22-00.541Z/{launch.json,report.md,stages.jsonl}.

OBSERVED
1. First failed stage = hot_client_gate, with stage_elapsed_ms=46692 and timeout_seconds=45. Ordered stages: request_accepted; artifact_selection (build 2026-11-20T20:20:51.788061Z:64cd9304..., source_generation=38, source_current=false); artifact_validation sha256 ok; launch_configuration map=funworld3 launch_mode=1 mode=current; firewall LOOPBACK_ONLY; stable_launch_stage; client_reconciliation; versioninfo; server_process_start pid=33940; server_logger_readiness ready (347.6 ms); server_gameplay_readiness ready (38020.3 ms); client_process_start pid=24476; client_logger_readiness ready (749.7 ms); hot_client_gate failed; complete failed.
2. The failure code `render_init_failed` is a hardcoded label for every hot_client_gate failure, not a specific diagnosis: devscripts/dev_loop/launch_report.py:66-67 maps "hot_client_gate" -> "render_init_failed". The gate has seven checks (devscripts/hot_client_gate.py:88-96) and the report does not say which one failed. wait_for_hot_client_gate (devscripts/dev-loop.py:5599-5627) keeps only the gate subprocess's last stdout line, which is always "[HOT GATE] FAIL" (hot_client_gate.py:155). failure_detail is therefore "[HOT GATE] FAIL" with zero disambiguation.
3. The gate's machine-readable result is written to Path(self.events_path).parent / "hot-client-gate.json" (dev-loop.py:5604), i.e. inside the run log directory. That run's logs/2026-11-20/20261120_232200/... is gone (logs/2026-11-20 is absent; logs/11-20-2026 exists but has 0 entries), and the original artifact .dev/builds/2026-11-20T20-20-51.788061Z is also gone. The failing check, the client journal, and the gate JSON for the original occurrence are unrecoverable.
4. The report directory does not meet docs/specs/dev-loop/launch-feedback.md:46-58, which requires probes.json, server-tail.jsonl, client-tail.jsonl, processes.json, and build-identity.json alongside launch.json/stages.jsonl/report.md. Across all 88 report directories there are 88 launch.json, 88 stages.jsonl, 45 report.md, and 0 of the other five files. Logs are pruned to 3 days (devscripts/dev_loop/game_build.py:897 and :1304), so any launch failure older than that cannot satisfy the spec acceptance "An AI can diagnose without human-provided logs" (launch-feedback.md:133).
5. Same signature recurred: unresolved duplicate issue-17e5b3041e37 (identical title "Launch failure: render_init_failed at hot_client_gate"), launch 2026-11-24T00:43:33.842Z, build 2026-11-23T19:22:15.804284Z..., funworld3/mode1, stage_elapsed_ms=45548, timeout 45; its run logs and journals are also absent. launch_report._create_launch_issue dedupes by exact title over store.issue_list(limit=200) (devscripts/dev_loop/launch_report.py:126-128); issue_list orders by updated_utc DESC LIMIT ? (devscripts/agent_coord.py:2101-2118) and the store currently holds 349 issues, so an older same-title issue fell outside the window and a duplicate was created.

CONTRARY EVIDENCE
- The same map+mode path passes on current source: launch 2026-11-24T05:25:52.119Z, funworld3/mode1, result succeeded; hot_client_gate ready at stage_elapsed_ms=31283.6 with the same 45 s timeout. The 11-20 failure is therefore not a permanently deterministic path defect. Caveat: that passing run's client journal and hot-client-gate.json are also absent from logs, so this is stage-level evidence only.
- The gate budget is tight rather than clearly broken: the passing 31.3 s run is only ~13.7 s under the 45 s gate, while the failing run reached 46.7 s. That fits a slow-startup/near-threshold condition better than a render-path bug.
- The gate detects and passes a healthy hot frame today: dust2cyberiav4/mode44 runs 2026-11-24T07:51:14 and 2026-11-24T08:15:18 have hot-client-gate.json with all seven checks true, path=dll, Connected, 656/675 frames.

INFERENCE / UNCERTAINTY
- Most likely class: funworld3/mode1 startup exceeded the fixed 45 s hot-frame gate on 2026-11-20; confidence moderate. The specific failing check is unknown because items 2-4 discard it.
- Not proven: whether the gate timed out on frame_progression, connected, render_path_dll, generation/activation, or a post-bootstrap stall. Without the gate JSON this is unknowable for the original occurrence.
- Not proven: that the generation-38 artifact was defective. It no longer exists and the same build id passed dust2cyberiav4 at 22:16:34 and 22:40:06 earlier that evening (issue-f98137479b80 review), which fits a transient host/session condition.
- Acceptance status: "the launch report identifies the first failed stage" is met only at stage granularity while the code is a misnomer and the failing check is unnamed; "the corrected build passes the same launch path" is met only by a later, different build on the same map/mode (05:25:52), not by the failed build. Acceptance is partially met; keep open.

NEXT DISCRIMINATING CHECK
- With a pinned current artifact and no build running, launch funworld3/mode1 2-3 times through the root dev-loop. For each run record the run's hot-client-gate.json (passed and per-check fields), the client journal, and the hot_client_gate stage_elapsed_ms. If gate time clusters near 31-46 s and occasionally exceeds 45 s, the fixed timeout is the cause (not rendering) and the fix is to persist failed-check names plus the gate JSON into .dev/launch-reports/<id>/ and then derive the timeout from history. If one check fails deterministically, that check is the first divergence.
- Until the gate result is persisted, treat the original occurrence as diagnostic-unrecoverable rather than inferring a render defect from the label.

VERDICT: occurrence confirmed as a real hot_client_gate timeout; original check/journal/artifact are unrecoverable; the same map+mode path passes on a later build with ~13.7 s margin; the "render_init_failed" label is a misnomer and the report-directory contract for probes/tails is unimplemented. No source edit, build, close, or fix claimed.
