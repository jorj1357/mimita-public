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

## Scheduled independent review

### 2026-11-23T08:34:39.665Z — mode3-live-dll-test

A bounded independent review was queued as wake wake-02e62d408ad8 after 378 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (verified)

### 2026-11-23T08:46:05.159Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review by issue-review-sweeper (wake wake-02e62d408ad8). No source edits, no build, no fix claim.

Observed facts (direct):
- Readiness: issue-6413477d361d is ready=true, score 8/8, missing=[], state=in_review; paths devscripts/dev-loop.py, devscripts/build_friction.py, devscripts/agent-wake.py.
- Proof: `agent-coord proof list` for this issue returns []; no proof task is attached. The prior review (build-wait-reviewer, 2026-11-20T15:57:52Z, verdict verified) already established the documented acceptance: required fields present (wait_id game-dll-build:2026-11-20T14:31:50.671Z, stage game_build, elapsed 65.09s, build_id, request_ids) and reviewer wake wake-34db0f2241ed queued exactly once.
- This wake: .dev/agent-wake/events.jsonl has exactly one wake.queued for wake-02e62d408ad8 at 2026-11-23T08:34:39.639Z, then claimed 08:34:40.259Z and launched 08:34:40.301Z.
- Original attempt artifact .dev/build-timings/game-dll-20261120T143150893302Z-296fc2.json: success=true, generation 401, started 14:31:50.893Z, completed 14:44:44.551Z, total_seconds 773.631; worker_metrics compiled 15, skipped 627, object_scan_seconds 451.381, compile_seconds 139.136, link_seconds 35.474. At the 65.09s flag point the build was in the manifest_validation prelude, not stalled; it later completed successfully.
- Current checked-out source still applies one flat total-elapsed threshold to game-dll builds: WAIT_ISSUE_SECONDS=60 (dev-loop.py:109-110) tested against elapsed-since-start for stage game_build (dev-loop.py:3744-3760). No per-stage budget and no progress/throughput test.
- Dedupe key is the exact wait_id (build_friction.py:131-140, 143-169); a fresh build yields a fresh wait_id and therefore a new issue even with identical kind/stage/reason.
- Recurrence: 46 non-terminal issues currently share the exact title "Wait over budget: game-dll-build at game_build" (39 in_review, 6 new, 1 assigned), first created 2026-11-20T08:32:55.917Z, last 2026-11-22T23:53:23.260Z. A sibling (issue-dd54a79216ad) already records the flood mechanism and a not-a-stall verdict.

Inference / uncertainty:
- The flag is a threshold false positive at the crossing instant, but the build was genuinely slow in absolute terms (773.6s, dominated by a 451.4s object-cache scan), so "wait exceeded budget" is real and belongs to the build-friction parent, not a product stall.
- Static-source evidence only: I did not restart or inspect the live dev-loop daemon, so I cannot assert the running process matches HEAD.
- I did not re-derive wake-34db0f2241ed exactly-once from the raw log in this sweep; I relied on the prior review's line-level citation plus my own new wake being exactly once.

Contrary / limiting evidence:
- Newest build in .dev/build-timings, game-dll-20261123T083904326142Z-3f61fe, total_seconds 30.463 for a single-TU edit, well under 60s. The flag does not fire on every build; frequency scales with edit size and object-scan cost (generation 550, 15 changed TUs, ~75s total).

Next discriminating check (read-only):
- Replay every .dev/build-timings/game-dll-*.json: for each completed attempt, compute the elapsed at the first 60s crossing and whether compile/link output was produced afterward. Classify healthy-progress false positive versus genuine stall. Target: state how many of the 46 identical-title issues were false positives and whether any represented a real hang.
