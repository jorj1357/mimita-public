# issue-8aae0621645f: Wait over budget: exe-build at setup

## Human report (verbatim, immutable)

> exe-build wait at setup exceeded the wait budget


## Review (confirmed-wait-policy-noise)

### 2026-11-22T03:45:44.794Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review by build-wait-reviewer, wake-e272820f542d, 2026-11-22. Read-only: no source edit, no build or test requested, no fix or closure claimed.

CONCLUSION
Not a stuck build. The exact EXE attempt behind this wait completed successfully as a no-op 192.6s after the request (build.py itself measured 143.1s), 76.7s after the flat 60s budget fired. The wait is expected noise from a 60s threshold measured against real, currently unobservable setup work. The dominant avoidable component is the optional presentation-DLL build inside setup: it ran, failed, and hid ~2 minutes; a separate issue now tracks it (issue-ae248907f1f1). Record completeness and one-wake-per-occurrence acceptance pass for wait mechanics; active-attempt attribution is still wrong.

VERIFIED WAIT OCCURRENCE
- wait_id exe-build:1:2026-11-22T02:34:20.132Z; kind exe-build; stage setup; reason "preparing build"; elapsed 60.019s vs 60.0s budget; started_utc 02:34:20.132Z; generation 1; source_generation 1; request_ids [2026-11-22T02:34:19.804Z-075c3f49].
- The request was accepted at 02:34:19.848Z (dev-loop.control action=build, requested_by codex-ingame-gui-reconnect). build-authority.json has dev-loop owner pid 21044 started_utc 02:34:31Z.
- logs/2026-11-22/20261122_011031/dev-loop/events.jsonl has exactly one dev-loop.wait.over_budget for this wait_id (created=true, wall_time 02:35:36.853Z) and exactly one dev-loop.wait.reviewer-wake for the same wait_id (wall_time 02:35:36.852Z). No occurrence was appended (issue occurrences=[]).
- gen-1 EXE build completed: dev-loop.build.completed wall_time 02:37:36.433Z, artifact 2026-11-22T02:37:06.972018Z, duration_seconds 192.626, request_ids match. .dev/state.json latest_successful_build_id 2026-11-22T02:37:06.972018Z:c94488e6... confirms success.

PINNED ATTEMPT EVIDENCE (no-op, so not compile latency)
- build/progress.json for this attempt: result=noop, compiled=0, skipped=87, elapsed_s=138.527, updated_utc 02:36:56.161Z; phase_seconds compile_s 0.586, pch_s 0.0, staging_s 0.062, scheduler_wait_s 0.007.
- build/build-metrics.jsonl 2026-11-22T02:36:56Z: elapsed_s 143.118, result noop, compiled 0, jobs 12. Only 0.655s of the 138.527s build.py wall is inside measured phases; ~142.5s is in the single "setup" progress span.
- .dev/build-output.log for this build: line 2 "[BUILD EXCLUSIVITY] acquired the single EXE build", line 3 "[HOT RELOAD] presentation DLL failed; continuing with the EXE build", then the SKIP list, "Nothing changed. (current)", "Time    : 138.53 sec".
- build/mimita-game.dll mtime 2026-11-13T00:00:19Z is older than src/effects/effect-part.cpp 2026-11-18T09:09:54Z, so build.py:662-673 sets game_dll_needed=true and runs build_game_dll.py on every EXE build until it succeeds. build_game_dll.py printed none of its own [HOT RELOAD]/[BUILD LOCK]/[TOOLCHAIN] lines, so its failure reason is invisible in the journal and in build-output.log.

WHAT THE WAIT ACTUALLY WAS / FIRST DIVERGENCE
- build.py writes progress "setup" once at build.py:631 (after slot+mutex acquisition) and does not write again until build.py:843 ("compiling"). build.py:647-685 then runs ccache_stats (timeout 15s, build.py:424-442), the optional presentation-DLL subprocess (build.py:662-673), and find_cpp_files (measured now at 0.95s for 45 EXE sources; build/source-manifest.json exists). Those ~142.5s stay labelled "setup"/"preparing build" with no sub-stage.
- First divergence: the dev-loop wait tracker (dev-loop.py:3483-3501) only checks elapsed time and the last written phase; build.py emits no setup sub-stage, so a legitimate long setup is indistinguishable from a stall. The flat WAIT_ISSUE_SECONDS=60 (dev-loop.py:109-110) is below this stage's observed cost.
- Corpus: build-metrics.jsonl shows normal no-op setup gaps of 4-30s, but this attempt's gap is 142.5s and prior no-op gaps reached 456.3s (2026-11-20T16:10:32Z). The top contributor here is the failed presentation-DLL attempt, which cannot help the EXE and repeats while the DLL is stale.

