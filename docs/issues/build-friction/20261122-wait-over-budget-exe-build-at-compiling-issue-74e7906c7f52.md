# issue-74e7906c7f52: Wait over budget: exe-build at compiling

## Human report (verbatim, immutable)

> exe-build wait at compiling exceeded the wait budget


## Review (confirmed-expected-build-duration)

### 2026-11-22T20:30:10.525Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed wait occurrence exe-build:3:2026-11-22T19:49:56.016Z (wake-99e5a0ccc172).

VERDICT: confirmed-expected-build-duration. The wait was a real, still-progressing EXE build crossing a flat 60s budget; it was not a stall, deadlock, or build failure. No source change was made.

DIRECT OBSERVATIONS (facts)
- Issue evidence: kind=exe-build, stage=compiling, elapsed=60.772s, budget=60.0s, reason="compiling 0/45 [CXX] src\replay\replay-factory-worker.cpp", generation=3, source_generation=4, started_utc=2026-11-22T19:49:56.016Z, build_id=2026-11-22T17:39:49.763492Z (prior successful artifact; check_wait_budget uses self.latest_build, devscripts/dev-loop.py:3498-3511).
- The build cycle that owned this wait completed successfully: .dev/state.json last_build_result=success, latest_successful_build=2026-11-22T19:53:22.116562Z, build_status=published_current, source_generation=4. Artifact .dev/builds/2026-11-22T19-53-22.116562Z/mimita.exe exists (20,391,164 bytes) with matching build-identity.json (sha256 131d88c3..., source_changed_during_build=false).
- Duration evidence for that build: last_build_duration_seconds=127.559s (setup 0.443s, build_script 105.84s, post_build 21.26s, publish 1.63s). build_output_tail: "BUILD SUCCESS ... Compiled: 1 Skipped: 86 Time: 101.14 sec".
- Root dev-loop console: "[DEV] published build 2026-11-22T19:53:22.116562Z" and "BUILD: phase=published_current ... stale=False" (status=success).
- Budget source: WAIT_ISSUE_SECONDS is a flat default 60s env-overridable (devscripts/dev-loop.py:109-110) applied to every wait kind (check_wait_budget, devscripts/dev-loop.py:3486-3577). A cold EXE build here runs ~101-128s, so crossing 60s mid-compile is expected.
- Sibling occurrence exe-build:4:2026-11-22T19:51:16.116Z for the same final build (19:53:22) was independently reviewed on issue-266ae06d18d0 with the same conclusion (confirmed-expected-build-duration). This issue is an earlier wait occurrence in that same build cycle, not a separate defect.
- Wake evidence: .dev/agent-wake/events.jsonl shows exactly one wake.queued for wake-99e5a0ccc172 at 2026-11-22T19:51:00.089Z (issue recorded 19:50:56.867Z), then codex claim/launch 19:51:03, stale recovery 20:21:34, and opencode claim/launch 20:21:35. One wake request per occurrence: acceptance met.

MISSING EVIDENCE / CAVEAT
- The dev-loop journal half of the proof (dev-loop.wait.over_budget / dev-loop.wait.reviewer-wake) is absent because the active root dev-loop events_path is C:\mimita-v9\logs\2026-11-22\20261122_011031\dev-loop\events.jsonl and that directory does not exist; every append fails ENOENT (e.g. .dev/dev-loop-watch.log 2026-11-22T13:56:08Z "unable to append journal event ... No such file or directory"). Already tracked by issue-6e2d7b91e7d9 / issue-0498cbef4ef3; not a property of this wait. The occurrence remains corroborated by the build-friction issue record and the wake request/journal.
- UNKNOWN: exact attribution of the "0/45 replay-factory-worker.cpp" sample to generation 3 versus the later generation-4 worker start at 19:51:16; the recorded stage/detail is the last sampled active_build_phase and may be a stale sample from the superseded attempt (same stale-phase effect reviewed on issue-38dd439ad62c). This does not change the outcome: the published build succeeded.

ACCEPTANCE: met. wait_id, stage, reason, elapsed, generation/source_generation, build_id, and evidence are present; exactly one reviewer wake was queued for this occurrence. No code fix is required for this wait. Suggested separate improvement (not applied, needs human approval and its own claimed task): make WAIT_ISSUE_SECONDS stage/kind-aware (or defer the issue when the build completes successfully within a grace window) so one slow-but-successful EXE build does not spawn an issue per minute of compilation.
