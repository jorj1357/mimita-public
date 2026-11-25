# issue-6e528c4dc127: Dev-loop GUI controls and issue discussion panel reliability

## Human report (verbatim, immutable)

> but we need dev loop gui to be super consistnet and no buttons are not working bc sometims the gui will have freezing issues or behave incosnsitetnly or slowly, i want to see like for example launch latest mimimta exe why it takes a long time and isnt instant whats wrong i watn to see


## Scheduled independent review

### 2026-11-25T06:42:41.996Z — root-mode3-latency

A bounded independent review was queued as wake wake-461c1ee42b8b after 705 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (needs-evidence)

### 2026-11-25T06:49:36.898Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Bounded independent review (issue-review-sweeper, wake-461c1ee42b8b, 2026-11-25T06:47Z). Read-only: no source edit, build, launch, close, task claim, or fix is claimed.

BRIEF / READINESS / PROOF
- issue readiness = 8/8 ready, state=new, owner empty, tasks=0, reviewers=[] before this review.
- No proof record exists for this issue: `agent-coord proof list --agent issue-review-sweeper` returned none for issue-6e528c4dc127 (0 rows).
- evidence field = {"runtime": "Fun Counter Strike report; no behavior independently verified", "source": "user"}. Doc docs/issues/improvement/20261121-dev-loop-gui-controls-and-issue-discussion-panel-issue-6e528c4dc127.md contained only the verbatim quote and the 2026-11-25T06:42:41.996Z scheduling note. So no acceptance evidence had ever been attached.

OBSERVED - live dashboard journal (read-only)
Source: C:\mimita-v9\.dev\dev-loop-dashboard.log (dashboard pid 4460). Window 2026-11-25T06:22:17Z..06:47:17Z (722 rows, sampled at 06:47:17Z):
- dashboard.coordination.slow: 720 events, min 1266 ms, max 2281 ms, avg 1717 ms (cooldown_key last_coordination_log).
- dashboard.status.slow: 2 events, both 203 ms.
- dashboard.ui.stall: 0; dashboard.refresh.slow: 0; dashboard.stage.slow: 0.
Whole-log totals 2026-11-20T14:55:56Z..2026-11-25T06:45:26Z: 26094 coordination.slow, 26233 ui.stall, 34704 refresh.slow, 52374 stage.slow, 13597 status.slow. ui.stall last fired 2026-11-25T06:02:25.017Z (event_loop_gap_ms 268); earlier same morning max 924 ms at 06:00:39.780Z and 393/455 ms at 06:00:41/42Z.
- Slow-event rows carry only duration_ms (and refresh_number for status/stage); there is no per-substage breakdown in the row.

OBSERVED - newest saved launch (read-only)
Source: .dev\launch-reports\2026-11-24T22-10-14.232Z\ (request dashboard-b80c92e7, run 20261124_221014, launch_mode 45, map dust2cyberiav4):
- result=succeeded after total elapsed_ms 78635.0. request_accepted accept_latency_ms=4098.7 (dashboard press epoch to daemon receipt; dev-loop.py:2485-2487) and queue_wait_ms=620.6.
- stage deltas (using stage_elapsed_ms where it differs from cumulative elapsed_ms): stable_launch 844.8 ms; client_reconciliation 1678.1; versioninfo 7200.8; server_logger_readiness 6095.5; client_process_start 74.4; client_logger_readiness 10054.5; client_connection 13023.1; mode_start 10078.0; hot_client_gate 2222.7.
- server_gameplay_readiness row has stage_elapsed_ms == elapsed_ms == 41904.0, i.e. the known cumulative-not-delta defect (tracked in issue-7d3dcfd7433b). Its true delta after server_logger_readiness (cumulative 15245.7) is 41904.0 - 15245.7 = 26658.3 ms. Accuracy: server logger->gameplay delta 26.7 s is the single largest real stage; next are client_connection 13.0 s, client_logger_readiness 10.1 s, mode_start 10.1 s, versioninfo 7.2 s, server_logger_readiness 6.1 s.
- artifact_selection recorded source_generation=40 and source_current=false; build_id 2026-11-24T19:27:02.742428Z:7ba5fb91...; build_pending=false. So this press used an older published artifact, not current source.