ATTRIBUTION DEFECTS (persist from sibling reviews)
- Evidence build_id is 2026-11-21T02:26:50.306117Z, the previously published EXE artifact, not this attempt; dev-loop.py:3497 sets build_id=self.latest_build. generation=1 equals source_generation here, but the in-flight artifact id 2026-11-22T02:37:06.972018Z is not in the evidence.
- reason is the generic "preparing build", not the sub-stage or the failing DLL attempt.
- occurrences stays empty: dev-loop.py:3539 suppresses repeats of the same wait_id via reported_wait_ids, so the terminal success (192.626s) is never appended even though build_friction.record_wait_incident can append (build_friction.py:164-169).

WAKE DELIVERY
- .dev/agent-wake/events.jsonl: wake-e272820f542d queued 02:35:20.925Z; first claimed/launched 02:35:36.721Z/754Z (pid 2112, exited); retry 02:42:21.040Z; finally claimed/launched 02:51:02.869Z/917Z (pid 11728, this opencode session). One wake, ~15m42s delivery latency from adapter fallback; same pattern as issue-44857c7d5b01 and issue-ee4dac126d29.

POLICY NOISE / DUPLICATION
- Brain: 201 issues, 81 build-friction, 70 "Wait over budget", 5 "Wait over budget: exe-build at setup" (3 in_review, 2 new). Dedupe is per exact wait_id (build_friction.py:131-169), so each occurrence intentionally creates a new issue; this issue's own risk note already names that risk. Do not lower or raise the 60s budget here; a per-class budget needs human approval.
- Prior same-title issues (issue-17abc587c89a, issue-723326f11472, issue-7d013b4a6c77, issue-dff7e8c0f758) are distinct wait occurrences by design; this review adds the 2026-11-22T02:34:20.132Z evidence rather than creating a duplicate.

ACCEPTANCE CHECK
- readiness ready=true, missing=[]; wait_id, stage, reason, elapsed, evidence present; build/run IDs present but build_id points at the previous artifact (defect above); exactly one reviewer wake for this occurrence. Acceptance is met for wait mechanics, not for accurate active-attempt attribution.

RECOMMENDATION (not claimed here)
- Follow-up filed: issue-ae248907f1f1 (failing presentation-DLL build inside EXE setup, ~2 min no-op cost, invisible child failure, deterministic while build/mimita-game.dll is stale).
- Systemic owners remain issue-e044a931c54b (build-runtime-subagents: remove avoidable waits) and issue-136d4553ff92 (build-coordinator: stage accounting). Add setup sub-stage progress (presentation_dll, source_scan) and stop attributing an in-flight wait to self.latest_build; append the terminal result as an occurrence.
- Keep this issue in_review until attribution/sub-stage evidence is fixed and verified in a journal.

Confidence: high on the wait mechanics, the no-op numbers, and the failed presentation-DLL attempt (direct log + progress + metrics + file mtimes); medium on the exact split between the DLL attempt and other unmeasured setup work, because build.py does not time those substeps. No source, build, or runtime behavior changed by this review.

## Agent suggestion

### 2026-11-22T07:41:39.943Z — build-wait-reviewer

Recurrence (build-wait-reviewer, wake-21757ebfb6a7, 2026-11-22T07:08:26.104Z): same signature and root cause, recorded here as an occurrence rather than a new analysis.

- wait_id exe-build:10:2026-11-22T07:08:26.104Z; stage setup; reason preparing build; elapsed 60.675s vs 60.0s budget; auto-created issue-34969db31bf6 and woke build-wait-reviewer once.
- Exact attempt: build/build-metrics.jsonl 2026-11-22T07:09:58Z result noop, compiled 0, skipped 87, elapsed_s 80.833, phase_seconds.presentation_dll_s 77.76, phase_details.presentation_dll outcome failed with build_game_dll.py exited with code 1. The 60.675s wait fired during that 77.76s failed presentation-DLL child.
- The 07:10:56 attempt then shows phase_details.presentation_dll outcome cached_failure_skip (previous_error build_game_dll.py exited with code 1, retry_after_s 1742.404), confirming the issue-ae248907f1f1 root cause and showing the new failure cache suppresses repeats for ~29 min.
- Attribution defect persists: evidence build_id 2026-11-22T02:37:06.972018Z is the previous published artifact, not this attempt; request_ids [].
- Root dev-loop journal logs/2026-11-22/20261122_011031/dev-loop/events.jsonl is absent, so no on-disk dev-loop.wait.over_budget event for this wait_id (issue-0498cbef4ef3).
- Counts after this occurrence: 6 open exe-build-at-setup wait issues, all 60.0-61.4s at the same stage/reason; 83 open game-dll-build wait issues. Dedupe is per wait_id, so each occurrence mints a new issue and wake.
