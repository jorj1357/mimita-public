# issue-e095a23117c8: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-not-a-stall;flat-budget-false-positive;stale-identity;late-wake-2769s;per-attempt-backlog)

### 2026-11-24T00:00:24.637Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded build-wait review by build-wait-reviewer (wake-30a0f17c872c), read-only. No source edit, build request, fix, or close performed. Reviewed 2026-11-23T23:44Z.

OBSERVED FACTS
1. Acceptance holds for this occurrence. Journal logs/2026-11-23/20261123_192224/dev-loop/events.jsonl has exactly one dev-loop.wait.over_budget (created=true) for wait_id game-dll-build:2026-11-23T22:57:01.813Z at wall_time 2026-11-23T22:58:08.210Z, and exactly one dev-loop.wait.reviewer-wake for the same wait. Wake .dev/agent-wake/requests/wake-30a0f17c872c.json: created 22:58:05.405Z, adapter opencode, attempts=2, status launched, launched 23:44:14.708Z. A scan of build-friction issues found exactly one issue carrying this wait_id (this one); no duplicate issue for this wait.
2. The waited-on build was real and healthy, not a stall. dev-loop.game-build.started: build_attempt_id game-dll-20261123T225702279702Z-0ed089, source_generation 35, revision 421df633d9b5bfad37947b035806e7a2aade16a2a2059f26443a626e0634e40e. dev-loop.game-build.result: status ok, no_op false, generation 574, compiled 158, skipped 485, compile_seconds 1208.949, link_seconds 28.862, total_seconds 1285.484, worker_total_seconds 1243.93, published 2026-11-23T23:18:28.575Z. Summary .dev/build-timings/game-dll-20261123T225702279702Z-0ed089.json status ok, total_seconds 1285.484, build_cause mixed.
3. Flat-budget false positive. The wait fired at 61.168s/60.0s (1.168s over) while the build needed ~1285s, driven by 3 high-fanout headers: changed_header_fanout camera.h=150, ragdoll-mode.h=18, post-fx.h=20 (slowest TUs ragdoll-mode.cpp 156.301s, melee-client.cpp 153.665s, npc.cpp 142.957s). The budget counts request-to-completion elapsed, not compile progress: request started_utc 22:57:01.813, worker_started_utc 22:57:44.440 (~42.6s of snapshot/preflight before any compile), so ~42.6s of the 60s budget was spent before compilation began. A flat total-elapsed threshold cannot distinguish progress from a stall on this class.
4. Stale identity. The wait evidence records generation=573 and build_id=2026-11-23T19:22:15.804284Z (the previously published EXE) while the in-flight attempt is generation 574 / game-dll-20261123T225702279702Z-0ed089 / started 22:57:01.813Z. The true attempt id exists only inside the reason text, not as a first-class evidence field. devscripts/dev-loop.py:3866-3882 reads self.game_build_generation, which is assigned only on completion at devscripts/dev-loop.py:3259. This violates the acceptance that the wait issue carries build/run IDs.
5. Late reviewer wake. Queue-to-launch = 23:44:14.708 - 22:58:05.405 = 2769.3s (~46m9s), attempts=2: the first adapter codex launch at ~22:58 was retried to opencode and only drained at 23:44 when a later over-budget wait's synchronous agent-wake.py run --once ran. This is the late-dispatch class in issue-84c584957e65, now worse than the ~9m41s seen at 22:09. Note: unlike the 22:09 occurrence, the dev-loop.wait.reviewer-wake payload embeds the correct wake-30a0f17c872c (not a foreign wake), so the misattribution defect did NOT recur here.
6. Recurrence/backlog. This is one of five over-budget game-dll-build waits in run 20261123_192224: 20:09:24 (61.659s), 21:51:15 (60.37s), 22:09:45 (60.745s), 22:57:01 (61.168s), 23:43:01 (61.338s); every one is barely over 60s and each minted a separate issue+wake. Current coordinator store: 144 open "Wait over budget: game-dll-build at game_build", 167 open "Wait over budget*", 180 open build-friction total. The issue's own risk ("A noisy wait policy could create duplicate friction issues") has materialized.

INFERENCE / UNCERTAINTY
- The detect -> one issue -> one queued wake -> durable journal path works. The remaining defects are policy (flat 60s total-elapsed budget), identity (previous published generation/EXE build_id), dispatch latency, and per-attempt duplication, all already tracked elsewhere (see cross-links). This occurrence adds no new defect class.
- The exact reason the codex adapter did not launch at 22:58 is not visible in this journal slice; the request was ultimately drained at 23:44, so delivery was late, not lost.
- The measured 144-issue backlog is a snapshot at review time; it may change as issues close.

CONTRARY EVIDENCE
- The build progressed and published normally; nothing indicates a hung worker or stalled queue.
- The misattributed wake payload did not recur for this occurrence (correct wake-30a0f17c872c embedded).
- No duplicate issue exists for this wait_id.

NEXT DISCRIMINATING CHECKS
- On the next over-budget game-dll-build wait, compare evidence generation/build_id against that run's dev-loop.game-build.started build_attempt_id and dev-loop.game-build.result generation; a continuing mismatch reconfirms the identity defect on current code.
- Measure queue-to-launch for the next 5 over-budget reviewer wakes; >300s each reconfirms the late-dispatch class.
- Confirm whether a closed/resolved canonical issue is absorbing repeats, or whether per-wait_id issues continue to accumulate, to quantify the noisy-policy cost.

CROSS-LINKS / OWNERSHIP (no new issue filed)
- Flat 60s budget, stale identity, per-attempt duplication: issue-9a4503f6ed68 (owner wake-proof-wait).
- Late reviewer wake dispatch: issue-84c584957e65.
- Wait-stage latency reduction: issue-7d3dcfd7433b (launch-latency-investigation); "Waiting is the enemy": issue-e044a931c54b (build-runtime-subagents).
This review does not claim runtime/behavior proof, does not close the issue, and proposes no source edit under this wake; any budget/identity/dispatch change should be a separately claimed task in its owning issue with before/after timing evidence.
