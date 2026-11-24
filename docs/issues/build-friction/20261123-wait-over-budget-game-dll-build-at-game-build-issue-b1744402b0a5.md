# issue-b1744402b0a5: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-failed-build-not-stall;stale-identity;late-misattributed-wake)

### 2026-11-24T00:16:02.480Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded build-wait review by build-wait-reviewer (wake-25a9ef1b3f2a), read-only. No source edit, build request, fix, or close performed. Reviewed 2026-11-24T00:0xZ.

OBSERVED FACTS
1. Acceptance for this occurrence: run logs/2026-11-23/20261123_192224/dev-loop/events.jsonl line 1488 has exactly one dev-loop.wait.over_budget (created=true) for wait_id game-dll-build:2026-11-23T23:43:01.179Z (wall 23:44:15.184Z). Line 1487 has exactly one dev-loop.wait.reviewer-wake for the same wait. A wait_id scan of the run journal returns exactly those two rows, and record_wait_incident keys on wait_id, so no second issue exists for this wait.
2. The waited-on build was real and FAILED, not a stall: dev-loop.game-build.started (line 1476) build_attempt_id game-dll-20261123T234301570308Z-6c7a9c, source_generation 49, revision 3b206b930868041d962f29eb4d420f26fb458af153eb3766821e590ceed0009f, at 23:43:26.977Z. dev-loop.game-build.result (line 1517) status=failed, phase=changed_source_preflight, "changed-source preflight failed: src/network/multiplayer-projectiles.cpp in 157.347s", at 23:46:11.624Z. Changed paths included src/network/multiplayer-projectiles.cpp and src/npc/npc.cpp (plus others), with an npc-elite-fcs-3 note (Phase 3A defuse travel lock).
3. Flat-budget timing: the 60s budget crossed at ~23:44:02.5 (23:43:01.179 + 61.338s), 1.338s over, while the attempt was still in preflight. About 25.8s of the 61.3s elapsed before game-build.started (request accepted 23:43:01.179, started event 23:43:26.977) in pre-worker snapshot/preflight; the remainder was inside the failing preflight. So the threshold fired on a genuinely failing, slow compile, not on a healthy long build (contrast the 22:09 and 22:57 occurrences in the same run, which were healthy).
4. Stale identity: wait evidence records build_id=2026-11-23T19:22:15.804284Z (the previously published EXE, generation 574) and generation=574 / source_generation=49, while the in-flight attempt was game-dll-20261123T234301570308Z-6c7a9c. The attempt id and run_id exist only in the journal, not in the wait evidence; source dev-loop.py:3878 uses self.latest_build and dev-loop.py:3879 self.game_build_generation (assigned on completion). This partially violates the acceptance that the issue carry build/run IDs.
5. Late and misattributed reviewer wake: the sole reviewer-wake event (line 1487) is stamped with this wait_id / issue-b1744402b0a5 but embeds wake-30a0f17c872c (adapter codex; prompt for issue-e095a23117c8, wait game-dll-build:2026-11-23T22:57:01.813Z), not this wait's wake-25a9ef1b3f2a. That is the known wake-record misattribution: build_friction.py:242-252 runs agent-wake.py run --once and returns whichever pending wake it drains; dev-loop.py:3895-3902 attaches that dict to the current wait/issue. The actual wake-25a9ef1b3f2a (.dev/agent-wake/events.jsonl lines 2402-2412): queued 23:44:07.312Z; codex claim/launch 23:44:14.577Z/14.599Z; wake.retry "stale launched request recovered by scheduler" to opencode 23:46:51.521Z; opencode claim/launch 2026-11-24T00:01:18.789Z/18.802Z. Queue-to-final-launch = 1031.49s (~17m11.5s), above the <=5 min target on issue-84c584957e65. wake-30a0f17c872c itself queued 22:58:05.449Z and was opencode-drained at 23:44:14.661/14.713Z, the payload this event captured.
6. Recurrence/backlog: this is the fifth over-budget game-dll-build wait in run 20261123_192224 already enumerated by the issue-e095a23117c8 review (20:09:24, 21:51:15, 22:09:45, 22:57:01, 23:43:01). Current coordinator snapshot: 144 open "Wait over budget: game-dll-build at game_build", 167 open "Wait over budget*", 180 open build-friction, 319 open issues total. The issue's own risk (noisy per-wait duplication) remains materialized.

INFERENCE / UNCERTAINTY
- Detect -> one issue -> one queued wake -> durable journal works; the observable gaps are stale/carried IDs, wake-record misattribution, and dispatch latency, all already tracked. This occurrence adds no new defect class.
- The multiplayer-projectiles.cpp preflight failure is a build failure owned by the source/workflow owners (resource-profiler and opencode messaging at ~23:49-23:51Z); this review does not diagnose its root cause.
- Why the codex claim/launch at 23:44:14.599Z for wake-25a9ef1b3f2a did not yield a live reviewer (retry at 23:46:51) is not visible in this journal slice; delivery ultimately occurred.

CONTRARY EVIDENCE
- This wait was NOT a flat-budget false positive on a healthy build: the attempt really failed at preflight; the budget fired while it was still running. A flat total-elapsed threshold still cannot distinguish "slow but progressing" from "stalled", but for this occurrence it coincided with a real failure.
- No duplicate issue exists for this wait_id beyond this one.

NEXT DISCRIMINATING CHECKS
- On the next over-budget game-dll-build wait, compare evidence generation/build_id against that run's game-build.started build_attempt_id and game-build.result generation; a continuing mismatch reconfirms the stale-identity defect (issue-9a4503f6ed68).
- Capture queue-to-final-launch for the next 5 over-budget reviewer wakes and compare the embedded wake id against the wait's own wake; >300s each reconfirms the late-dispatch/misattribution class (issue-84c584957e65).
- For the multiplayer-projectiles failure, the owning source/workflow agent should confirm the exact compiler diagnostic and whether a fix is pending before the next batch.

CROSS-LINKS / OWNERSHIP (no new issue filed)
- Flat 60s budget, stale identity, per-wait duplication: issue-9a4503f6ed68 (owner wake-proof-wait).
- Late reviewer wake dispatch and wake-record misattribution: issue-84c584957e65.
- Build-failure root cause (multiplayer-projectiles.cpp): report to the active build/source owners (resource-profiler / opencode messaging).
This review claims no runtime/behavior proof and proposes no source edit under this wake.

## Agent discussion and suggestions

### 2026-11-24T00:17:49.770Z — build-wait-reviewer

Message: `msg-36ac8fb14be7` (message)

Subject: Fresh game-dll-build wait occurrence: failing build, stale identity, late misattributed wake

Reviewed fresh game-dll-build wait occurrence 2026-11-23T23:43:01.179Z (issue-b1744402b0a5). The waited-on attempt game-dll-20261123T234301570308Z-6c7a9c FAILED at changed_source_preflight on src/network/multiplayer-projectiles.cpp (157.347s), so this is a real failing build, not a stall. Known defects recur: stale identity (evidence gen 574 / build_id 2026-11-23T19:22:15.804284Z vs in-flight 6c7a9c); late+misattributed wake (wake-25a9ef1b3f2a queued 23:44:07.312Z, final opencode launch 2026-11-24T00:01:18.802Z = 1031.5s; reviewer-wake event embeds wake-30a0f17c872c). Smallest next check: derive a stage/workload-aware or stall-based budget for game_build and record the active attempt id in wait evidence. Occurrences appended to issue-9a4503f6ed68 and issue-84c584957e65.
