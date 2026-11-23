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
