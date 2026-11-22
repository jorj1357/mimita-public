# issue-17abc587c89a: Wait over budget: exe-build at setup

## Human report (verbatim, immutable)

> exe-build wait at setup exceeded the wait budget


## Agent suggestion

### 2026-11-22T01:51:47.964Z — cpu-build-efficiency

Correlated wait evidence for this EXE request: request 2026-11-22T01:33:57.208Z-bc836913 was accepted/queued at 01:33:57Z. Root dev-loop showed the EXE build start/setup at 01:47:30Z, a ~13m33s request-to-setup delay; it created this budget issue after setup waited 61.375s. At ~01:49:50Z status still reported setup / preparing build after ~139s, with output [BUILD EXCLUSIVITY] waiting for the single EXE build followed by cquired; no compile worker was consuming CPU during the earlier setup sample. Candidate DLL generations 439/440 completed separately in ~530s and ~525s. Current client/server stayed alive throughout. Inference: build-slot and preparation delays are measured contributors; process CPU contention during actual EXE compile remains unmeasured. The documented gent-coord retro <issue> command is absent from the CLI (reproduced invalid-choice error), so I could not run the required reflection; propose implementing the command or updating the spec, with a focused CLI smoke check. Next: inspect this EXE attempt's timed phases and exact mutex owner/build worker, then matched idle-game versus active-game build timing.

## Review (reviewed-duplicate-wait-policy)

### 2026-11-22T02:32:32.980Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed the exact wait for issue-17abc587c89a as build-wait-reviewer (wake-ff9869ed026f). No source edits; no file leased by another agent was claimed.

Wait identity and acceptance
- wait_id exe-build:6:2026-11-22T01:47:30.748Z; kind exe-build; stage setup; reason "preparing build"; elapsed 61.375s vs 60.0s budget; generation 6; source_generation 6; build_id 2026-11-21T02:26:50.306117Z; request_ids [2026-11-22T01:33:57.208Z-bc836913].
- Journal record: logs/2026-11-22/20261122_011031/dev-loop/events.jsonl line 155, event dev-loop.wait.over_budget at 01:48:36.354Z created=true; line 154 dev-loop.wait.reviewer-wake queued once (wake-ff9869ed026f). Acceptance (wait_id/stage/reason/elapsed/build IDs/one reviewer wake) is met.
- Issue readiness was already 8/8 with no missing fields before this review.

First divergence: pre-compile "setup" latency under a flat 60s budget, not a stuck build
- The waited-on EXE build did real work and finished successfully. Root build/progress.json reports the EXE build subprocess (pid 33160) at phase=build_complete, result=success, updated_utc 2026-11-22T01:53:27.861Z, elapsed_s 346.097, phase_seconds compile_s 190.844, link_s 6.578, staging_s 0.031. Subtracting compile+link+staging leaves about 148.6s before compile, so the dev-loop "setup" window ran roughly 150-160s (active-build start 01:47:30.748Z to about 01:50:10Z) while the flat check fires at 60s. The 61.375s was the first budget tick inside a genuinely long setup, not noise around the threshold.
- Serialization: request bc836913 was accepted/queued at 01:33:57.208Z (journal line 104) and its queue wait had already produced issue-4891874dd02b (exe-build-queue, 60.922s). It entered setup at 01:47:30.748Z, 244ms after game-DLL generation 440 published (dev-loop.game-generation.ready 01:47:30.504Z, journal line 151). The EXE build could only start once the game-DLL build released; this is the second over-budget issue for the same request, matching cpu-build-efficiency's 01:51:47Z append on this issue.
- build.py's "setup" window also includes waiting on the OS-wide single-EXE mutex (devscripts/build_exclusive.py:37-44 "[BUILD EXCLUSIVITY] waiting/acquired"), which cpu-build-efficiency observed. The mutex holder is not logged (owner string is literally "build.py"), so lock-wait time cannot be separated from source-scan time. That is the same unmeasured pre-compile gap targeted by issue-136d4553ff92.

Root cause and duplicate disposition
- Root cause is the flat budget policy, not this wait. devscripts/dev-loop.py:109 sets one WAIT_ISSUE_SECONDS=60 for every stage, and check_wait_budget() (dev-loop.py:3475-3566) has no per-stage budget and no progress/stall test, so a normal EXE pre-compile/source-scan on this host under load exceeds 60s. Same reviewed root cause as issue-ca900e60ed4b and issue-450cfc80a0b9, parent issue-e044a931c54b (owner build-runtime-subagents), measurement issue-7d3dcfd7433b.
- This is a recurrence of the exe-build over-budget chain on issue-00b133ae7e13 (review 2026-11-20T20:08Z) and its occurrence for issue-723326f11472. There are now at least four "exe-build at setup" auto-issues (723326f11472, 7d013b4a6c77, dff7e8c0f758, 17abc587c89a) because build_friction.py keys uniqueness on wait_id, so each stage/occurrence mints a new issue and wake. I am recording this as an occurrence on issue-00b133ae7e13 instead of opening a duplicate fix.
- No new code fix claimed here. Fix candidates stay owned elsewhere: a per-stage budget/progress test in check_wait_budget, cross-build compile-worker arbitration (cap EXE+game-DLL+module workers below logical CPUs), and logging the mutex holder during EXE setup.

Related finding (separate owner): successful EXE build was never published
- Although build/progress.json shows the EXE subprocess (pid 33160) succeeded at 01:53:27.861Z, latest_successful_build stayed 2026-11-21T02:26:50.306117Z and .dev/builds has no 2026-11-22 artifact. The run journal records no dev-loop events between 01:48:36.354Z and dev-loop.watcher.restarted at 02:01:23.908Z (reason daemon-not-alive, old_daemon_pid 4008 -> new 16872). This is consistent with the daemon dying while the build subprocess finished orphaned, so the completion was never drained/published. Confidence: medium (timeline + missing artifact + restart event); the daemon's first cause is not in this run journal and is not claimed here. It belongs to the daemon/publication and lifecycle-measurement owners (issue-136d4553ff92; publication-failure class issue-61b5edc937d9), not to this wait issue.

Confidence
- High that the wait is a real over-budget setup (~150s), not a hang, and that the flat 60s policy is the direct trigger.
- High that this is a duplicate of the existing exe-build wait-policy chain; no new fix should be opened here.
- Medium on the daemon-death/lost-publish sequence; the exact first cause is not established.

Verification performed: read the issue record, dev-loop journal lines 104-156, root build/progress.json, .dev/state.json, build_exclusive.py, build_friction.py, dev-loop.py check_wait_budget, and the prior nullable reviews on issues 00b133ae7e13 and 723326f11472. No build, compiler, or test run was started.
