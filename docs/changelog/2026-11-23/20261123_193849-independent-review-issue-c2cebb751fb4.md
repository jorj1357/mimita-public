# Independent review: over-budget game-DLL wait issue-c2cebb751fb4

**Date:** 2026-11-23 UTC
**Agent:** issue-review-sweeper
**Wake:** `wake-a33d0b4713e3` (scheduled issue review, sweep-439)
**Issue:** `issue-c2cebb751fb4` — "Wait over budget: game-dll-build at game_build"
**Reviewed occurrence:** `game-dll-build:2026-11-20T18:27:26.113Z`
**In-flight attempt:** `game-dll-20261120T182726149737Z-df8100` (generation 409)

## Result

Read-only bounded review: no source edits, no build, no lease, no close, no owner
claim. Appended review id 293 (verdict confirmed-build-wait); issue moved to
`in_review`.

## Findings

- Wait was a true positive, not a policy false positive. At the 61.047 s trigger
  the attempt was ~27 s into `changed_source_preflight`, compiling its first
  three changed TUs. The build succeeded later (total 2247.6 s, ~37.5 min).
- `build/run IDs` acceptance is only partially met: the evidence records
  `build_id`/`generation` from `self.latest_build` / `self.game_build_generation`
  (a prior build), not the in-flight attempt df8100 / generation 409. The
  `request_ids` and `source_generation` do match. Same defect already on
  `issue-9a4503f6ed68`.
- Reviewer wake was queued exactly once (`wake-2ed65f7d688f`, 18:28:28) but
  failed (codex exit -> opencode retry -> launched 19:43:26 -> `wake.failed`
  "adapter process exited" 20:19:28). No review was produced; the issue sat
  `new` ~3 days until this sweep.
- Proof expectation is 2/3 available: issue record and wake record exist; the
  2026-11-20 dev-loop `events.jsonl` wait row is not retained (only the
  build-timings timeline survives).

## Process review

The two process gaps seen here (failed reviewer wake with no review; duplicate
same-signature wait issues) are already tracked on `issue-9a4503f6ed68` with
measurable targets and smallest experiments. No duplicate proposal was added;
this occurrence is additional confirming evidence.

## References

- `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-c2cebb751fb4.md`
- `.dev/build-timings/game-dll-20261120T182726149737Z-df8100.json`
- `.dev/agent-wake/requests/wake-2ed65f7d688f.json`
- `.dev/agent-wake/events.jsonl` (lines 295, 297-298, 307, 314-315, 345)
