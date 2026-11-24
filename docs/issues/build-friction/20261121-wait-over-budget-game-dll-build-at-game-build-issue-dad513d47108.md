# issue-dad513d47108: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Scheduled independent review

### 2026-11-24T13:48:31.665Z — root-mode3-latency

A bounded independent review was queued as wake wake-0b2ec75c5a9c after 561 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (confirmed-occurrence-acceptance-met; reviewer-wake-failed-no-review-until-sweep; recurrent-build-latency; wake-completion-event-missing; no-fix-claimed)

### 2026-11-24T14:00:42.418Z — issue-review-sweeper

Reviewer: issue-review-sweeper

﻿Independent review 2026-11-24 by issue-review-sweeper (wake-0b2ec75c5a9c). Read-only: no source edit, no build, no closure, no fix claimed.

OBSERVED FACTS
1. Occurrence record: issue created 2026-11-21T00:13:11Z for wait_id game-dll-build:2026-11-21T00:12:05.834Z. Evidence carries budget_seconds=60, elapsed_seconds=62.567, stage=game_build, build_id 2026-11-20T20:20:51.788061Z, generation 417, source_generation 44, request_ids [2026-11-20T23:43:45.960Z-88670501]. The acceptance data (wait_id/stage/reason/elapsed/build IDs/evidence) is complete.
2. Reviewer wake, queued exactly once: wake-a89ef41a94a2 queued 2026-11-21T00:13:26.963Z for build-wait-reviewer (.dev/agent-wake/events.jsonl), claimed 00:13:29.766Z, launched 00:13:30.172Z adapter=codex pid=29068. So "queued exactly once" is met.
3. Delivery failed: the same wake retried to opencode 13:50:41.105Z (~13.6h later), relaunched 15:08:13.782Z pid=10800, then wake.failed 15:53:26.041Z "adapter process exited". The adapter log .dev/agent-wake/logs/2026-11-21T00-13-26.930Z-build-wait-reviewer.log ends with {"type":"error","name":"UnknownError","data":{"message":"Failed to execute statement"}}. This issue's reviews stayed empty; not until the scheduled sweep 2026-11-24T13:48:31Z was a replacement review queued (about 3 days later).
4. Recurrence: 32 open "Wait over budget: game-dll-build at game_build" issues (3 new, 29 in_review) and 44 Wait-over-budget issues overall. Latest build evidence game-dll-20261124T122208532874Z-63061a (generation 594, published 12:23:47Z) total 99.4s: changed_source_preflight 60.6s, single TU src/network/server-projectiles.cpp 60.52s, compile phase 0.86s, link 22.4s; host mean CPU 99.85% (peak 100%), minimum available memory 312.8MB. Earlier attempts: gamepad-input.cpp 15.1s then 74.5s; link 4.6s then 51.7s.
5. Wake observability gap: .dev/agent-wake/events.jsonl spans 2026-11-20T03:38Z..2026-11-24T13:48Z with 504 distinct wakes and has no wake.completed event type. All 203 distinct build-wait-reviewer wakes show a terminal wake.failed; adapters launched were codex 190 / opencode 203.

INFERENCE / UNCERTAINTY
- The per-wait_id model (build_friction.record_wait_incident) intentionally creates one issue per new wait_id, so 32 sibling issues is policy behavior, not a dedup defect; it is this issue's own "noisy wait policy" risk, now realized.
- The dominant recurrence cause is build/resource latency (slow heavy single-TU preflight, ~100% CPU, ~313MB free), tracked by issue-136d4553ff92 (per-stage dll accounting), issue-7d3dcfd7433b (wait-stage latency), and issue-ed3ac775283d (CPU). This issue is a symptom record, not the build-latency owner.
- Because no wake.completed event exists, wake.failed "adapter process exited" alone cannot prove an adapter did no work; for this occurrence the direct log UnknownError, empty reviews, and 3-day re-queue are the delivery-failure evidence. The 203/203 failed tally must NOT be read as a 0% success rate.
- Intrinsic compile cost vs memory-pressure-induced slowness is unproven; 312.8MB free with 100% CPU is a plausible contributor, not a measured cause.

CONTRARY EVIDENCE
- The issue's own acceptance (complete record fields + exactly one wake) is satisfied; the wait is documented, not proof the mechanism is broken.
- Not every build exceeds 60s: generation 592 total 31.9s (gamepad-input.cpp 15.1s, link 4.6s). "Wait over budget" is intermittent, not constant.

NEXT DISCRIMINATING CHECK (read-only, no build, no edit)
Join .dev/agent-wake/events.jsonl wake.queued(issue, wall_time) for build-wait-reviewer to issue_reviews.created_utc for the same issue: count wakes whose issue gained a review within 60 min vs never, split by adapter. This resolves the success/failure mislabel and measures whether the reviewer-wake channel delivers, without starting a build. Separate owner follow-up (issue-136d4553ff92): matched per-TU compile wall plus peak memory for server-projectiles.cpp across >=3 attempts to separate intrinsic compile cost from memory pressure.
