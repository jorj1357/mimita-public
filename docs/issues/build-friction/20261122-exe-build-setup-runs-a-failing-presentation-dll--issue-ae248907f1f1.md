# issue-ae248907f1f1: EXE build setup runs a failing presentation-DLL build and hides ~2 minutes of no-op cost

## Human report (verbatim, immutable)

> [HOT RELOAD] presentation DLL failed; continuing with the EXE build


## Assignment

### 2026-11-22T06:59:44.787Z — cpp-latency-pressure-profile

Owner: cpp-latency-pressure-profile

## Agent suggestion

### 2026-11-22T07:13:13.319Z — cpp-latency-pressure-profile

Implementation and root-dev-loop evidence, 2026-11-22: build.py now keeps the legacy presentation DLL optional, emits one-second live stage elapsed updates, persists phase_details into final build/progress.json and build/build-timing.json, records return code/error, and negative-caches an unchanged failed-input fingerprint for 30 minutes. Source/build/toolchain/compiler stat changes invalidate the cache; MIMITA_RETRY_PRESENTATION_DLL=1 forces retry. No-op EXE result stayed success with 0 compiled / 87 skipped after child failures. Controlled sequence on identical stale inputs: initial helper failed in 19.101s and EXE no-op completed in 19.707s; repeated request skipped cached failure and completed in 0.941s. After phase-detail instrumentation changed build.py, retry failed in 12.069s (EXE no-op 12.599s). After changing build_game_dll.py to expose captured stdout, retry failed in 77.760s under a congested host (EXE no-op 78.284s); final progress now records outcome=failed, elapsed_s=77.760, error='build_game_dll.py exited with code 1'. The next unchanged retry skipped the failure and root dev-loop build/progress completed in 0.329s, outcome=cached_failure_skip, prior_error=exit code 1, 87 skipped, 0 compiled. The root request was accepted at 07:10:44.023Z but the worker process started around 07:10:55.948Z, leaving roughly 12s request-to-build.py setup latency to investigate; <=10s end-to-end is not yet met. During the 77.760s optional attempt, a synchronized 5s psutil sample saw 94.1% host CPU busy and PID 26748 at 5.309 CPU cores; compiler parent processes showed no CPU in that sample, so pressure is correlated but not proven to cause the helper delay. The shared runtime server was left untouched. Limitation: compiler failure is still only identified as child exit code 1; stderr-level diagnostics remain uncaptured, and the 30-minute cache key does not include all transitive headers. Next: capture merged stdout/stderr and actual error, reconcile run identity (status says FCS while server argv says ssbm), then instrument/optimize the ~12s pre-worker gap. Do not claim gameplay behavior or DLL activation proof.

## Agent suggestion

### 2026-11-22T08:37:00.676Z — cpp-latency-pressure-profile

Live fail-open evidence on root build requests: request 2026-11-22T08:32:29.828Z-55caf2f5 had 0 EXE translation units compiled / 87 skipped; optional presentation DLL failed with exit 1 after 21.616s, while primary EXE request succeeded and retained build 2026-11-22T08:00:31.376619Z. The unchanged request 2026-11-22T08:33:26.342Z-1a4f5b97 reported cached_failure_skip at 0.0s and again succeeded. The failed optional child is contained, but a newly fingerprinted failure can still add its full 21.616s once; input-change fingerprint/retry behavior remains the key latency check. Required EXE compiler/linker correctness gates must remain fail-closed.

## Agent suggestion

### 2026-11-22T09:37:51.642Z — cpp-latency-failure-containment

Correction to my prior generic candidate-DLL note on issue-136d4553ff92: user meant this issue's optional presentation-DLL helper failing during an EXE build. The existing build.py path already fails open: the primary EXE build continues, stores the optional result, and negative-caches a failed unchanged input fingerprint for 30 minutes; MIMITA_RETRY_PRESENTATION_DLL=1 forces a retry. Recorded matched check from prior work: failed optional helper 21.616s but EXE request succeeded; the next unchanged request skipped the helper in 0.0s and succeeded. The long-goal continuity practice is already documented in docs/gold/20261030_033224-agent-session-handoff-gold.md under 'Long-running goals survive turns and session resets' and 'Optional tools and accelerators fail open'; it says continue on 'continue', retain verified checkpoint, and do not let optional failures mark the primary goal blocked. Remaining concrete gap for this issue is whether the failure fingerprint covers transitive headers used by the two SOURCES: build_game_dll.py compiles direct source list with no dependency file, while build.py fingerprints only those sources, game-api.h, scripts, toolchain, and compiler. Thus a changed included header may not invalidate skip/retry. Claim: fail-open behavior is observed and effective; dependency completeness is unknown; next discriminating work is a dev-loop-owned controlled header-touch/retry measurement and a dependency-aware fingerprint implementation that keeps warm setup under 1s.

## Occurrence

### 2026-11-22T19:06:50.343Z — build-wait-reviewer

Recurrence during the EXE attempt behind exe-build:4:2026-11-22T17:23:49.758Z (issue-6990b0ff69ec): .dev/presentation-dll-attempt.json records outcome failed, elapsed_s 92.516, exit_code 1, error 'build_game_dll.py exited with code 1', failed_at_epoch ~17:25:38Z. The 61.454s over-budget wait sampled while this optional child was running. build.py:652-812 makes the child fail-open, so it did not fail the EXE; the EXE attempt still failed at 17:32:12.249Z after 494.796s for a reason not retained (build-output.log was overwritten by the 17:38:44 successful build). The child added ~92.5s of avoidable EXE-build time.

Evidence: {"exe_attempt_duration_seconds": 494.796, "exe_attempt_failed_utc": "2026-11-22T17:32:12.249Z", "exe_attempt_started_utc": "2026-11-22T17:23:49.758Z", "issue": "issue-6990b0ff69ec", "journal": "logs/2026-11-22/20261122_144609_01/dev-loop/events.jsonl", "presentation_dll_elapsed_s": 92.516, "presentation_dll_error": "build_game_dll.py exited with code 1", "presentation_dll_exit_code": 1, "presentation_dll_failed_at_epoch": 1795368338.42, "presentation_dll_outcome": "failed", "wait_id": "exe-build:4:2026-11-22T17:23:49.758Z"}
