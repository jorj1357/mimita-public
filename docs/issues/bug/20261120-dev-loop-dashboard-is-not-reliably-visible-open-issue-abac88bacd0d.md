# issue-abac88bacd0d: Dev-loop dashboard is not reliably visible/open

## Human report (verbatim, immutable)

> the dev loop gui dashboard its not open i want to ensure that its open


## Scheduled independent review

### 2026-11-21T23:32:09.602Z — cpp-latency-pressure-followup

A bounded independent review was queued as wake wake-b7248bd0b19e after 197 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Context

### 2026-11-21T23:39:04.745Z — issue-review-sweeper

reproduction: Read-only 2026-11-21T23:34:57Z: dashboard PID 33128 is alive and its window is visible, un-minimized, rect [-986,40,-200,788] on the attached left display [-1920,0,0,1080]. `.dev/dashboard-keepalive/events.jsonl` (through 2026-11-21T22:28:36Z) contains 43 dashboard.window-missing, 23 dashboard.restored (including visible:false to true), 14 dashboard.failed, and 74 dashboard.start events. No dashboard-keepalive process is running; the only 2026-11-21 healthy sample is the PID 33128 start, ~66 min old at review. Reproduce the symptom by hiding/minimizing the dashboard window while PID 33128 stays alive: the watchdog PID-only presence check leaves it unreachable and writes no new health event.
expected_behavior: Per docs/specs/dev-loop/dashboard.md, required=true means one dashboard process exists and is reachable from the taskbar/Alt-Tab and is not forced foreground; a verified existing dashboard is left as placed; create/restore only when no verified dashboard exists; a truly off-screen window may be recovered without stealing focus or moving a window that intersects a connected monitor. Window health must be observable and restorable without a human-only step.
acceptance: Dashboard window health (pid, hwnd, visible, minimized, rect, monitor) is emitted at a bounded cadence while required=true, or a read-only command returns the latest sample; minimizing or closing the window restores one dashboard to the same connected display without focus theft; no duplicate dashboards; the evidence journal shows no observation gap longer than the stated budget during a measured 10-minute check.
paths: ['devscripts/dashboard-keepalive.py', 'devscripts/dev-loop-watch.py', 'devscripts/dev-loop.py', 'docs/specs/dev-loop/dashboard.md', '.dev/dashboard-keepalive/events.jsonl']
evidence: {'reviewed_utc': '2026-11-21T23:34:57Z', 'dashboard_pid': 33128, 'hwnd': 10751366, 'visible': True, 'minimized': False, 'rect': [-986, 40, -200, 788], 'monitors': [[0, 0, 1536, 864], [-1920, 0, 0, 1080]], 'keepalive_last_event_utc': '2026-11-21T22:28:36.357Z', 'keepalive_counts': {'healthy': 5886, 'start': 74, 'window_missing': 43, 'restored': 23, 'failed': 14}, 'status_visibility_field': 'game launcher launch_visibility, not dashboard'}
risk: Touching watchdog/keepalive flow can create duplicate dashboards, steal focus, or move a user-placed window. Any fix must be serialized by the existing dashboard lock and must not force topmost/focus except on an explicit request.
proof_expectation: Attach `.dev/dashboard-keepalive/events.jsonl` records (dashboard.healthy or dashboard.restored with before/after window state) for a minimize/close/restore cycle plus a live window probe; show the same connected-monitor rect and no duplicate dashboard PID. A build alone is not proof.

## Review (inconclusive)

### 2026-11-21T23:39:05.029Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Bounded independent sweep 2026-11-21T23:34Z (read-only; no source edit, build, close, or fix claim).

SCOPE
issue-abac88bacd0d ("Dev-loop dashboard is not reliably visible/open"), readiness/brief, proof status, linked artifacts, current dashboard process/window, monitor layout, keepalive journal history, and the `dev-loop --status` visibility field.

