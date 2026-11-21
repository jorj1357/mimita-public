# issue-0cdac76fb006: Make small desktop behavior fixes discoverable and proof-gated

## Human report (verbatim, immutable)

> can u launch the dashboard and mkae it so  its a issue to be traqckd and figured out how come dashbaord still resetting its position, nad issue is to expadn ai awareness of these things like little thigns like  the dashboard position idk how  to make that  known but generalized known so that these things  are proven to work and then when it proven working then  it is counted as  afix, also u can close and restart the gui , keep doing that until u prove that it moves andthen doenst get moved back to the other monitor


## Task linked

### 2026-11-20T04:55:43.483Z — dashboard-position-proof

Task: small-desktop-state-evidence-standard (step 0)

## Triage

### 2026-11-20T04:56:02.805Z — dashboard-position-proof

Summary: Specify and pilot an AI intake/proof path for small persistent desktop behaviors, using the dashboard's move/restart/monitor-retention cycle as the example; no behavior is called fixed without observed before/after proof.
Owner: dashboard-position-proof
Priority: high

## Scheduled independent review

### 2026-11-21T15:34:07.725Z — general-issue-review-scheduler

A bounded independent review was queued as wake wake-d3c001075d14 after 134 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (inconclusive)

### 2026-11-21T15:50:23.081Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Bounded independent sweep 2026-11-21 (read-only; no source edit, build, close, or fix claim).

SCOPE
issue-0cdac76fb006 deliverable (reusable small-desktop proof standard), its linked task, proof records, the exemplar dashboard monitor-retention evidence, and the current dashboard keepalive/state.

OBSERVED
1. `issue brief issue-0cdac76fb006`: readiness score 1/8, ready=false, missing reproduction, expected_behavior, acceptance, paths, evidence, proof_expectation, risk. State=triaged; owner=dashboard-position-proof; task small-desktop-state-evidence-standard is the only linked task.
2. Deliverable still absent. The task is state=open with latest proof task-proof-bac2b095c7af = result=unproven, evidence empty, verified_utc empty. Its acceptance requires a documented checklist covering setup, action, persisted state, restart/recovery cycle, measured before/after state, repeat count, and separate visual acceptance, with dashboard position as the example. A repo search of docs for those criteria ("small desktop", "persistent desktop", before/after, repeat count) found no reusable standard document. Only the generic `docs/workflows/issue-intake-and-triage.md` and `docs/specs/dev-loop/agent-proof-and-verification.md` cover intake/proof, and the hashes recorded for both in task-proof-bac2b095c7af no longer match current files (issue-intake-and-triage.md 56d48ea9... -> f3d370b5...; agent-proof-and-verification.md still 9d2eaefb...). So even the unproven record is not pinned to the current workflow doc.
3. The exemplar dashboard monitor-retention fix was proven on 2026-11-20 under issue-6e59229d7386: evidence-7acdda6eaee9 and `docs/regressions/dev-loop/dashboard-monitor-snap-REG-2026-11-20.md`; three close/reopen cycles (PIDs 27672, 14700, 26504) produced `dashboard.healthy` rect [-1920,25,-1040,785] and saved geometry 880x760+-1920+25.
4. That proof's task record task-proof-ed45c1b9e923 is now result=stale, reason "source changed: devscripts/dev_loop/gui.py". Recorded gui.py sha256 20fac330..., current 5739625d.... The current gui.py diff is an unrelated read-receipt display edit (+8/-2 near line 1452), not the geometry/persistence functions named in the 2026-11-20 changelog.
5. Current state has no new retention proof: `.dev/dashboard-ui.json` (mtime 2026-11-21 10:37:49) is geometry "786x759+234+25" with bounds x=234 (primary display), not the proven x=-1920. Dashboard PID 29868 has run `dev-loop.py --dashboard --no-terminal-relaunch` since 10:34:04. `.dev/dashboard-keepalive/events.jsonl` last wrote 2026-11-20 14:29:05; its final event is `dashboard.start` with reason "no dashboard process was present", and the preceding healthy rects were [-1877,42,-997,802], not the proven [-1920,25,-1040,785]. There are no 2026-11-21 keepalive events although a dashboard is running.

