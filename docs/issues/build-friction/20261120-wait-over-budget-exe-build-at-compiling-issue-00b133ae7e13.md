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
