# issue-84c584957e65: Over-budget reviewer wakes dispatch late: codex primary adapter fails model check and no daemon drains the fallback

## Human report (verbatim, immutable)

> An over-budget wait review reached the reviewer ~39 minutes after the wait. Sequence (`.dev/agent-wake/events.jsonl`, wake-26c4ad5a6f09): wake.queued 2026-11-22T12:13:32.440Z; primary adapter `codex` launched 12:13:34.154Z; codex exited ~12:14:52Z with model_not_found ("The 'gpt-6-luna' model is not supported when using Codex with a ChatGPT account"); wake.retry 12:34:35.056Z switched the request to fallback `opencode`; opencode only claimed/launched at 12:52:39.040Z because no persistent agent-wake daemon was running and the retried request sat queued until the next over-budget wait's `run --once` drained it. See review of issue-f1c6106a580d.


## Occurrence

### 2026-11-22T14:48:12.771Z — build-wait-reviewer

Over-budget wait game-dll-build:2026-11-22T13:57:36.293Z (issue-40ebd14d5cf4) reproduced the late-dispatch behavior: one wake.queued for wake-a37dc2879375 at 13:58:39.668Z; codex primary claimed/launched 13:58:41.859Z then exited; wake.retry switched to opencode 14:22:49.103Z; opencode claimed/launched 14:24:02.589Z/14:24:03.211Z (about 25.4 minutes after the wait). Build attempt game-dll-20261122T135736380483Z-d10b1b was still compiling/linking at the 60.9s mark and finished successfully at 118.0s, so this is not a stalled build.

Evidence: {"build_attempt_id": "game-dll-20261122T135736380483Z-d10b1b", "build_total_seconds": 118.006, "fallback_adapter": "opencode", "fallback_launched_utc": "2026-11-22T14:24:03.211Z", "issue": "issue-40ebd14d5cf4", "journal": ".dev/agent-wake/events.jsonl", "primary_adapter": "codex", "primary_launched_utc": "2026-11-22T13:58:41.859Z", "total_dispatch_delay_seconds": 1523.543, "wait_id": "game-dll-build:2026-11-22T13:57:36.293Z", "wake": "wake-a37dc2879375", "wake_queued_utc": "2026-11-22T13:58:39.668Z", "wake_retry_utc": "2026-11-22T14:22:49.103Z"}

## Occurrence

### 2026-11-22T17:53:17.930Z — build-wait-reviewer

Over-budget wait game-dll-build:2026-11-22T17:17:22.162Z (issue-34993e327e19) reproduces late reviewer dispatch: wake-0eacfd7f99aa queued 17:18:27.783Z; codex primary claimed/launched 17:18:36.732Z/17:18:37.191Z then exited; wake.retry switched to opencode 17:23:09.539Z; opencode claimed/launched 17:24:14.245Z/17:24:14.446Z, about 5m46.7s (346.7s) after the wait, above the proposed <=5 min bound. Build attempt game-dll-20261122T171723932165Z-86a612 was actively compiling at the 60.2s mark and failed at 275.6s on src/npc/npc.cpp hasLoggedDash, so this is not a stalled build. The dev-loop.wait.reviewer-wake event at 17:18:39.195Z was stamped with this wait and issue but embedded the stale wake-53e8678c15f2 (issue-cfe902b4b659), not wake-0eacfd7f99aa.

Evidence: {"build_attempt_id": "game-dll-20261122T171723932165Z-86a612", "build_outcome": "failed", "build_total_seconds": 275.557, "failed_phase": "changed_source_preflight", "fallback_adapter": "opencode", "fallback_launched_utc": "2026-11-22T17:24:14.446Z", "issue": "issue-34993e327e19", "journal": ".dev/agent-wake/events.jsonl", "misattribution": "dev-loop.wait.reviewer-wake at 2026-11-22T17:18:39.195Z is stamped with wait_id game-dll-build:2026-11-22T17:17:22.162Z / issue-34993e327e19 but embeds wake-53e8678c15f2 (issue-cfe902b4b659).", "primary_adapter": "codex", "primary_launched_utc": "2026-11-22T17:18:37.191Z", "total_dispatch_delay_seconds": 346.663, "wait_id": "game-dll-build:2026-11-22T17:17:22.162Z", "wake": "wake-0eacfd7f99aa", "wake_queued_utc": "2026-11-22T17:18:27.783Z", "wake_retry_utc": "2026-11-22T17:23:09.539Z"}

## Occurrence

### 2026-11-22T19:06:50.314Z — build-wait-reviewer

Over-budget wait exe-build:4:2026-11-22T17:23:49.758Z (issue-6990b0ff69ec) reproduced late reviewer dispatch: wake-d53716cfb2d1 queued 17:24:55.483Z; codex primary claimed/launched 17:24:59.887Z/17:25:00.143Z then failed; wake.retry switched to opencode 18:02:46.039Z; opencode claimed/launched 18:02:51.130Z/18:02:51.176Z. Total dispatch delay ~37m55.7s after the wait, far beyond the proposed <=5 min bound. The underlying EXE attempt was real (started 17:23:49.758Z, failed 17:32:12.249Z at 494.796s), so this is not a stalled build.

Evidence: {"build_outcome": "failed", "build_total_seconds": 494.796, "fallback_adapter": "opencode", "fallback_launched_utc": "2026-11-22T18:02:51.176Z", "issue": "issue-6990b0ff69ec", "journal": ".dev/agent-wake/events.jsonl", "primary_adapter": "codex", "primary_launched_utc": "2026-11-22T17:25:00.143Z", "total_dispatch_delay_seconds": 2275.693, "wait_id": "exe-build:4:2026-11-22T17:23:49.758Z", "wake": "wake-d53716cfb2d1", "wake_queued_utc": "2026-11-22T17:24:55.483Z", "wake_retry_utc": "2026-11-22T18:02:46.039Z"}

## Occurrence

### 2026-11-22T19:24:17.006Z — build-wait-reviewer

Over-budget reviewer wake dispatched ~29m20s late for wait game-dll-build:2026-11-22T18:05:34.493Z (issue-d668de6bfc5e). wake-fe718d06a423 queued 18:06:35.961Z; codex claimed/launched 18:06:37.632Z/18:06:37.819Z then exited; wake.retry to opencode 18:25:25.086Z; opencode claimed/launched 18:35:56.541Z/18:35:56.591Z. Same codex-adapter-failure class as this issue.

Evidence: {"delay_seconds": 1760.63, "issue": "issue-d668de6bfc5e", "opencode_launched_utc": "2026-11-22T18:35:56.591Z", "queued_utc": "2026-11-22T18:06:35.961Z", "wake": "wake-fe718d06a423"}

## Agent suggestion

### 2026-11-23T06:54:42.067Z — issue-review-sweeper

Process-improvement proposal (from review of issue-e84cfa28c7f7): assert wake delivery, not only wake queueing. Evidence: wake-22029ea56410 was queued exactly once but delivered a bounded-test prompt, ran a wake test, and exited (status=failed) without appending a review; the hardcoded test prefix was removed later by d89884f6. Measurable target: over the next 5 over-budget reviewer wakes (or 1 controlled manual wake), 5/5 launches have a log/prompt artifact containing the exact "Requested work: Review build/test wait issue <id>" text, each target issue gains a review or occurrence within 15 minutes, and no wait_id has more than one launch. Smallest experiment: queue one manual wake for a scratch issue with the opencode adapter, then assert those three facts from .dev/agent-wake/events.jsonl, the request prompt.txt, the spawned log, and the issue record; repeat for codex. If the requested-work line is missing, fail the wake instead of marking it launched. This extends the existing acceptance (one wake.queued/launched) with delivered-content and review-append checks.

## Agent suggestion

### 2026-11-23T07:25:00.479Z — issue-review-sweeper

Process-improvement proposal from issue-review-sweeper (wake-bd9412956e26), grounded in issue-6bc05eadfb7f and the sibling finding on issue-e84cfa28c7f7.

EVIDENCE
- 2026-11-20T14:59:20.617Z wake-363c44b7c8d6 was queued exactly once for over-budget game-dll wait issue-6bc05eadfb7f (elapsed 60.364s; the pinned build game-dll-20261120T145818136704Z-502a3e really ran 1716.948s and succeeded). codex claimed/launched 14:59:21.4/21.5Z, wake.retry to opencode 15:27:57.8Z, opencode launched 15:41:42.7Z, then wake.failed "adapter process exited" at 16:03:49.9Z (attempts=2). No issue_reviews row was written; the issue stayed state=new for 64.3h until the automated 2026-11-23T07:17Z sweep.
- The delivered prompt.txt contained the correct "Requested work: Review build/test wait issue issue-6bc05eadfb7f", but the transcript contained 0 mentions of the issue and ended with "Bounded test activation acknowledged ... No code"; codex logged model-manager decode errors. Same failure appeared on issue-e84cfa28c7f7 (wake-22029ea56410, wake.failed 16:03:49.677Z).
- Scale: 130 distinct "Wait over budget: game-dll-build" issues exist (16 new, 113 in_review, 1 assigned) created 2026-11-20T08:32Z..2026-11-22T23:53Z; 182/333 recorded game-dll builds exceeded 60s (median 76.3s), so reviewer-wake reliability is load-bearing, not a corner case.

PROPOSAL (measurable)
Make reviewer-wake delivery outcome-checked rather than fire-and-forget: after launch, require within a bounded window (proposed 15 min) either an issue_reviews row for the issue or one wake.escalated event plus one issue occurrence; never leave a reviewer wake at status=failed with no surfaced escalation. Also separate the bounded-test prompt template from the production reviewer template so an automatic reviewer wake can never be constructed with "This is a bounded test activation" / --task "wake test".
Target: move from the observed baseline (0 reviews for 2 of 2 sampled over-budget waits, 64.3h silent for issue-6bc05eadfb7f) to >=90% of over-budget waits yielding an issue_reviews row or wake.escalated within 15 min, with exactly one wake.launched per wait_id and 0 duplicate launches.

SMALLEST EXPERIMENT
Queue one manual reviewer wake against a scratch issue using current agent-wake.py and assert: (a) delivered prompt contains the exact "Requested work" text, (b) exactly one wake.launched, (c) an issue_reviews row appears within 15 min OR a wake.escalated event is written. Repeat 3 times for a baseline rate before changing code. No source edit is proposed here; the owner decides.

## Agent suggestion

### 2026-11-23T07:27:11.179Z — issue-review-sweeper

Correction from issue-review-sweeper (folding the 07:25 proposal into the existing proposal above; do not implement it twice).

- The earlier proposal already covers the delivered-content assertion, the 15-minute review-or-escalation bound, and the one-launch check. Treat mine as superseded by it.
- Net-new evidence only: the same failure predates this issue's recorded occurrences. wake-363c44b7c8d6 for issue-6bc05eadfb7f (2026-11-20T14:59:20Z) was queued once, retried codex->opencode, and failed "adapter process exited" at 16:03:49Z without appending a review; its prompt.txt began "This is a bounded test activation" and its transcript ended at "Bounded test activation acknowledged ... No code". So the baseline is at least 3 distinct waits (issue-e84cfa28c7f7, issue-6bc05eadfb7f, plus this issue's 4 later occurrences), not a single event.
- Correction to my 07:25 text: the bounded-test prefix was already removed by d89884f6 (2026-11-21), so prompt assembly is no longer the first suspect for new wakes; the open question is adapter-exit handling and the missing escalation/review-append check. `_prompt` still instructs `begin --task "wake test"` (devscripts/agent-wake.py:288), which is cosmetic but should be reworded so a production reviewer does not think it is a test.
- Revised smallest experiment stays the same but focuses on (b): one manual reviewer wake to a scratch issue, assert exactly one wake.launched and either an issue_reviews row or a wake.escalated within 15 min; repeat 3 times. No source edit proposed by the reviewer.

## Agent suggestion

### 2026-11-23T09:50:40.358Z — issue-review-sweeper

Supporting evidence for the existing 2026-11-23T07:25 outcome-checked-delivery proposal on this issue (not a new proposal; no duplicate suggested).

An earlier over-budget wait reproduces the same failure class. issue-dff7e8c0f758 (wait exe-build:19:2026-11-20T16:02:46.964Z, stage setup, 60.833s vs 60.0s). Exactly one wake, wake-ca89658c9f56, was queued (created 2026-11-20T16:03:49.179Z, attempts=2, fallbacks=[], status=failed, error='adapter process exited', updated 16:39:30.762Z). Its log (.dev/agent-wake/logs/2026-11-20T16-03-49.179Z-build-wait-reviewer.log) ends ~16:39:15Z and its only issue-scoped reply targets issue-61b5edc937d9; the dff7 review was appended later at 16:48:57.903Z and is not attributable to the wake from any surviving artifact. The dev-loop.wait.over_budget event is also unreadable because logs/2026-11-20 was pruned (owned by issue-79e68f63fb01).

Why it matters to the existing proposal: this is an additional distinct wait (earlier than the 14:59Z wait-363c44b7c8d6 already cited) where the wake was queued exactly once but produced 0 attributable reviews, even though the adapter session did run. It strengthens the proposed 15-minute review-or-wake.escalated bound and the need for a per-wake review-correlation field.

Measurable target (unchanged from existing proposal): 5/5 consecutive over-budget reviewer wakes have exactly one wake.launched per wait_id and either an issue_reviews row or a wake.escalated event within 15 minutes, with a recorded wake-to-review link. Baseline now includes this dff7 occurrence.

Smallest experiment (unchanged): queue one manual reviewer wake to a scratch issue, then assert from .dev/agent-wake/events.jsonl, the request prompt.txt, the spawned log, and the issue record that (a) exactly one wake.launched occurred, (b) the target gains an issue_reviews row or wake.escalated within 15 minutes, and (c) the review carries the wake id. Repeat 3 times for a baseline before changing code.

## Agent suggestion

### 2026-11-23T15:55:52.383Z — issue-review-sweeper

Incremental proposal from the issue-3dad54c7224d review (issue-review-sweeper, wake-2cd479fc071e, 2026-11-23). Evidence: the single queued wake wake-8627cef525a5 used codex first; codex failed `model_not_found` (gpt-6-luna unsupported), retried to opencode ~3m24s later, opencode appended review id 16 at 2026-11-20T17:52:15Z (~49m53s after wake.queued), and the request was then marked wake.failed "adapter process exited" at 18:28:29Z despite that review existing. The same codex-primary choice is live again on wake-6f2f936370f3 (queued 2026-11-23T15:25:57Z), whose codex log already shows the same model_not_found.

Measurable target: over the next 10 over-budget wakes, (a) queued-to-review latency <=5 min for all 10, and (b) 0 requests with status=failed that have an issue_reviews row created after launched_utc.

Smallest experiment: (1) in build_friction.wake_reviewer, prefer the adapter whose most recent wake appended work (or simply prefer opencode while the codex model check fails), and (2) in agent-wake.run_once/reconcile_launched, capture the adapter process exit code and treat "work landed" (issue_reviews/messages by MIMITA_AGENT_ID after launched_utc) plus exit code 0 as completed instead of failed. Run 24h and count wake.failed-after-review and queue-to-review latency. Preserves this issue's acceptance ("one wake.queued ... no duplicate launches") and issue-9a4503f6ed68's per-occurrence idempotency.

## Occurrence

### 2026-11-23T16:23:59.983Z — build-wait-reviewer

Over-budget wait game-dll-build:2026-11-23T15:24:55.624Z (issue-266c2a856cfc) reproduces late reviewer dispatch: wake-6f2f936370f3 queued 15:25:57.335Z; codex primary claimed/launched 15:25:57.912Z/15:25:58.000Z then exited; wake.retry switched to opencode 15:58:40.915Z ("previous adapter process exited"); opencode claimed/launched 16:01:38.666Z/16:01:38.737Z. Total dispatch delay 2141.402s (~35m41s), far beyond the proposed <=5 min bound. The underlying build game-dll-20261123T152455729876Z-62f5f6 was real and succeeded at 107.935s, so this is not a stalled build.

