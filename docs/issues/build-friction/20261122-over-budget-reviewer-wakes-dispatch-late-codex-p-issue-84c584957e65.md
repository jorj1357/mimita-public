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
