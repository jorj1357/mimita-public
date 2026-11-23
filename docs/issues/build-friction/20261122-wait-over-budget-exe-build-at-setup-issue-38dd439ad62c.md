# issue-38dd439ad62c: Wait over budget: exe-build at setup

## Human report (verbatim, immutable)

> exe-build wait at setup exceeded the wait budget


## Review (confirmed-build-friction-stale-stage-not-hang)

### 2026-11-22T20:22:16.985Z — build-wait-reviewer

Reviewer: build-wait-reviewer

REVIEW of issue-38dd439ad62c (exe-build wait at setup, wait_id exe-build:1:2026-11-22T19:46:07.237Z)

VERDICT: Confirmed build-friction occurrence; not a deadlock or queue stall. Acceptance met. Root cause is a legitimately slow EXE build mislabeled by a stale phase sample.

DIRECT OBSERVATIONS (facts)
- Issue evidence: kind=exe-build, stage=setup, reason=preparing build, elapsed=60.698s, budget=60.0s, started_utc=2026-11-22T19:46:07.237Z, build_id=2026-11-22T17:39:49.763492Z, generation=1, source_generation=3, request_ids=[].
- Budget is the default 60s: devscripts/dev-loop.py:109-110 (WAIT_ISSUE_SECONDS default 60).
- Wait is built from active_build_* state and elapsed is measured from active_build_started_monotonic (build-worker start), while stage is the last sampled active_build_phase: devscripts/dev-loop.py:3494-3512. Stage setup maps to preparing build: devscripts/dev-loop.py:3750-3751.
- The dev-loop only re-reads build progress when the build subprocess prints a stdout line: devscripts/dev_loop/build.py:205-211.
- build.py writes its first phase setup very early, before long silent work (build.py:634-635 writes setup; ccache_stats at 648; presentation-DLL decision; then the compile loop prints [CXX] at build.py:908).
- Authoritative build/progress.json for the EXE attempt that published at 2026-11-22T19:53:22Z (build_complete, updated 2026-11-22T19:53:02.472Z): elapsed_s=101.139, scheduler_wait_s=0.035 (no build-slot wait), compile_s=77.136, link_s=18.583, staging_s=0.036, pch_s=0.0, compiled=1 skipped=86, presentation_dll=cached_failure_skip.
- .dev/build-output.log for that attempt: one [CXX ] src/network/server.cpp, then [LINK] mimita.exe reason=object_set_changed; 65 [SKIP] lines. A single heavy TU dominated.
- .dev/state.json last_build_timing: build_script_seconds=105.84, source_identity_publish_seconds=19.432, post_build_processing_seconds=21.26, total_worker_seconds=127.559.
- Published artifact .dev/builds/2026-11-22T19-53-22.116562Z/build-identity.json: source_generation=4, source_changed_during_build=false. The waited attempt was generation=1; the published artifact was a later generation-4 attempt.
- Wake: exactly one wake.queued for wake-07e4d7ed39c4 at 2026-11-22T19:47:10.849Z (.dev/agent-wake/events.jsonl), then claimed codex, retried on opencode fallback. No second issue record carries this wait_id (dedup by _wait_issue holds).

INFERENCES
- The wait fired during an actively progressing EXE build. The real elapsed exceeded 60s because compiling one heavy changed TU (src/network/server.cpp, ~77s) plus linking (~18.6s) took longer than the budget; scheduler_wait_s=0.035 rules out a slot/queue stall. It was not a setup hang.
- The reported stage setup/preparing build is a stale last-sampled phase, not the phase that consumed the time: setup is written early, then the build goes silent for a long single-file compile, and the dev-loop only refreshes phase on stdout lines. This is the same account-for-every-stage / no-unknown-interval gap tracked by issue-136d4553ff92.
- The 60s budget is below the normal EXE build cost for a one-heavy-TU change in this checkout (~106s build script, ~128s worker), so this wait class keeps firing for legitimate builds (many open build-friction wait issues share this signature).
- The wait evidence build_id (2026-11-22T17:39:49.763492Z) is self.latest_build (the previous published artifact), not the in-progress attempt; generation=1 vs source_generation=3 shows the source moved during the build.

UNKNOWNS
- The generation-1 attempt per-phase split is not directly recoverable: build/progress.json and .dev/build-output.log were overwritten by the later generation-4 attempt, and .dev/dev-loop-watch.log is 1.13 GB with embedded NUL bytes and was not parsed. The split above is inferred from the completed build with identical inputs.

CONFIDENCE
- High that this was not a deadlock/queue stall (completed build; scheduler_wait 0.035s; only one compile+link).
- High that the stage label lags the true phase.
- Medium on the exact generation-1 setup-vs-compile split (progress file overwritten).

ACCEPTANCE CHECK
- Issue carries wait_id, stage, reason, elapsed, budget, build/generation/source_generation, request_ids: yes.
- Reviewer wake queued exactly once for this wait occurrence: yes (one wake.queued; later events are claims/retry of the same wake).

RECOMMENDED FIX (not claimed here; needs human approval / separate claim, owner: dev-loop build observability; cross-ref issue-136d4553ff92)
1. Refresh progress on the 1s wait-check tick, or have build.py emit a heartbeat during long compiles, so stage reflects the current phase; attribute elapsed per phase entry rather than from build start.
2. Report the in-progress requested generation and active request ids as build_id for an active build, instead of self.latest_build.
3. While progress.json elapsed_s is advancing, suppress a single over-budget EXE occurrence (or raise WAIT_ISSUE_SECONDS above normal one-TU build time) and file only when progress stalls.
