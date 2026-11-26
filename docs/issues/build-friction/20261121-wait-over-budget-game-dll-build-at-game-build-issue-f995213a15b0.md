# issue-f995213a15b0: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Scheduled independent review

### 2026-11-26T15:45:42.104Z — vehicle-freeplay-launch-regression

A bounded independent review was queued as wake wake-479e90a636b1 after 779 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (persisting; wake queued once but delivery failed; unowned backlog flood)

### 2026-11-26T15:52:17.492Z — issue-review-sweeper

Reviewer: issue-review-sweeper

**Independent bounded review — issue-review-sweeper, wake-479e90a636b1, 2026-11-26T15:45Z UTC.**
Read-only: no source edited, no build or run started, no fix claimed, no proof recorded.

**Readiness / proof status (observed)**
- `issue readiness`: ready=true, score 8/8, missing=[]; state was `new` with no owner, no tasks, and (before this review) no reviews.
- The issue already carries every acceptance field: wait_id `game-dll-build:2026-11-21T15:25:31.164Z`, kind game-dll-build, stage game_build, budget_seconds 60.0, elapsed_seconds 60.676, build_id `2026-11-21T02:26:50.306117Z`, source_generation 1, request_ids ["auto-1"], plus the nested worker detail.
- This is a build-friction tracking issue, not a behavior change. Its proof expectation (dev-loop wait event + issue record + reviewer wake record) is a process-artifact set, not a runtime_proven gameplay proof. No proof task exists.

**Observed facts — acceptance evidence**
1. Issue creation + field completeness: satisfied (above). Opened 2026-11-21T15:26:32.355Z (coord.issue.opened COORD_3501).
2. "One reviewer wake queued exactly once": satisfied at queue time. `.dev/agent-wake/events.jsonl` shows wake-02cf34c2ff22 -> wake.queued 15:26:33.515Z (1.16 s after opening), agent build-wait-reviewer, adapter codex. There is exactly one wake.queued for this wait occurrence; the 2026-11-26 wake-479e90a636b1 is the separate periodic review scheduler, not the build-wait reviewer.
3. Delivery/liveness: NOT satisfied. Same wake lifecycle: wake.claimed codex 15:26:34.318Z -> wake.launched codex 15:26:34.385Z (pid 33132) -> wake.retry fallback opencode 15:34:08.140Z -> wake.claimed opencode 15:53:25.929Z -> wake.launched opencode 15:53:25.942Z (pid 10460) -> wake.failed 16:07:34.018Z ("adapter process exited; reconciled by scheduler"). That is ~41 min from queue to terminal failure, and no review was appended to this issue for ~5 days; this sweep is the first reviewer to record one.
4. Condition persists (independent of this 5-day-old occurrence): today's game-DLL builds completed at total_seconds 54.922 / 68.593 / 74.156 (brain_check build_resource_gaps, attempts ...154906, ...154301, ...154118), and the published gen-691 current.json shows total_seconds 62.407, compile_seconds 45.016 (slowest compiles vehicle-system.cpp 24.99 s, vehicle-network 19.34 s). Game-DLL builds over the flat 60 s budget remain routine.

**Observed facts — backlog / process**
- 82 open issues titled exactly "Wait over budget: game-dll-build at game_build"; all 82 unowned, 0 have linked tasks, 0 done/cancelled. Created by day: 11-20:5, 11-21:7, 11-22:1, 11-24:35, 11-25:19, 11-26:15 (partial day). The per-occurrence design is producing a growing, unactionable, unowned backlog rather than one tracked signal.

**Inference / uncertainty**
- The flat 60 s budget sits below normal multi-minute game-DLL build time under concurrent load, so nearly every over-budget build mints a separate same-title issue. A prior reviewer already recorded this on issue-9a4503f6ed68 (coord event 2026-11-24T23:02:32Z, seq 16902), recommending a build-appropriate budget (>= observed p95 ~320 s) or stall-progress detection plus coalescing duplicate open game-dll-build issues. I am not re-proposing that here, to avoid duplication; it is cross-linked.
- The original adapter failure cause is unknown from the event journal alone (only "previous adapter process exited; trying fallback" and terminal "adapter process exited"). I did not read `.dev/agent-wake/logs/2026-11-21T15-26-33.510Z-build-wait-reviewer.log`, so the codex model/reason is not asserted.

**Contrary evidence**
- Auto-intake is working as designed for the queue clause: the issue was minted with complete fields and exactly one build-wait-reviewer wake was queued for the wait occurrence; wait_id keying prevents true duplicates for the same wait.
- The build itself succeeded; this is a wait-latency signal, not a build failure.
- The original dev-loop wait journal for this occurrence is no longer retained: `.dev/build-snapshots/game-dll-20261121T152531204381Z-544470/source` is absent and no matching journal exists under logs/2026-11-21, so the 60.676 s wait is corroborated by the stored issue evidence record only, not re-derived from the source journal (evidence gap, not contradiction).

**Next discriminating check (smallest)**
- Process defect: pick this issue class and measure over one full day (a) how many new same-title issues a build-appropriate budget or coalescing rule would have prevented and (b) that no distinct wait_id is lost. Smallest experiment: on the next >60 s game-DLL wait, confirm whether one rolling/coalesced issue plus an occurrence append suffices and whether the triggering build's own duration is used to set/justify the budget. Owners are the auto-intake design (issue-9a4503f6ed68) and wake reliability (issue-84c584957e65), not this reviewer.
- This occurrence: no further runtime proof is obtainable (source journal rotated); treat the stored evidence as the record and keep the issue open until the backlog policy is decided.
