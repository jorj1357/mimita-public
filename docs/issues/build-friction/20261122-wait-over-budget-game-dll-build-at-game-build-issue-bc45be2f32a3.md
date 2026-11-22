# issue-bc45be2f32a3: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (not-wedged-policy-miscalibrated)

### 2026-11-22T04:24:54.472Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer (this review delivered by opencode retry wake-dfa7421507d6 at 04:15:49Z after the first codex launch went stale). Read-only: no source edit, no build/test requested, no fix or closure claimed.

VERDICT: not-wedged / policy miscalibrated. The 60s trip fired while the build was making forward progress inside the changed-TU preflight compile; ~96s of legitimate work still remained and the same attempt succeeded (generation 451). Same root cause as sibling issue-2b6479feedc1 / issue-60f01796d952 / issue-565c90a0d817. This review records this occurrence's exact evidence; no duplicate issue was created (exactly one build-friction issue exists for this wait_id).

VERIFIED WAIT OCCURRENCE (run 20261122_011031, logs/2026-11-22/20261122_011031/dev-loop/events.jsonl)
- dev-loop.wait.over_budget at 2026-11-22T04:11:51.893Z: wait_id game-dll-build:2026-11-22T04:10:48.461Z, kind game-dll-build, stage game_build, elapsed 61.256s, budget 60.0s, started_utc 04:10:48.461Z, reason revision 57aee374 sources=642 jobs=10 priority=high cache=build\game\objects\b10e3a31b3ae8637 seeded=0, request_ids [2026-11-22T04:10:48.383Z-73f531d2], created=true.
- dev-loop.wait.reviewer-wake at 04:11:51.892Z for the same wait_id (one wake). .dev/agent-wake/events.jsonl: wake-dfa7421507d6 wake.queued 04:11:50.232Z, claimed by codex 04:11:51.825Z, launched pid 20920 04:11:51.868Z; stale request recovered / wake.retry to opencode 04:15:46.257Z, claimed+launched (pid 24020) 04:15:49.1Z. One issue, one wake request; the second launch is a retry of the same wake, not a duplicate.
- No second issue row for this wait_id (checked all build-friction issues: only issue-bc45be2f32a3). occurrences stays empty, same attribution gap as the sibling reviews.

PINNED ATTEMPT EVIDENCE (.dev/build-timings/game-dll-20261122T041048501387Z-47e1f2.json and .jsonl)
- attempt game-dll-20261122T041048501387Z-47e1f2; started 04:10:48.506Z; completed 04:13:25.937Z; success true; no_op false; generation 451; code_hash 9d012f3f...; total_seconds 157.422; worker_total_seconds 120.791; request 2026-11-22T04:10:48.383Z-73f531d2; changed_paths [src/network/multiplayer-tick.cpp]; source_generation 0.
- Pre-worker stages (journal): resident_state_codegen 0.43s, packet_registry_codegen 1.59s, manifest_snapshot 19.01s, manifest_validation 0.01s, source_snapshot 9.90s. dev-loop.game-build.started at 04:11:23.647Z, i.e. the worker did not start until ~35.1s after attempt start.
- Phases: changed_source_preflight 55.517s, compile 23.630s (wall), link 37.387s, object_cache_scan 2.426s, publish 0.541s, toolchain_setup 0.142s. 13 TUs compiled, 630 skipped; compile_sum 182.77s across 13 parallel jobs, max single TU src/network/multiplayer-tick.cpp 55.402s.
- The 61.256s deadline (04:11:49.7Z) landed ~26s into the 55.40s multiplayer-tick.cpp preflight compile, which began at worker start 04:11:24Z and finished ~04:12:20Z (that TU's dev-loop.game-build.file compiled event is 04:12:20.752Z). Twelve module TUs then compiled in parallel (done ~04:12:40Z) and the link ran to 04:13:25Z. Every interval was forward progress; no hang, crash, or no-op.
- The wait's stage is labeled game_build, but at trip time the effective active stage was changed_source_preflight; the wait reason is the generic build line rather than the live phase.

ROOT CAUSE OF RECURRING FRICTION
- dev-loop.py WAIT_ISSUE_SECONDS = 60 (dev-loop.py:109-110) is a flat wall-clock budget applied to game-dll-build from attempt start (dev-loop.py:3520-3536). It therefore counts pre-worker codegen+manifest+source snapshot (~35s here) plus the changed-TU preflight compile (55.5s here, 92.5% of budget by itself). Any edit to a heavy TU (multiplayer-tick.cpp = 55.4s) structurally trips the budget even though the build is healthy.
- Per docs/specs/dev-loop/change-to-evidence-latency.md, budgets should be measured per class and a human approves budget changes; the flat 60s is below the observed whole-game DLL distribution, so recurrence is policy noise, not per-occurrence evidence of a fault.

ATTRIBUTION / EVIDENCE GAPS (first divergence in this occurrence)
1. Evidence build_id is the prior artifact 2026-11-22T02:37:06.972018Z, generation null, source_generation 0; the in-flight attempt game-dll-20261122T041048501387Z-47e1f2 and resulting generation 451 are not in the structured fields (only implied elsewhere). dev-loop.py:3532-3533 sets build_id to self.latest_build and generation to self.game_build_generation (assigned after success, dev-loop.py:2967).
2. occurrences remains empty: the terminal result (157.422s, generation 451, success) is never appended to this issue for the same wait_id.
3. The reviewer-wake event's launch_result is captured from agent-wake.py run --once stdout and may describe a different queued wake; and delivery is adapter-dependent (codex launch here went stale, opencode retry delivered this review ~4 min later).

RECOMMENDATION (human approval needed; no source edited or claimed)
- Start the game-dll-build wait clock at dev-loop.game-build.started (worker start) instead of attempt start, and gate issue creation on absence of forward progress (no new stage/file event within N seconds), and/or adopt a measured p95 class budget. Systemic owner: issue-136d4553ff92 (build-coordinator, account for every DLL build stage). This is the same recommendation as issue-2b6479feedc1; no new issue should be opened for this occurrence.
- Optional separate perf item: src/network/multiplayer-tick.cpp is a 55.4s single-TU compile and dominates the preflight; worth a distinct cold-change/compile-time tech-debt issue with profiles, not part of the wait-policy fix.

Confidence: high (pinned attempt JSON/JSONL, journal over_budget + reviewer-wake + wake events, phase durations, single-issue check). No source, build, or runtime behavior changed by this review.
