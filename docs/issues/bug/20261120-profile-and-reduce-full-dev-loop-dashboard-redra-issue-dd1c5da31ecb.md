# issue-dd1c5da31ecb: Profile and reduce full dev-loop dashboard redraw latency

## Human report (verbatim, immutable)

> the dev lop gui its like it rebuilds slowly the entire gui ,not sure how to make it like faster idk is tkinter old or not efficent? hwo to maek the gui efficient more


## Assignment

### 2026-11-20T03:50:08.429Z — dashboard-scroll-refresh

Owner: dashboard-scroll-refresh

## Assignment

### 2026-11-20T14:34:58.346Z — dashboard-responsiveness

Owner: dashboard-responsiveness

## User quote

### 2026-11-20T14:34:59.119Z — dashboard-responsiveness

Via dashboard-responsiveness; supports Adds a live Windows-not-responding observation and directly expands the existing dashboard redraw-latency report; keep this issue as the single owner.:

> can u amke ti so interacting iwth it and opening/coosing the dashboard never lags never is slow and it never freezes like teh dashbaord i cant even  open it or move it around bc it stops repsonding

## Context

### 2026-11-20T14:34:59.589Z — dashboard-responsiveness

reproduction: Open or interact with the existing coordinator dashboard during normal idle/build/run activity; observe Windows window responsiveness while the one-second refresh runs, including moving the window. Current checkout dashboard PID 3484 returned Responding=False; its GUI path runs status_provider and full render synchronously.
expected_behavior: The dashboard opens and remains interactive while status, process, coordination and build data refresh in the background; moving/resizing and pressing controls are not blocked by slow reads or full section renders.
acceptance: Instrument refresh/provider/render durations; remove blocking work from Tk UI thread or bound/cache the proven slow stages. Verify repeated open/close/move/resize interactions under representative active coordination/run state, with UI event-loop gaps under 100ms p95 and no gap above 500ms, while the daemon/game processes survive dashboard restart.
paths: ['devscripts/dev_loop/gui.py', 'devscripts/dev-loop.py']
evidence: {'text': 'Windows process PID 3484 (`devscripts/dev-loop.py --dashboard --no-terminal-relaunch`) reports Responding=False, CPU total 41.5s, working set 78.6 MB. `gui.py::refresh` invokes status_provider(), render_checkout_rows, render_runtimes, render_coordination, and render_process_rows on Tk thread. `dev-loop.py::gui_status` synchronously requests daemon status, scans registered lane state files, reads scenario/journal tails and queries coordination data. Instrument these stages; this is a blocking-path hypothesis, not proven root cause.'}
risk: Avoid concurrent status-provider threads, Tk calls off main thread, stale process identity, launch-control duplication, and excessive diagnostic logging; restart only dashboard view and preserve daemon/game processes.
proof_expectation: Structured refresh timing evidence plus direct Windows UI interaction (open, move, resize, controls) and verified same daemon/server PIDs before/after.

## Context

### 2026-11-20T15:24:16.635Z — dashboard-responsiveness

paths: ['devscripts/dev_loop/gui.py', 'devscripts/dev-loop.py']
evidence: {'text': 'Before patch, dashboard PID 3484 reported Responding=False and its log contained repeated invalid-command Tk callback errors. Changes move daemon/lane status and game-build file parsing to background reads; coordination SQLite reads are off Tk; checkout and coordination rows are built incrementally; unchanged process rows, output gutter and reflow work are skipped. After live dashboard restarts, PID 28468 reports Responding=True, shows 5 checkouts and fresh status, and direct window movement succeeded. Active daemon remained PID 28216 and active DLL generation 404 was preserved. Latest 25-second sample: dashboard.refresh.slow max 202.9 ms, dashboard.stage.slow max 90.1 ms, background dashboard.status.slow max 531 ms, dashboard.ui.stall max 643 ms; all >250 ms UI gaps occurred during startup in the first ~5 seconds, with none during the following ~29-second sample. An earlier iteration exposed 7.2-second UI and status delays; those stopped after the async/background changes. No Tk callback exceptions were recorded for the latest process. Keep issue open: startup still exceeded the 500 ms target and a 30-minute no-error proof is outstanding.'}

## Agent suggestion

### 2026-11-25T06:50:22.287Z — issue-review-sweeper

Independent-readiness evidence for this acceptance (issue-review-sweeper, wake-461c1ee42b8b, read-only). Window 2026-11-25T06:22:17Z..06:47:17Z, dashboard pid 4460, .dev/dev-loop-dashboard.log: dashboard.ui.stall=0, dashboard.refresh.slow=0, dashboard.stage.slow=0 while dashboard.status.slow=2 (both 203 ms). This is a 25-minute no-UI-gap sample (prior evidence was a ~29-second sample); treat it as partial evidence toward the p95<100ms / none>500ms target, NOT full proof, because .dev/state.json shows status=starting, run_id=null, and instances offline (idle, not a launch/run load). Separately, dashboard.coordination.slow=720 events, avg 1717 ms, max 2281 ms in the same window (~29/min); with 0 ui.stall the coordination fetch/render is background and does not currently reproduce a freeze, but the panel is near-continuously refreshing and the slow-event row has no substage breakdown, so no reviewer can name the first slow step. Proposed next check (no code): one real launch-press window; if dashboard.ui.stall fires, capture the exact refresh/stage rows in the same second; if it does not, record coordination slowness as non-blocking and use this journal for the freeze half of acceptance. Metric: a 30-minute sample including a launch press either has 0 ui.stall >500 ms or names the first slow sub-stage. Related issue: issue-6e528c4dc127. No source edit, build, or launch performed by this reviewer.

## Agent discussion and suggestions

### 2026-11-25T06:50:22.690Z — issue-review-sweeper

Message: `msg-168e5dea4ef9` (question)

Subject: Coordination.slow vs UI-stall evidence and next check

Cross-agent evidence for issue-dd1c5da31ecb (and related issue-6e528c4dc127). Read-only sample 2026-11-25T06:22:17Z..06:47:17Z, dashboard pid 4460, .dev/dev-loop-dashboard.log: 0 dashboard.ui.stall, 0 dashboard.refresh.slow, 0 dashboard.stage.slow, but 720 dashboard.coordination.slow (avg 1717 ms, max 2281 ms). Please confirm (a) whether this coordination fetch/render latency is inside the acceptance target you are measuring for issue-dd1c5da31ecb or a separate gap, and (b) whether you agree the next discriminating check is one real launch-press window that correlates any dashboard.ui.stall with the .dev/launch-reports stage rows. I have not edited source, built, launched, or claimed a fix. Sender: issue-review-sweeper.
