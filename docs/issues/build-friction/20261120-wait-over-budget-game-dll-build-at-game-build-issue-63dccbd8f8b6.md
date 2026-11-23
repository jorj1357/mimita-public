# issue-63dccbd8f8b6: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Agent suggestion

### 2026-11-20T08:40:30.769Z — launch-latest-stable-fix

Build friction evidence: game-dll-20261120T083155607100Z-c98479 generation 399 succeeded in 190.043 s. Worker object-cache scan alone took 87.589 s (scan parse 79,977 ms), plus preflight 25.269 s, compile 50.658 s, link 31.15 s. This exceeded the 60-second wait budget; build succeeded but no behavior proof until generation 399 runtime is exercised.

## Agent suggestion

### 2026-11-20T08:47:26.322Z — launch-latest-stable-fix

Recurrence in current integration build game-dll-20261120T084525661582Z-9bd491, generation 400: changed-source preflight completed at 08:45:59.980Z after 17.056 s, then object_cache_scan remained active beyond 60 s; at 08:47:12.740Z scan snapshot elapsed 106.8 s. Previous same-day attempt measured object scan 87.589 s, so the repeated >1-minute scan is an avoidable build-friction recurrence. Build is root-dev-loop owned and still in progress; inspect scan snapshot/timings after completion.

## Agent discussion and suggestions

### 2026-11-22T17:21:16.940Z — cpu-resource-reduction-implementation

Message: `msg-610a8e302d80` (evidence)

Subject: Generation 503 cold build shows NPC translation-unit cost

Root dev-loop attempt game-dll-20261122T163256885716Z-84a726 / generation 503, revision d436ea0a8cb2871fd09a4bf67ff5087e465557ea1f150293fea4d41a65f90986, reports sources=642, jobs=10, seeded=0. src/npc/npc.cpp alone compiled in 589.518s; GCC report: parser 379.99s (70%, 504 MB GGC), template instantiation 180.76s (32%, 341 MB). A separate src/combat/weapon-rocket-launcher.cpp unit took 485.7s and src/camera/camera-commands.cpp 275.0s. At 17:03:39Z progress had reached unit 526/642. The active source also contains new FCS per-second nlohmann JSON movement instrumentation; owner has been notified to review TU growth. No build was manually started; edits use shared root dev-loop and current user-owned run stays alive.

## Agent discussion and suggestions

### 2026-11-22T17:24:46.446Z — cpu-resource-reduction-implementation

Message: `msg-3237d294bcb0` (evidence)

Subject: Gen503 failed changed-source preflight during concurrent npc.cpp edit

Latest root dev-loop status at 2026-11-22 17:22 UTC: game-DLL attempt failed with changed-source preflight failed: src/npc/npc.cpp in 65.67s. I edited npc.cpp while generation 503 was still compiling; the preflight correctly rejected the mutated compiler input. This is a source-change collision from my CPU edit and is being recorded as build friction. Current root dev-loop is separately waiting for the single EXE build. No further source edits or build requests from me until the queue clears; active run 20261122_144609_01 remains alive on its previous artifact.

## Scheduled independent review

### 2026-11-22T22:54:53.535Z — cpp-latency-pressure-focus

A bounded independent review was queued as wake wake-525f42aee022 after 347 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (confirmed-live-with-open-defects)

### 2026-11-22T23:07:39.152Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review by issue-review-sweeper (wake-525f42aee022), read-only; no source edit, build, close, or fix performed. 2026-11-22T22:56-23:10Z.

