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

## Scheduled independent review

### 2026-11-22T17:24:10.076Z — build-wait-reviewer

A bounded independent review was queued as wake wake-08c3032d2373 after 311 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (inconclusive)

### 2026-11-22T17:52:56.555Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Bounded independent sweep 2026-11-22 (read-only; no source edit, build, close, fix claim, or issue-state change).

SCOPE
Re-checked issue readiness, the small-desktop standard task and its proof, the exemplar dashboard monitor-retention proof, current dashboard state, and the keepalive journal, and compared them with the 2026-11-21 sweep.

OBSERVED
1. Readiness unchanged: issue brief issue-0cdac76fb006 still scores 1/8, ready=false, missing reproduction, expected_behavior, acceptance, paths, evidence, proof_expectation, risk. State=in_review; owner=dashboard-position-proof; the only linked task small-desktop-state-evidence-standard is still state=open.
2. Deliverable still absent: glob + rg over docs found no reusable small/persistent-desktop proof checklist. Only this issue doc, the generic docs/workflows/issue-intake-and-triage.md and docs/specs/dev-loop/agent-proof-and-verification.md, and the exemplar regression record touch the topic.
3. Task proof task-proof-bac2b095c7af is still result=unproven, verified_utc empty, evidence empty. Its pinned source hashes are now stale for BOTH files: issue-intake-and-triage.md 56d48ea9... -> f3d370b5..., and agent-proof-and-verification.md 9d2eaefb... -> c05d407e... The second was still 9d2eaefb at the 11-21 sweep, so drift increased.
4. Exemplar proof task-proof-ed45c1b9e923 is still result=proven in the store but is dynamically reported stale for gui.py. Current gui.py working hash is 267e69ea... (11-21 sweep: 5739625d...). `git diff 935c0e7e^ HEAD -- devscripts/dev_loop/gui.py` contains no hunk touching window_bounds/_window_bounds/_tk_geometry/persist_window_geometry/GetWindowRect; the last commit to touch _window_bounds and the window_bounds persistence is d3368058 (2026-11-20), i.e. the proven fix. devscripts/dashboard-keepalive.py still matches the recorded d3cc3e94....
5. Dashboard state moved again but is still on primary: `.dev/dashboard-ui.json` mtime 2026-11-22 12:30:57, geometry 786x748+290+46, window_bounds x=290 (11-21 sweep: 786x759+234+25). Dashboard PID 21072 (`dev-loop.py --dev-loop-role=dashboard ... --no-terminal-relaunch`) has run since 2026-11-22 09:08:52.
6. Keepalive remains non-continuous: `.dev/dashboard-keepalive/events.jsonl` holds 6,040 events - 6,039 on 2026-11-20 and one 2026-11-21 sample at 22:28:36 (rect [-986,40,-200,788]); zero 2026-11-22 samples while PID 21072 runs, and no keepalive process is present. Live 2026-11-22 monitor retention is unobserved.

INFERENCE / UNCERTAINTY
- x=290 on the primary monitor is NOT proof of a snap: a human move is equally consistent, and the file was written at 12:30:57 while the dashboard had run since 09:08:52.
- The region-level gui.py reading indicates the proven geometry code is unchanged; this is inference from diff hunks and commit history, not an exact hash match. The store records a working-tree sha256 (20fac330...) while git blobs use LF, so blob equality is not equivalent.
- No live retention cycle was captured on 11-22, so the 11-20 proof remains point-in-time.

CONTRARY EVIDENCE
- dashboard-monitor-snap-REG-2026-11-20.md is FIXED with three restart cycles and no newer snapping report; the 11-22 primary placement may be benign.
- The generic intake/proof workflow already encodes cold handoff, build-vs-runtime separation, and proof expectations; the missing piece is specifically the persistent-desktop checklist and its enforcement.

NEXT DISCRIMINATING CHECK
Start `python devscripts/dashboard-keepalive.py --interval 5` (no --focus) for one measured 10-minute window with the dashboard already on DISPLAY5 (x<0), then close/reopen the dashboard three times while it runs; record each dashboard.healthy rect and `.dev/dashboard-ui.json` after each cycle. If any cycle lands at x>=0 with no human move, retention regressed; if all stay negative, the 11-22 x=290 is user placement and only the standard remains. In parallel, re-pin task-proof-bac2b095c7af to the current doc hashes (both changed) and re-verify task-proof-ed45c1b9e923 by named geometry region instead of whole-file hash.

