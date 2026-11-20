# issue-6e59229d7386: Dashboard snaps to another monitor

## Human report (verbatim, immutable)

> can u investigate and make ti so the gui doesnt have th snappgin issue


## Triage

### 2026-11-20T04:30:54.448Z — dashboard-no-snap

Summary: Fix the dashboard custom maximize action so it uses the work area of the monitor that currently contains the window; preserve its position if monitor lookup fails.
Owner: dashboard-no-snap
Priority: high

## Triage

### 2026-11-20T04:55:41.591Z — dashboard-position-proof

Summary: Exercise dashboard monitor placement through close/restart cycles, identify the exact persistence or recovery step that changes its location, and prove retention on the selected monitor before treating the issue as fixed.
Owner: dashboard-position-proof
Priority: high

## Task linked

### 2026-11-20T04:55:42.879Z — dashboard-position-proof

Task: dashboard-cross-monitor-restart-loop (step 0)

## Triage

### 2026-11-20T05:18:00.923Z — dashboard-position-proof

Summary: Fixed: negative monitor coordinates were not saved, startup fell back to primary defaults, and keepalive discovery missed the active role marker and could match its own query. Three live close/reopen cycles plus maximize/restore now retain DISPLAY5; see the regression record.
Owner: dashboard-position-proof
Priority: high

## Review (pass)

### 2026-11-20T05:18:01.347Z — dashboard-position-proof

Reviewer: dashboard-position-proof

Runtime proof: moved dashboard to DISPLAY5 at x=-1920; persisted bounds/geometry matched. Maximize filled DISPLAY5's work area and restore returned to x=-1920,y=25. Closed and relaunched three times: PIDs 27672, 14700, 26504 each yielded keepalive dashboard.healthy with visible rect [-1920,25,-1040,785]. Saved .dev/dashboard-ui.json remained 880x760+-1920+25. Proof task task-proof-ed45c1b9e923 verified proven. Regression record has full evidence.

## Resolution

### 2026-11-20T05:18:42.032Z — dashboard-position-proof

Fixed and runtime-proven: dashboard stays on the selected secondary monitor across three close/reopen cycles and maximize/restore; details in docs/regressions/dev-loop/dashboard-monitor-snap-REG-2026-11-20.md.

Evidence: evidence-7acdda6eaee9; task-proof-ed45c1b9e923; keepalive dashboard.healthy rects [-1920,25,-1040,785] for PIDs 27672, 14700, 26504; saved geometry 880x760+-1920+25 (integration, proven)
