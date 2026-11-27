# issue-796a7430913c: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-wait-policy-noise-and-dispatch-defect)

### 2026-11-27T20:34:23.325Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review by build-wait-reviewer (wake-dda0fc4bb16b) at 2026-11-27T20:3xZ. Read-only: no source edit, no build/test/run started, no fix or closure claimed.

ACCEPTANCE / PROOF-EXPECTATION CHECK (observability-friction issue, not a behavior claim)
- Issue carries wait_id game-dll-build:2026-11-27T18:21:05.061Z, stage game_build, kind game-dll-build, budget_seconds 60.0, elapsed 60.648s, source_generation 73, build_id 2026-11-27T17:45:19.700118Z, request_ids [build-request-eda27fe665de], and a readable reason. Acceptance data present.
- Reviewer wake was queued exactly once: wake.queued wake-dda0fc4bb16b at 2026-11-27T18:22:12.063Z.
- BUT the dev-loop journal has NO dev-loop.wait.over_budget and NO dev-loop.wait.reviewer-wake event for this wait_id. The only related event is dev-loop.wait.issue-failed at 18:22:42.294Z: error "Command [.../agent-wake.py, run, --once] timed out after 30 seconds", reason "could not record over-budget wait". The issue record and the queued wake survive, but the issue's stated proof expectation ("Dev-loop wait event plus the issue record and reviewer wake record") is only partially met: the over_budget/reviewer-wake events were lost.

WAIT OCCURRENCE (verified, pinned)
- Attempt game-dll-20261127T182105404575Z-a60ac3, started 18:21:05.404Z, generation 802 (issue's generation 801 is the prior published generation, not the attempt), revision 4344082cb17d..., total 318.544s, success=true.
- 18:21:05-18:21:33 pre-compile: resident_state codegen cache hit; packet_registry codegen 2.8s; game-build.started 18:21:33.
- First TUs submitted only at 18:22:21 (started_elapsed_ms ~74780), so at the 60.648s wait mark the main compile batch had NOT started. The 8 recompiled TUs then took compile 270.307s: server-gamemode.cpp 231.536s, server-npcs.cpp 173.941s, map-authored-geometry.cpp 150.051s, npc-navigator.cpp 121.262s, recast-navigation.cpp 120.161s + authored-block-rebuild/server-authored-rebuild (~116s each) + identity-provider 38.7s.
- Cache invalidations name recast-navigation.h and authored-block-rebuild.h ("dependency newer than object") from the map-authored-parity navmesh change, i.e. header-triggered recompilation, not a stall.
- Resource postmortem: mean host CPU 93.76%, peak 100.0%, minimum free memory 370.8 MB. Under this saturation a flat 60s wall-clock check cannot distinguish slow pre-compile/compile from a hang.
- why_missing=true for this retry; the map-authored-parity build notes were consumed by the preceding failed attempt game-dll-20261127T181622271460Z-618464 (codegen check failed), so this retry has no recorded purpose.

FIRST DIVERGENCE (no code fix claimed)
1) Wait policy (recurrence, not new): devscripts/dev-loop.py fires on wall-clock age alone with one global 60s budget and no stage-duration/progress check; build_friction.py keys uniqueness to wait_id, which embeds per-attempt start time, so each >60s attempt mints a new issue. Canonical: issue-9a4503f6ed68 (feature) and issue-81d40f5f20b3 (prior noise review). This occurrence is an instance, not a duplicate to file.
2) Reviewer dispatch + journaling (open defect, independently seen here): devscripts/build_friction.py:244-246 wraps agent-wake.py run --once in subprocess.run(timeout=30) with no TimeoutExpired guard. It fired here at 18:22:42 and is 1 of 4 identical 30s timeouts in run 20261127_174537. Mechanism and smallest safe fix are already documented in review 546 on issue-fdbac689adea; cross-linked, not re-filed.
3) Wake latency for this exact occurrence: queued 18:22:12.063Z, codex claimed/launched 18:42:10.202/18:42:10.246Z (drained by the later 18:41 wait's run --once, not its own), wake.retry to opencode 18:47:45.506Z, opencode claimed/launched 20:29:58.074/20:29:58.092Z pid 9344 (this reviewer). Queue-to-final-launch = 7666s (~2h07m46s), two wake.launched events for one wait. Pattern owned by late reviewer dispatch issue-84c584957e65.

ASSESSMENT / NEXT (report only)
- Not a hang. The build did real, compile-dominated work and succeeded; the wait is wait-policy noise plus a dispatch/journaling defect, matching issue-fdbac689adea review 546.
- Smallest safe owner fix (report only): catch subprocess.TimeoutExpired in build_friction.py and still write dev-loop.wait.reviewer-wake/over_budget (or launch the wake asynchronously), so a slow adapter cannot lose durable wait events or mislabel the failure. Do NOT change the 60s policy here; that belongs to issue-9a4503f6ed68 / issue-136d4553ff92.
- Build-cost cause of this occurrence is the recast-navigation.h / authored-block-rebuild.h header fanout plus 231s server-gamemode.cpp TU under 100% CPU; tracked by issue-136d4553ff92 (build latency/header fanout). Missing build note on the retry is a build-note carry-over gap worth its own follow-up.
- Confidence: high (pinned dev-loop journal lines, attempt JSON/postmortem, wake request + wake events). No files edited; no build/test requested.