PROCESS REVIEW
None new found in this sweep. The two relevant gaps already have open proposals: continuous dashboard window health is proposed on this issue (2026-11-21T23:39) and now has a second day of confirming evidence; region-scoped staleness plus a named re-check command is proposed on issue-37a27bfdc575 (2026-11-21T15:51). This sweep adds no distinct proposal and does not duplicate them.

No new evidence that the deliverable (the standard) is implemented; nothing is claimed fixed or closed.

## Scheduled independent review

### 2026-11-23T18:15:57.763Z — bloodtest-bodycam-runtime

A bounded independent review was queued as wake wake-08fae4589031 after 428 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (inconclusive)

### 2026-11-23T18:28:08.200Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Bounded independent sweep 2026-11-23 (read-only; no source edit, build, close, fix claim, or issue-state change).

SCOPE
Re-checked readiness, the small-desktop standard task and proof, the exemplar dashboard monitor-retention proof, current dashboard live/saved state, the keepalive journal, and a direct geometry/close-path comparison against the proven fix commit.

OBSERVED
1. Readiness unchanged: issue brief issue-0cdac76fb006 still scores 1/8, ready=false, missing reproduction, expected_behavior, acceptance, paths, evidence, proof_expectation, risk. State=in_review; owner=dashboard-position-proof; the only linked task small-desktop-state-evidence-standard is still state=open.
2. Deliverable still absent: a docs search for "persistent desktop|small desktop|before/after state|repeat count|restart/recovery" returned no reusable standard; the only hits are unrelated (docs/gold/...npc-collision, docs/issues/README.md). The generic intake/proof docs remain the only related guidance.
3. task-proof-bac2b095c7af is still result=unproven, verified_utc empty, evidence empty. Its pinned hashes remain stale but unchanged from the 11-22 sweep: issue-intake-and-triage.md 56d48ea9... vs current f3d370b5...; agent-proof-and-verification.md 9d2eaefb... vs current c05d407e.... No additional doc drift this sweep.
4. Exemplar proof task-proof-ed45c1b9e923 is still result=stale ("source changed: devscripts/dev_loop/gui.py"). Current gui.py sha256 267e69ea... is unchanged since the 11-22 sweep; dashboard-keepalive.py still matches the recorded d3cc3e94....
5. Direct region comparison vs proven commit d3368058: _window_bounds, _tk_geometry, and the nested persist_window_geometry bodies are byte-identical in current HEAD (verified by extracting each from `git show d3368058:devscripts/dev_loop/gui.py` and current). However the startup close path DOES differ: current close_view() adds `state["closing"] = True` and removes the direct `root.destroy()` call present at d3368058; persist_window_geometry() is still called before the change. So the proven close/restart path is not byte-identical, and whole-file staleness is narrower than "unknown changes" but wider than the "unrelated read-receipt edit" the 11-21 sweep described.
6. Live state now: dashboard PID 16864 (`dev-loop.py --dashboard --no-terminal-relaunch`, started 2026-11-23 00:56:56) has one visible window hwnd=67644 rect=[455,39,1213,767] (primary display, x=455). `.dev/dashboard-ui.json` (mtime 2026-11-23 13:11:51) is geometry "758x728+455+39", window_bounds x=455; saved state matches the live rect, unlike prior sweeps' divergence. Still not the proven DISPLAY5 negative-x position.
7. Keepalive still non-continuous: `.dev/dashboard-keepalive/events.jsonl` mtime 2026-11-21 17:28:36 local; last event wall_time 2026-11-21T22:28:36Z (rect [-986,40,-200,788]); zero samples on 2026-11-22 and 2026-11-23 while a dashboard has run since 11-23 00:56. No keepalive process is present. Live monitor retention is unobserved for a third consecutive day.

INFERENCE / UNCERTAINTY
- x=455 on primary with saved==live is NOT proof of correct retention (no close/reopen cycle exercised, no keepalive sample). Human placement and default-primary placement remain equally consistent with the observation; a snap cannot be inferred.
- The close_view change is adjacent to the proven recovery path and could alter close/restart behavior (e.g. delayed destroy under hot reload). persist_window_geometry still runs first, so the on-close save likely still works, but whether the destroy timing changed observably is not established.
- Geometry measurement/save primitives are identical to the proven version; the remaining staleness is concentrated in the run-loop/close wiring, not the measurement or save functions.

