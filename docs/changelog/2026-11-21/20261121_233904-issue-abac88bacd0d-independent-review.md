# Issue-abac88bacd0d Independent Review

# Task

- Task ID: issue review sweep wake-b7248bd0b19e
- Session ID: work-ef6a85675e79
- Owner: issue-review-sweeper
- Focus: bounded independent review of issue-abac88bacd0d (bug: dev-loop dashboard is not reliably visible/open)
- Summary: Reviewed issue readiness/brief, proof state, the current dashboard process/window, the dashboard monitor layout, the keepalive journal history, and the `dev-loop --status` visibility field. Added the missing readiness context (score 1/8 to 8/8) and appended one evidence review (verdict `inconclusive`). No source, build, run, close, or fix was performed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-21T23:39:04Z

# Pre-existing changes

- The shared checkout contains extensive pre-existing modifications from other agents. This session edited no source and reverted nothing.
- Files created by this session: this changelog and temporary probe scripts/body files outside the repository. The append-only issue records were written through `agent-coord.py issue context`, `issue review`, and `issue suggest`.

# Requested behavior

Complete one bounded independent review of issue-abac88bacd0d without editing source, building, closing, or claiming a fix; record observed facts, inference/uncertainty, contrary evidence, and the next discriminating check, plus one measured process proposal only when supported.

# Findings

- Issue readiness was 1/8 (ready=false) with no tasks, evidence, or runs. No proof record's task is linked to the issue; the nearest dashboard proofs (`task-proof-ed45c1b9e923`, `task-proof-96a213027bff`) are stale after gui.py changes.
- At 2026-11-21T23:34:57Z the dashboard is open and visible: PID 33128 (`dev-loop.py --dev-loop-role=dashboard`), hwnd 10751366, visible=True, minimized=False, rect [-986,40,-200,788]. It sits fully on the attached left display [-1920,0,0,1080]; MonitorFromWindow is non-zero. `.dev/dev-loop-dashboard.lock` holds 33128 and `.dev/dashboard-ui.json` geometry "786x748+-986+40" matches the rect.
- The dashboard is actively refreshing (`.dev/dev-loop-dashboard.log`, PID 33128 through 23:33:10Z), so the symptom is not reproduced at review time.
- Keepalive evidence is not continuous. `.dev/dashboard-keepalive/events.jsonl` (6,040 events, 2026-11-20T03:34:30Z to 2026-11-21T22:28:36Z) contains 5,886 healthy, 74 start, 43 window-missing, 23 restored, and 14 failed events. Its last event is one healthy sample for PID 33128 at 22:28:36.357Z; no keepalive process is running now, so that start sample is the only 2026-11-21 record (~66 min old). There is a ~32 h gap from 2026-11-20T14:29:05Z to 2026-11-21T22:28:36Z.
- Historical failure signatures include window-missing events where the only matched process was the keepalive's own PowerShell query, repeated "could not be started" failures (`started` counts to 40), restored events with visible:false to true, and relocation of negative-x windows to (20,20) that the current `restore_window` no longer does on a connected display.
- `dev-loop --status` prints `visibility=` from `current.get('launch_visibility')` (dev-loop.py:7668-7671), the game-client launch policy, not dashboard visibility. The same field is launch visibility at dev-loop.py:5612.
- `dashboard-keepalive.py` is standalone and only invoked manually. The watchdog `ensure_dashboard_presence` (dev-loop-watch.py:131-144) checks process presence only via the lock PID (`dashboard_present`, dev-loop-watch.py:123-128); it does not sample window visibility, so `docs/specs/dev-loop/dashboard.md` window-level recovery has no automatic driver while a dashboard PID stays alive.

# Specification alignment

- Followed the router's issue-intake, agent-proof-and-verification, dev-loop dashboard, and agent-coordination paths.
- Treated process liveness, window visibility, runtime behavior, and human acceptance as separate claims; no behavior fix was claimed.
- Added context/acceptance grounded in the existing quote and `docs/specs/dev-loop/dashboard.md`.

# Exact implementation changes

- None to source. Append-only issue context, issue review, and a process proposal only.

# Validation

- Build status: no build requested; no compiler, worker, or second dev-loop was invoked.
- Runtime evidence: read-only process query, window-state probe, monitor enumeration, and keepalive journal aggregation.
- Review artifact: `issue-abac88bacd0d` review (verdict `inconclusive`); readiness now 8/8 ready.
- Process proposal: appended to `issue-0cdac76fb006` (small desktop behavior proof standard).

# Measured evidence

- Live dashboard: PID 33128, hwnd 10751366, visible=True, minimized=False, rect [-986,40,-200,788], on monitor [-1920,0,0,1080]; title "MiMITA Coordinator Dashboard — mimita-v9".
- Keepalive counts: healthy 5,886; start 74; window-missing 43; restored 23; failed 14; last event 2026-11-21T22:28:36.357Z.
- Current observation gap with no keepalive process: ~66 minutes at review.

# Regression review

- Regression entry appended: no.
- Why: the reported symptom was not reproduced at review time; the historical failures are recorded in the keepalive journal and were not re-triggered by this read-only sweep.

# Human acceptance

- Visual review: not performed (read-only API window probe only).
- Gameplay review: not applicable.
- Still unverified: sustained window-level reliability, and whether a minimized/hidden dashboard is restored automatically rather than only when keepalive is run manually.

# Agent handoff context

- Owner: melee-networking (unchanged; the owner does not match the dev-loop/dashboard scope and reassignment needs a triage decision).
- Next exact step: with PID 33128, run `python devscripts/dashboard-keepalive.py --once` (no `--focus`) and confirm one healthy event matching rect [-986,40,-200,788]; then minimize the window and run `--once` again, expecting `dashboard.restored` with visible=true. Attach both journal records before any fix is claimed.
- Proposed capability: continuous window-level dashboard health while `dashboard-policy.json` required=true (see `issue-0cdac76fb006`), with a 10 s cadence and >30 s gap target.
- Do not redo: this readiness/probe/journal review.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue records.