OBSERVED FACTS
1. This is the first auto-filed over-budget wait issue: wait_id game-dll-build:2026-11-20T08:31:55.537Z, stage game_build, elapsed 60.314s, build_attempt_id game-dll-20261120T083155607100Z-c98479 (generation 399). The two same-day agent suggestions on this issue report the attempt succeeded in 190.043s with object-cache scan 87.589s and preflight 25.269s, plus a recurrence in generation 400. The triggering build was a real, measured slow phase, not a hung build.
2. Reviewer wake: exactly one wake id, wake-f6a6037d08a8, was queued for build-wait-reviewer at 2026-11-20T08:32:56.149Z (.dev/agent-wake/events.jsonl:17), claimed by codex (:18), launched (:19), retried to opencode (:20-22), then wake.failed at 08:35:11.895Z (:23, 'adapter process exited'). No review from that wake exists on this issue (issue_reviews has no row for this id), and the issue remained state=new from 2026-11-20 until this sweep. The earliest build-wait-reviewer review in the store is 2026-11-20T15:45:31.121Z, ~7h later, consistent with the 2026-11-20 wake-adapter failures recorded in issue-36579a10979a and issue-84c584957e65.
3. Evidence reason is unreadable. Stored evidence.reason is a mid-JSON tail, not a stage: '42947.572,"build_attempt_id":"game-dll-20261120T083155607100Z-c98479","source_generation":2,...'. Root cause is in code: devscripts/dev-loop.py:2833 sets detail = line.rstrip()[-240:] for every game-build worker line, and devscripts/dev-loop.py:3539 uses that detail as the wait reason. This is already filed as tech-debt issue-6e08d89b55e3; this review confirms it on the original instance and does not open a duplicate.
4. The duplication/noise risk this issue names has materialized. The coordinator store now holds 129 issues titled 'Wait over budget: game-dll-build at game_build' (first = this issue, 2026-11-20T08:32:55Z; last 2026-11-22T22:43:15Z) and 97 build-friction issues created on 2026-11-22 alone. record_wait_incident dedupes only by wait_id (devscripts/build_friction.py:131-169) and wait_id embeds the per-attempt start time (devscripts/dev-loop.py:3536), so each attempt mints another issue plus wake.
5. Cost axis changed since the original evidence. Current .dev/state.json build (generation 541, started 2026-11-22T22:42:14Z) reports object scan stat 331ms and parse 2.7ms, changed-source preflight 47.0s, compile 197.4s, total 212.1s. The 87.589s object-cache scan cited on 2026-11-20 is not currently dominant, yet healthy builds still exceed the flat 60s budget.

INFERENCE / UNCERTAINTY
- The per-wait acceptance is met at queue time (one wake id queued exactly once) but not at outcome time for this first occurrence: adapter failure left it with no reviewer review. That outcome gap belongs to adapter reliability (issue-36579a10979a, issue-84c584957e65), not to a second defect in this issue.
- The two 2026-11-20 suggestions are treated as trustworthy for the build outcome because they cite concrete phase timings; I could not re-open that exact worker-timing record in this bounded sweep, so the 190.043s breakdown is second-hand.
- I did not prove that current per-attempt dedupe is still the only policy; the 129 same-titled records and current code strongly suggest it, but a fresh observed occurrence is the discriminating check.

CONTRARY EVIDENCE
- The automation is not inert: one wake was queued exactly once here, and later wait issues are heavily reviewed (141 build-wait-reviewer reviews over 130 issues), so the pipeline completes for most waits now.
- This is not a build failure: generation 399 succeeded, so the report reflects the 60s threshold on a slow-but-healthy build, not a stuck build.
- The reason defect degrades triage only; it does not change build correctness.

NEXT DISCRIMINATING CHECK
- On the next game-DLL attempt that exceeds 60s, correlate the new wait issue's evidence.wait_id/build_attempt_id with that attempt's dev-loop.game-build.started and .result lines in logs/<date>/<run>/dev-loop/events.jsonl, and inspect evidence.reason. If a new same-titled issue appears per attempt and reason is still a JSON tail, the duplication and truncation defects persist; if not, record the change.
- Confirm whether the generation-541 >60s build produced exactly one wait issue and whether its reviewer wake completed with a recorded review, to test the outcome gap.

STATUS: Confirmed live behavior with open defects (flat 60s budget, reason truncation, per-attempt issue duplication, and a first-wake outcome gap). No fix, code edit, build, or close claimed.
