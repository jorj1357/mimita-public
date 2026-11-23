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
