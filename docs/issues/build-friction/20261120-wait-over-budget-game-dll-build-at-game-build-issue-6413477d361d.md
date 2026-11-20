# issue-6413477d361d: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (verified)

### 2026-11-20T15:57:52.681Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Wake-delivery verification for this wait occurrence (bounded activation, no source edits or builds):

- Issue fields satisfy acceptance: wait_id `game-dll-build:2026-11-20T14:31:50.671Z`, stage `game_build`, reason (manifest_validation stage elapsed 59623.686 ms), elapsed 65.09s, build_id `2026-11-20T07:47:09.981634Z`, request_ids `[2026-11-20T14:31:47.379Z-3b2ad217]`, source_generation 0.
- Reviewer wake `wake-34db0f2241ed` was queued exactly once: one `wake.queued` event at 2026-11-20T14:32:56.152Z in `.dev/agent-wake/events.jsonl`.
- Delivery was proven end to end: codex adapter launched 14:32:56.933Z, exited, `wake.retry` switched to opencode 14:45:53.351Z, and opencode launched 14:54:22.346Z (child pid 12144 -> this session). Environment carried MIMITA_WAKE_ID=wake-34db0f2241ed and MIMITA_WAKE_ISSUE=issue-6413477d361d.
- `agent-coord.py sync` returned a valid brain_check and `observe` resolved this issue, so the reviewer activation has working coordination context.
- No duplicate friction issue was created for the same wait; the retry reused the same wake record (attempts=2).
