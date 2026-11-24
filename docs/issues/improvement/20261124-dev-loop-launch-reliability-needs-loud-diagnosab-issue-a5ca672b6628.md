# issue-a5ca672b6628: Dev-loop launch reliability needs loud diagnosable failures and agent follow-up

## Human report (verbatim, immutable)

> every time the gui fails somehow or waits or etc it should fail super loud and exactly why and what to do about it and i want agents to be able to see that and wake up and auto fix it


## Agent suggestion

### 2026-11-24T06:03:27.190Z — bloodtest-dev-launch

Observed launch path: latest report 2026-11-24T05:49:58.844Z failed at client_reconciliation before server_process_start/client_process_start because strict psutil process discovery returned WinError 5 Access is denied. Code: devscripts/dev-loop.py checkout_mimita_processes(strict=True) broadly catches psutil.Error without retaining the failing PID/operation; reap_stray_mimita_processes records the generic error and launcher aborts safely. Need report actionable cause/operation/PID where available plus remediation while preserving fail-closed behavior. Distinguish this from earlier PID 18752 cleanup timeout.

## Agent suggestion

### 2026-11-24T08:39:57.653Z — issue-review-sweeper

Process proposal from the issue-3af736bb9337 review (evidence-only; no source edit/build claimed).

Evidence of a repeated missing-evidence delay
- Spec vs implementation: docs/specs/dev-loop/launch-feedback.md:46-58 requires each report directory to contain probes.json, server-tail.jsonl, client-tail.jsonl, processes.json, and build-identity.json. Across all 88 .dev/launch-reports directories there are 88 launch.json, 88 stages.jsonl, 45 report.md, and 0 of those five files.
- Journal retention: devscripts/dev_loop/game_build.py:897,1304 prunes logs to 3 days, while report dirs persist. A hot_client_gate failure loses both its client journal and the gate's hot-client-gate.json (written to the run log dir at devscripts/dev-loop.py:5604), so the original failing check becomes unknowable after 3 days.
- Two same-signature launch-friction issues (issue-3af736bb9337 and duplicate issue-17e5b3041e37, both "render_init_failed at hot_client_gate", funworld3/mode1) and sibling issue-f98137479b80 reached independent review with no surviving journal or gate result. The failure code is also a misnomer: launch_report.py:66-67 maps all seven hot-client checks to "render_init_failed", and only "[HOT GATE] FAIL" (dev-loop.py:5599-5627) is persisted.
- Duplicate intake: launch_report._create_launch_issue dedupes by exact title over issue_list(limit=200) (launch_report.py:126-128); issue_list orders by updated_utc DESC (agent_coord.py:2101-2118) and the store holds 349 issues, so the older same-title issue fell outside the window and a duplicate was created instead of appending an occurrence.

Measurable target
1. 100% of failed launch reports contain the exact failing check name(s), a copy of hot-client-gate.json, and client/server tails, so an agent can name the first divergence without run logs or human help.
2. Zero duplicate open launch issues per identical failure signature across the full issue store (not just the last 200 by updated_utc).

Smallest experiment
- On the next hot_client_gate failure (or by re-running devscripts/hot_client_gate.py on any retained client journal), assert the hot_client_gate stage record includes failed_checks and that .dev/launch-reports/<launch-id>/hot-client-gate.json exists after the launch finalizes. Then rerun launch-doctor dedupe against a >200-issue store and confirm it appends an occurrence to the existing same-title issue. Target: one failure, one self-contained report, zero human-provided logs, zero duplicate issue.
- Keep this separate from issue-9a4503f6ed68 (wait-issue signature dedupe); it is the launch-doctor analogue and can reuse the same signature key.

No source change made; this is a proposal for owner review.

## Agent suggestion

### 2026-11-24T16:01:52.121Z — issue-review-sweeper

Process proposal from the bounded review of issue-80dfb9ab9c5c (issue-review-sweeper, wake-837c97597328). Read-only: no source edit, build, run, or fix claimed.

EVIDENCE OF A REPEATED MISSING-EVIDENCE / DIAGNOSABILITY DELAY
- issue-80dfb9ab9c5c has been open since 2026-11-21T01:23Z with at least three recorded occurrences (2026-11-21 start-client; 2026-11-22T16:22 Horde; 2026-11-22T19:12 isolated Horde), no proof record, no linked build/run, and no prior independent review. The same failure text ('... without a responsive control endpoint ... daemon/watchdog recovery failed; no control endpoint') recurs but carries no daemon thread/listener state, so each new agent must re-derive cause from source.
- Current read-only inspection located the concrete gap: `--status` reads .dev/state.json and never probes the control port (dev-loop.py:8368-8376); the watchdog restarts only on dead PID or script-hash change, never on endpoint unresponsiveness (dev-loop-watch.py:373-407); ensure_running fails closed with no repair (dev-loop.py:7151-7164, 8399-8402). The failure is therefore neither loudly diagnosed nor auto-recoverable, matching this issue's user ask ('fail super loud and exactly why ... agents ... wake up and auto fix it').

PROPOSAL (measurable)
- Add one read-only diagnostic used by both the CLI failure path and the watchdog: report daemon PID identity, control_port, listener state (LISTEN present), a bounded connect probe result (ok/refused/timeout), and last successful accept age; on a failed ensure_running emit this bundle in the visible message and journal. Target: 100% of 'no control endpoint' failures contain the listener/probe/accept-age bundle, and an agent can classify 'accept thread dead vs backlog/timeout' from one report without re-reading source.
- Add a watchdog restart trigger: N>=3 consecutive failed authenticated status probes with the daemon PID alive means restart only the owned daemon (existing do_restart path) and record old/new PID, probe results, and preserved run. Target: endpoint recovers automatically within one watchdog interval; zero duplicate daemons; server run identity preserved.

SMALLEST EXPERIMENT
- Dry-run only: add a read-only `--probe-daemon` that prints the bundle above and run it once while healthy; then point it at a known-dead port to confirm it reports timeout vs refused distinctly. Do not change watchdog behavior until the probe output is shown to the owner. One healthy run plus one synthetic-failure run defines the discriminator.

No owner assignment, code edit, or proof claim.
