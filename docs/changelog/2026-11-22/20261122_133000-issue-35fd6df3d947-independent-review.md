# Issue-35fd6df3d947 Independent Review

# Task

- Task ID: bounded independent review of issue-35fd6df3d947 (wake-80c90da11376)
- Session ID: session-d458c7d81def
- Work session: work-9ba3860d52bc
- Owner: issue-review-sweeper
- Focus: dev-loop GUI process inventory can omit or misclassify available clients
- Summary: Reviewed issue readiness, proof state, the issue doc, current process-provider source, launch reports, run manifests, the dashboard log, and a live cross-interpreter provider call. Appended one evidence review (id 174, verdict `needs-evidence`) and one process proposal to issue-7ee67a85a40c. No source, build, run, close, or fix was performed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-22T13:30:00Z

# Pre-existing changes

- The shared checkout already contained extensive modifications from other agents. This session edited no source and reverted nothing.
- Files created by this session: this changelog. The append-only issue review and suggestion were written through `agent-coord.py`.

# Requested behavior

Complete one bounded independent review of issue-35fd6df3d947 without editing source, building, closing, or claiming a fix; record observed facts, inference, contrary evidence, and the next discriminating check, plus a measured process proposal when supported.

# Findings

- Readiness before review was ready (8/8, state `triaged`) with one open task, `process-panel-client-identity`. No proof record exists for that task, so it is unproven.
- Current provider source: `devscripts/process_identity.py:97` `windows_process_inventory` uses a Toolhelp snapshot plus `QueryFullProcessImageNameW`/`GetProcessTimes`; `devscripts/dev-loop.py:895` `checkout_mimita_processes` prefers `psutil` and otherwise uses that native path. `process_identity.py` is committed at `d89884f6` (2026-11-21T21:47Z).
- Live cross-interpreter check in the running dashboard's own interpreter `C:\Python314\python.exe` (3.14.0, `psutil` import absent): `windows_process_inventory()` returned exactly one process, PID 26748, `C:\mimita-v9\.dev\current\mimita.exe`, start `2026-11-22T01:10:32.216738Z`, with a full command line, in 9.86 ms and 22.02 ms on two calls. The no-WMI/no-psutil fallback is functional and fast on this host now.
- Dashboard PID 33128 (pythonw 3.14, started 2026-11-21T22:28:01Z, after the fix commit) has 4308 `process_scan_request` stage events through 2026-11-22T13:21:56Z and zero `PROCESS SCAN FAILED`/`scan failed`/`scan_error` lines in `.dev/dev-loop-dashboard.log`.
- Limitation: `devscripts/dev_loop/gui.py:2391-2392,2404-2405` catches scan exceptions and stores them only in UI state; it never writes a categorized event. Absence of scan-error log lines is therefore not evidence of successful scans, and scan failure counts are not recoverable from journals.
- New failure evidence: `.dev/launch-reports/2026-11-22T01-10-06.744Z/launch.json` failed at `client_reconciliation` with "MiMITA process(es) still alive after cleanup: [24192]" (`stopped_pids` [24192]) after 6177.5 ms; request accept latency was 6062.3 ms. The next request, `2026-11-22T01-10-31.858Z`, succeeded with press-to-ready 34844.7 ms (accept 1.8 ms, queue 124.5 ms; `mode_start` 20641 ms; `client_connection` 5021.9 ms).
- Cleanup logic at `dev-loop.py:5499-5531` appends a PID to `stopped_pids` after `taskkill` without checking the `taskkill` return code, then immediately rescans; a PID still present in that immediate rescan fails the launch. `taskkill_rc` and the `dev-loop.process.reconcile` event are unavailable because `logs/2026-11-22/20261122_011031/dev-loop/events.jsonl` does not exist even though `dev-loop.py --status` links it.
- Run `20261122_011031` `run.json` has `server-01` online as PID 26748 and no client. Dev status shows the server alive and `client pids=[]`. Exited-client removal is consistent for this run.
- `.dev/process-snapshots/20261122_025848.json` uses a separate PowerShell CIM collector, not the GUI provider; it must not be cited as GUI scan proof.

# Specification alignment

- Followed the routed proof/coordination expectations: readiness and proof inspected; observed facts, inference, contrary evidence, and a next check separated; no behavior completion claimed.
- Preserved all prior issue text; only appended the review and suggestion through `agent-coord.py`.

# Exact implementation changes

- None. Append-only issue review and suggestion only.

# Validation

- Build status: no build requested; no compiler, worker, dev-loop, or second loop invoked.
- Runtime evidence: read the issue brief/readiness/observe output, `proof status`, the issue doc, provider source, launch reports, run manifests, and dashboard log; performed one read-only provider call in the dashboard interpreter.
- Review artifact: review id 174 on issue-35fd6df3d947 (state now `in_review`, reviewers `[issue-review-sweeper]`).
- Process proposal: appended to issue-7ee67a85a40c (emit one bounded categorized process-scan event per scan; target 100% of scans produce exactly one event, 0 failures missing an event, red harness failure within 2 s; current baseline is 0 observable scan outcomes).

# Measured evidence

- Issue age at review: ~2.2 days (created 2026-11-20T07:09:45Z; review 2026-11-22T13:29:35Z), state advanced from `triaged` to `in_review`.
- Provider latency: 9.86 ms and 22.02 ms; one live process found in the actual dashboard interpreter with no psutil.
- Launch evidence: one failure with 6177.5 ms reconciliation and one success with 34844.7 ms press-to-ready.
- Observability baseline: 4308 scan-request events, 0 scan-outcome events.

# Regression review

- Regression entry appended: no.
- Why: no behavior change was made; this was a read-only review with append-only issue updates.

# Human acceptance

- Visual review: not performed by this session; the dashboard Processes panel was not observed.
- Gameplay review: not applicable.
- Still unverified: rendered row accuracy, removal-on-exit in the visible panel, and whether the cleanup failure is a race or a kill denial.

# Agent handoff context

- Owner: launch-latest-stable-fix.
- Next exact step: (1) add one bounded categorized `gui.py` process-scan outcome event and verify one event per manual Refresh and launch; (2) capture `taskkill` rc plus a bounded post-kill wait in `dev-loop.process.reconcile` and repeat a launch with a live client; (3) visually confirm on dashboard PID 33128 that Refresh shows one server row pid=26748 and a client row appears/disappears on launch/exit.
- Do not redo: this readiness/proof inspection, the provider live call, or the launch-report correlation.
- Do not close or mark done without owner/human review; this independent review is not acceptance.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue review and suggestion.