Evidence: {"build_attempt_id": "game-dll-20261123T152455729876Z-62f5f6", "build_outcome": "success", "build_total_seconds": 107.935, "fallback_adapter": "opencode", "fallback_claimed_utc": "2026-11-23T16:01:38.666Z", "fallback_launched_utc": "2026-11-23T16:01:38.737Z", "issue": "issue-266c2a856cfc", "journal": ".dev/agent-wake/events.jsonl", "primary_adapter": "codex", "primary_claimed_utc": "2026-11-23T15:25:57.912Z", "primary_launched_utc": "2026-11-23T15:25:58.000Z", "retry_reason": "previous adapter process exited; trying fallback", "total_dispatch_delay_seconds": 2141.402, "wait_id": "game-dll-build:2026-11-23T15:24:55.624Z", "wake": "wake-6f2f936370f3", "wake_queued_utc": "2026-11-23T15:25:57.335Z", "wake_retry_utc": "2026-11-23T15:58:40.915Z"}

## Occurrence

### 2026-11-23T16:39:39.134Z — build-wait-reviewer

Over-budget wait game-dll-build:2026-11-23T16:00:36.698Z (issue-ec790f30f5ad) reproduces late reviewer dispatch: wake-bc86ede93b34 queued 2026-11-23T16:01:37.511Z; codex primary claimed/launched 16:01:38.798/16:01:38.826Z then exited; wake.retry to opencode 16:18:27.553Z (stale launched request recovered by scheduler); opencode claimed/launched 16:18:29.845/16:18:29.871Z. Total dispatch delay 1073.173s (~17m53s) after the wait and 1012.360s after queue, far beyond the proposed <=5 min bound. The underlying build game-dll-20261123T160037032706Z-485a05 was real and succeeded at 69.123s, so this is not a stalled build.

Evidence: {"build_attempt_id": "game-dll-20261123T160037032706Z-485a05", "build_outcome": "success", "build_total_seconds": 69.123, "fallback_adapter": "opencode", "fallback_launched_utc": "2026-11-23T16:18:29.871Z", "issue": "issue-ec790f30f5ad", "primary_adapter": "codex", "primary_launched_utc": "2026-11-23T16:01:38.826Z", "total_dispatch_delay_seconds": 1073.173, "wait_id": "game-dll-build:2026-11-23T16:00:36.698Z", "wake": "wake-bc86ede93b34", "wake_queued_utc": "2026-11-23T16:01:37.511Z", "wake_retry_utc": "2026-11-23T16:18:27.553Z"}

## Occurrence

### 2026-11-23T17:33:28.122Z — build-wait-reviewer

Over-budget wait game-dll-build:2026-11-23T16:54:31.578Z (issue-3d02ab0de320) reproduced late reviewer dispatch: wake-7c08ac9af12c queued 16:55:32.987Z; codex primary claimed/launched 16:55:39.720Z/16:55:39.770Z; wake.retry 'stale launched request recovered by scheduler' 17:00:35.582Z; opencode claimed/launched 17:12:02.986Z/17:12:03.012Z, about 16m30s after queue. Same misattribution: dev-loop.wait.reviewer-wake at 16:55:39.869Z is stamped wait_id game-dll-build:2026-11-23T16:54:31.578Z / issue-3d02ab0de320 but embeds wake-9724b287e9d8 (mode3-latency-goal). Build attempt game-dll-20261123T165431661826Z-6c221c compiled normally and published generation 565 at 90.92s, so this is not a stalled build.

Evidence: {"build_attempt_id": "game-dll-20261123T165431661826Z-6c221c", "build_total_seconds": 90.92, "fallback_adapter": "opencode", "fallback_launched_utc": "2026-11-23T17:12:03.012Z", "issue": "issue-3d02ab0de320", "journal": ".dev/agent-wake/events.jsonl", "misattribution": "dev-loop.wait.reviewer-wake at 2026-11-23T16:55:39.869Z embeds wake-9724b287e9d8 (mode3-latency-goal)", "primary_adapter": "codex", "primary_launched_utc": "2026-11-23T16:55:39.770Z", "total_dispatch_delay_seconds": 990.025, "wait_id": "game-dll-build:2026-11-23T16:54:31.578Z", "wake": "wake-7c08ac9af12c", "wake_queued_utc": "2026-11-23T16:55:32.987Z", "wake_retry_utc": "2026-11-23T17:00:35.582Z"}

## Occurrence

### 2026-11-23T22:41:17.247Z — build-wait-reviewer

Recurrence — codex primary exit delays review ~28 min (issue-5643605344b4).
Wake wake-e3c9f4d9f098 for wait game-dll-build:2026-11-23T21:51:15.080Z: wake.queued 21:52:15.878Z; codex launched 21:52:16.547Z (pid 32856) then exited; wake.retry 22:10:52.365Z switched to opencode; opencode claimed 22:20:28.855Z and launched 22:20:29.420Z (pid 28580); request attempts=2. The review began ~28 min after the wait, same class as the 39-min case in this issue's summary but shorter and via opencode fallback. Journal: .dev/agent-wake/events.jsonl. Reviewer: build-wait-reviewer.

Evidence: {"attempts": 2, "delay_minutes_approx": 28, "fallback_adapter": "opencode", "fallback_launched_utc": "2026-11-23T22:20:29.420Z", "issue": "issue-5643605344b4", "journal": ".dev/agent-wake/events.jsonl", "primary_adapter": "codex", "primary_launched_utc": "2026-11-23T21:52:16.547Z", "retry_utc": "2026-11-23T22:10:52.365Z", "wait_id": "game-dll-build:2026-11-23T21:51:15.080Z", "wake": "wake-e3c9f4d9f098"}

## Occurrence

### 2026-11-23T22:45:02.037Z — build-wait-reviewer

Over-budget wait game-dll-build:2026-11-23T22:09:45.196Z (issue-1f0c919bdbaa) reproduces late reviewer dispatch: wake-6008dd7db0f2 queued 22:10:47.979Z; codex primary claimed/launched 22:10:51.040Z/22:10:51.622Z then exited (attempts=2); wake.retry 'stale launched request recovered by scheduler' switched to opencode 22:20:20.510Z; opencode claimed/launched 22:20:28.658Z/22:20:28.690Z. Total dispatch delay 580.711s (~9m41s) after the wait and 580.711s after queue, above the proposed <=5 min bound. Same misattribution: dev-loop.wait.reviewer-wake at 22:10:52.446Z is stamped wait_id game-dll-build:2026-11-23T22:09:45.196Z / issue-1f0c919bdbaa but embeds wake-ba365e6b1b58 (issue-review-sweeper, issue-87d8c9c7ffb1), not wake-6008dd7db0f2. The underlying build game-dll-20261123T220945368970Z-09cabb compiled normally and published generation 573 at 845.325s, so this is not a stalled build.

Evidence: {"build_attempt_id": "game-dll-20261123T220945368970Z-09cabb", "build_outcome": "success", "build_total_seconds": 845.325, "fallback_adapter": "opencode", "fallback_launched_utc": "2026-11-23T22:20:28.690Z", "issue": "issue-1f0c919bdbaa", "journal": ".dev/agent-wake/events.jsonl", "misattribution": "dev-loop.wait.reviewer-wake at 2026-11-23T22:10:52.446Z embeds wake-ba365e6b1b58 (issue-review-sweeper, issue-87d8c9c7ffb1)", "primary_adapter": "codex", "primary_launched_utc": "2026-11-23T22:10:51.622Z", "retry_reason": "stale launched request recovered by scheduler", "total_dispatch_delay_seconds": 580.711, "wait_id": "game-dll-build:2026-11-23T22:09:45.196Z", "wake": "wake-6008dd7db0f2", "wake_queued_utc": "2026-11-23T22:10:47.979Z", "wake_retry_utc": "2026-11-23T22:20:20.510Z"}


Evidence: {"build_attempt_id": "game-dll-20261123T220945368970Z-09cabb", "build_outcome": "success", "build_total_seconds": 845.325, "fallback_adapter": "opencode", "fallback_launched_utc": "2026-11-23T22:20:28.690Z", "issue": "issue-1f0c919bdbaa", "journal": ".dev/agent-wake/events.jsonl", "misattribution": "dev-loop.wait.reviewer-wake at 2026-11-23T22:10:52.446Z embeds wake-ba365e6b1b58 (issue-review-sweeper, issue-87d8c9c7ffb1)", "primary_adapter": "codex", "primary_launched_utc": "2026-11-23T22:10:51.622Z", "retry_reason": "stale launched request recovered by scheduler", "total_dispatch_delay_seconds": 580.711, "wait_id": "game-dll-build:2026-11-23T22:09:45.196Z", "wake": "wake-6008dd7db0f2", "wake_queued_utc": "2026-11-23T22:10:47.979Z", "wake_retry_utc": "2026-11-23T22:20:20.510Z"}

## Occurrence

### 2026-11-24T00:16:02.566Z — build-wait-reviewer

Over-budget wait game-dll-build:2026-11-23T23:43:01.179Z (issue-b1744402b0a5) reproduces both late dispatch and wake-record misattribution. Wake wake-25a9ef1b3f2a: queued 23:44:07.312Z; codex claimed/launched 23:44:14.577Z/14.599Z; wake.retry "stale launched request recovered by scheduler" to opencode 23:46:51.521Z; opencode claimed/launched 2026-11-24T00:01:18.789Z/18.802Z - queue-to-final-launch 1031.49s (~17m11.5s), above the <=5 min target. The dev-loop.wait.reviewer-wake event at 23:44:15.183Z is stamped with this wait_id / issue-b1744402b0a5 but embeds wake-30a0f17c872c (adapter codex; prompt for issue-e095a23117c8, wait game-dll-build:2026-11-23T22:57:01.813Z), not wake-25a9ef1b3f2a; build_friction.py:242-252 drains the oldest pending wake and returns its result. wake-30a0f17c872c itself was queued 22:58:05.449Z and opencode-drained at 23:44:14.661/14.713Z. The waited-on build (game-dll-20261123T234301570308Z-6c7a9c) FAILED at changed_source_preflight, so the wake subject was a real failure, not a stall.

Evidence: {"build_attempt_id": "game-dll-20261123T234301570308Z-6c7a9c", "build_outcome": "failed", "failed_phase": "changed_source_preflight", "fallback_adapter": "opencode", "fallback_claimed_utc": "2026-11-24T00:01:18.789Z", "fallback_launched_utc": "2026-11-24T00:01:18.802Z", "issue": "issue-b1744402b0a5", "journal": ".dev/agent-wake/events.jsonl", "misattribution": "dev-loop.wait.reviewer-wake at 2026-11-23T23:44:15.183Z is stamped wait_id game-dll-build:2026-11-23T23:43:01.179Z / issue-b1744402b0a5 but embeds wake-30a0f17c872c (issue-e095a23117c8)", "primary_adapter": "codex", "primary_claimed_utc": "2026-11-23T23:44:14.577Z", "primary_launched_utc": "2026-11-23T23:44:14.599Z", "retry_reason": "stale launched request recovered by scheduler", "total_dispatch_delay_seconds": 1031.49, "wait_id": "game-dll-build:2026-11-23T23:43:01.179Z", "wake": "wake-25a9ef1b3f2a", "wake_queued_utc": "2026-11-23T23:44:07.312Z", "wake_retry_utc": "2026-11-23T23:46:51.521Z"}

## Agent suggestion

### 2026-11-24T02:17:20.921Z — issue-review-sweeper

Process-improvement proposal (issue-review-sweeper, wake-af5cc0494785, 2026-11-24). Net-new relative to the 2026-11-23T07:25 outcome-checked-delivery and 15:55 adapter-preference proposals; it targets only the wake-record identity/observability defect, not dispatch timing.

Evidence (negative control)
- For wait exe-build:49:2026-11-24T01:41:57.628Z the dev-loop.wait.reviewer-wake event embedded the CORRECT wake-70987c3bb01d (queued 01:43:04.135Z) because `agent-wake.py run --once` drained the just-queued request. Earlier misattribution cases embed a different wake because that same drain picked the oldest pending request, and build_friction.py:242-252 returns the drain's result rather than the wake id returned by `agent-wake.py add`. So the defect is identity capture, not queueing.

Measurable target
- 10/10 consecutive over-budget wait occurrences have their dev-loop.wait.reviewer-wake event embed the wake id whose wake.queued row references the same issue_id and wait_id (0 misattributed reporter events), while the existing <=5 min dispatch and review-or-escalation targets stay unchanged.

Smallest experiment
- In build_friction.wake_reviewer, capture the wake id from the `agent-wake.py add` output/return and record that id in the event; move the `run --once` launch result into a separate dev-loop.wait.reviewer-launch field/event. Verify over the next 10 over-budget waits by joining .dev/agent-wake/events.jsonl (wake.queued issue/wait) to the dev-loop journal; no dispatch-timing behavior change. No source edit is made by this reviewer; the owner decides.

## Occurrence

### 2026-11-24T02:50:08.100Z — build-wait-reviewer

Over-budget wait exe-build:49:2026-11-24T01:41:57.628Z (issue-c4b82c2ed632) reproduces late reviewer dispatch: wake-70987c3bb01d queued 2026-11-24T01:43:04.135Z; codex primary claimed/launched 01:43:10.131Z/01:43:10.487Z then exited; wake.retry switched to opencode 01:46:08.868Z ('previous adapter process exited; trying fallback'); opencode claimed/launched 02:27:47.135Z/02:27:47.306Z. Total dispatch delay 2683.171s (~44m43s) after queue, far beyond the proposed <=5 min bound. Underlying build exe-build-20261123T204157-f923e86c was real and succeeded at 166.920s (build_script 152.578s) under 99.995% mean host CPU and 228MB minimum available memory, so this is not a stalled build.

Evidence: {"build_attempt_id": "exe-build-20261123T204157-f923e86c", "build_outcome": "success", "build_script_seconds": 152.578, "build_total_seconds": 166.92, "fallback_adapter": "opencode", "fallback_claimed_utc": "2026-11-24T02:27:47.135Z", "fallback_launched_utc": "2026-11-24T02:27:47.306Z", "host_mean_cpu_percent": 99.995, "issue": "issue-c4b82c2ed632", "journal": ".dev/agent-wake/events.jsonl", "minimum_available_memory_bytes": 228171776, "primary_adapter": "codex", "primary_claimed_utc": "2026-11-24T01:43:10.131Z", "primary_launched_utc": "2026-11-24T01:43:10.487Z", "retry_reason": "previous adapter process exited; trying fallback", "total_dispatch_delay_seconds": 2683.171, "wait_id": "exe-build:49:2026-11-24T01:41:57.628Z", "wake": "wake-70987c3bb01d", "wake_queued_utc": "2026-11-24T01:43:04.135Z", "wake_retry_utc": "2026-11-24T01:46:08.868Z"}

## Agent suggestion

### 2026-11-24T03:16:08.983Z — issue-review-sweeper

Process-improvement evidence and one bounded proposal (issue-review-sweeper, wake-2d17204f557c, 2026-11-24). This extends, and does not replace, the 2026-11-23T15:55 work-landed/adapter-preference proposal; it adds a distinct baseline occurrence and a read-only audit that no prior suggestion records.

EVIDENCE (new distinct occurrence, earlier than the occurrences listed above)
- Wait game-dll-build:2026-11-20T19:55:23.269Z (issue-191782d7d9c5). Pinned build game-dll-20261120T195523321490Z-e1b74d was real and succeeded at 853.471s (generation 411), so this is not a stalled build.
- wake-a5dd648492be: wake.queued 19:56:29.235Z; codex claimed/launched 19:56:29.961/19:56:30.004Z then exited; wake.retry to opencode 20:10:41.252Z; opencode claimed/launched 20:19:29.151/20:19:29.185Z; review appended 20:38:10.363Z; request finalized status=failed (adapter process exited) 20:48:37.261Z, attempts=2. Queue-to-final-launch 1379.95s (~23m0s); the status=failed was written after work had landed.
- Surviving artifacts: .dev/agent-wake/events.jsonl and .dev/agent-wake/requests/wake-a5dd648492be.json. This is a fourth distinct 2026-11-20 wait with the same class as the dff7 (16:03) and 6bc05 (14:59) cases already cited.

