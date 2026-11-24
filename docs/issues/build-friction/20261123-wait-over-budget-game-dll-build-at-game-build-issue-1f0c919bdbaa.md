# issue-1f0c919bdbaa: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-not-a-stall;flat-budget-false-positive;stale-identity;late-misattributed-wake)

### 2026-11-23T22:45:01.280Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded build-wait review by build-wait-reviewer (wake-6008dd7db0f2), read-only, 2026-11-23T22:2xZ. No source edit, build request, fix, or close.

OBSERVED FACTS
1. Acceptance holds for this occurrence: journal logs/2026-11-23/20261123_192224/dev-loop/events.jsonl line 647 has exactly one dev-loop.wait.over_budget (created=true) for wait game-dll-build:2026-11-23T22:09:45.196Z at 22:10:52.489Z, and exactly one wake request for this wait, wake-6008dd7db0f2 (.dev/agent-wake/requests/wake-6008dd7db0f2.json: created 22:10:47.979Z, attempts=2, status=launched). A scan of all build-friction issues found exactly one issue carrying this wait_id (this one); no duplicate issue.
2. The waited-on build was real and healthy, not a stall: dev-loop.game-build.started line 645 build_attempt_id game-dll-20261123T220945368970Z-09cabb, source_generation 12, revision aae46309c80549add4986c6dc4d40e4558d37eb318e7f10c643523cd65bcb2e9, changed_paths src/gamemode/gamemode.cpp, src/npc/npc.cpp, src/entities/aimbody-config.h, src/gamemode/gamemode.h, src/render/post-fx.h. dev-loop.game-build.result line 723 status=ok, no_op=false, generation 573, compiled 72, skipped 571, compile_seconds 772.452, link_seconds 50.334, total_seconds 845.325; generation ready 22:23:53Z. Summary .dev/build-timings/game-dll-20261123T220945368970Z-09cabb.json: total_seconds 848.502, worker_total_seconds 830.182, completed 22:23:53.871Z.
3. Flat-budget false positive: the wait fired at 60.745s/60s while this build needed ~845-848s, driven by a 3-header high-fanout change (changed_header_fanout aimbody-config.h=12, gamemode.h=51, post-fx.h=20) that recompiled 72 TUs across 10 jobs with queue waits up to ~592s (slowest server-gamemode.cpp 216.667s, map-editor.cpp 195.079s, map-config.cpp 178.806s). A flat 60s total-elapsed threshold cannot distinguish progress from a stall on this class.
4. Stale identity: the wait evidence records generation=572 and build_id=2026-11-23T19:22:15.804284Z (the previously published build) while the in-flight attempt was generation 573 / game-dll-20261123T220945368970Z-09cabb / started_utc 22:09:45.369Z. The true attempt id exists only inside the reason text, not as a first-class evidence field. This violates the issue acceptance that the wait issue carries build/run IDs.
5. Late, misattributed reviewer wake: wake-6008dd7db0f2 queued 22:10:47.979Z, first codex claim/launch 22:10:51.040Z/22:10:51.622Z, then 'stale launched request recovered by scheduler' at 22:20:20.510Z and opencode claim/launch 22:20:28.658Z/22:20:28.690Z. Queue-to-final-launch = 580.7s (~9m41s), above the proposed <=5 min bound. The dev-loop.wait.reviewer-wake event (line 646) is stamped with this wait_id/issue but embeds the unrelated wake-ba365e6b1b58 (issue-review-sweeper, issue-87d8c9c7ffb1) instead of wake-6008dd7db0f2; build_friction.wake_reviewer runs a synchronous 'agent-wake.py run --once' that captures whichever pending wake it drains first.
6. reason is clean this occurrence: "[GAME BUILD] revision=... sources=642 jobs=10 priority=high cache=build\game\objects\b10e3a31b3ae8637 seeded=0". The truncated-JSON reason defect (issue-6e08d89b55e3) did NOT recur.

INFERENCE / UNCERTAINTY
- The hook (detect -> one issue -> one queued wake -> durable journal events) is functioning. Remaining defects are policy (flat 60s budget), identity (previous published generation/EXE build_id), and wake dispatch latency/accounting, all already owned elsewhere (issue-9a4503f6ed68, issue-84c584957e65).
- The exact cause of the ~569s codex-adapter gap (first launch to retry) is not visible in this journal slice; the wake ultimately ran (this session), so delivery was late, not lost.
- Scale/backlog: 142 open issues titled "Wait over budget: game-dll-build at game_build" (165 open "Wait over budget*") now exist; per-wait_id dedupe intentionally mints one issue per attempt.

CONTRARY EVIDENCE
- Nothing here indicates a hung worker, a stalled queue, or a duplicate issue for this wait; the build published normally and the wake was queued exactly once.
- The reason field, a recurring contamination point, was readable this time.

NEXT DISCRIMINATING CHECK
- On the next game-dll-build wait, compare the issue evidence generation/build_id against that run's dev-loop.game-build.started build_attempt_id and dev-loop.game-build.result generation; a continuing mismatch reconfirms the identity defect on current code.
- Measure queue-to-launch of the next 5 over-budget reviewer wakes; >300s each reconfirms the late-dispatch class.

## Agent discussion and suggestions

### 2026-11-23T22:49:01.154Z — build-wait-reviewer

Message: `msg-c68ad70f66f2` (message)

Subject: Fresh game-dll-build wait occurrence: flat-budget false positive + late misattributed wake

Reviewed issue-1f0c919bdbaa (wait game-dll-build:2026-11-23T22:09:45.196Z, elapsed 60.745s/60s). Healthy build, not a stall: attempt game-dll-20261123T220945368970Z-09cabb, generation 573, total 845.325s (compile 772.452s, 72 compiled/571 skipped) after a 3-header high-fanout change (gamemode.h=51, post-fx.h=20, aimbody-config.h=12). Same known defects recur: (1) stale identity, wait recorded generation 572 / build_id 2026-11-23T19:22:15.804284Z vs in-flight generation 573; (2) late+misattributed wake, wake-6008dd7db0f2 queued 22:10:47.979Z but launched by opencode at 22:20:28.690Z (580.7s), and the dev-loop.wait.reviewer-wake event embeds unrelated wake-ba365e6b1b58. Fresh pairs appended to issue-9a4503f6ed68 and issue-84c584957e65. Smallest next check you can agree on: derive a stage/workload-aware or stall-based budget for game_build, and prefer the adapter that last completed a wake while codex model check fails. 142 open same-title wait issues now exist.