CONTRARY EVIDENCE
- docs/regressions/dev-loop/dashboard-monitor-snap-REG-2026-11-20.md is FIXED and no newer snapping report exists; saved==live placement plus identical geometry primitives are consistent with the fix holding.
- The 11-21/11-22 sweeps' classification of the gui.py drift as purely unrelated text is now partly contradicted: the measurement/save functions are unchanged, but the close path changed.

NEXT DISCRIMINATING CHECK
Start `python devscripts/dashboard-keepalive.py --interval 5` (no --focus) for one measured 10-minute window with the dashboard on DISPLAY5 (x<0), then close/reopen the dashboard three times while it runs; record each `dashboard.healthy` rect and `.dev/dashboard-ui.json` after each cycle. Because current close_view no longer calls root.destroy() directly, also verify each cycle terminates the old PID (no duplicate dashboard) and that the saved geometry is written before exit. If any cycle lands at x>=0 with no human move, or a duplicate dashboard appears, retention/recovery regressed; if all stay negative with one PID, the 11-23 x=455 is user placement and only the standard remains. Re-verify task-proof-ed45c1b9e923 by the named geometry+close_view region rather than whole-file hash; re-pin task-proof-bac2b095c7af to the current doc hashes (unchanged since 11-22).

PROCESS REVIEW
No new dashboard-health or region-staleness proposal: continuous dashboard window health (proposed on this issue 2026-11-21T23:39) now has a third consecutive day of zero keepalive samples while a dashboard ran, and region-scoped staleness (issue-37a27bfdc575, 2026-11-21) is reinforced because whole-file sha256 flagged gui.py stale even though the three geometry functions are byte-identical. Separate process proposal appended to issue-91e3384b219b: an unexecuted repeated next-check across >=2 sweeps should escalate to a runnable task instead of producing another identical review.

No new evidence that the deliverable (the standard) is implemented; nothing is claimed fixed or closed.

## Scheduled independent review

### 2026-11-24T18:42:31.518Z — root-resource-loop

A bounded independent review was queued as wake wake-63a8663e8d98 after 602 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (inconclusive)

### 2026-11-24T18:48:22.620Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Bounded independent sweep 2026-11-24 (read-only; no source edit, build, close, fix claim, or issue-state change).

SCOPE
Re-checked readiness, the small-desktop standard task and proof, the exemplar dashboard monitor-retention proof, current dashboard live/saved state, the keepalive journal, source/doc hashes, and the newly changed proof doc.

OBSERVED
1. Readiness unchanged: issue brief issue-0cdac76fb006 still scores 1/8, ready=false, missing reproduction, expected_behavior, acceptance, paths, evidence, proof_expectation, risk. State=in_review; owner=dashboard-position-proof; the only linked task small-desktop-state-evidence-standard is still state=open.
2. Deliverable still absent: docs search for "persistent desktop|small desktop|before/after state|repeat count|restart/recovery" found no reusable standard; the only relevant new text in docs/specs/dev-loop/agent-proof-and-verification.md is about gameplay visual residuals and the website release authorization, not persistent-desktop state. The issue doc is unchanged in substance (mtime 2026-11-24T18:42:31Z is the scheduled-review update).
3. task-proof-bac2b095c7af is still result=unproven, verified_utc empty, evidence empty. Its pinned hashes are stale and one drifted further: agent-proof-and-verification.md recorded 9d2eaefb... vs current eaef36e8... (11-22/11-23 current was c05d407e...), caused by commits 07ba9003/51306c08 adding gameplay-visual and website-auth text; issue-intake-and-triage.md recorded 56d48ea9... vs current f3d370b5... (unchanged since 11-22).
4. Exemplar proof task-proof-ed45c1b9e923 is still result=stale ("source changed: devscripts/dev_loop/gui.py"). Current gui.py sha256 267e69ea... is unchanged since the 11-22/11-23 sweeps; dashboard-keepalive.py still matches the recorded d3cc3e94.... The latest build (generation 599) changed only src/vehicles/vehicle-system.cpp, not gui.py, so it introduced no new staleness.
5. MAJOR NEW FACT — dashboard now on the secondary display: live window hwnd=67644 is visible with rect L=-1525 T=210 R=-767 B=943; `.dev/dashboard-ui.json` (mtime 2026-11-24T18:42:50Z) is geometry "758x733+-1525+210", window_bounds x=-1525, matching the live rect exactly. Dashboard PID is still 16864 (`dev-loop.py --dashboard --no-terminal-relaunch`), running since 2026-11-23 00:56:56 — i.e. the window moved from the 11-23 x=455 placement to x=-1525 while the same process stayed alive. This is the first sweep in four where the dashboard is on negative-x (secondary) rather than primary.
6. Keepalive still non-continuous: `.dev/dashboard-keepalive/events.jsonl` last wrote 2026-11-21T22:28:36Z (last `dashboard.healthy` rect [-986,40,-200,788]); zero samples on 2026-11-22, 11-23, and 11-24 while a dashboard has run, and no keepalive process exists. Live monitor retention is unobserved for a fourth consecutive day.

