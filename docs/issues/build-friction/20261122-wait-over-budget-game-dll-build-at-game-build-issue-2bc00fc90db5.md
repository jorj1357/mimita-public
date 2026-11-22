# issue-2bc00fc90db5: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (pass)

### 2026-11-22T03:37:39.757Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Verdict: pass for this issue's acceptance. The over-budget game-DLL wait was detected, recorded exactly once, and woke exactly one reviewer. No source edit was made.

Verified facts
- Wait: game-dll-build:2026-11-22T02:49:52.361Z, stage game_build, elapsed 60.165s, budget 60.0s, request_ids ['auto-3'], source_generation 3.
- Dev-loop journal (logs/2026-11-22/20261122_011031/dev-loop/events.jsonl) line 350 has dev-loop.wait.reviewer-wake and line 351 has dev-loop.wait.over_budget for this wait_id; both reference issue-2bc00fc90db5, wake-c6c110fc5468, created=true. Exactly one pair exists for this wait_id.
- Issue readiness reports ready=true, missing=[]. Evidence carries wait_id, stage, reason, elapsed, budget, build_id 2026-11-22T02:37:06.972018Z, generation 442, source_generation 3, request_ids. Reproduction, expected behavior, acceptance, paths, and proof expectation are present.
- Wake: .dev/agent-wake/requests/wake-c6c110fc5468.json is the only request for this issue (created 02:50:53.459Z, attempts 2, final claim/launch 03:04:04). .dev/agent-wake/events.jsonl has one wake.queued for this issue.
- Build outcome: the waited-on build completed successfully. .dev/build-timings/game-dll-20261122T024952546213Z-9d33d3.json reports success=true, generation 443, total_seconds 361.455, worker_total_seconds 276.799, changed_paths ['src/npc/npc.cpp'], with build note bb717928341e from fun-cs-npc-objective. The wait was genuine build time, not a hang.

Findings and limitations (not grounds to fail this occurrence)
1. The 60s budget is far below real build time. This one-source build took 361s, dominated by changed_source_preflight 104.6s, link.gnu_linker 135.9s, publish.external_dependency_manifest 57.8s, and the src/npc/npc.cpp compile at 104.2s. In run 20261122_011031 alone, 12 over-budget waits created 12 separate high-priority build-friction issues between 01:25 and 03:17. That is the 'noisy wait policy' risk stated on each issue. This systemic theme is already owned by issue-e044a931c54b ('waiting is the enemy'); a consolidated per-run/per-build-cause issue or a budget scaled to expected build time would avoid one new issue per wait. Do not open another duplicate issue for it.
2. Reviewer wake delivery was retried and delayed: queued 02:50:53, first claimed/launched 02:51:02, failed and retried 03:02:24, re-claimed/launched 03:04:04; reviewer activation began around 03:07. Evidence: .dev/agent-wake/events.jsonl wake.queued 02:50:53.464Z, wake.claimed/launched 02:51:02, wake.retry 03:02:24.986Z, wake.claimed/launched 03:04:04, and attempts=2 in the request file. This is change-to-evidence latency separate from the build itself.
3. Minor identity note: the wait evidence records generation 442 at detection, while the same build attempt published generation 443. Neither is wrong, but consumers should not treat the wait's generation as the published one.

Confidence: high. Evidence is the exact dev-loop journal, wake queue record, build timing summary, and the issue record itself.
