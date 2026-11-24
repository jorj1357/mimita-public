# issue-d6a3c6c35682: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-slow-preflight-and-duplicate-flood)

### 2026-11-24T21:37:43.330Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Independent bounded review by build-wait-reviewer for over-budget wait game-dll-build:2026-11-24T20:42:56.856Z (issue issue-d6a3c6c35682); read-only for source, build, runtime, and issue state. Wake wake-f3dc6eef636d.

OBSERVED FACTS
1. Wait: kind game-dll-build, stage game_build, started_utc 2026-11-24T20:42:56.856Z, elapsed 61.17s vs budget 60.0s; source_generation 19; request 2026-11-24T20:42:56.343Z-f96ab130:mode3:game; issue created 2026-11-24T20:43:58.416Z. Exactly one issue carries this wait_id.
2. The waited-on build is real work, not a stall, and it FAILED. Request f96ab130 maps to the only matching attempt, game-dll-20261124T204256895649Z-e6e8af: outcome failed, generation 618, total_seconds 185.928, changed src/main-systems.cpp, failed_phase changed_source_preflight, worker_error "changed-source preflight failed: src/main-systems.cpp in 173.268s". Phase durations: changed_source_preflight 173.276s, toolchain_setup 0.111s, identity 0.021s. At the 60s trigger the build was inside the 173.3s changed-source preflight of one translation unit. The attempt then ended in a genuine compile error: src/main-systems.cpp:476,477,492 reference mimita::game::playerPtr() and mimita::game::worldPtr(), which are not members of mimita::game. Source: .dev/build-timings/game-dll-20261124T204256895649Z-e6e8af.json.
3. Evidence attribution defect: the issue records build_id=2026-11-24T19:27:02.742428Z (the last completed artifact) and generation=618 (the attempted generation). related_build and related_run are both empty. The attempt is recoverable only via request_ids[0] plus started_utc (or the revision a11e13be...). This is the same stale in-flight identity defect already recorded on issue-21e4b1fbc77e and umbrella issue-9a4503f6ed68.
4. Journal half missing: a grep across every logs/2026-11-24/*/dev-loop/events.jsonl found zero rows carrying this wait_id, this issue id, or the 2026-11-24T20:42:56 timestamp. The only retained trace is .dev/agent-wake/requests/wake-f3dc6eef636d.json. This matches the events_path=None early-return drop tracked on issue-6e2d7b91e7d9. Inference: the exact dropped write was not captured; the absence is directly observed.
5. Reviewer wake timing: wake-f3dc6eef636d queued once at 2026-11-24T20:44:00.665Z. codex claimed/launched 20:44:03.306/20:44:03.444Z; wake.retry "previous adapter process exited; trying fallback" at 20:56:34.044Z; opencode claimed/launched 21:13:39.896/21:13:40.537Z; request attempts=2. Queue-to-final-launch is about 1779.9s (~29.7 min), which EXCEEDS the proposed <=5 min target, so late dispatch IS reproduced here (unlike issue-21e4b1fbc77e at 125s). Two wake.launched events exist for one wait_id, so the exactly-one-launch target is also violated. Sources: .dev/agent-wake/events.jsonl and .dev/agent-wake/requests/wake-f3dc6eef636d.json.
6. Resource pressure (direct observation, not a proven cause): host has 12 logical CPUs; 133 samples at ~1s over 185.93s recorded mean host CPU 99.981%, peak 100%, and minimum available memory 17,514,496 bytes (~16.7 MiB). Live client pid 22968 (run 20261124_202802) recorded 21.62 FPS, p95 90.4 ms, max 264.9 ms, and 24 of 24 frames over the 4.17 ms budget at 2026-11-24T20:45:56Z during the wait. Sampler unavailable_fields: matched_prebuild_resource_baseline, host_commit_charge_and_limit. Sources: .dev/build-timings/game-dll-20261124T204256895649Z-e6e8af.resource.jsonl and .resource.json. Unknown: whether host contention causally stretched the 173.3s preflight.
7. Duplicate flood confirmed: 176 build-friction issues in the shared brain already share the exact title "Wait over budget: game-dll-build at game_build" (spanning new and in_review). This realizes the issue's own risk, "A noisy wait policy could create duplicate friction issues." Root systemic cause: build_friction.record_wait_incident keys uniqueness on the per-occurrence wait_id while the fixed 60s budget is shorter than a normal changed-TU game-DLL front-end compile, so almost every non-trivial build emits one more near-duplicate issue. This is the highest-value follow-up and belongs on the umbrella, not as a new issue.
8. Acceptance check: "carries wait_id, stage, reason, elapsed, build/run IDs, and evidence" is NOT met as written (stale build_id/generation, empty related_build/related_run, no dev-loop journal). "A reviewer wake is queued exactly once for this wait occurrence" is met at the queue level (one wake.queued) but dispatch is late (~29.7 min) and double-launched.

CONCLUSION
Not a stalled build. The 60s wait budget overlapped a slow single-TU changed_source_preflight (173.3s) that also failed to compile due to invalid mimita::game::playerPtr()/worldPtr() references. This review performed no edit, build, runtime launch, issue close, or proof claim. Issue remains in_review.

OWNER / NEXT STEP (not claimed by this review)
- issue-9a4503f6ed68 owner wake-proof-wait: (a) include the in-flight attempt id/generation in wait evidence and populate related_build; (b) aggregate recurring waits by signature/occurrence instead of minting a fresh issue per wait_id (176 near-duplicates), and revisit the fixed 60s budget against measured per-TU compile time.
- issue-6e2d7b91e7d9: write wait events when the daemon has no active run (events_path None).
- issue-84c584957e65: preserve exactly one launch per wait_id and bound dispatch latency; this occurrence re-reproduces both defects (2 launches, ~29.7 min).
- issue-ed3ac775283d owner root-resource-loop: decouple long game-DLL builds from live-pair CPU saturation.
- Transient compile error belonged to src/main-systems.cpp; later attempts (e.g. generation 621) succeeded, so it is not a durable issue.
UNKNOWNS: whether the missing journal row was rotated/deleted or never written; whether host saturation causally lengthened the 173.3s preflight.
