# issue-727472d32238: Wait over budget: exe-build at compiling

## Human report (verbatim, immutable)

> exe-build wait at compiling exceeded the wait budget


## Context

### 2026-11-22T08:28:44.138Z — build-wait-reviewer

related_build: 2026-11-22T08:00:31.376619Z:4fa387f46e02a3a9d136abff3849b879306ecd354f07e145b35b10d20a8d2136
related_run: 20261122_011031

## Review (confirmed-benign)

### 2026-11-22T08:29:14.832Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed 2026-11-22T08:29Z against the root dev-loop and build artifacts. First divergence is NOT a stalled build: it is the global 60s wait budget being shorter than a legitimate full EXE compile.

Evidence:
- Wait occurrence exe-build:3:2026-11-22T07:58:48.330Z fired at elapsed 60.472s with reason 'compiling 0/45 [CXX] src\\host\\host-snapshot-fallback.cpp'. It was created by request 2026-11-22T07:58:47.625Z-997bacf1 (session cpp-latency-pressure-profile, recorded in .dev/agent-coord/events.jsonl seq 6473-6474 at 07:58:47.853Z).
- The same build completed and published artifact 2026-11-22T08:00:31.376619Z (source_generation 3, sha256 4fa387f4...); .dev/state.json and .dev/builds/2026-11-22T08-00-31.376619Z/build-identity.json confirm it. Total ~103s, so the wait was simply the compile still running, not stuck.
- Subsequent EXE requests 2026-11-22T08:10:42.398Z-5e6cca1b, 08:14:05.864Z-af7a1a5e, and 08:20:18.311Z-9e9bbdaa all succeeded (incremental/no-op, 5.4s for the last). No retry or failure is associated with this occurrence.
- The build succeeded on current source; no duplicate wait_id exists and the wake was queued exactly once (WAKE_9e188efba3fa40c7b8166707af300d8e, 2026-11-22T07:59:50.265Z), satisfying the issue acceptance criteria.

Root cause of the recurring friction: dev-loop WAIT_ISSUE_SECONDS is a single global 60s threshold (devscripts/dev-loop.py check_wait_budget ~3478-3539), applied equally to EXE compiling (45 TUs), game-DLL game_build (642 TUs), and queue wait, with no per-kind allowance and no progress-stall requirement. Observed issue set shows 20+ near-identical waits at 60.1-60.9s in the last hour while the corresponding builds completed successfully. Full EXE/DLL builds are expected to exceed 60s, so each legitimate build emits a new issue and reviewer wake.

Recommendation (needs a separate claim and human approval; do not fix here): calibrate per-kind/stage budgets from measured p95 (e.g. exe-build compiling well above ~103s, game-dll-build game_build above the observed ~96.5s) or require active_build_progress to be unchanged for N seconds before filing, and coalesce same-signature occurrences. This would suppress benign wakes without hiding genuine stalls.

Verdict: confirmed benign, build resolved; issue retained for the budget-calibration follow-up.

## Agent suggestion

### 2026-11-22T08:31:04.346Z — build-wait-reviewer

Review addendum (build-wait-reviewer): two known-but-unrecorded evidence defects apply to this occurrence. (1) The persisted evidence build_id is 2026-11-22T02:37:06.972018Z, which is the previously published artifact, not the in-flight EXE attempt that produced the wait; request_ids correctly carries 2026-11-22T07:58:47.625Z-997bacf1. In-flight attempt identity remains owned by issue-136d4553ff92. (2) The dev-loop journal path declared in .dev/state.json, logs/2026-11-22/20261122_011031/dev-loop/events.jsonl, does not exist, so the wait event itself cannot be read from disk; this matches open issue-0498cbef4ef3. Neither defect invalidates the review: the artifact 2026-11-22T08:00:31.376619Z and the successful follow-up requests prove the wait was a legitimate long compile.