OBSERVED - failure path
Source: .dev\launch-reports\2026-11-24T21-56-58.514Z\: result=failed at client_reconciliation in 560.3 ms, detail "cannot verify existing MiMITA processes: MiMITA process scan failed: [WinError 5] Access is denied.", auto-linked to issue-651bb3982693. So a single press can terminate early for a permissions/scan reason and the daemon does record it.

INFERENCE / UNCERTAINTY
- The 4.1 s press-to-daemon and 78.6 s press-to-ready numbers are real but were captured 2026-11-24T22:10Z, ~8.6 h before this review. No launch report exists after that; .dev/state.json at 06:44Z shows build_status=published_stale, latest_build_stale=true, published EXE 2026-11-25T01:21:28.987918Z (source_generation 58). Current press latency and whether "Launch latest" selects current source are therefore UNKNOWN, not proven good or bad.
- coordination.slow is measured around a queued snapshot result; gui.py:1829-1848 polls state["coordination_result_queue"] and chunks rendering, and the report that SQLite reads are off Tk is consistent with 0 ui.stall in the same window. Therefore the current 1.7 s coordination refresh is NOT proof that button clicks are blocked. The user's freeze complaint is not reproduced in this window; the strongest current fact is continuous coordination-panel refresh slowness (~29 slow events/min), not event-loop freezing.
- Issue/discussion browsing: gui.py builds a COORDINATION section with issue rows (id/state/kind/priority/owner/title) and a messages list plus a send composer. I found no per-issue discussion view or test; the acceptance item "verify issue discussion browsing and human contribution path" is not demonstrably implemented.

CONTRARY EVIDENCE
- The same 25-min window has zero ui.stall/refresh.slow/stage.slow, which argues against an active GUI freeze right now.
- Historical freezes did occur earlier this morning (ui.stall up to 924 ms at 06:00:39Z), so the complaint is plausible but intermittent, not continuously present.
- The launch report used a stale artifact (source_generation 40 vs current 58); any "Launch latest is slow" conclusion from it may include stale-artifact selection unrelated to GUI responsiveness.

NEXT DISCRIMINATING CHECK (one bounded live attempt; dashboard/launch owner)
1) From the live dashboard press "Launch latest stable MiMITA.exe" once and capture the new .dev/launch-reports/<id>/{launch.json,stages.jsonl}; record artifact_selection source_generation/source_current/build_pending, accept_latency_ms, and per-stage deltas computed with the cumulative-stage caveat (never read stage_elapsed_ms when it equals elapsed_ms).
2) In the same window grep .dev/dev-loop-dashboard.log for dashboard.ui.stall / refresh.slow / stage.slow / coordination.slow and state whether any >500 ms event-loop gap coincides with the press. That is the missing GUI<->launch correlation.
3) Expand COORDINATION and confirm issue rows and one message render; attempt to view one issue discussion. If no discussion view exists, record that as the acceptance gap; do not patch in a review.
Relevant owners/related evidence: issue-dd1c5da31ecb (dashboard UI responsiveness acceptance, p95 gap <100 ms / none >500 ms), issue-48e061680b80 (coordination panel), issue-7d3dcfd7433b (launch-stage latency and cumulative-stage classification), issue-bbc291c180f8 (Launch latest regression), issue-651bb3982693 (WinError 5 reconciliation).

DEV-PROCESS REVIEW (short)
- This issue was readiness 8/8, priority high, unowned, 0 tasks, 0 proof records from 2026-11-21T14:27Z until this first review at 2026-11-25T06:42Z (~88 h). That is a concrete confirming instance of the existing "reviewed-but-unowned handoff" proposal already recorded on issue-91e3384b219b (2026-11-24T15:05 entry), so no new mechanism is proposed here; I am appending only the measured age/sample to that existing proposal. No other new process defect is supported by this sweep's evidence.
