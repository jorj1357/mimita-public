# issue-84c584957e65: Over-budget reviewer wakes dispatch late: codex primary adapter fails model check and no daemon drains the fallback

## Human report (verbatim, immutable)

> An over-budget wait review reached the reviewer ~39 minutes after the wait. Sequence (`.dev/agent-wake/events.jsonl`, wake-26c4ad5a6f09): wake.queued 2026-11-22T12:13:32.440Z; primary adapter `codex` launched 12:13:34.154Z; codex exited ~12:14:52Z with model_not_found ("The 'gpt-6-luna' model is not supported when using Codex with a ChatGPT account"); wake.retry 12:34:35.056Z switched the request to fallback `opencode`; opencode only claimed/launched at 12:52:39.040Z because no persistent agent-wake daemon was running and the retried request sat queued until the next over-budget wait's `run --once` drained it. See review of issue-f1c6106a580d.


## Occurrence

### 2026-11-22T14:48:12.771Z — build-wait-reviewer

Over-budget wait game-dll-build:2026-11-22T13:57:36.293Z (issue-40ebd14d5cf4) reproduced the late-dispatch behavior: one wake.queued for wake-a37dc2879375 at 13:58:39.668Z; codex primary claimed/launched 13:58:41.859Z then exited; wake.retry switched to opencode 14:22:49.103Z; opencode claimed/launched 14:24:02.589Z/14:24:03.211Z (about 25.4 minutes after the wait). Build attempt game-dll-20261122T135736380483Z-d10b1b was still compiling/linking at the 60.9s mark and finished successfully at 118.0s, so this is not a stalled build.

Evidence: {"build_attempt_id": "game-dll-20261122T135736380483Z-d10b1b", "build_total_seconds": 118.006, "fallback_adapter": "opencode", "fallback_launched_utc": "2026-11-22T14:24:03.211Z", "issue": "issue-40ebd14d5cf4", "journal": ".dev/agent-wake/events.jsonl", "primary_adapter": "codex", "primary_launched_utc": "2026-11-22T13:58:41.859Z", "total_dispatch_delay_seconds": 1523.543, "wait_id": "game-dll-build:2026-11-22T13:57:36.293Z", "wake": "wake-a37dc2879375", "wake_queued_utc": "2026-11-22T13:58:39.668Z", "wake_retry_utc": "2026-11-22T14:22:49.103Z"}
