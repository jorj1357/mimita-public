# issue-c632de44f8d6: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-build-friction)

### 2026-11-21T21:58:36.090Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer (independent bounded review)

Verdict: confirmed real wait, not a false alarm; not a queue/worker stall.

Evidence (exact journal, .dev/build-timings):
- Wait occurrence: wait_id game-dll-build:2026-11-21T21:40:24.143Z, elapsed 60.566s, attempt game-dll-20261121T214024518362Z-449ec0, generation 432.
- That attempt was a no-op build (no_op=true, 0 compiled, 630 skipped) yet still took 98.895s wall:
  - manifest_snapshot 48,146ms (21:40:30.158 -> 21:41:18.514)
  - manifest_validation 33ms
  - source_snapshot 24,252ms (21:41:19.635 -> 21:41:44.164)
  - codegen packet_registry 3,458ms; resident_state 695ms
  - worker_input_validation 9.7ms; initial_source_coherence 85ms; toolchain_setup 27ms; object_cache_scan 1,062ms; no_op 4.7ms
  The 60s budget is crossed inside game_build before the worker even starts, dominated by manifest_snapshot + source_snapshot + codegen.
- Next automatic attempt game-dll-20261121T215015817067Z-d501fb (generation 433, real change, non-no-op) took 395.54s:
  - manifest_snapshot 137,377ms; source_snapshot 26,382ms
  - unexplained 43.4s gap between source_snapshot completion (169,849ms) and worker_input_validation start (213,176ms)
  - changed_source_preflight 36,386ms; object_cache_scan 4,903ms
  - compile 40,592ms; link 61,980ms; publish 29,101ms; coherence_validation 1,103ms
  So the over-budget wait recurs on every build, not just this occurrence.

Root cause owner: this is the same systemic pre-worker cost tracked by issue-136d4553ff92 ("Account for every DLL build stage...", owner build-coordinator), open tasks dll-build-stage-accounting and dll-snapshot-cause-and-incremental-index. The 60s wait is a correct symptom; widening the budget would hide a real regression, so the fix belongs in stage cost reduction, not in the threshold.

Evidence-integrity defect found while reviewing (actionable):
- In attempt 449ec0's summary, no_op=true/compiled=0/wall=98.895s, but worker_metrics still reports the previous compile's numbers: compile_seconds 32.645, compiled 12, worker_total_seconds 143.658, total_seconds 170.04 (these equal generation-432's 20:14 attempt). This misattributes time and undercuts the umbrella issue's "account for every stage" goal. The per-attempt no-op summary should clear or recompute worker_metrics, or copy only metrics belonging to the current worker invocation.
- In attempt d501fb, publish phase is 29,101ms in the dev-loop but worker_metrics.publish_seconds is 0.497, so published-stage time is largely unattributed.

Duplicate-issue policy: this is the ~10th identical "game-dll-build at game_build" build-friction record today (e.g. issue-c3ecfab72cba 21:38, issue-1b08300935ae 21:51, plus earlier occurrences). build_friction.record_wait_incident intentionally creates one issue per wait_id even for identical stage/reason, and its own risk field flags noisy duplicates. Recommend either one rolling occurrence issue per (kind, stage) signature with occurrences appended, or suppress/link new issues to issue-136d4553ff92 while it is open. No new duplicate issue created by this review.

Acceptance review: the issue record already carries wait_id, stage, reason, elapsed, build IDs, and evidence, and the reviewer wake fired once; the record is complete. No behavior task here, so no runtime proof is claimed. Recommend linking this issue to issue-136d4553ff92 and closing it as a duplicate symptom once that root cause is owned and tracked.