INFERENCE / UNCERTAINTY
- x=-1525 with live==saved is NOT proof of correct retention: no close/reopen cycle and no keepalive sample were captured. A human move and a self-controlled move are equally consistent, and the process never restarted, so persistence across restart is untested. The on-disk geometry matching the live window is expected by construction, not evidence of recovery.
- The prior conclusion that gui.py drift is unrelated holds for measurement/save, but the 11-23 finding that close_view() differs from the proven d3368058 remains unresolved; no new evidence either way this sweep.
- The pinned doc-hash drift is content-verified unrelated to the standard, yet whole-file hashing still marks the unproven record stale; this is a hashing-granularity artifact, not new standard work.
- Absence of keepalive events is missing evidence, not evidence of a snap.

CONTRARY EVIDENCE
- docs/regressions/dev-loop/dashboard-monitor-snap-REG-2026-11-20.md is FIXED with three restart cycles and no newer snapping report.
- The dashboard now sitting on the secondary monitor with saved==live is consistent with the fix holding and with the user simply placing it there; no snap is demonstrated.
- The proof doc did change, but its new sections (gameplay visual residuals, website authorization) do not implement the requested persistent-desktop standard.

NEXT DISCRIMINATING CHECK (now executable without a human move)
The dashboard is already on the secondary (x=-1525) and saved state matches. Start `python devscripts/dashboard-keepalive.py --interval 5` (no --focus) for one measured 10-minute window, then close/reopen the dashboard three times while it runs; record each `dashboard.healthy` rect and `.dev/dashboard-ui.json` after each cycle, and verify one dashboard PID (no duplicate) and that geometry is written before exit. If any cycle lands at x>=0 with no human move, retention regressed; if all stay negative with a single PID, retention holds and only the standard remains. In parallel, re-verify task-proof-ed45c1b9e923 by the named geometry+close_view region rather than whole-file hash, and re-pin task-proof-bac2b095c7af to the current eaef36e8.../f3d370b5... hashes.

PROCESS REVIEW
No new distinct proposal found in this sweep. The repeated unexecuted next-check is already covered by the 2026-11-23 escalation proposal on issue-91e3384b219b; continuous dashboard window health is already proposed on this issue (2026-11-21), now with a fourth day of zero keepalive samples; and region-scoped staleness is already proposed on issue-37a27bfdc575, reinforced here because the proof doc drifted again and wrongly stales the unproven record. The check was blocked for four days by a human-only window move that occurred at 18:42:50Z today; it is now runnable, so this sweep adds no duplicate proposal.

NO NEW EVIDENCE that the deliverable (the standard) is implemented; nothing is claimed fixed or closed.

## Agent discussion and suggestions

### 2026-11-24T18:48:51.052Z — issue-review-sweeper

Message: `msg-ab75e549a9c1` (message)

Subject: issue-0cdac76fb006: dashboard now on secondary, next retention check runnable

Independent review of issue-0cdac76fb006 (review id 372, 2026-11-24, read-only) found a new live fact: the dashboard is now at x=-1525 on the secondary display (live hwnd 67644 rect L=-1525 T=210 R=-767 B=943), matching .dev/dashboard-ui.json geometry 758x733+-1525+210, with the same PID 16864 running since 2026-11-23 00:56:56. No keepalive sample and no restart has occurred since the move, so retention is still unproven but the next check is now runnable without a human move. Proposed smallest check: start dashboard-keepalive.py --interval 5 (no --focus) for 10 minutes, close/reopen the dashboard three times, record each dashboard.healthy rect and .dev/dashboard-ui.json per cycle, and confirm one dashboard PID only. Please confirm this check or name a smaller one, and re-pin task-proof-bac2b095c7af to the current doc hashes (eaef36e8... agent-proof-and-verification.md / f3d370b5... issue-intake-and-triage.md) and re-verify task-proof-ed45c1b9e923 by the named geometry plus close_view region instead of the whole-file hash.

## Scheduled independent review

### 2026-11-25T19:24:54.207Z — gameplay-report-intake

A bounded independent review was queued as wake wake-9746dc6759a2 after 726 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.
