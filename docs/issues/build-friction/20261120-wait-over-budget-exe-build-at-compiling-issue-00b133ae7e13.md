# issue-00b133ae7e13: Wait over budget: exe-build at compiling

## Human report (verbatim, immutable)

> exe-build wait at compiling exceeded the wait budget


## Review (reviewed)

### 2026-11-20T20:08:12.406Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed the exact wait rather than just acknowledging it. No source edits, no file claims.

Wait and identity
- wait_id exe-build:32:2026-11-20T19:42:22.969Z, kind exe-build, stage compiling, elapsed 60.298s, generation 32, source_generation 32.
- Auto issue created 2026-11-20T19:43:23.339Z; reviewer wake wake-2ed65f7d688f claimed 2026-11-20T19:43:26.110Z. Only one issue exists for this exact wait_id, so the dedupe key worked for this occurrence.

Journal gap (blocking the requested exact-journal check)
- .dev/state.json points the active run journal at logs/2026-11-20/20261120_170259/dev-loop/events.jsonl, but that file does not exist (Test-Path False; the run dir now holds only run.json). The exact dev-loop journal was pruned while the run was still active. I used build/build-metrics.jsonl, build/build-timing.json, .dev/state.json, and published build-identity.json instead. The pruning itself belongs to the log-retention/reconnect owners, not this wait issue.

First dominant delay: single-TU compile under CPU contention, not a stuck build
- 19:45:36 build: compiled=1, skipped=86, elapsed=184.849s, phase_seconds.compile_s=130.017s, link_s=5.111s.
- 19:55:14 build: compiled=1, skipped=86, elapsed=266.061s, phase_seconds.compile_s=171.983s, link_s=6.063s.
- Idle-baseline same shape 07:47:06 (debug): compiled=1, elapsed=46.59s, compile_s=19.328s.
- So one translation unit took 130-172s while another build was running, versus 19s when idle (about 7-9x). The 60s budget is below the achievable incremental build latency under current load; the build did publish successfully (19:45:42 and 19:55:20), so this is latency/friction, not a hang.

Cause evidence
- build.py sets BUILD_JOBS to os.cpu_count() (12 here) when MIMITA_BUILD_JOBS is unset (build.py:125-135). The dev-loop game-DLL build also ran with jobs=12 (game-dll timing for game-dll-20261120T190454164010Z: jobs=12, compile 68.254s, object_scan 215.0s, total 374.222s).
- This host reports 12 logical CPUs, so an EXE build (12 workers) plus a game-DLL build (12 workers) is roughly 2x oversubscribed, before agent/test processes.
- Source churn compounds it: build-identity for 2026-11-20T19-45-42.207284Z has source_changed_during_build=true, and the 19:55:20 artifact also has source_changed_during_build=true. .dev/state.json now shows source_generation=34, latest_successful_source_generation=33, latest_build_stale=true, and a build request queued at 19:52:38. Each published EXE is already stale, which triggers another build and more over-budget waits.
- Recurrence: generation 33 started 19:50:40.561Z and hit 60.253s at stage setup ("preparing build"), auto-creating issue-723326f11472 at 19:51:40.840Z. I recorded it as an occurrence on this issue; same root cause. Note build_friction.py keys uniqueness on wait_id, so a stage change in the same build chain mints a new issue and a new reviewer wake (four reviewer wakes were claimed at 19:43:26).

Fix candidate (separate ownership, not claimed)
- Add cross-build compile-worker arbitration: cap EXE + game-DLL + module total workers at logical CPUs minus headroom, or serialize EXE compile against the game-DLL compile. Optionally debounce/verify source stability before publishing so artifacts are not immediately stale.
- Expected effect: restore 1-TU incremental EXE builds toward the observed ~19s idle compile baseline, which is well under the 60s budget, and reduce repeated waiter/reviewer churn.
- Confidence: high on the measurement and contention; medium on the best cap policy. No code changed here.

## Occurrence

### 2026-11-20T20:10:36.888Z — build-wait-reviewer

Recurrence 2026-11-20T19:50:40.561Z: exe-build generation 33 at stage setup exceeded the 60s budget at 60.253s (reason 'preparing build'), auto-created issue-723326f11472. Same root cause as this review: immediate EXE rebuild after the 19:45:42 artifact under a concurrent 12-job game-DLL build. Recorded as an occurrence of the exe-build latency finding rather than a duplicate analysis.

Evidence: {"budget_seconds": 60.0, "build_id": "2026-11-20T19:45:42.207284Z", "elapsed_seconds": 60.253, "generation": 33, "kind": "exe-build", "note": "Same exe-build chain as the reviewed wait; published 19:55:20 with compiled=1 elapsed=266.061s and source_changed_during_build=true", "reason": "preparing build", "recurrence_issue": "issue-723326f11472", "source_generation": 34, "stage": "setup", "started_utc": "2026-11-20T19:50:40.561Z", "wait_id": "exe-build:33:2026-11-20T19:50:40.561Z"}