OBSERVED
1. `issue readiness`: score 1/8, ready=false; missing reproduction, expected_behavior, acceptance, paths, evidence, proof_expectation, risk. State=new; owner=melee-networking; no linked tasks, evidence, or runs. No proof record's task is linked to this issue; the nearest dashboard records (task-proof-ed45c1b9e923, task-proof-96a213027bff) are dashboard-health proofs and both are now result=stale because gui.py changed.
2. Current dashboard process: PID 33128, `pythonw.exe dev-loop.py --dev-loop-role=dashboard --profile npc-navigation --no-terminal-relaunch --launch-mode 58 --visible`, started 2026-11-21T17:28:01 local (22:28:01Z). `.dev/dev-loop-dashboard.lock` contains 33128. It is actively refreshing: `.dev/dev-loop-dashboard.log` has `dashboard.refresh`/`dashboard.ui.stall` records for PID 33128 through 2026-11-21T23:33:10Z.
3. Live read-only window probe at 2026-11-21T23:34:57Z: hwnd 10751366, title "MiMITA Coordinator Dashboard — mimita-v9", visible=True, minimized=False, rect [-986,40,-200,788]. Two monitors are attached: primary [0,0,1536,864] and left display [-1920,0,0,1080]. The rect lies fully inside the left display; MonitorFromWindow returned non-zero. `.dev/dashboard-ui.json` (mtime 23:32:28Z) geometry "786x748+-986+40" matches the rect. The dashboard IS currently open, visible, un-minimized, and on a connected monitor.
4. Keepalive evidence is not continuous. `.dev/dashboard-keepalive/events.jsonl` (6,040 events, 2026-11-20T03:34:30Z to 2026-11-21T22:28:36Z) counts: 5,886 dashboard.healthy, 74 dashboard.start, 43 dashboard.window-missing, 23 dashboard.restored, 14 dashboard.failed. Its last event is one dashboard.healthy for PID 33128 at 2026-11-21T22:28:36.357Z whose rect equals the live probe. No `dashboard-keepalive.py` process is running now, so that restart sample is the only 2026-11-21 health record and was ~66 minutes old at review time. There is a 115,171 s (~32 h) gap from 2026-11-20T14:29:05Z to 2026-11-21T22:28:36Z.
5. Historical failure signatures in the same journal: dashboard.window-missing events where the only matched "dashboard" process was the keepalive's own PowerShell query (older detection matched the query command line); repeated "dashboard process could not be started" failures with `started` counts up to 40; dashboard.restored events with visible:false to true (window hidden); and repeated relocation of negative-x windows to (20,20), which the current `restore_window` no longer does on a connected display.
6. `[DEV STATUS] ... visibility=visible` from `dev-loop --status` is NOT dashboard visibility. It prints `current.get('launch_visibility')` at dev-loop.py:7668-7671, which is the game-client launch policy; the same field is described as launch visibility at dev-loop.py:5612. Treating it as dashboard evidence is a false positive.
7. `dashboard-keepalive.py` is standalone and only invoked manually; repo search finds only docs/changelog references and the file itself as callers. The watchdog `ensure_dashboard_presence` (dev-loop-watch.py:131-144) only verifies process presence through the dashboard lock PID (`dashboard_present`, dev-loop-watch.py:123-128); it does not sample or restore window visibility. The spec's window-level recovery (docs/specs/dev-loop/dashboard.md:89-90) therefore has no automatic driver while a dashboard process stays alive.

INFERENCE / UNCERTAINTY
- At review time the reported symptom is not reproduced; current evidence supports "open and visible".
- The historical journal shows the symptom class was real and episodic (hidden windows, off-screen/relocation handling, repeated start/verify failures). The current script has changed (MonitorFromWindow offscreen test, name-filtered discovery), so old rates may not describe current code.
- Reliability is still unproven because there is no continuous health stream: the last independent sample is from dashboard start, and a minimized/behind/other-window dashboard is not detected by the watchdog while PID 33128 stays alive.
- The issue owner (melee-networking) does not match this dev-loop/dashboard scope; ownership is left unchanged because reassignment needs a triage decision.

CONTRARY EVIDENCE
- The live probe, lock file, dashboard log, and dashboard-ui.json all agree the dashboard is open and stable now, so an "absent dashboard" claim would be wrong.
- `docs/regressions/dev-loop/dashboard-monitor-snap-REG-2026-11-20.md` records three successful close/reopen cycles with healthy rects, so at least the older position-retention form of the symptom was addressed.

FIRST DIVERGENCE
There is no failing data path at this moment. The missing step is continuous autonomous window-level health observation: after the dashboard starts, no owner samples or emits its visibility until a human or agent manually runs `dashboard-keepalive.py`. The watchdog covers process presence (PID alive) only, not window visibility.

NEXT DISCRIMINATING CHECK
With PID 33128, run the documented bounded check `python devscripts/dashboard-keepalive.py --once` (no `--focus`) and confirm one dashboard.healthy event matching rect [-986,40,-200,788]. Then minimize the dashboard window (human action) and run `--once` again; a correct recovery should emit dashboard.restored and set visible=true. If it does not, capture the before/after state and the exact event. Do not use `--focus` for this check; focus is a separate, user-affecting action. Attach both journal records to this issue before any fix is claimed.

No source edit, build, fix claim, or close is made by this review.