INFERENCE / UNCERTAINTY
- The current x=234 geometry and [-1877,42,...] rect show the dashboard is not where the 11-20 proof left it, but this is NOT proof of a snap regression. A human move or a primary-monitor default is equally consistent. Cause is unverified.
- Because no keepalive events are being written on 2026-11-21, live monitor-retention is currently unobserved; the 11-20 proof is point-in-time.
- The stale exemplar proof does not show the geometry fix broke. Staleness is file-granular and was triggered by unrelated gui.py text; whether the owning functions changed is not established.

CONTRARY EVIDENCE
- The exemplar regression record is FIXED with three independent restart cycles and no newer snapping report in issue-6e59229d7386, so the drift may be benign user placement.
- The generic intake/proof workflow already encodes cold handoff, claim labels, build-vs-runtime separation, and proof expectations; part of the intended standard exists implicitly. The gap is specifically the persistent-desktop checklist and its enforcement, not total absence of guidance.

NEXT DISCRIMINATING CHECK
From a known DISPLAY5 position (x=-1920), run keepalive and restart the dashboard three times, recording each cycle's `dashboard.healthy` rect and `.dev/dashboard-ui.json`. If any cycle returns to primary (x>=0) with no human move, retention regressed; if all stay negative, the current x=234 is user placement and the standard is the only remaining deliverable. In parallel, re-verify task-proof-ed45c1b9e923 noting the current gui.py diff is outside the geometry functions, and re-pin task-proof-bac2b095c7af to the current issue-intake-and-triage.md.

NO NEW EVIDENCE that the deliverable (the standard) is implemented; no fix is claimed or closed by this review.

## Agent suggestion

### 2026-11-21T23:39:05.312Z — issue-review-sweeper

Process proposal from the issue-abac88bacd0d sweep (issue-review-sweeper, wake-b7248bd0b19e). This is evidence of repeated missing evidence, not a one-off: `.dev/dashboard-keepalive/events.jsonl` holds 6,040 events ending 2026-11-21T22:28:36Z, but the only 2026-11-21 sample is that dashboard-restart moment, after a ~32 h gap (2026-11-20T14:29:05Z to 2026-11-21T22:28:36Z); the earlier 2026-11-21T15:50Z independent review on this issue reported the same stale-journal condition. No continuous keepalive process exists. `dev-loop --status` prints `visibility=` from the game-launch policy (dev-loop.py:7668-7671), so the obvious status field is not a dashboard-window measurement. The watchdog only checks dashboard PID liveness (dev-loop-watch.py:123-144), so window-level visibility can be stale, hidden, or on another monitor with no automated, agent-readable health cadence. Result: a human or agent must manually run `devscripts/dashboard-keepalive.py` to learn whether the dashboard window is visible; this is a human-only evidence step repeated across at least two independent reviews.

Proposal: make dashboard window health a first-class, continuously observed fact while `dashboard-policy.json` required=true. Measurable target: window-level health (pid, hwnd, visible, minimized, rect, monitor) is written to `.dev/dashboard-keepalive/events.jsonl` at least every 10 s with zero observation gaps >30 s over a measured 24 h run, a read-only command exposes the latest sample without changing window state, and the run creates zero duplicate dashboards. Smallest experiment: start the existing loop `python devscripts/dashboard-keepalive.py --interval 5` for a measured 10 minutes in this checkout (no `--focus`) and record event cadence, maximum gap, CPU cost, and dashboard count; accept only if maximum gap is <=30 s with one dashboard PID. If acceptable, wire that bounded loop under the existing watchdog tick; if CPU is excessive, replace the per-iteration PowerShell process scan with a native/read-only probe before wiring. This advances this issue's proof-gating goal and gives issue-abac88bacd0d a real reliability dataset instead of a single point-in-time sample.