MEASURABLE TARGET
- Baseline and then target over the surviving request set and the next 10 over-budget wakes: (a) queue-to-final-launch <=300s for 10/10; (b) 0 request files with status=failed that have an issue_reviews row created after launched_utc. Current baseline includes >=1 such request (wake-a5dd648492be).

SMALLEST EXPERIMENT
- Add a read-only audit: load every .dev/agent-wake/requests/*.json, join to issue_reviews on issue plus created_utc>launched_utc, and report counts for (i) failed-after-review, (ii) attempts>1, (iii) queue-to-final-launch>300s. Run it once on the current store and attach the exact counts; then re-run after the work-landed completion predicate lands. No process launch and no source edit are proposed by this reviewer; the owner decides. This keeps the existing one-wake.queued acceptance intact and adds a measurable failed-after-review count.

## Occurrence

### 2026-11-24T04:21:37.501Z — issue-review-sweeper

Distinct failed-wake occurrence from the bounded review of issue-41a872c08642 (issue-review-sweeper, wake-4a9bcecd5b4c, 2026-11-24T04:0xZ). Net-new to this issue's recorded occurrences; this is NOT a new proposal. The fixes are already proposed above (2026-11-23T07:25 outcome-checked delivery, 2026-11-23T15:55 adapter-preference/work-landed, 2026-11-24T02:17 wake-id capture). Status: already proposed; awaiting owner/human decision.

Facts: wake-bb452d13c88d was queued exactly once for over-budget game-dll wait game-dll-build:2026-11-20T21:17:34.934Z (elapsed 60.671s vs 60.0s). codex claimed/launched 2026-11-20T21:18:39 and failed model_not_found ('gpt-6-luna' unsupported on this ChatGPT account); wake.retry moved to opencode 23:44:04.762Z; opencode launched 2026-11-21T00:13:32.190Z; wake.failed 'adapter process exited' 2026-11-21T13:50:41.110Z (attempts=2). Queue-to-failed 59284s (~16h28m). The issue stayed unreviewed until a periodic sweeper wake on 2026-11-24T04:02:53Z (~3d6h44m after creation, 503 completed sessions). No issue_reviews row was attributed to this wake.

Deltas vs already-recorded baseline: a distinct 2026-11-20T21:18 occurrence that failed via model_not_found then opencode exit without a review; and it shows the current backlog at 147 open 'Wait over budget: game-dll-build at game_build' issues / 185 open build-friction issues.

Evidence: {"attempts": 2, "budget_seconds": 60.0, "creation_to_first_review_seconds": 289000, "decision_status": "already-proposed; awaiting owner/human decision", "fallback_adapter": "opencode", "fallback_launched_utc": "2026-11-21T00:13:32.190Z", "first_review_queued_utc": "2026-11-24T04:02:53.151Z", "first_review_wake": "wake-4a9bcecd5b4c", "issue": "issue-41a872c08642", "journal": ".dev/agent-wake/events.jsonl", "open_build_friction_issues": 185, "open_game_dll_wait_issues": 147, "primary_adapter": "codex", "primary_error": "model_not_found: gpt-6-luna not supported when using Codex with a ChatGPT account", "queue_to_failed_seconds": 59284, "request_file": ".dev/agent-wake/requests/wake-bb452d13c88d.json", "wait_elapsed_seconds": 60.671, "wait_id": "game-dll-build:2026-11-20T21:17:34.934Z", "wake": "wake-bb452d13c88d", "wake_error": "adapter process exited", "wake_failed_utc": "2026-11-21T13:50:41.110Z", "wake_queued_utc": "2026-11-20T21:18:37.158Z"}

## Agent suggestion

### 2026-11-24T04:24:42.380Z — issue-review-sweeper

Correction to the 2026-11-24T04:21 occurrence just above (issue-review-sweeper). The narrative durations are correct (~16h32m, ~3d6h44m); two structured seconds values in its Evidence were miscalculated. Correct exact values: queue_to_failed_seconds = 59523.952 (2026-11-20T21:18:37.158Z to 2026-11-21T13:50:41.110Z); creation_to_first_review_seconds = 283457.497 (2026-11-20T21:18:35.654Z to 2026-11-24T04:02:53.151Z). No other value changes; this is an arithmetic correction, not a new proposal or occurrence.

## Occurrence

### 2026-11-24T04:30:58.726Z — build-wait-reviewer

Over-budget wait exe-build:20:2026-11-24T03:29:41.809Z (issue-627ca4b9f577) reproduces late reviewer dispatch: wake-7d93d4ea8cb4 queued 2026-11-24T03:30:43.177Z; codex primary claimed/launched 03:30:44.585/03:30:44.598Z then exited; wake.retry switched to opencode 03:35:41.743Z ("previous adapter process exited; trying fallback"); opencode claimed/launched 04:02:53.740/04:02:53.753Z, about 27m12s after the retry and only when an unrelated scheduled review woke the daemon. Total queued-to-launch delay 1930.576s (~32m11s), far beyond the proposed <=5 min bound. The underlying EXE build 2026-11-24T03:30:52.372066Z (exe-build-20261123T222942-3b725c52) was real and succeeded at 71.185s worker total (build_script 61.413s) under 99.955% mean host CPU and 452MB minimum available memory, so this is not a stalled build.


Evidence: {"build_attempt_id": "exe-build-20261123T222942-3b725c52", "build_outcome": "success", "build_script_seconds": 61.413, "build_total_seconds": 71.185, "fallback_adapter": "opencode", "fallback_claimed_utc": "2026-11-24T04:02:53.740Z", "fallback_launched_utc": "2026-11-24T04:02:53.753Z", "issue": "issue-627ca4b9f577", "journal": ".dev/agent-wake/events.jsonl", "primary_adapter": "codex", "primary_claimed_utc": "2026-11-24T03:30:44.585Z", "primary_launched_utc": "2026-11-24T03:30:44.598Z", "retry_reason": "previous adapter process exited; trying fallback", "total_dispatch_delay_seconds": 1930.576, "wait_id": "exe-build:20:2026-11-24T03:29:41.809Z", "wake": "wake-7d93d4ea8cb4", "wake_queued_utc": "2026-11-24T03:30:43.177Z", "wake_retry_utc": "2026-11-24T03:35:41.743Z"}

## Occurrence

### 2026-11-24T09:46:42.511Z — build-wait-reviewer

Over-budget wait game-dll-build:2026-11-24T07:53:14.620Z (issue-816c05804889) reproduces late reviewer dispatch: wake-30dce0a84a51 queued 2026-11-24T07:54:20.143Z; codex primary claimed/launched 07:54:24.597Z/24.892Z (pid 18204) then delivered nothing; wake.retry 'stale launched request recovered by scheduler' 08:32:49.965Z switched the request to opencode; opencode claimed/launched 09:20:38.674Z/38.761Z (pid 18756). Queue-to-final-launch 5178.618s (~86m18.6s); queue-to-retry 2309.822s (~38m29.8s). Both far beyond the proposed <=5 min bound. This occurrence does NOT show wake-record misattribution: the dev-loop.wait.reviewer-wake event embeds the correct wake-30dce0a84a51. The underlying build game-dll-20261124T075315478413Z-04a3b7 was real and succeeded at total_seconds 221.684 (generation 587; src/vehicles/vehicle-system.cpp 101.057s; link 53.644s), so this is not a stalled build. Reviewer: build-wait-reviewer.

Evidence: {"agent": "build-wait-reviewer", "build_attempt_id": "game-dll-20261124T075315478413Z-04a3b7", "build_outcome": "success", "build_total_seconds": 221.684, "fallback_adapter": "opencode", "fallback_claimed_utc": "2026-11-24T09:20:38.674Z", "fallback_launched_utc": "2026-11-24T09:20:38.761Z", "fallback_pid": 18756, "generate": 587, "issue": "issue-816c05804889", "journal": ".dev/agent-wake/events.jsonl", "misattribution": false, "primary_adapter": "codex", "primary_claimed_utc": "2026-11-24T07:54:24.597Z", "primary_launched_utc": "2026-11-24T07:54:24.892Z", "primary_pid": 18204, "queue_to_final_launch_seconds": 5178.618, "queue_to_retry_seconds": 2309.822, "retry_reason": "stale launched request recovered by scheduler", "wait_id": "game-dll-build:2026-11-24T07:53:14.620Z", "wake": "wake-30dce0a84a51", "wake_queued_utc": "2026-11-24T07:54:20.143Z", "wake_retry_utc": "2026-11-24T08:32:49.965Z"}

## Occurrence

### 2026-11-24T12:22:44.465Z — build-wait-reviewer

Over-budget wait game-dll-build:2026-11-24T11:31:17.691Z (issue-9a301a157e7c) reproduced duplicate + late reviewer dispatch. wake-90ca64fe06de was queued once at 11:32:20.925Z; codex primary claimed/launched 11:32:24.905/11:32:25.007Z (pid 32076); wake.retry at 11:34:08.350Z with reason 'stale launched request recovered by scheduler' switched the request to opencode; opencode claimed/launched 11:58:04.966/11:58:05.004Z (pid 36532); the request file records attempts=2. That is two launches for one wait_id (~25m44s queue-to-final-launch), violating this issue's 'exactly one wake.launched per wait_id' target. The underlying attempt game-dll-20261124T113118610851Z-05382c was real and succeeded at 163.37s, so this is not a stalled build.

Evidence: {"attempts": 2, "build_attempt_id": "game-dll-20261124T113118610851Z-05382c", "build_outcome": "ok", "build_total_seconds": 163.37, "fallback_adapter": "opencode", "fallback_launched_utc": "2026-11-24T11:58:05.004Z", "issue": "issue-9a301a157e7c", "journal": ".dev/agent-wake/events.jsonl", "launches_for_wait_id": 2, "primary_adapter": "codex", "primary_launched_utc": "2026-11-24T11:32:25.007Z", "queue_to_fallback_launch_seconds": 1544.079, "wait_id": "game-dll-build:2026-11-24T11:31:17.691Z", "wake": "wake-90ca64fe06de", "wake_queued_utc": "2026-11-24T11:32:20.925Z", "wake_retry_reason": "stale launched request recovered by scheduler", "wake_retry_utc": "2026-11-24T11:34:08.350Z"}

## Agent suggestion

### 2026-11-24T12:35:59.520Z — build-wait-reviewer

Fresh occurrence on wait game-dll-build:2026-11-24T10:36:51.643Z (issue-8576955c4fb5): wake-7949763eebb4 queued 2026-11-24T10:37:55.012Z; codex primary claimed/launched 10:54:31.727/10:54:31.811Z (996.8s = 16.6 min after queue); wake.retry switched to opencode 11:16:29.381Z; opencode claimed/launched 11:32:24.818/11:32:24.878Z (3269.9s = 54.5 min after queue). Above the <=5 min dispatch target. Build attempt game-dll-20261124T103652183287Z-745ef3 compiled normally and published generation 590 at 10:53:22Z, so the wait was real, not a stall. Same codex-primary-fails/no-daemon-drains pattern already on this issue. Journal: .dev/agent-wake/events.jsonl (wake.queued/wake.claimed/wake.launched/wake.retry) and logs/2026-11-24/20261124_084513/dev-loop/events.jsonl line 274.

## Occurrence

### 2026-11-24T12:57:01.646Z — issue-review-sweeper

Over-budget wait game-dll-build:2026-11-24T12:22:08.237Z (issue-3472692a57c2) reproduced duplicate + late reviewer dispatch. wake-e471a391868c was queued once at 12:23:09.565Z; codex primary claimed/launched 12:23:21.774/844Z (pid 34560); scheduler retry 'stale launched request recovered by scheduler' at 12:38:27.642Z switched the request to opencode; opencode claimed/launched 12:43:54.446/499Z (pid 17520); the request records attempts=2 and two wake.launched events for one wait_id (~1244.9s queue-to-final-launch, ~20m45s). The underlying attempt game-dll-20261124T122208532874Z-63061a was real and succeeded at total_seconds=97.566 (generation 594; server-projectiles.cpp 60.521s; host mean CPU 99.853%, memory used 94%), so this is not a stalled build. Same class as the 2026-11-24T11:31 occurrence above; net-new occurrence, not a new proposal. Reviewer: issue-review-sweeper (wake-d4324fdcd3cf).

Evidence: {"attempts": 2, "build_attempt_id": "game-dll-20261124T122208532874Z-63061a", "build_outcome": "success", "build_total_seconds": 97.566, "compile_seconds": 61.449, "fallback_adapter": "opencode", "fallback_launched_utc": "2026-11-24T12:43:54.499Z", "generation": 594, "host_mean_cpu_percent": 99.853, "host_peak_cpu_percent": 100.0, "issue": "issue-3472692a57c2", "journal": ".dev/agent-wake/events.jsonl", "launches_for_wait_id": 2, "matched_prebuild_resource_baseline": "unavailable", "memory_used_percent": 94.0, "minimum_available_memory_bytes": 312864768, "primary_adapter": "codex", "primary_launched_utc": "2026-11-24T12:23:21.844Z", "queue_to_final_launch_seconds": 1244.934, "resource_journal": ".dev/build-timings/game-dll-20261124T122208532874Z-63061a.resource.jsonl", "retry_reason": "stale launched request recovered by scheduler", "slowest_compile": {"path": "src/network/server-projectiles.cpp", "seconds": 60.521}, "wait_build_id": "2026-11-24T07:48:35.684015Z", "wait_generation": 593, "wait_id": "game-dll-build:2026-11-24T12:22:08.237Z", "wake": "wake-e471a391868c", "wake_queued_utc": "2026-11-24T12:23:09.565Z", "wake_retry_utc": "2026-11-24T12:38:27.642Z"}

## Agent suggestion

### 2026-11-24T14:01:27.477Z — issue-review-sweeper

Measurable target: by 2026-12-08, >=90% of build-wait-reviewer wakes reach an appended issue review within 30 minutes of wake.queued, and every terminal wake emits a journal-visible wake.completed (success) or wake.failed (failure) event. Baseline evidence (read-only, 2026-11-24): .dev/agent-wake/events.jsonl spans 2026-11-20T03:38Z..2026-11-24T13:48Z with 504 distinct wakes and has NO wake.completed event type; all 203 distinct build-wait-reviewer wakes show only a terminal wake.failed, with adapters launched codex 190 / opencode 203. Concrete occurrence: issue-dad513d47108 woke once (wake-a89ef41a94a2, 2026-11-21T00:13:26Z), codex launched 00:13:30Z, retried to opencode 13:50Z, relaunched 15:08Z, failed 15:53Z; the adapter log ends with UnknownError "Failed to execute statement" and the issue had zero reviews until a scheduled sweep at 2026-11-24T13:48Z (about 3 days later). Smallest experiment: read-only join of wake.queued(issue, wall_time) to issue_reviews.created_utc to measure real per-adapter delivery latency and to test whether wake.failed "adapter process exited" is a mislabel of a normal adapter exit; then add a wake.completed event carrying issue_id so success is journal-visible. Do not infer the adapter success/failure mapping before that join.

## Occurrence

### 2026-11-24T16:44:39.807Z — issue-review-sweeper

Distinct failed-wake occurrence from the bounded review of issue-6415397775a2 (issue-review-sweeper, wake-3efa68064700, 2026-11-24T16:29Z). Occurrence only; the fixes are already proposed above (2026-11-23T07:25 outcome-checked delivery, 2026-11-23T15:55 adapter-preference/work-landed, 2026-11-24T14:01 >=90%-within-30min + wake.completed). No new proposal filed to avoid a duplicate report.

Facts: wake-fd61691ffd48 was queued exactly once (2026-11-21T02:30:08.058Z, ~6 s after issue creation) for over-budget game-dll wait game-dll-build:2026-11-21T02:29:01.212Z (elapsed 60.44 s vs 60.0 s). codex claimed/launched 13:50:41.470Z/.493Z (queue-to-claim 40833.4 s ~= 11h20m33s); wake.retry to opencode 15:08:17.043Z; opencode claimed/launched 15:20:50.356Z/.464Z (queue-to-final-launch 46242.4 s ~= 12h50m42s); wake.failed "adapter process exited" 15:53:26.109Z (queue-to-failed 48198.1 s ~= 13h23m18s), attempts=2, request fallbacks=[]. No issue_reviews row was written by this wake; issue-6415397775a2 stayed state=new for 309562.9 s (~3d13h59m) until this scheduled sweep. The waited-on build was a real failure: attempt game-dll-20261121T022901426487Z-d15759 failed changed_source_preflight on src/network/multiplayer-projectiles.cpp after 1240.728 s (total 1914.474 s), so this is not a stalled-healthy-build false alarm for this occurrence.

Delta vs recorded baseline: earlier than the 2026-11-21T13:50Z-class occurrences cited above, with an 11h20m queue-to-first-claim gap and a failure on the opencode fallback as well; the request file records fallbacks=[] even though a retry to opencode occurred.

Evidence: {"attempts": 2, "budget_seconds": 60.0, "build_attempt_id": "game-dll-20261121T022901426487Z-d15759", "build_outcome": "failed", "creation_to_sweep_review_seconds": 309562.853, "failed_phase": "changed_source_preflight", "failed_source": "src/network/multiplayer-projectiles.cpp", "fallback_adapter": "opencode", "fallback_claimed_utc": "2026-11-21T15:20:50.356Z", "fallback_launched_utc": "2026-11-21T15:20:50.464Z", "first_claim_utc": "2026-11-21T13:50:41.470Z", "issue": "issue-6415397775a2", "issue_created_utc": "2026-11-21T02:30:01.668Z", "journal": ".dev/agent-wake/events.jsonl", "preflight_seconds": 1240.728, "primary_adapter": "codex", "primary_launched_utc": "2026-11-21T13:50:41.493Z", "queue_to_failed_seconds": 48198.051, "queue_to_final_launch_seconds": 46242.406, "queue_to_first_claim_seconds": 40833.412, "request_fallbacks": [], "request_file": ".dev/agent-wake/requests/wake-fd61691ffd48.json", "retry_reason": "previous adapter process exited; trying fallback", "wait_elapsed_seconds": 60.44, "wait_id": "game-dll-build:2026-11-21T02:29:01.212Z", "wake": "wake-fd61691ffd48", "wake_error": "adapter process exited", "wake_failed_utc": "2026-11-21T15:53:26.109Z", "wake_queued_utc": "2026-11-21T02:30:08.058Z", "wake_retry_utc": "2026-11-21T15:08:17.043Z"}

## Occurrence

### 2026-11-24T17:41:28.800Z — build-wait-reviewer

Over-budget wait game-dll-build:2026-11-24T16:47:16.707Z (issue-ee437972915e) reproduced a misleading 'issue-failed' plus late/duplicate dispatch. dev-loop.wait.issue-failed at 16:48:52.667Z because agent-wake.py run --once hit the 30 s subprocess timeout in build_friction.wake_reviewer while the issue and wake had already been created. wake-742bed53da17 queued 16:48:22.319Z; codex claimed/launched 17:04:40.975Z/17:04:41.005Z; wake.retry to opencode 17:08:26.715Z; opencode claimed/launched 17:08:29.198Z/17:08:29.237Z; attempts=2; queue-to-final-launch 966.9 s (>5 min target). The waited-on game-DLL build was real and succeeded in 810.0 s, so this is dispatch/telemetry, not a stalled build.

Evidence: {"attempts": 2, "build_attempt_id": "game-dll-20261124T164717275109Z-44a127", "build_outcome": "success", "build_total_seconds": 810.0, "codex_claimed_utc": "2026-11-24T17:04:40.975Z", "codex_launched_utc": "2026-11-24T17:04:41.005Z", "dev_loop_event": "dev-loop.wait.issue-failed", "dev_loop_event_error": "agent-wake.py run --once timed out after 30 seconds", "dev_loop_event_utc": "2026-11-24T16:48:52.667Z", "dev_loop_journal": "logs/2026-11-24/20261124_150432/dev-loop/events.jsonl", "issue": "issue-ee437972915e", "journal": ".dev/agent-wake/events.jsonl", "opencode_claimed_utc": "2026-11-24T17:08:29.198Z", "opencode_launched_utc": "2026-11-24T17:08:29.237Z", "queue_to_final_launch_seconds": 966.918, "wait_id": "game-dll-build:2026-11-24T16:47:16.707Z", "wake": "wake-742bed53da17", "wake_queued_utc": "2026-11-24T16:48:22.319Z", "wake_retry_utc": "2026-11-24T17:08:26.715Z"}

## Agent suggestion

### 2026-11-24T17:44:59.463Z — issue-review-sweeper

Occurrence evidence for this late-dispatch issue, from the independent review of issue-e96486c8cc3e (read-only, 2026-11-24T17:44Z). Wait game-dll-build:2026-11-21T03:10:13.609Z queued exactly one reviewer wake, wake-6b00f2b0af56, at 2026-11-21T03:11:23.488Z (.dev/agent-wake/events.jsonl L480).

Dispatch timeline from .dev/agent-wake/events.jsonl and requests/wake-6b00f2b0af56.json:
- 03:11:23.488Z wake.queued (manual).
- 03:48:07.456Z wake.claimed (codex); 03:48:08.055Z wake.launched (codex) — 36m44s after queue.
- 03:49:22-23Z codex log model-cache failure ("unknown variant `max`…"/models refresh stream disconnected) — same primary-adapter failure class this issue names.
- 13:50:41.009Z wake.retry -> opencode — ~10h01m after the codex launch, matching the "no daemon drains the fallback" pattern.
- 15:08:10.228Z wake.claimed (opencode); 15:08:10.255Z wake.launched (opencode).
- Reviewer session log shows the session opening the issue at 15:14:44Z and dying at 15:18:08Z with "Unexpected server error" (ref err_9dbc78ec) before appending any review.
- 15:20:49.982Z wake.failed "adapter process exited"; requests/wake-6b00f2b0af56.json status=failed, attempts=2.

Result for this occurrence: queue contract held (one wake), but time-to-healthy-review was 12h09m and no review was written. Suggested smallest check: replay one synthetic over-budget wait with codex unavailable and a persistent opencode daemon, and assert attempts=1, queue-to-launch <=300s, and a non-empty review or an explicit failure event that names the adapter crash. Owner is unassigned; this evidence is appended rather than creating a duplicate issue.

## Agent discussion and suggestions

### 2026-11-24T17:45:40.040Z — issue-review-sweeper

Message: `msg-a889be261964` (evidence)

Subject: Occurrence: wake-6b00f2b0af56 took 12h09m and died before writing a review

Evidence for issue-84c584957e65, appended as an occurrence (no duplicate issue). Wake wake-6b00f2b0af56 for wait game-dll-build:2026-11-21T03:10:13.609Z (issue-e96486c8cc3e): wake.queued 2026-11-21T03:11:23.488Z; codex claimed 03:48:07.456Z (36m44s later) and its log shows model-cache failures 03:49:22-23Z; wake.retry -> opencode only at 13:50:41.009Z (~10h later, 'no daemon drains the fallback' pattern); opencode claimed/launched 15:08:10Z; the reviewer session opened the issue at 15:14:44Z and died at 15:18:08Z with 'Unexpected server error' (ref err_9dbc78ec) before appending any review; wake.failed 15:20:49.982Z 'adapter process exited' (attempts=2). Net: queue contract held (one wake) but time-to-healthy-review was 12h09m with zero review written. Suggested smallest check: replay one synthetic over-budget wait with codex unavailable and a persistent opencode daemon; assert attempts=1, queue-to-launch <=300s, and either a non-empty review or a named adapter-crash failure event.

## Occurrence

### 2026-11-24T18:29:53.740Z — build-wait-reviewer

Over-budget wait game-dll-build:2026-11-24T17:12:09.235Z (issue-d82b02478dff) reproduced late reviewer dispatch. wake-5c0ec2e79124 queued 2026-11-24T17:13:32.714Z; codex primary claimed/launched 17:13:36.495/17:13:36.589Z then exited; wake.retry 'previous adapter process exited; trying fallback' switched to opencode 17:48:09.917Z; opencode claimed/launched 17:53:05.835/17:53:05.998Z. attempts=2 and two wake.launched events exist for one wait_id; the request file records fallbacks=[] despite the opencode retry. Queue-to-final-launch 2373.284 s (~39m33s), far beyond the proposed <=5 min bound. The waited-on attempt game-dll-20261124T171210403561Z-968a7e was not a stalled healthy build at trigger time: the wait fired 63.935s after dev-loop thread launch but 25ms after build.started; the 63.910s pre-worker gap matches synchronous BuildResourceSampler.start() setup (sampler_setup_wall_ms=62750.0, sampler_setup_cpu_ms=4484.375). The attempt later failed compile at 17:36:57.457Z in vehicle-profile.cpp/vehicle-system.cpp, already recorded in issue-136d4553ff92. Reviewer: build-wait-reviewer.

Evidence: {"attempts": 2, "build_attempt_id": "game-dll-20261124T171210403561Z-968a7e", "build_outcome": "failed", "build_started_utc": "2026-11-24T17:13:13.145Z", "dev_loop_journal": "logs/2026-11-24/20261124_170535/dev-loop/events.jsonl", "failed_phase": "compile", "fallback_adapter": "opencode", "fallback_claimed_utc": "2026-11-24T17:53:05.835Z", "fallback_launched_utc": "2026-11-24T17:53:05.998Z", "game_build_started_utc": "2026-11-24T17:12:09.235Z", "issue": "issue-d82b02478dff", "journal": ".dev/agent-wake/events.jsonl", "launches_for_wait_id": 2, "preworker_gap_seconds": 63.91, "primary_adapter": "codex", "primary_claimed_utc": "2026-11-24T17:13:36.495Z", "primary_launched_utc": "2026-11-24T17:13:36.589Z", "queue_to_final_launch_seconds": 2373.284, "request_fallbacks": [], "request_file": ".dev/agent-wake/requests/wake-5c0ec2e79124.json", "retry_reason": "previous adapter process exited; trying fallback", "sampler_setup_cpu_ms": 4484.375, "sampler_setup_wall_ms": 62750, "wait_id": "game-dll-build:2026-11-24T17:12:09.235Z", "wait_queued_utc": "2026-11-24T17:13:32.714Z", "wake": "wake-5c0ec2e79124", "wake_retry_utc": "2026-11-24T17:48:09.917Z"}

## Occurrence

### 2026-11-24T20:08:00.223Z — build-wait-reviewer

Over-budget wait game-dll-build:2026-11-24T19:41:25.941Z (issue-536424f63590) reproduced late reviewer dispatch plus wake-record misattribution. wake-76763ae5a22d was queued exactly once at 2026-11-24T19:42:28.520Z; codex claimed/launched 19:42:45.151Z/19:42:45.252Z then exited (its session ended 19:47:06Z without appending a review); wake.retry 'stale launched request recovered by scheduler' 19:55:42.336Z; opencode claimed/launched 19:55:43.646Z/19:55:43.762Z; requests/wake-76763ae5a22d.json attempts=2. Queue-to-final-launch 795.24s (~13m15s), above the <=5 min target, with two wake.launched events for one wait_id (violates the exactly-one-launch target; the exactly-once queue contract held). The dev-loop.wait.reviewer-wake event at 19:42:45.432Z is stamped with this wait_id / issue-536424f63590 but its launch_result embeds wake-d6cb2a6d7a8e (prompt for issue-a0fa88737830, wait game-dll-build:2026-11-24T19:17:58.359Z), not wake-76763ae5a22d; build_friction.py wake_reviewer returns the tail of 'agent-wake.py run --once' output, which drained another pending wake. The waited-on attempt game-dll-20261124T194125987951Z-4d7bb0 was real and succeeded at 801.055s (generation 609), so this is not a stalled build.

Evidence: {"attempts": 2, "build_attempt_id": "game-dll-20261124T194125987951Z-4d7bb0", "build_outcome": "success", "build_total_seconds": 801.055, "dev_loop_journal": "logs/2026-11-24/20261124_192708/dev-loop/events.jsonl", "fallback_adapter": "opencode", "fallback_claimed_utc": "2026-11-24T19:55:43.646Z", "fallback_launched_utc": "2026-11-24T19:55:43.762Z", "generation": 609, "issue": "issue-536424f63590", "journal": ".dev/agent-wake/events.jsonl", "launches_for_wait_id": 2, "misattribution": "dev-loop.wait.reviewer-wake at 2026-11-24T19:42:45.432Z embeds wake-d6cb2a6d7a8e (issue-a0fa88737830), not wake-76763ae5a22d", "primary_adapter": "codex", "primary_claimed_utc": "2026-11-24T19:42:45.151Z", "primary_launched_utc": "2026-11-24T19:42:45.252Z", "queue_to_final_launch_seconds": 795.24, "request_file": ".dev/agent-wake/requests/wake-76763ae5a22d.json", "retry_reason": "stale launched request recovered by scheduler", "wait_id": "game-dll-build:2026-11-24T19:41:25.941Z", "wake": "wake-76763ae5a22d", "wake_queued_utc": "2026-11-24T19:42:28.520Z", "wake_retry_utc": "2026-11-24T19:55:42.336Z"}

## Occurrence

### 2026-11-24T20:48:12.346Z — build-wait-reviewer

Over-budget wait game-dll-build:2026-11-24T20:00:45.963Z (issue-a4eea8fe2246) reproduced late reviewer dispatch and a duplicate launch. Wake wake-ad0bab437950 was queued exactly once at 2026-11-24T20:01:47.676Z; codex claimed/launched 20:01:49.039Z/20:01:49.191Z (pid 24936) then exited; wake.retry 'previous adapter process exited; trying fallback' at 20:07:52.113Z; opencode claimed/launched 20:12:42.414Z/20:12:42.536Z (pid 7516, this run). requests/wake-ad0bab437950.json attempts=2. Queue-to-final-launch 654.86s (~10m54.9s), above the proposed <=5 min bound, with two wake.launched events for one wait_id (the exactly-once queue held; the one-launch target did not). The dev-loop.wait.reviewer-wake event is missing for this occurrence (events_path=None), so the usual launch_result mis-attribution could not be checked. The waited-on attempt game-dll-20261124T200045986936Z-3f16b3 was real work and failed at compile in src/entities/player-render.cpp:634 at 134.4s, so this is dispatch/telemetry, not a stalled build.

Evidence: {"agent": "build-wait-reviewer", "attempts": 2, "build_attempt_id": "game-dll-20261124T200045986936Z-3f16b3", "build_failed_phase": "compile", "build_outcome": "failed", "build_total_seconds": 134.394, "dev_loop_event_missing": true, "duplicate_launch": true, "fallback_adapter": "opencode", "fallback_claimed_utc": "2026-11-24T20:12:42.414Z", "fallback_launched_utc": "2026-11-24T20:12:42.536Z", "fallback_pid": 7516, "issue": "issue-a4eea8fe2246", "journal": ".dev/agent-wake/events.jsonl", "launch_result_misattribution_checked": false, "primary_adapter": "codex", "primary_claimed_utc": "2026-11-24T20:01:49.039Z", "primary_launched_utc": "2026-11-24T20:01:49.191Z", "primary_pid": 24936, "queue_to_final_launch_seconds": 654.86, "retry_reason": "previous adapter process exited; trying fallback", "wait_id": "game-dll-build:2026-11-24T20:00:45.963Z", "wake": "wake-ad0bab437950", "wake_queued_utc": "2026-11-24T20:01:47.676Z", "wake_retry_utc": "2026-11-24T20:07:52.113Z"}

## Agent suggestion

### 2026-11-24T21:03:22.955Z — build-wait-reviewer

Recurrence 2026-11-24 (reviewer of issue-74f939fb99fc): wake-5a26878e0275 reproduces the duplicate-and-late launch. .dev/agent-wake/events.jsonl sequence: wake.queued 2026-11-24T20:25:22.352Z; wake.claimed adapter=codex 20:25:24; wake.launched adapter=codex pid=25496 20:25:24; wake.retry adapter=opencode 20:36:52; wake.claimed adapter=opencode 20:38:43; wake.launched adapter=opencode pid=19140 20:38:43. That is one wake.queued but TWO wake.launched rows and a 13m21s queue-to-delivery delay. Direct evidence for the acceptance criterion 'one wake.queued and one wake.launched ... no duplicate launches'. Cross-ref: this delay is orthogonal to the wait record, which was valid. No source edited; reporter is build-wait-reviewer. Suggest capturing codex exit reason from the wake log to confirm the same model_not_found path; unknown whether a fallback daemon was running at 20:36.

## Occurrence

### 2026-11-24T21:09:16.632Z — build-wait-reviewer

Over-budget wait game-dll-build:2026-11-24T20:24:20.106Z (issue-74f939fb99fc) reproduced duplicate + late reviewer dispatch. wake-5a26878e0275 was queued exactly once at 2026-11-24T20:25:22.352Z; codex primary claimed/launched 20:25:24 (pid 25496); wake.retry adapter=opencode at 20:36:52; opencode claimed/launched 20:38:43 (pid 19140). attempts=2 in requests/wake-5a26878e0275.json; two wake.launched events for one wait_id; queue-to-final-launch 800.7s (~13m20.7s), above the proposed <=5 min bound (the exactly-once queue held; the exactly-one-launch target did not). The waited-on attempt game-dll-20261124T202420131204Z-8cc10c was real work and succeeded at total_seconds=128.646 (generation 617; changed_source_preflight 70.499s; link 33.852s). Reviewer: build-wait-reviewer.

Evidence: {"agent": "build-wait-reviewer", "attempts": 2, "build_attempt_id": "game-dll-20261124T202420131204Z-8cc10c", "build_generation": 617, "build_outcome": "success", "build_total_seconds": 128.646, "changed_source_preflight_seconds": 70.499, "duplicate_launch": true, "fallback_adapter": "opencode", "fallback_claimed_utc": "2026-11-24T20:38:43Z", "fallback_launched_utc": "2026-11-24T20:38:43Z", "fallback_pid": 19140, "issue": "issue-74f939fb99fc", "journal": ".dev/agent-wake/events.jsonl", "launches_for_wait_id": 2, "link_seconds": 33.852, "primary_adapter": "codex", "primary_claimed_utc": "2026-11-24T20:25:24Z", "primary_launched_utc": "2026-11-24T20:25:24Z", "primary_pid": 25496, "queue_to_final_launch_seconds": 800.7, "request_file": ".dev/agent-wake/requests/wake-5a26878e0275.json", "wait_id": "game-dll-build:2026-11-24T20:24:20.106Z", "wake": "wake-5a26878e0275", "wake_queued_utc": "2026-11-24T20:25:22.352Z", "wake_retry_utc": "2026-11-24T20:36:52Z"}

## Occurrence

### 2026-11-24T21:26:25.029Z — build-wait-reviewer

Wait game-dll-build:2026-11-24T20:35:35.515Z (issue issue-21e4b1fbc77e) reproduced two launches for one wait_id, but within the <=5 min bound. wake-477d42954674 was queued exactly once at 20:36:37.848Z; codex claimed/launched 20:36:52.484/20:36:52.610Z (pid 34424); wake.retry 'stale launched request recovered by scheduler' at 20:38:37.184Z; opencode claimed/launched 20:38:42.741/20:38:42.828Z (pid 34476); attempts=2. Queue-to-final-launch 124.98s, so late dispatch is NOT reproduced; the exactly-one-launch target is still violated. The waited-on attempt game-dll-20261124T203535650414Z-718e6f was real work and succeeded at 228.997s (generation 618), so not a stalled build. Reviewer: build-wait-reviewer.

Evidence: {"attempts": 2, "build_attempt_id": "game-dll-20261124T203535650414Z-718e6f", "build_outcome": "success", "build_total_seconds": 228.997, "fallback_adapter": "opencode", "fallback_claimed_utc": "2026-11-24T20:38:42.741Z", "fallback_launched_utc": "2026-11-24T20:38:42.828Z", "issue": "issue-21e4b1fbc77e", "journal": ".dev/agent-wake/events.jsonl", "late_dispatch_reproduced": false, "launches_for_wait_id": 2, "primary_adapter": "codex", "primary_claimed_utc": "2026-11-24T20:36:52.484Z", "primary_launched_utc": "2026-11-24T20:36:52.610Z", "queue_to_final_launch_seconds": 124.98, "wait_id": "game-dll-build:2026-11-24T20:35:35.515Z", "wake": "wake-477d42954674", "wake_queued_utc": "2026-11-24T20:36:37.848Z", "wake_retry_utc": "2026-11-24T20:38:37.184Z"}

## Occurrence

### 2026-11-24T21:29:39.440Z — build-wait-reviewer

Recurrence 2026-11-24 wait (issue-7ef5043ce95c, wake-c29456cabf35): wake.queued 2026-11-24T20:21:00.308Z; codex claimed/launched 20:21:11.629/.698Z then exited; wake.retry 20:25:24.985Z "previous adapter process exited; trying fallback"; opencode claimed/launched 20:36:52.814/.889Z; .dev/agent-wake/requests/wake-c29456cabf35.json attempts=2. Queue-to-final-launch 952.6s (~15m52.6s) and two wake.launched rows for one wait_id, above the <=5 min bound and against one-launch-per-wait.

Evidence: {"issue": "issue-7ef5043ce95c", "launches": 2, "queue_to_launch_seconds": 952.6, "wake": "wake-c29456cabf35"}

## Agent suggestion

### 2026-11-24T22:01:01.756Z — build-wait-reviewer

Recurrence evidence (2026-11-24, issue-6692376723be review): wake-dbeab2f5c771 was queued once at 21:01:34.368Z for a 62.207s game-dll-build wait, then launched twice: codex 21:13:39.832Z and opencode 21:45:08.474Z. wake.retry logged 21:24:13.300Z but opencode did not claim until 21:45:08.388Z, immediately after a different wait's agent-wake run --once drained the queue (wake-59dc989b28d0 for issue-ee3e906c8273). Queue-to-final-launch 2614.1s (~43.6 min). This directly reproduces the no-persistent-daemon/next-wait-drain mechanism described in this issue and the duplicate-launch acceptance violation. Evidence: .dev/agent-wake/events.jsonl, .dev/agent-wake/requests/wake-dbeab2f5c771.json, logs/2026-11-24/20261124_205956/dev-loop/events.jsonl (dev-loop.wait.reviewer-wake at 21:45:08.562Z embeds wake-dbeab2f5c771 under wait 21:43:56.034Z). Suggested next check: bound dispatch with a persistent daemon or scheduled retry and assert exactly one wake.launched per wake id.

## Occurrence

### 2026-11-24T22:03:16.454Z — build-wait-reviewer

Recurrence 2026-11-24 (issue-98a885a0bb81, wake-a61c8e5d2cf2): request created 2026-11-24T20:56:07.426Z; primary codex wake.claimed 2026-11-24T20:56:33.917Z and wake.launched 20:56:34.041Z; wake.retry with adapter fallback to opencode at 21:13:39.208Z; opencode wake.claimed 21:14:31.612Z and wake.launched 21:14:31.937Z. The reviewer reached the issue ~18m24s after the wake was queued, matching the original codex-primary-to-opencode-fallback pattern. Exactly one wake.queued exists for the occurrence (no duplicate), so the "queued exactly once" half holds; only dispatch latency regressed. Evidence: .dev/agent-wake/events.jsonl wake-a61c8e5d2cf2 rows and .dev/agent-wake/requests/wake-a61c8e5d2cf2.json (attempts=2). Read-only; no source change.

Evidence: {"attempts": 2, "dispatch_delay_seconds": 1104.5, "issue": "issue-98a885a0bb81", "opencode_launched_utc": "2026-11-24T21:14:31.937Z", "queued_utc": "2026-11-24T20:56:07.426Z", "wait_id": "game-dll-build:2026-11-24T20:55:04.680Z", "wake": "wake-a61c8e5d2cf2"}

## Occurrence

### 2026-11-24T22:45:55.005Z — build-wait-reviewer

Occurrence 2026-11-24 (build-wait-reviewer review of issue-ee3e906c8273, wake-59dc989b28d0). A distinct, more precise instance of the no-daemon/oldest-drain mechanism already named in the 2026-11-24T22:01 occurrence.

Facts from .dev/agent-wake/events.jsonl:
- wake-59dc989b28d0 queued 21:45:00.693Z for wait game-dll-build:2026-11-24T21:43:56.034Z (issue-ee3e906c8273).
- codex claimed/launched 21:45:05.480Z/21:45:07.333Z (pid 26096).
- wake.retry to opencode 21:51:21.275Z ('stale launched request recovered by scheduler').
- opencode claimed/launched 21:59:10.107Z/21:59:10.601Z (pid 18976) - only when the NEXT wait (21:58:06.724Z) ran its own drain.
- One wake.queued, two wake.launched, queue-to-final-launch 849.908s (~14m10s).

Wake-record misattribution (both directions):
- The dev-loop.wait.reviewer-wake event for wait 21:43:56.034Z / issue-ee3e906c8273 at 21:45:08.562Z embeds wake-dbeab2f5c771 (log 2026-11-24T21-01-34.352Z, pid 33676) instead of wake-59dc989b28d0 (logs/2026-11-24/20261124_205956/dev-loop/events.jsonl line 287).
- The event for the next wait 21:58:06.724Z / issue-a2de003a1900 at 21:59:11.373Z embeds wake-59dc989b28d0 (log 2026-11-24T21-45-00.685Z, pid 18976) (logs/2026-11-24/20261124_215719_01/dev-loop/events.jsonl line 48).
- Mechanically consistent with build_friction.py:242-252 returning the tail of 'agent-wake.py run --once', which drains pending requests other than the one just added.

Underlying build was real work (attempt game-dll-20261124T214357271677Z-7f59d1 failed changed_source_preflight on src/main-systems.cpp after 261.697s), not a stalled healthy build. Reviewer: build-wait-reviewer.

Evidence: {"agent": "build-wait-reviewer", "fallback_adapter": "opencode", "fallback_launched_utc": "2026-11-24T21:59:10.601Z", "issue": "issue-ee3e906c8273", "journal_205956_line": 287, "journal_215719_line": 48, "launches_for_wait_id": 2, "misattribution": "wait 21:43:56.034Z event embeds wake-dbeab2f5c771 (issue-6692376723be); wait 21:58:06.724Z event embeds wake-59dc989b28d0", "primary_adapter": "codex", "primary_launched_utc": "2026-11-24T21:45:07.333Z", "queue_to_final_launch_seconds": 849.908, "wait_id": "game-dll-build:2026-11-24T21:43:56.034Z", "wake": "wake-59dc989b28d0", "wake_queued_utc": "2026-11-24T21:45:00.693Z", "wake_retry_reason": "stale launched request recovered by scheduler", "wake_retry_utc": "2026-11-24T21:51:21.275Z"}

## Occurrence

### 2026-11-24T22:53:53.333Z — build-wait-reviewer

Over-budget wait game-dll-build:2026-11-24T21:58:06.724Z (issue-a2de003a1900) reproduced late reviewer dispatch and a duplicate launch. Wake wake-3145a03d66ba was queued exactly once at 21:59:08.153Z (.dev/agent-wake/events.jsonl L2912). Codex claimed 21:59:09.006Z and launched pid 26300 at 21:59:09.145Z; wake.retry 'stale launched request recovered by scheduler' fired at 22:01:27.596Z; opencode claimed 22:07:28.050Z and launched pid 35280 at 22:07:28.087Z. Queue-to-final-launch 499.9s (~8m20s), attempts=2, with two wake.launched events for one wait_id (requests/wake-3145a03d66ba.json). The exactly-once queue held; the one-launch target did not. The dev-loop.wait.reviewer-wake row at 21:59:11.373Z is stamped with this wait_id/issue but its wake.launch_result (2000-char tail of 'agent-wake.py run --once', build_friction.py:250) embeds wake-59dc989b28d0 / issue-ee3e906c8273 / log 2026-11-24T21-45-00.685Z / pid 18976, not this wake, so the row does not attribute this launch. The waited-on attempt game-dll-20261124T215807388728Z-84be9e was real and succeeded (generation 625, total 182.618s), so this is dispatch/attribution, not a stalled build. Reviewer: build-wait-reviewer.

Evidence: {"attempts": 2, "build_attempt_id": "game-dll-20261124T215807388728Z-84be9e", "build_outcome": "success", "build_total_seconds": 182.618, "dev_loop_reviewer_wake_wall_time": "2026-11-24T21:59:11.373Z", "fallback_adapter": "opencode", "fallback_claimed_utc": "2026-11-24T22:07:28.050Z", "fallback_launched_utc": "2026-11-24T22:07:28.087Z", "fallback_pid": 35280, "generation": 625, "issue": "issue-a2de003a1900", "journal": ".dev/agent-wake/events.jsonl", "launch_result_misattribution": {"embedded_issue": "issue-ee3e906c8273", "embedded_log": "C:\\mimita-v9\\.dev\\agent-wake\\logs\\2026-11-24T21-45-00.685Z-build-wait-reviewer.log", "embedded_pid": 18976, "embedded_wake": "wake-59dc989b28d0", "source": "devscripts/build_friction.py:250"}, "launches_for_wait_id": 2, "primary_adapter": "codex", "primary_claimed_utc": "2026-11-24T21:59:09.006Z", "primary_launched_utc": "2026-11-24T21:59:09.145Z", "primary_pid": 26300, "queue_to_final_launch_seconds": 499.9, "retry_reason": "stale launched request recovered by scheduler", "wait_id": "game-dll-build:2026-11-24T21:58:06.724Z", "wake": "wake-3145a03d66ba", "wake_queued_utc": "2026-11-24T21:59:08.153Z", "wake_retry_utc": "2026-11-24T22:01:27.596Z"}

## Occurrence

### 2026-11-24T23:02:33.162Z — build-wait-reviewer

For wait_id game-dll-build:2026-11-24T22:06:22.495Z (issue-739845c7624a), the dev-loop.wait.reviewer-wake event (run 20261124_215719_01, 2026-11-24T22:07:31.053Z) reports queued=true but its launch_result tail embeds wake-dffe08dfc219, agent bodycam-spec-implementation, and context ctx-41260d9f9184, not this wait. Same launch_result attribution defect described in this issue. The actual reviewer wake for this wait is wake-1aabfb035087, queued once at 2026-11-24T22:07:24.897Z.

Evidence: {"correct_wake": "wake-1aabfb035087", "correct_wake_queued_utc": "2026-11-24T22:07:24.897Z", "issue": "issue-739845c7624a", "misattributed_agent": "bodycam-spec-implementation", "misattributed_context": "ctx-41260d9f9184", "misattributed_wake": "wake-dffe08dfc219", "reviewer_wake_event_utc": "2026-11-24T22:07:31.053Z", "wait_id": "game-dll-build:2026-11-24T22:06:22.495Z"}

## Occurrence

### 2026-11-25T02:37:23.006Z — build-wait-reviewer

Over-budget wait game-dll-build:2026-11-25T02:11:23.680Z (issue-0612aaee5a6e) reproduced late reviewer dispatch and a duplicate launch. wake-47aeb34e7160 queued exactly once at 2026-11-25T02:12:26.258Z; codex claimed/launched 02:12:40.866/.932Z (pid 14944) then exited; wake.retry 'previous adapter process exited; trying fallback' at 02:21:12.067Z; opencode claimed/launched 02:22:20.392/.420Z (pid 12324); requests attempts=2, two wake.launched events for one wait_id. Queue-to-final-launch 594.16 s (~9m54s), above the <=5 min bound (the exactly-once queue held; the one-launch target did not). No wake-record misattribution: the dev-loop.wait.reviewer-wake row embeds pid 14944 / wake-47aeb34e7160. The waited-on attempt game-dll-20261125T021123740140Z-5e8bdb was real work and failed at worker_supervised timeout_seconds=300 at 02:16:32.288Z, not a stalled healthy build. Reviewer: build-wait-reviewer.

Evidence: {"attempts": 2, "build_attempt_id": "game-dll-20261125T021123740140Z-5e8bdb", "build_outcome": "failed", "failed_phase": "worker_supervised", "failed_reason": "timeout", "failed_timeout_seconds": 300.0, "failed_utc": "2026-11-25T02:16:32.288Z", "fallback_adapter": "opencode", "fallback_claimed_utc": "2026-11-25T02:22:20.392Z", "fallback_launched_utc": "2026-11-25T02:22:20.420Z", "fallback_pid": 12324, "issue": "issue-0612aaee5a6e", "journal": ".dev/agent-wake/events.jsonl", "launches_for_wait_id": 2, "misattribution": false, "primary_adapter": "codex", "primary_claimed_utc": "2026-11-25T02:12:40.866Z", "primary_launched_utc": "2026-11-25T02:12:40.932Z", "primary_pid": 14944, "queue_to_final_launch_seconds": 594.162, "request_file": ".dev/agent-wake/requests/wake-47aeb34e7160.json", "retry_reason": "previous adapter process exited; trying fallback", "wait_id": "game-dll-build:2026-11-25T02:11:23.680Z", "wake": "wake-47aeb34e7160", "wake_queued_utc": "2026-11-25T02:12:26.258Z", "wake_retry_utc": "2026-11-25T02:21:12.067Z"}

## Agent suggestion

### 2026-11-25T06:11:06.171Z — build-wait-reviewer

Occurrence 2026-11-25T05:46:30.064Z (issue-9c1fc3e3e668): wake-40cc83217bc0 queued 05:47:33.393Z; primary codex claimed/launched 05:47:35.941/05:47:35.981Z (pid 11392) then exited; wake.retry to opencode 05:54:03.422Z; opencode claimed/launched 05:55:32.315/05:55:32.367Z (pid 22364). Wait to final launch is about 481.8s, queue to final launch about 479.0s, and two wake.launched rows exist for one wake id. Same codex-fallback dispatch-latency defect. Reviewer build-wait-reviewer.

## Agent suggestion

### 2026-11-25T06:19:03.073Z — build-wait-reviewer

Occurrence 2026-11-25T05:52:56.306Z wait (issue-8af9b97c9a2a, wake-9c7b2299476c): wake.queued 05:54:00.283Z; codex claimed/launched 05:54:03.964/05:54:04.006Z then exited; wake.retry to opencode 05:55:32.416Z; opencode claimed/launched 06:12:40.263/06:12:40.279Z. Queue-to-final-launch 1120.0 s (~18m40s), above the <=5 min target and worse than the ~481.8 s occurrence on issue-9c1fc3e3e668; two wake.launched rows for one wake id. Occurrence added by build-wait-reviewer.

## Agent suggestion

### 2026-11-25T06:19:04.310Z — issue-review-sweeper

Process proposal (issue-review-sweeper, 2026-11-25, evidence from issue-9aeec1fbae9d and 2026-11-25 occurrences): the repeated delay/avoidable-retry class is the synchronous `agent-wake.py run --once` (timeout=30) inside build_friction.wake_reviewer plus blind primary(primary codex)->fallback(opencode) retry. Measured: this issue waited 44,939.8s to a working adapter and had 2 launches for one wait_id; wake-47aeb34e7160 (wait game-dll-build:2026-11-25T02:11:23.680Z) had 2 launches and 594.16s queue-to-final-launch; other 2026-11-24 occurrences measured 654.97-1104.5s. Measurable target: over the next 5 fresh over-budget wait occurrences, p95 queue-to-launch <=300s and exactly one wake.launched per wait_id (zero duplicate launches). Smallest experiment: in a dev-loop wait test, enqueue the reviewer wake and return without calling run --once (or launch it detached with no join), letting the persistent agent-wake daemon drain exactly one healthy adapter; then compare wake.queued/wake.launched counts and queue-to-launch across 5 occurrences against the current baseline. This is the same defect already named here and in issue-9aeec1fbae9d, so no new issue is needed; keep ownership here.

## Agent suggestion

### 2026-11-25T21:32:43.916Z — build-wait-reviewer

Occurrence 2026-11-25 for wake-7af862b0e8f8 (issue-f1de1fcd87a2): one wake.queued at 20:52:21.242Z; codex claimed/launched 20:52:29.126/20:52:29.429Z then exited; adapter retry to opencode 21:02:10.130Z; opencode claimed/launched 21:21:41.942/21:21:42.278Z. Queue-to-final-launch ~1760.9s (~29m21s); two wake.launched rows for one wake id. Same late-primary-adapter-fallback class; no duplicate wake was queued. Reviewed by build-wait-reviewer.

## Agent suggestion

### 2026-11-25T21:36:30.781Z — build-wait-reviewer

New confirming occurrence (2026-11-25T21:3xZ, review of issue-2e5c794199f1), .dev/agent-wake/events.jsonl wake-69935de55b84: wake.queued 21:01:58; codex claimed+launched 21:02:10; adapter exited -> wake.retry 21:10:01 to opencode; opencode claimed+launched 21:21:41. One queued wake (acceptance met) but the working reviewer arrived ~19.7 min after queue, matching this issue's late-fallback pattern; retry sat ~11.7 min before a daemon drained it. Read-only; no fix claimed.

## Agent suggestion

### 2026-11-25T22:02:11.239Z — build-wait-reviewer

### 2026-11-25T21:5xZ — build-wait-reviewer
Occurrence 2026-11-25 for wake-25ef593488d5 (issue-0843a94eae4b): wake.queued exactly once at 21:26:52.309Z; primary adapter codex claimed/launched 21:26:56.505/56.581Z (pid 13272) then exited; wake.retry "previous adapter process exited; trying fallback" to opencode at 21:47:26.846Z; opencode claimed/launched 21:54:44.997/45.041Z (pid 32348). Queue-to-final-launch ~1672.7s (~27m53s), two wake.launched rows for one wake id. Exactly one wake was queued (acceptance met) but the working reviewer arrived ~28 min late via fallback. The waited-on attempt game-dll-20261125T212509367039Z-c77aed was healthy and succeeded (generation 648, total 189.141s), so this is dispatch latency, not a stalled build. Same codex-primary/late-opencode-fallback class. Journal .dev/agent-wake/events.jsonl. Reviewer build-wait-reviewer.

## Agent suggestion

### 2026-11-25T22:05:17.896Z — build-wait-reviewer

Dispatch occurrence on wake-52852b584468 (issue-63ec094b8859, wait game-dll-build:2026-11-25T21:45:58.563Z): wake.queued once at 21:47:09.435Z; codex claimed/launched 21:47:26.894/990Z (pid 28780) then exited; wake.retry to opencode at 21:54:45.068Z; working opencode launch 21:55:03.022Z (pid 19440), about 7m54s after queue. One wake record with a fallback launch, not a duplicate. Same late-codex-fallback class as issue-0843a94eae4b. Evidence: .dev/agent-wake/events.jsonl lines around WAKE_143eac.../WAKE_240dbd.../WAKE_a24e373d. No source edit made.

## Agent suggestion

### 2026-11-25T22:33:00.156Z — build-wait-reviewer

Occurrence (issue-bb51d96a1200, wake-88fb777780c8): queued 2026-11-25T21:54:34.570Z; codex claimed/launched 21:54:45.094Z/21:54:45.150Z (pid 30168); scheduler then logged "stale launched request recovered by scheduler" and retried with opencode at 22:29:01.006Z; opencode launched 22:29:01.956Z (pid 28388). That is ~34m16s between queue and the working reviewer launch even though the issue was created and the wake queued exactly once (requests/wake-88fb777780c8.json attempts=2). Reviewer: build-wait-reviewer.

## Agent suggestion

### 2026-11-26T00:14:36.218Z — build-wait-reviewer

Recurrence evidence (2026-11-26, wake-ec92bcb2152d, reviewer build-wait-reviewer), from issue-af48c43ce6c5 / wait game-dll-build:2026-11-26T00:03:44.421Z. Lifecycle in .dev/agent-wake/events.jsonl: wake.queued once 00:04:47.254Z; codex claimed/launched 00:04:48.885Z/00:04:49.015Z pid 31080; wake.retry "stale launched request recovered by scheduler" 00:06:21.150Z; opencode claimed/launched 00:06:22.288Z/00:06:22.335Z pid 36248 (this reviewer); attempts=2, two wake.launched rows for one wait_id -> duplicate-launch target miss (exactly-once queue held). Queue-to-final-launch 95.081s, within the <=5min target, so this occurrence does not reproduce the late-dispatch bound. launch_result misattribution reproduced: dev-loop.wait.reviewer-wake for this wait (logs/2026-11-25/20261125_234424/dev-loop/events.jsonl L183) embeds the previous wait's wake prompt (issue-4ab3b491ad6c / game-dll-build:2026-11-25T23:04:23.764Z), not wake-ec92bcb2152d - consistent with build_friction.py returning the tail of agent-wake.py run --once. The waited-on attempt game-dll-20261126T000344661706Z-d96bf3 was real work (vehicle-profile.h fanout; vehicle-system.cpp compile 73.453s) and succeeded at 100.563s, so this is dispatch telemetry, not a stalled build.

## Agent suggestion

### 2026-11-26T00:14:41.580Z — issue-review-sweeper

Occurrence (adds a total-failure datapoint to this late/duplicate-dispatch issue; reviewer=issue-review-sweeper, read-only): wake-4e70fc4a272b for issue-9215d710021d was queued exactly once at 2026-11-21T15:08:04.408Z (.dev/agent-wake/events.jsonl line 606). Sequence: codex claimed/launched 15:08:09.296/.906Z; wake.retry "previous adapter process exited; trying fallback" 15:20:49.434Z; opencode claimed/launched 15:26:34.601/.609Z; wake.failed "adapter process exited" 15:53:25.984Z. .dev/agent-wake/requests/wake-4e70fc4a272b.json: status=failed, attempts=2, error="adapter process exited". The issue's reviews stayed empty; the first review only arrived via the scheduled sweeper on 2026-11-26, ~5 days later. The adapter log and 2026-11-21 run journal are gone, so the failure cause is unrecoverable. This is the same class as the recorded late/duplicate fallback dispatches, but it adds the end state where BOTH adapters exit and the occurrence silently produces zero review.

Proposal: make terminal wake failure observable and self-healing rather than silent.
- On reaching terminal status=failed for a review wake, append exactly one occurrence/note to the linked issue (include wake id, attempts, last error) and re-queue the reviewer once through the same exactly-once path.
- Do not mark the issue reviewed; keep it claimable so a sweeper or idle agent can pick it up.
Measurable target: 100% of terminal wake failures publish exactly one issue-visible failure occurrence and one bounded re-queue within one retry window (<=300s); 0 linked over-budget issues remain with a failed wake and no issue-visible record for >3600s; still exactly one wake.queued per wait occurrence and one launched adapter after the retry (no new duplicate launches).
Smallest experiment: add the terminal-failure handler + single re-queue, then induce a scratch review wake whose primary and fallback adapters both exit nonzero; verify (a) one issue occurrence appears, (b) one re-queue is queued, (c) no duplicate wake.queued for the original wait_id, and (d) after the re-queue succeeds the issue shows a delivered review. Compare against this baseline occurrence (0 issue-visible records, 5-day gap). This is a proposal only; no fix claimed here.

## Occurrence

### 2026-11-26T01:39:32.142Z — build-wait-reviewer

New occurrence of the same late-dispatch mode (wake-56382fd7e40e; over-budget wait game-dll-build:2026-11-26T00:55:56.696Z; issue-ac23ab33d184). Sequence: wake.queued 00:56:59.578Z; primary codex claimed 00:57:03.161Z and launched pid 29932 at 00:57:03.213Z; codex failed with model_not_found "The 'gpt-6-luna' model is not supported when using Codex with a ChatGPT account" (wake log 2026-11-26T00-56-59.572Z lines 11-12); wake.retry 00:58:51.867Z switched to opencode; opencode did not claim/launch until 01:19:47.074/01:19:47.086Z (pid 30588). Queue-to-final-launch 1367.5s (22m47.5s), attempts=2, one wake.queued and no duplicate launch. The fallback was drained 2.86s after the next over-budget wait created issue-347500dfb032 at 01:19:44.214Z, i.e. no persistent daemon was draining the retried request. This is the same failure mode as the 2026-11-22 occurrence and misses the <=5 min target. No source fix claimed; no duplicate issue filed.

Evidence: {"attempts": 2, "fallback_adapter": "opencode", "fallback_claim_utc": "2026-11-26T01:19:47.074Z", "fallback_launch_utc": "2026-11-26T01:19:47.086Z", "issue": "issue-ac23ab33d184", "journal": ".dev/agent-wake/events.jsonl", "primary_adapter": "codex", "primary_failure": "model_not_found: gpt-6-luna not supported with a ChatGPT account", "primary_launch_utc": "2026-11-26T00:57:03.213Z", "queue_to_final_launch_seconds": 1367.508, "queued_utc": "2026-11-26T00:56:59.578Z", "request": ".dev/agent-wake/requests/wake-56382fd7e40e.json", "retry_utc": "2026-11-26T00:58:51.867Z", "trigger": "issue-347500dfb032 created 2026-11-26T01:19:44.214Z", "wake": "wake-56382fd7e40e"}

## Agent suggestion

### 2026-11-26T02:32:39.459Z — build-wait-reviewer

Occurrence 2026-11-26 (wake-793dc730ceba; over-budget wait game-dll-build:2026-11-26T01:35:04.053Z; issue-fa0da68ca2b3). Lifecycle in .dev/agent-wake/events.jsonl and requests/wake-793dc730ceba.json:
- wake.queued exactly once 2026-11-26T01:36:10.215Z (one request file references the issue).
- primary codex claimed/launched 01:36:11.920Z/01:36:11.938Z (pid 10284) — but no review was written.
- wake.retry "stale launched request recovered by scheduler" at 02:26:48.876Z (~50m38.7s after the primary launch) switched to opencode.
- opencode claimed/launched 02:26:50.693Z/02:26:50.742Z (pid 20656, this reviewer).
- attempts=2, two wake.launched rows for one wait_id (exactly-once queue held; one-launch target missed), queue-to-final-launch 3040.5s (~50m40.5s), far beyond the proposed <=5min bound.

The waited-on attempt game-dll-20261126T013504174400Z-8ca897 was healthy and succeeded (generation 664, 116.234s), so this is dispatch latency, not a stalled build. Same "stale launched request recovered by scheduler" + late-fallback class already recorded here. No fix claimed; no duplicate issue filed. Reviewer: build-wait-reviewer.

## Occurrence

### 2026-11-26T04:14:53.594Z — build-wait-reviewer

Fresh ~92 min queue-to-working-review data point (occurrence wait game-dll-build:2026-11-26T02:29:43.891Z, issue-5137be4d2f57). wake-bdaae328c6d8: wake.queued 2026-11-26T02:30:46.325Z; claimed+launched 02:30:50Z (codex); wake.retry 04:01:56Z; claimed+launched 04:02:45.520Z (opencode, this session). The first codex launch exited without producing a review, and the fallback was only retried/drained by a later trigger, so the review landed ~5520s after the wait. Matches this issue's codex-primary / no-daemon-drain mode. Evidence: .dev/agent-wake/events.jsonl, .dev/agent-wake/requests/wake-bdaae328c6d8.json.

Evidence: {"issue": "issue-5137be4d2f57", "queue_to_working_review_seconds": 5520, "wait_id": "game-dll-build:2026-11-26T02:29:43.891Z", "wake": "wake-bdaae328c6d8", "wake_queued_utc": "2026-11-26T02:30:46.325Z", "wake_retry_utc": "2026-11-26T04:01:56Z", "wake_working_review_utc": "2026-11-26T04:02:45.520Z"}

## Agent suggestion

### 2026-11-26T06:23:51.136Z — build-wait-reviewer

Occurrence from issue-a114980aee90 (wait game-dll-build:2026-11-26T06:07:55.877Z, run 20261126_055157). One wake.queued for wake-fc2eaee53560 at 2026-11-26T06:08:58.473Z; codex primary claimed/launched 06:09:01.728/06:09:01.753Z (pid 4732) but produced no review; wake.retry "stale launched request recovered by scheduler" 06:13:17.754Z switched to opencode; opencode claimed/launched 06:13:18.758/06:13:18.819Z (pid 18876). Queue-to-final-launch 260.3s (~4m20.3s), attempts=2, two wake.launched rows for one wait_id. The underlying build was actively compiling at the crossing and succeeded at 88.921s, so this is not a stalled build. Within the proposed <=5 min bound but confirms primary-codex-no-completion + fallback recovery.

## Agent suggestion

### 2026-11-26T06:25:56.297Z — issue-review-sweeper

Proposal from issue-review-sweeper (2026-11-26 sweep), measured target + smallest experiment, grounded in issue-121f867b430a review:

Evidence (observed): For over-budget wait issue-121f867b430a the single reviewer wake (wake-67f7ae30cfff) ended `status=failed / "adapter process exited"`, its log is missing, and NO review was appended for ~5 days until a scheduled sweep. The 5 currently-claimable sibling build-friction issues each have exactly 1 wake and it is `failed:1`. Across `.dev/agent-wake/requests` (647 files): failed=486 (484 "adapter process exited"), cancelled=156, launched=4, queued=1; build-wait-reviewer 259 failed vs 4 launched. Critically, a sweeper wake that DID append review msg-13a00ff6d98f at 06:11 is also labeled failed — so "failed/adapter process exited" conflates clean one-shot exit with crash. Separately, 13 game-dll-build wait issues were created on 2026-11-26 alone, so these wakes are high-volume.

Inference/uncertainty: Cannot tell from artifacts whether wake-67f7ae crashed or exited cleanly without output; exit code / terminal reason is not recorded. So two distinct defects may coexist: (a) mislabeled wake status, (b) real non-delivery. Unknown which dominates.

Measurable target: 
- 100% of wake requests record an adapter exit code and a terminal status that distinguishes `completed-clean` (exit 0) from `failed`; 
- 0 requests labeled `failed` solely because the one-shot adapter exited; 
- every over-budget wait issue reaches `reviewed` or an explicit `reviewed-noop` within 24h (currently this one took ~5 days).

Smallest experiment: instrument `devscripts/agent-wake.py` to persist the adapter process return code into the request JSON and map exit 0 to a non-failed terminal status. Then launch one build-wait-reviewer wake for a fresh wait (e.g. newest build-friction issue) and verify: (1) request shows the exit code, (2) a wake that appends a review is no longer `failed`, (3) the review or explicit no-op appears on the issue the same day. Compare the pre/post count of `failed/"adapter process exited"` for 5 wakes.

Cross-ref: this extends the existing late/failed-dispatch reporting here; the "exactly one wake queued" acceptance passed, so the gap is delivery/completion accounting, not duplicate wakes.

## Agent suggestion

### 2026-11-26T15:03:32.082Z — build-wait-reviewer

Occurrence (build-wait-reviewer, 2026-11-26, read-only): wait game-dll-build:2026-11-26T06:17:59.474Z -> issue-3385f308560f, wake wake-14d62006319a.

Positive: dispatch was NOT late this time. Crossing ~06:18:59.47Z, wake log 2026-11-26T06-19-02.266Z-build-wait-reviewer.log, dev-loop.wait.over_budget and dev-loop.wait.reviewer-wake both stamped 06:19:05.640Z; the reviewer was this session, so queue-to-launch was ~3s and no duplicate launch was observed for this wait_id. The wake journal shows one over_budget (created=true) and one reviewer-wake row.

Still defective: the reviewer-wake row fields.wake.launch_result again contains the full reviewer prompt text (this time correctly embedding wake-14d62006319a and issue-3385f308560f, so NOT the prior misattribution case), rather than a bounded launch result. Root remains build_friction.py wake_reviewer returning agent-wake.py run --once stdout. This shows the launch_result pollution persists even when attribution is correct; the journal row cannot be read as a compact launch result.

No fix claimed; no duplicate issue created.

## Agent suggestion

### 2026-11-26T15:52:23.378Z — issue-review-sweeper

**Process proposal from issue-review-sweeper (wake-479e90a636b1), 2026-11-26.**

Observed (distinct from the existing 39-min dispatch-latency case): build-wait issue-f995213a15b0's reviewer wake wake-02cf34c2ff22 was queued exactly once (15:26:33.515Z), launched codex (15:26:34.385Z), retried to opencode (15:34:08.140Z), launched opencode (15:53:25.942Z), then ended `wake.failed` at 16:07:34.018Z ("adapter process exited; reconciled by scheduler") with NO review appended for ~5 days. The wake DID reach `wake.launched` on the fallback yet still terminated with no completion signal, so this issue's "one wake.launched to a healthy adapter" criterion is insufficient to prove a review actually happened.

Measurable target: over a 7-day window, 0 over-budget wait reviewer wakes end in `wake.failed` with no issue review appended, and >=95% append a review within 10 min of wake.queued. Baseline from this case: review appended ~5 days later by a different scheduler.

Smallest experiment: add a terminal-completion predicate to the wake journal/scheduler — when a wake reaches `wake.failed`, or is claimed with no review within N minutes, re-queue once and record the outcome; then run one over-budget wait and verify the journal shows either a review append or exactly one bounded re-queue. I suggest adding a "review appended (not just launched)" completion clause to this issue's acceptance.

Cross-links: issue-f995213a15b0 (occurrence), issue-9a4503f6ed68 (budget/coalescing, already filed). No duplicate issue created.

## Agent suggestion

### 2026-11-26T17:25:37.715Z — build-wait-reviewer

Occurrence 2026-11-26 / issue-4fb7a24c54b0, wake-6f86e0fbb409: queued 16:57:27.591Z; codex claimed 16:57:38.649Z and launched pid 19112 at 16:57:38.674Z, then exited; wake.retry -> opencode 17:09:06.725Z; opencode claimed 17:10:10.167Z and launched pid 4136 at 17:10:10.197Z. One wake request for the issue; ~12m43s queue-to-effective-launch, same codex-primary-fails pattern.

## Agent suggestion

### 2026-11-26T17:40:17.274Z — build-wait-reviewer

Fresh late-dispatch occurrence (wake-04a89c03a141; wait game-dll-build:2026-11-26T17:13:10.456Z; issue-24863e1f595e). Sequence (.dev/agent-wake/events.jsonl lines 3571-3573, 3576, 3582-3583): wake.queued 2026-11-26T17:14:21.091Z; codex claimed/launched 17:14:28.226/28.346Z (pid 22576); wake.retry to opencode 17:21:54.669Z; opencode claimed/launched 17:35:26.922/27.010Z (pid 20612). One wake.queued, attempts=2, queue-to-working-launch 1265.9s (~21m06s), missing the <=5 min target. The fallback sat ~13.5 min after wake.retry, i.e. no persistent daemon drained it; same codex-primary/no-daemon mode as the recorded occurrences. No source fix claimed; no duplicate issue filed.

## Agent suggestion

### 2026-11-26T17:48:05.456Z — build-wait-reviewer

Occurrence (issue-a02e5a6588db review): wake-35c5c0492a2a queued once 2026-11-26T17:21:51.344Z; codex claimed/launched 17:21:55.154/17:21:55.294Z (pid 3516) then exited; wake.retry -> opencode 17:35:27.028Z; opencode claimed/launched 17:42:01.823/17:42:01.859Z (pid 33192). Queue-to-effective-launch ~1210.5s (~20m10s), attempts=2, one queued wake. Same late fallback-drain pattern; the review for this wait was only performed by the opencode fallback ~20 min later.

## Agent suggestion

### 2026-11-26T23:35:21.649Z — build-wait-reviewer

Recurrence (2026-11-26 23:06-23:29Z), same primary-codex-stall / opencode-retry pattern. Wake-497e6d80722f for issue-a42b497de2d4 was queued exactly once at 23:06:36.362Z. Primary adapter codex launched at 23:06:41.621Z (pid 31984) and never completed the request. Scheduler emitted wake.retry at 23:29:24.904Z and switched to adapter opencode, which claimed at 23:29:26.279Z and launched at 23:29:26.321Z (pid 2368). The reviewer therefore started about 23 minutes after the wait. Request file .dev/agent-wake/requests/wake-497e6d80722f.json shows attempts=2, adapter=opencode, launched_utc 23:29:26.321Z. Evidence: .dev/agent-wake/events.jsonl wake-497e6d80722f rows; logs/2026-11-26/20261126_230415/dev-loop/events.jsonl dev-loop.wait.reviewer-wake at 23:06:41.741Z. The wake was not duplicated at queue time, but dispatch took two adapters and ~23 min.

## Occurrence

### 2026-11-27T02:10:40.005Z — build-wait-reviewer

Occurrence for wait game-dll-build:2026-11-27T01:38:23.090Z / issue-e99701641a1c, wake-0e4507371e7c (.dev/agent-wake/events.jsonl). Exactly one wake.queued at 2026-11-27T01:39:29.622Z (exactly-once queue held). codex claimed 01:39:41.915 / launched 01:39:42.117 (pid 11340) then exited; wake.retry 'previous adapter process exited; trying fallback' 01:46:15.791; opencode claimed 01:55:03.186 / launched 01:55:03.253 (pid 34076). Queue-to-final-launch 333.631s (~5m33.6s), above the proposed <=5 min bound; two wake.launched events for one wait. requests/wake-0e4507371e7c.json attempts=2. launch_result wake-record misattribution could not be checked because the dev-loop wait event is missing for this occurrence (see issue-6e2d7b91e7d9). The underlying build game-dll-20261127T013823160277Z-8b3773 was real and succeeded (generation 750, 186.672s), so this is dispatch latency, not a stalled build. Reviewer build-wait-reviewer.

Evidence: {"attempts": 2, "build_attempt_id": "game-dll-20261127T013823160277Z-8b3773", "build_outcome": "success", "build_total_seconds": 186.672, "fallback_adapter": "opencode", "fallback_claimed_utc": "2026-11-27T01:55:03.186Z", "fallback_launched_utc": "2026-11-27T01:55:03.253Z", "fallback_pid": 34076, "issue": "issue-e99701641a1c", "launches_for_wait_id": 2, "primary_adapter": "codex", "primary_claimed_utc": "2026-11-27T01:39:41.915Z", "primary_launched_utc": "2026-11-27T01:39:42.117Z", "primary_pid": 11340, "queue_to_final_launch_seconds": 333.631, "retry_reason": "previous adapter process exited; trying fallback", "wait_id": "game-dll-build:2026-11-27T01:38:23.090Z", "wake": "wake-0e4507371e7c", "wake_queued_utc": "2026-11-27T01:39:29.622Z", "wake_retry_utc": "2026-11-27T01:46:15.791Z"}

## Agent suggestion

### 2026-11-27T02:27:27.407Z — build-wait-reviewer

Late-dispatch + misattribution occurrence (build-wait-reviewer) on wait game-dll-build:2026-11-27T01:44:56.218Z (issue-f6177f624ba9). wake-08578d19ed29 queued exactly once at 2026-11-27T01:46:04.240Z (.dev/agent-wake/events.jsonl line 3730). Primary codex claimed/launched 01:46:15.692/15.766Z pid 12000 then exited without a review; wake.retry switched to opencode 01:55:03.147Z; opencode claimed/launched 02:17:51.271/51.322Z pid 34420 (this session). Queue-to-final-launch 1907.08s (~31m47s), attempts=2, one wake.queued and no duplicate queue (exactly-once queue contract held). Misattribution also recurred: the dev-loop.wait.reviewer-wake event at 2026-11-27T01:46:16.187Z (journal logs/2026-11-27/20261127_014331/dev-loop/events.jsonl line 62) is stamped with wait_id game-dll-build:2026-11-27T01:44:56.218Z / issue-f6177f624ba9 but its launch_result embeds wake-c6fb7ac32356 (issue-dea21a9c722d, wait game-dll-build:2026-11-27T01:33:03.139Z), not wake-08578d19ed29, because build_friction.wake_reviewer's `agent-wake.py run --once` drains the oldest pending wake and returns that result. The waited-on build game-dll-20261127T014456458183Z-893a11 was healthy (success generation 751, total 166.0s), so this is not a stalled build. No source fix claimed.

## Occurrence

### 2026-11-27T07:59:02.858Z — build-wait-reviewer

Over-budget wait game-dll-build:2026-11-27T07:23:16.007Z (issue-778fe2977118) reproduced late reviewer dispatch plus a duplicate launch. wake-5377afd8a3ae: wake.queued 2026-11-27T07:24:17.449Z; codex claimed 07:24:28.556Z and launched pid 9236 at 07:24:28.649Z, then exited without producing a review; wake.retry 'previous adapter process exited; trying fallback' at 07:52:52.248Z; opencode claimed 07:53:02.722Z and launched pid 8608 at 07:53:02.749Z (request launched_utc 07:53:02.744Z). Queue-to-final-launch 765.3s (~12m45s), attempts=2 in requests/wake-5377afd8a3ae.json, and two wake.launched events for one wait_id; above the proposed <=5 min bound (the exactly-once queue held; the one-launch target did not). No wake-record misattribution: the dev-loop.wait.reviewer-wake row embeds the correct wake-5377afd8a3ae / pid 9236. Underlying attempt game-dll-20261127T072316157011Z-06c929 was real work and succeeded (generation 784, total 68.594s), so this is dispatch, not a stalled build. Reviewer: build-wait-reviewer.

Evidence: {"agent": "build-wait-reviewer", "attempts": 2, "build_attempt_id": "game-dll-20261127T072316157011Z-06c929", "build_outcome": "success", "build_total_seconds": 68.594, "fallback_adapter": "opencode", "fallback_claimed_utc": "2026-11-27T07:53:02.722Z", "fallback_launched_utc": "2026-11-27T07:53:02.749Z", "fallback_pid": 8608, "generation": 784, "issue": "issue-778fe2977118", "journal": ".dev/agent-wake/events.jsonl", "launches_for_wait_id": 2, "misattribution": false, "primary_adapter": "codex", "primary_claimed_utc": "2026-11-27T07:24:28.556Z", "primary_launched_utc": "2026-11-27T07:24:28.649Z", "primary_pid": 9236, "queue_to_final_launch_seconds": 765.3, "request_file": ".dev/agent-wake/requests/wake-5377afd8a3ae.json", "retry_reason": "previous adapter process exited; trying fallback", "wait_id": "game-dll-build:2026-11-27T07:23:16.007Z", "wake": "wake-5377afd8a3ae", "wake_queued_utc": "2026-11-27T07:24:17.449Z", "wake_retry_utc": "2026-11-27T07:52:52.248Z"}

## Agent suggestion

### 2026-11-27T08:00:29.981Z — issue-review-sweeper

Proposal from issue-review-sweeper's independent review of issue-194c538d7cbc (2026-11-27), refining this issue's still-open dispatch mode.

Gap the existing occurrences do not explain: today's wake-5377afd8a3ae did not fail fast. The primary codex adapter launched at 07:24:28.649Z (pid 9236) and only "exited" at 07:52:52.248Z, a ~28m24s hang, before wake.retry moved to opencode; final launch 07:53:02.749Z, queue-to-final-launch 765.3s. The 2026-11-22/11-26 occurrences describe a model_not_found fail-fast plus no daemon drain; this one is a slow/long-running primary attempt, so a fix aimed only at the model check would not bound it.

Measurable target: for one over-budget wait, if the primary adapter has not produced a review within a fixed per-attempt bound (proposal: <= 120s), wake.retry must be enqueued/executed and a working fallback launched, with at most one live adapter for the wake at any time. Success = queue-to-working-review <= 5 min (this issue's existing target), one wake.queued, and no two simultaneously-started adapters for one wait_id.

Smallest experiment (no policy invention): instrument adapter start/exit/heartbeat with a timestamp in the wake journal, then run one synthetic/manual wake whose primary adapter is deliberately hung (e.g. a sleep stub) and confirm wake.retry fires at the 120s bound and opencode launches without waiting for the process to exit. Compare against the recorded 765.3s from wake-5377afd8a3ae.

Evidence: `.dev/agent-wake/events.jsonl` L3898-3910 (wake-5377afd8a3ae queued 07:24:17.449Z, codex launched 07:24:28.649Z, wake.retry 07:52:52.248Z, opencode launched 07:53:02.749Z); `.dev/agent-wake/requests/wake-5377afd8a3ae.json` (attempts=2). No source change or fix claimed here; read-only review.

## Agent suggestion

### 2026-11-27T16:03:00.363Z — build-wait-reviewer

Occurrence 2026-11-27 (severe): wait game-dll-build:2026-11-27T07:55:43.159Z / issue-3736371ce4de on attempt game-dll-20261127T075543204684Z-cefc30, generation 787, a genuine 107.3s header-fanout build (not stalled). Wake wake-8af078e5e1a7 queued 07:56:44.337Z; codex claimed/launched 07:56:45.675Z/45.791Z (pid 23976) and delivered no review; wake.retry 'stale launched request recovered by scheduler' 15:53:48.099Z; opencode claimed/launched 15:53:49.013Z/49.034Z (pid 13200). Queue-to-actual-review ~7h57m, far above the <=5 min target. Exactly-once queue held; the launch was duplicated and only the fallback delivered. Reviewer: build-wait-reviewer; full evidence on issue-3736371ce4de.

## Agent suggestion

### 2026-11-27T17:55:35.046Z — build-wait-reviewer

Occurrence 2026-11-27: over-budget reviewer wake wake-83e438c02e22 for wait game-dll-build:2026-11-27T16:18:04.967Z (issue-4aacd4d05363). New mechanism observed for the late dispatch: wake.queued 16:19:06.299Z, but build_friction.wake_reviewer's synchronous `python agent-wake.py run --once` call uses subprocess timeout=30s (devscripts/build_friction.py) and does not catch TimeoutExpired. It timed out, so the dev-loop recorded dev-loop.wait.issue-failed at 16:19:36.384Z ("could not record over-budget wait") even though the issue and the wake request had already been created. The queued request then sat until codex claimed/launched 16:48:57.331/402Z; codex failed model_not_found ("gpt-6-luna" not supported with a ChatGPT account); wake.retry -> opencode 16:55:21.731Z; opencode claimed/launched 17:06:14.184/445Z. Queue-to-final-launch 2827.7s (~47m08s), attempts=2. Evidence: logs/2026-11-27/20261127_155356/dev-loop/events.jsonl line 81; .dev/agent-wake/events.jsonl for wake-83e438c02e22; request .dev/agent-wake/requests/wake-83e438c02e22.json. Smallest fix candidate: catch TimeoutExpired in wake_reviewer and treat the queued request as a successful dispatch (do not abort the over_budget/reviewer-wake bookkeeping), and/or start the drain detached so the 30s cap cannot kill it. No duplicate issue; no fix claimed.

## Agent suggestion

### 2026-11-27T17:56:15.424Z — build-wait-reviewer

Correction to my immediately preceding 2026-11-27 occurrence note: the `agent-wake.py run --once` 30s TimeoutExpired path is NOT new. This issue already recorded the same `dev-loop.wait.issue-failed` / "run --once timed out after 30 seconds" mechanism on 2026-11-24 (wait game-dll-build:2026-11-24T16:47:16.707Z, wake-742bed53da17, evidence at the 16:47 occurrence above). The 2026-11-27 wake-83e438c02e22 case is a recurrence of both known parts: (a) the 30s drain timeout leaving the request queued, and (b) codex primary model_not_found then a late opencode drain. No new root cause; no source fix claimed.

## Agent suggestion

### 2026-11-27T19:04:47.851Z — build-wait-reviewer

Occurrence (build-wait-reviewer, from review of issue-d17523339353): over-budget reviewer wake dispatched late again. wait_id game-dll-build:2026-11-27T17:56:33.046Z, issue-d17523339353, wake-538c984b697a. .dev/agent-wake/events.jsonl: wake.queued 2026-11-27T17:57:36.566Z (adapter codex); wake.launched codex 17:57:37.475Z (pid 27756); reached 18:34:04.733Z as wake.retry "previous adapter process exited; trying fallback" (opencode); opencode only claimed/launched 18:42:09.856Z (pid 8496). Effective delivery ~44.7 min after queue. Same shape as the 2026-11-22 sample: primary codex launch did not survive, fallback retry sat queued until a later run drained it. DM: at 17:57:37 the same run --once also launched older stale wake-ca3c45e6a7a8 (pid 20704, 17:57:37.535Z) for issue-6e15970df117. No source edit or build made.

## Agent discussion and suggestions

### 2026-11-27T19:11:12.749Z — build-wait-reviewer

Message: `msg-4bf34d025152` (note)

Subject: Occurrence: 30s run --once timeout + 54m45s dispatch delay (issue-fdbac689adea)

New occurrence for the late-dispatch defect, from wait game-dll-build:2026-11-27T17:46:16.304Z / issue-fdbac689adea (reviewed by build-wait-reviewer).

NEW MECHANISM EVIDENCE: the synchronous drain in devscripts/build_friction.py:244-246 (`agent-wake.py run --once`, timeout=30) raised TimeoutExpired with no guard. The dev-loop caught it at devscripts/dev-loop.py:3927-3931 and logged dev-loop.wait.issue-failed at 2026-11-27T17:47:55.167Z with error "Command [...agent-wake.py, run, --once] timed out after 30 seconds" and reason "could not record over-budget wait". The issue and wake WERE recorded (issue-fdbac689adea created 17:47:18.967Z; wake-53293a27a9c5 created 17:47:24.471Z, wake.queued 17:47:24.479Z), so the reason is misleading. Because the exception aborts lines 3913-3921, this occurrence has NO dev-loop.wait.over_budget or dev-loop.wait.reviewer-wake journal rows (only wait.issue-failed), so the issue's "dev-loop wait event plus issue record and reviewer wake record" proof is incomplete.

DISPATCH TIMELINE (wake-53293a27a9c5.json, attempts=2, .dev/agent-wake/events.jsonl):
- wake.queued 17:47:24.479Z (exactly once)
- codex claimed 17:57:37.324Z / launched 17:57:37.404Z pid 30232 (drained ~10m after queue by the NEXT over-budget wait, not by its own run --once)
- wake.retry -> opencode 18:34:04.697Z "previous adapter process exited; trying fallback"
- opencode claimed 18:42:09.705Z / launched 18:42:09.779Z pid 31952
- queue-to-final-launch 3285.3s (~54m45s); two wake.launched events for one wait (exactly-once queue held, single-launch target did not)
- same 30s run --once timeout recurred 4x in this run: 17:47:55, 18:18:16, 18:22:42, 18:34:04

Underlying attempt game-dll-20261127T174616512286Z-7470c4 was real work and failed on a genuine compile error after 440.8s (engine-tick-camera.cpp sVehicleCamera), so this is dispatch/journaling, not a stalled build. Suggested owner fix (not implemented by me): guard TimeoutExpired and still write the wait events, or launch the reviewer asynchronously. Cross-links: issue-9a4503f6ed68, issue-6e2d7b91e7d9.

## Agent suggestion

### 2026-11-27T20:34:45.082Z — build-wait-reviewer

Occurrence (recurrence of this dispatch defect), observed by build-wait-reviewer for wait issue-796a7430913c:
- Wake wake-dda0fc4bb16b: wake.queued 2026-11-27T18:22:12.063Z; codex claimed/launched 18:42:10.202/18:42:10.246Z pid 16940 -- i.e. drained ~20 min later by the NEXT over-budget wait's run --once, not its own (its own run --once had timed out at 18:22:42, one of 4 such 30s timeouts in run 20261127_174537).
- wake.retry to opencode 18:47:45.506Z ("previous adapter process exited; trying fallback"); opencode claimed/launched only 20:29:58.074/20:29:58.092Z pid 9344.
- Queue-to-final-launch 18:22:12 -> 20:29:58 = 7666s (~2h07m46s); two wake.launched events for one wait.
Same mechanism as the issue summary: no persistent agent-wake daemon drains a retried fallback; it waits for the next run --once. Evidence: .dev/agent-wake/events.jsonl lines for wake-dda0fc4bb16b; dev-loop journal logs/2026-11-27/20261127_174537/dev-loop/events.jsonl; request .dev/agent-wake/requests/wake-dda0fc4bb16b.json (attempts=2, created 18:22:12.059Z, launched 20:29:58.089Z). Suggest prioritizing a persistent wake daemon (or draining due retries on every loop tick) so a queued fallback cannot sit for hours. No code fix claimed here.

## Agent suggestion

### 2026-11-28T08:28:59.319Z — build-wait-reviewer

Occurrence 2026-11-28 (build-wait-reviewer, this run, review on issue-30a5dbfd79be).
Over-budget wait game-dll-build:2026-11-28T07:11:57.814Z reproduced late + duplicate launch. wake-63b15223f7f1 queued exactly once 07:13:07.053Z (.dev/agent-wake/events.jsonl); no daemon drained it until codex claimed/launched 07:41:06.083Z/07:41:06.107Z (28 min idle), then exited; wake.retry 07:57:36.925Z switched to opencode, which claimed/launched 08:20:38.266Z/08:20:38.291Z. requests/wake-63b15223f7f1.json attempts=2. Queue-to-final-launch ~4051s (~67.5 min), far above the <=5 min target, with two wake.launched events for one wait (the exactly-once queue held; the one-launch target did not). The inline run --once also timed out at 30s (dev-loop.wait.issue-failed, logs/2026-11-28/20261128_061051/dev-loop/events.jsonl:189), so the wait over_budget/reviewer-wake rows are missing for this occurrence. Waited-on attempt game-dll-20261128T071157863196Z-8d63e6 was real work (failed at link at 416.813s), not a stall.

## Agent suggestion

### 2026-11-28T17:56:30.636Z — build-wait-reviewer

Occurrence 2026-11-28T17:34:21.168Z (issue-6ed1b2897c5b): wake-6fde8872c1a3 queued exactly once at 17:35:24.330Z; codex claimed/launched 17:35:26.086/26.176Z (pid 21068) then exited; wake.retry 'previous adapter process exited; trying fallback' 17:41:55.769Z; opencode claimed/launched 17:42:46.163/46.187Z (pid 24200). Queue-to-final-launch 441.857s, attempts=2, two wake.launched events for one wait_id. Launch-result misattribution: dev-loop.wait.reviewer-wake at 17:35:26.281Z is stamped wait_id game-dll-build:2026-11-28T17:34:21.168Z / issue-6ed1b2897c5b but embeds wake-7f80ca3ba980 (issue-136d4553ff92, issue-review-sweeper), not wake-6fde8872c1a3; build_friction.py:250 stdout[-2000:] drains the oldest pending wake. The waited-on attempt game-dll-20261128T173421264252Z-3b08fd was real work (77.641s) but failed at link on undefined MimitaNet::handleSpyKnifeHitClaim (cache fault, REG-2026-11-27), so this is dispatch/telemetry, not a stalled build.

## Agent suggestion

### 2026-11-28T18:36:56.446Z — build-wait-reviewer

Evidence-backed suggestion from build-wait-reviewer (wake-fef2e8967edc) on the synchronous reviewer-wake launch in the over-budget wait path.

New occurrence (2026-11-28, run 20261128_171225): wait_id game-dll-build:2026-11-28T18:00:34.216Z, elapsed 63.662s.
- Issue issue-13b902990adc WAS created at 18:01:38.716Z (state new).
- But the journal for the same wait_id shows dev-loop.wait.issue-failed at 18:02:12.828Z: "agent-wake.py run --once timed out after 30 seconds", and there is NO dev-loop.wait.over_budget event for that wait_id.
- Cause path: build_friction.wake_reviewer runs `agent-wake.py run --once` synchronously with timeout=30 inside dev-loop.check_wait_budget; on TimeoutExpired the outer except records issue-failed and never reaches reported_wait_ids.add, and the over_budget success event (which is emitted after the wake block) is skipped.
- Consequences: (1) a 30s block of the dev-loop wait check; (2) the occurrence is journaled as a failure even though the issue exists; (3) reported_wait_ids stays unset, so a later check can re-enter record_wait_incident (occurrence append) and re-invoke wake_reviewer for the same wait_id, a latent duplicate-wake path that violates "wake queued exactly once".

Smallest useful next check (proposal, not proven): emit the dev-loop.wait.over_budget event and add wait_id to reported_wait_ids as soon as record_wait_incident returns, before launching any wake; and make the wake dispatch non-blocking (short timeout, detach, or a persistent daemon) so a slow/queued adapter cannot stall the check loop or mask a created issue. Related background: issue-84c584957e65 currently covers codex-adapter/no-daemon dispatch delay; this adds the in-process synchronous-timeout/order variant. Please review and agree the smallest next check; no build, run, or fix is claimed here.

## Occurrence

### 2026-11-28T18:37:10.027Z — build-wait-reviewer

Over-budget wait game-dll-build:2026-11-28T18:00:34.216Z (issue-13b902990adc) reproduced late reviewer dispatch plus a duplicate launch. wake-1dbab16fcd3b queued exactly once at 18:01:42.600Z; codex claimed/launched 18:08:28.049/18:08:28.131Z (pid 30516) then exited; wake.retry "previous adapter process exited; trying fallback" 18:12:23.833Z; opencode claimed/launched 18:31:34.693/18:31:34.718Z (pid 26348). Queue-to-final-launch 1792.118s (~29m52.1s), attempts=2, two wake.launched events for one wait_id - above the <=5 min bound (the exactly-once queue held; the one-launch target did not). The waited-on build game-dll-20261128T180034349425Z-3007f5 was real compile work and succeeded (generation 825, total 134.187s, completed 18:02:48.537Z), so the reviewer launched ~1726s after the build was already done; this is dispatch, not a stalled build. No wake-record misattribution observed: the reviewer-wake event is missing for this wait (covered by issue-9a4503f6ed68). Reviewer: build-wait-reviewer.

Evidence: {"attempts": 2, "build_attempt_id": "game-dll-20261128T180034349425Z-3007f5", "build_outcome": "success", "build_total_seconds": 134.187, "fallback_adapter": "opencode", "fallback_claimed_utc": "2026-11-28T18:31:34.693Z", "fallback_launched_utc": "2026-11-28T18:31:34.718Z", "fallback_pid": 26348, "issue": "issue-13b902990adc", "journal": ".dev/agent-wake/events.jsonl", "launches_for_wait_id": 2, "primary_adapter": "codex", "primary_claimed_utc": "2026-11-28T18:08:28.049Z", "primary_launched_utc": "2026-11-28T18:08:28.131Z", "primary_pid": 30516, "queue_to_final_launch_seconds": 1792.118, "retry_reason": "previous adapter process exited; trying fallback", "wait_id": "game-dll-build:2026-11-28T18:00:34.216Z", "wake": "wake-1dbab16fcd3b", "wake_queued_utc": "2026-11-28T18:01:42.600Z", "wake_retry_utc": "2026-11-28T18:12:23.833Z"}
