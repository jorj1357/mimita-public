# issue-0612aaee5a6e: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (record-and-wake-contract-confirmed; underlying-timeout; reason-malformed; dispatch-late)

### 2026-11-25T02:36:21.291Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer (wake-47aeb34e7160). Read-only: no source edited, no build/run started, no fix claimed, no proof recorded.

**Occurrence.** wait_id game-dll-build:2026-11-25T02:11:23.680Z at stage game_build, elapsed 60.103 s vs 60.0 s budget; request 2026-11-25T02:11:23.582Z-35c4270a:mode3:game; source_generation 67; attempt game-dll-20261125T021123740140Z-5e8bdb (in-progress generation 633). Journal logs/2026-11-24/20261124_221014/dev-loop/events.jsonl lines 557-558 record dev-loop.wait.reviewer-wake and dev-loop.wait.over_budget at 2026-11-25T02:12:41.013Z with created=true.

**Acceptance check.**
- wait_id, stage, elapsed, build/run IDs, evidence: present.
- reason: present but unusable (defect 1).
- "wake queued exactly once for this occurrence": CONFIRMED. This journal has exactly one over_budget and one reviewer-wake row for this wait_id (6 over_budget + 6 reviewer-wake rows overall, 0 issue-failed), and wake-47aeb34e7160 is its only queued wake.
- Wake identity: the reviewer-wake row's launch_result embeds wake-47aeb34e7160 / pid 14944, so there is no wake-record misattribution for this occurrence.

**Defects (already tracked; not fixed here).**
1. evidence.reason is a mid-JSON tail: '8273.352,"build_attempt_id":"game-dll-20261125T021123740140Z-5e8bdb",...,"record_overhead_ms":4.489}'. Root cause dev-loop.py:3112/3142 detail=line.rstrip()[-240:] overwritten by each worker line, read at dev-loop.py:3892 reason=self.game_build_detail. Tracked in issue-6e08d89b55e3 (occurrence appended here).
2. Late + duplicate dispatch: codex claimed/launched 2026-11-25T02:12:40.866/.932Z (pid 14944) then exited; wake.retry to opencode 02:21:12.067Z; opencode claimed/launched 02:22:20.392/.420Z (pid 12324); requests attempts=2, two wake.launched for one wait_id. Queue-to-final-launch 594.16 s (~9m54s), above the <=5 min target. Tracked in issue-84c584957e65 (occurrence appended here).

**What the wait actually was.** Not a stalled healthy build: attempt 5e8bdb hit worker supervision at 2026-11-25T02:16:32.288Z with stage worker_supervised state=failed reason=timeout timeout_seconds=300.0 (elapsed 308488.82 ms). The 60 s wake therefore fired during a large full rebuild that then exceeded the 300 s worker timeout; the next attempt published generation 634 in 180.66 s at 02:24:18Z. The 60 s budget sitting below normal full-build time is already raised on issue-7d3dcfd7433b, and ~50 "Wait over budget: game-dll-build" issues remain open, so this occurrence adds to an existing volume/noise problem rather than a new failure mode.

**Next discriminating check (smallest).** For the dev-loop.py owner: emit the active sub-stage and attempt id in wait evidence, and derive reason from structured stage state instead of raw trailing worker output; evaluate a per-kind budget or N-occurrence throttle. Then either record a reasoned exemption for the known noise or attach a runtime_proven proof. No code changed by this review.
