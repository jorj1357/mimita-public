# issue-6415397775a2: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Agent suggestion

### 2026-11-21T03:26:27.455Z — build-wait-reviewer

Bounded wake observation: the next attempt after this wait also failed, and the failure is a slow changed-source preflight, not codegen. Dev status at activation: lifecycle=game_build_failed, detail='changed-source preflight failed: src/network/multiplayer-projectiles.cpp in 1240.728s', attempt=game-dll-20261121T022901426487Z-d15759, latest published build=2026-11-21T02:26:50.306117Z:8b5838cbbf4c1bcda47c3f35e33583fde83bdacc89bdc52961e13980c976b871. Evidence suggests the 60s wait budget fires long before preflight finishes (~20.7 min here), so reviewer wakes are expected and not by themselves proof of a wedged builder. Suggested next evidence: record per-attempt preflight duration plus the source hash/paths that changed, and check whether concurrent agent edits to src/network/multiplayer-projectiles.cpp during preflight are the intended trigger. No edits or build requests made by this activation.

## Scheduled independent review

### 2026-11-24T16:29:24.521Z — root-resource-loop

A bounded independent review was queued as wake wake-3efa68064700 after 589 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (confirmed-recorded-wake-failed-noisy-budget)

### 2026-11-24T16:44:06.717Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review by issue-review-sweeper (wake-3efa68064700, trigger=sync, 2026-11-24T16:29Z). Read-only: no source edit, build, close, or fix claimed.

OBSERVED FACTS
1. Readiness/proof. issue-6415397775a2 is readiness.ready with no missing fields; before this review it was state=new with 0 reviews and 0 tasks. It has no dedicated task proof record (it is a build-friction tooling issue). Its own acceptance is: "The issue carries wait_id, stage, reason, elapsed, build/run IDs, and evidence; a reviewer wake is queued exactly once for this wait occurrence."
2. The recorded occurrence. wait_id `game-dll-build:2026-11-21T02:29:01.212Z`, kind=game-dll-build, stage=game_build, elapsed=60.44 s vs budget 60.0 s. Evidence carries wait_id, stage, reason, elapsed, build_id=2026-11-21T02:26:50.306117Z (previously published build), source_generation=3, and request_ids `auto-3` / `2026-11-21T02:13:33.127Z-90a9b3aa`. All named fields are present.
3. Underlying attempt independently confirmed. `.dev/build-timings/game-dll-20261121T022901426487Z-d15759.json`: generation 417, total_seconds 1914.474, worker_total_seconds 924.881, failed_phase=changed_source_preflight, worker_error="changed-source preflight failed: src/network/multiplayer-projectiles.cpp in 1240.728s". So this occurrence flagged a genuinely slow/failing build, not a healthy >60 s build.
4. Exactly-once queueing holds. `wake-fd61691ffd48` was queued once at 2026-11-21T02:30:08.058Z (~6 s after issue creation); `.dev/agent-wake/events.jsonl` has exactly one `wake.queued` for this issue/wait.
5. The reviewer wake did not deliver a review. Timeline from `.dev/agent-wake/events.jsonl` and `.dev/agent-wake/requests/wake-fd61691ffd48.json`: queued 02:30:08.058Z; codex claimed/launched 13:50:41.470Z/.493Z (queue-to-claim 40833.4 s ~= 11h20m33s); `wake.retry` to opencode 15:08:17.043Z (queue-to-retry 45489.0 s); opencode claimed/launched 15:20:50.356Z/.464Z (queue-to-final-launch 46242.4 s ~= 12h50m42s); `wake.failed` "adapter process exited" 15:53:26.109Z (queue-to-failed 48198.1 s ~= 13h23m18s), attempts=2, request fallbacks=[]. No issue_reviews row was written by it; the issue stayed state=new for 309562.9 s (~3d13h59m) until this scheduled sweep.
6. Traceability caveat. The existing "Agent suggestion" dated 2026-11-21T03:26:27.455Z by build-wait-reviewer predates this wake's first recorded claim at 13:50:41Z, so from the surviving artifacts that suggestion is not attributable to wake-fd61691ffd48.

INFERENCE / UNCERTAINTY
- The mechanical acceptance (all fields present; exactly one reviewer wake queued per wait occurrence) is satisfied. The behavioral intent (one reviewer actually appends a review) was not met for this occurrence.
- From these artifacts alone I cannot single out one cause of the failed delivery: codex primary exit, no persistent daemon draining the queued request, and the opencode fallback exit are all consistent with the recorded "adapter process exited" and the observed 11h20m queue-to-claim gap.
- Whether the 03:26 suggestion was produced by a different reviewer activation is unknown; artifact timestamps only rule out this wake.

CONTRARY EVIDENCE
- The wake was not duplicated for this wait_id; wait_id dedupe worked as designed for this occurrence.
- For this specific occurrence the alert was justified: the build really failed preflight after 1240.7 s, so it was not merely a healthy long build.
- However, the broader policy remains noisy: as of this review the store holds 160 issues titled "Wait over budget: game-dll-build at game_build" (145 in_review, 14 new, 1 assigned), and the same 60 s check still fired on 2026-11-24 (issue-3472692a57c2, 60.733 s, generation 593) while normal changed-source builds measured gen594 total 99.428 s / worker 86.147 s (`.dev/build-timings/game-dll-20261124T122208532874Z-63061a.json`) and gen593 total 181.029 s. This is the risk already named in the issue ("A noisy wait policy could create duplicate friction issues").

NEXT DISCRIMINATING CHECK
- Join the open "game-dll-build at game_build" wait issues to their pinned `.dev/build-timings` attempt and classify each as healthy-success vs failed/stalled; compute the fraction of successful attempts with total >60 s. If that fraction is high (current evidence suggests it is), the flat 60 s budget is measuring normal incremental work and this class cannot be called resolved without a stage/progress-aware budget. Separately, for the wake path, verify whether any subsequent over-budget reviewer wake emits a `wake.completed`/review (not only `wake.failed`) for its wait_id.

DEVELOPMENT-PROCESS REVIEW
- The evidence demonstrates a repeated delay/failed-delivery class, but it is already covered with measurable proposals: issue-84c584957e65 (outcome-checked delivery 2026-11-23T07:25; adapter preference/work-landed 2026-11-23T15:55; >=90% review within 30 min plus a `wake.completed` event 2026-11-24T14:01) and issue-9a4503f6ed68 (flat-budget / per-attempt duplication proposals 2026-11-22T23:07, 2026-11-23T13:28). To prevent duplicate reports I filed no new proposal; this occurrence's exact numbers are appended to issue-84c584957e65 as a distinct occurrence.