## Review (reviewed)

### 2026-11-20T20:14:18.650Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Second reviewer activation for wake wake-62cbbb8af41f (request claimed 2026-11-20T19:56:26Z). The first activation already produced the substantive review on this issue at 20:08Z; I verified it against current state instead of restating it.

What I confirmed
- No duplicate issue exists for wait_id exe-build:32:2026-11-20T19:42:22.969Z; this issue is the only record for that exact wait.
- The exact run journal is still absent: Test-Path logs/2026-11-20/20261120_170259/dev-loop/events.jsonl is False, and the run directory still has no dev-loop events file. The journal gap noted in the first review is therefore not transient. I used build/build-metrics.jsonl, .dev/state.json, and .dev/builds/2026-11-20T19-55-20.256758Z instead.
- The latency and contention evidence in the first review still holds: the 19:45:36 and 19:55:14 EXE metrics show compiled=1 with compile_s 130.017 and 171.983 under the concurrent 12-job game-DLL build, versus compile_s 19.328 idle at 07:47:06. Builds did publish (19:45:42, 19:55:20), so this remains latency/friction, not a hang.

Chain state after this wait (supporting evidence, handled by their own issues)
- .dev/state.json at 20:10:51Z: source_generation=38, latest_successful_source_generation=33, latest_build_stale=true, build_status=pending, and build request 2026-11-20T19:52:38.802Z-2695cd08 still queued. The queued-build wait is issue-af958c3e10eb; recurring game-DLL waits are issue-191782d7d9c5 and issue-f66e6e82656b. No new issue or wake is needed for this occurrence.
- build-identity for 2026-11-20T19:55:20.256758Z still records source_changed_during_build=true and source_current=false, matching the first review's stale-publish observation.

Fix candidate remains as reported by the first review (cross-build worker arbitration / source-stability debounce), owned separately; no source edited and no files claimed by this activation.

## Occurrence

### 2026-11-22T02:32:33.000Z — build-wait-reviewer

Recurrence 2026-11-22T01:47:30.748Z (issue-17abc587c89a): exe-build generation 6 at stage setup exceeded the 60s budget at 61.375s (reason 'preparing build'). Request bc836913 had already produced queue issue-4891874dd02b and entered setup 244ms after game-DLL generation 440 published. The EXE build subprocess (pid 33160) then ran about 150s of setup and completed at 2026-11-22T01:53:27.861Z (root build/progress.json: elapsed_s 346.097, compile_s 190.844, link_s 6.578), but it was never published because the dev-loop daemon restarted (dev-loop.watcher.restarted daemon-not-alive at 02:01:23.908Z). Same flat-60s-budget root cause as this issue; recorded as an occurrence rather than a duplicate analysis. Reviewer build-wait-reviewer, wake-ff9869ed026f.

Evidence: {"budget_seconds": 60.0, "daemon_restart_reason": "daemon-not-alive", "daemon_restart_utc": "2026-11-22T02:01:23.908Z", "elapsed_seconds": 61.375, "exe_build_compile_s": 190.844, "exe_build_completed_utc": "2026-11-22T01:53:27.861Z", "exe_build_elapsed_s": 346.097, "exe_build_link_s": 6.578, "exe_build_subprocess_pid": 33160, "generation": 6, "journal_path": "logs/2026-11-22/20261122_011031/dev-loop/events.jsonl", "kind": "exe-build", "recurrence_issue": "issue-17abc587c89a", "request_ids": ["2026-11-22T01:33:57.208Z-bc836913"], "source_generation": 6, "stage": "setup", "wait_id": "exe-build:6:2026-11-22T01:47:30.748Z"}

## Scheduled independent review

### 2026-11-24T01:16:06.034Z — resource-profiler

A bounded independent review was queued as wake wake-aca5a63477a9 after 487 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (confirmed-recurring-class; root-fix-not-landed; journal-gap-persists; no-fix-claimed)

### 2026-11-24T01:38:33.251Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review (issue-review-sweeper, wake-aca5a63477a9, 2026-11-24T01:2xZ). Read-only: no source edit, no build request, no compiler, no second dev-loop, no close, no fix claim. Prior text preserved.

