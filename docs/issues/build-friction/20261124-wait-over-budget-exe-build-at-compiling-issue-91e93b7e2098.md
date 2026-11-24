# issue-91e93b7e2098: Wait over budget: exe-build at compiling

## Human report (verbatim, immutable)

> exe-build wait at compiling exceeded the wait budget


## Review (acceptance-met; dev-loop-journal-proof-gap)

### 2026-11-24T08:34:50.423Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review of the wait occurrence. No source edits; no files claimed.

ACCEPTANCE (issue-91e93b7e2098) — all three items check out:
1. Issue record carries every required field. Evidence JSON = {"budget_seconds":60.0,"build_id":"2026-11-24T07:18:06.640256Z","elapsed_seconds":60.769,"generation":8,"kind":"exe-build","reason":"compiling 0/45 [CXX] src\\network\\server.cpp","request_ids":["2026-11-24T07:44:14.234Z-0b63cacb"],"source_generation":8,"stage":"compiling","started_utc":"2026-11-24T07:44:14.544Z","wait_id":"exe-build:8:2026-11-24T07:44:14.544Z"}. wait_id/stage/reason/elapsed/build_id/generation all present.
2. Issue created once at 2026-11-24T07:45:15.503Z by reporter/via_agent dev-loop-wait; readiness ready=true score=8/8 missing=[] (no missing reproduction/expected/acceptance/paths/proof fields).
3. Reviewer wake queued exactly once. Exactly one wake request references this issue: wake-de552c3791fb (created 07:45:17.449Z, adapter opencode, launched 07:50:40.140Z, attempts=2). No duplicate wake request for the issue.

DIRECT OBSERVATION — host pressure on the exact build attempt that produced the wait:
The exe-build that tripped the 60s budget was exe-build-20261124T024414-6357cfed (resource capture started 2026-11-24T07:44:14.599Z, completed 07:48:38.103Z; 263.504 s, 169 samples @1 s). mean_host_cpu_percent=99.993, peak_host_cpu_percent=100.0 across host logical_cpu_count=12; minimum available memory 202,489,856 B (~0.2 GiB) of 16.8 GB (98% used); pagefile 39.45/45.51 GB used. Runtime alignment observed=true: run 20261124_072002, actual_mode=bloodtest on server (pid 29932, gen586, 3 NPC / 2 players) and clients (pid 34840 @68.67 FPS p95 24.9 ms; pid 32296 @46.52 FPS p95 80.2 ms; frame budget 4.167 ms). Top competing processes at sample: SNAPOS64 pid 7384 97.6% of one core; FL64 pid 27936 90.2%; pythonw pid 16864 51.3%; Code pid 12760 29.4%. Artifacts: .dev/build-timings/exe-build-20261124T024414-6357cfed.resource.json (+ .resource.jsonl). Unavailable fields on this capture: matched_prebuild_resource_baseline, host_commit_charge_and_limit.

EVIDENCE GAP — proof expectation not fully satisfiable from retained journals:
The issue proof expectation is "Dev-loop wait event plus the issue record and reviewer wake record." Issue record and wake record exist, but NO dev-loop.wait.over_budget / dev-loop.wait.reviewer-wake event for wait_id exe-build:8:2026-11-24T07:44:14.544Z exists in any retained dev-loop journal. rg over logs/ and .dev found the wait_id only in .dev/agent-wake/requests/wake-de552c3791fb.json. The run active around the wait, logs/2026-11-24/20261124_074209_arena-test-3, has a dev-loop/ directory but no dev-loop/events.jsonl. INFERENCE (not confirmed): record_runtime_event returns silently when self.events_path is empty (devscripts/dev-loop.py:2333-2335), so a daemon without a bound events_path can still create the issue + wake through the coordination store while dropping the journal proof line. UNKNOWN: whether the daemon live at 07:44 had events_path set; its journal is not retained. This is an observability gap, not a failure of the issue-creation/wake acceptance criteria.

OCCURRENCE / DUPLICATE CHECK:
Many sibling "Wait over budget: exe-build ..." issues exist, but each is a different wait_id. This wait_id is unique, so a separate issue is correct per build_friction.py uniqueness-by-wait_id. No occurrence for this wait_id needs to be added and no duplicate issue should be created for it.

NEXT STEP:
Acceptance for this wait occurrence is met. The separate follow-up (owner/decision outside this review) is to persist dev-loop wait evidence even when events_path is unset (bind a default journal or fall back to a stable path) so the stated proof expectation can be met. No code fix claimed here; requires human approval / owner coordination.