OBSERVED FACTS
1. Brief/readiness: kind=build-friction, state=in_review before this append, owner empty, 0 tasks, readiness ready=true missing=[] score=8. Two prior reviews exist (build-wait-reviewer, ids 18 and 20, 2026-11-20T20:08Z/20:14Z).
2. Exact-journal gap persists: Test-Path logs/2026-11-20/20261120_170259/dev-loop/events.jsonl = False on 2026-11-24T01:2xZ. The original 60.298s wait still cannot be re-derived from its own dev-loop journal; the prior reviews' fallback evidence remains the only 11-20 record. I did not re-open those 11-20 artifacts this sweep.
3. The same wait class recurs. The store holds 7 issues titled "Wait over budget: exe-build at compiling": this one (2026-11-20T19:43:23Z), issue-693e63059a7c (11-20 20:19), issue-4d9eb4b1a8ad (11-22 04:04), issue-727472d32238 (11-22 07:59), issue-74e7906c7f52 (11-22 19:50), issue-266ae06d18d0 (11-22 19:52), issue-3e601947fc16 (11-23 19:22).
4. Aggregate wait noise (live store, 2026-11-24T01:2xZ): 168 open "Wait over budget:*" build-friction issues: 145 game-dll-build at game_build, 7 exe-build at compiling, 7 exe-build at setup, 6 exe-build-queue at build_pending, plus 1 starting / 1 presentation_dll / 1 staging. States: 151 in_review, 16 new, 1 assigned. A prior independent measurement on issue-9a4503f6ed68 recorded 127 game-dll / 149 total at 2026-11-22T22:25Z, so the backlog grew about 19 in about 27h.
5. Flat budget fires on healthy builds (direct journal, logs/2026-11-23/20261123_192224/dev-loop/events.jsonl): over_budget at 21:52:16.574Z for wait game-dll-build:2026-11-23T21:51:15.080Z elapsed 60.37s, while the same attempt game-dll-20261123T215115211280Z-e6886c succeeded at 21:52:26.188Z total 70.85s. Same journal: 61.659s for attempt ...7401c3 (gen571, ok 827.407s), 60.745s for gen573 (ok 845.325s), 61.168s for gen574 (ok 1285.484s), and 61.338s for attempt ...6c7a9c which genuinely failed at changed_source_preflight. The 60s signal is therefore non-specific: it fires on normal 70-1285s builds and on a real failure alike.
6. Prior fix candidates have not landed. build.py:125-136 still defaults BUILD_JOBS to os.cpu_count() (12 here) with no cross-build arbitration/headroom; the latest EXE metrics entry (2026-11-23T19:22:12Z) is elapsed 70.314s, compile 44.143s, jobs 12. devscripts/build_friction.py was last committed d651e133 on 2026-11-20T18:25:38-0500, before this issue was created at 19:43Z, and still dedupes only on exact wait_id (_wait_issue / record_wait_incident, lines about 131-169).
7. Wake-status discrepancy on a newer same-automation occurrence: .dev/agent-wake/requests/wake-c4a5d204a3c9.json (issue-ecdc697b2415) shows created 2026-11-24T00:40:03Z, claimed/launched 00:59:44Z (about 19.7 min later), attempts=2, status="failed", error "adapter process exited; reconciled by scheduler". Yet a completed build-wait-reviewer review for that issue exists at 2026-11-24T01:05:55Z. Direct observation: queued-once is true, but request status and actual completion disagree.

INFERENCE / UNCERTAINTY
- The original exe-build-at-compiling wait is still best explained as EXE compile latency under concurrent 12-job game-DLL work (prior review: one TU 130-172s busy vs 19.3s idle). I did not independently re-measure 11-20 because that journal is gone; confidence stays medium-high on persistence of the class, not on the exact 11-20 ratio.
- Most of the 168 wait issues are a policy artifact (flat 60s + per-attempt minting), not 168 distinct defects; I did not individually triage all 168.
- The status=failed wake that nonetheless produced a review may be a reconciler naming/timing defect rather than non-delivery. Unknown; I did not read the reconciler.

CONTRARY EVIDENCE
- The automation is not dead: run 20261123_192224 recorded durable dev-loop.wait.over_budget / dev-loop.wait.reviewer-wake events for real waits, and the newest game DLL (gen577, 2026-11-24T01:05:45.955Z -> 01:06:33Z, about 47s) finished under budget with no wait issue. Not every build trips it.
- This issue's own acceptance was met for the original occurrence (record carries wait_id/stage/reason/elapsed/build identity; one reviewer wake), per the two prior reviews.

NEXT DISCRIMINATING CHECK (one)
- On the next 3 EXE builds with a 1-TU change, record build-metrics phase_seconds.compile_s and jobs plus whether a game-DLL build was in flight (dev-loop.game-build.started/result in the same window), then test: (a) if contention is the cause, concurrent-vs-idle 1-TU compile ratio should fall from the prior about 7-9x toward <2x after a worker cap, with 0 exe-build-at-compiling waits across those 3 builds; (b) if the flat budget is the cause, no exe-build wait should fire for builds that succeed within 2x budget (<=120s) across the same 3 builds. Either outcome is measurable from existing journals without new instrumentation.

Verdict: confirmed-recurring-class / root-fix-not-landed / journal-gap-persists / no-fix-claimed.
