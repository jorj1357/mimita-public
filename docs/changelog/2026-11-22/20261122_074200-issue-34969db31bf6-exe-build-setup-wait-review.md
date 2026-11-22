# Over-budget exe-build setup wait review (build-wait-reviewer)

Date: 2026-11-22 ~07:42Z
Agent: build-wait-reviewer
Context: wake-21757ebfb6a7
Issue: issue-34969db31bf6 (Wait over budget: exe-build at setup)

## Scope

Read-only evidence review. No source, config, or build-script edits. No compiler,
build.py, module worker, or second dev-loop invoked. No new runtime started. This
is the 6th open occurrence of the same signature, so it was recorded as a
recurrence, not a new analysis.

## Wait occurrence

- wait_id `exe-build:10:2026-11-22T07:08:26.104Z`, stage `setup`, reason
  `preparing build`, elapsed 60.675s vs 60.0s budget, generation 10,
  source_generation 10.
- Waiting EXE attempt was a no-op: `build/build-metrics.jsonl`
  2026-11-22T07:09:58Z, result noop, compiled 0, skipped 87, elapsed_s 80.833,
  `phase_seconds.presentation_dll_s` 77.76,
  `phase_details.presentation_dll` outcome `failed` (`build_game_dll.py exited
  with code 1`). The 60.675s wait fired while that child was still running.
- The very next attempt 07:10:56 records `presentation_dll` outcome
  `cached_failure_skip` (`previous_error` `build_game_dll.py exited with code 1`,
  `retry_after_s` 1742.404), so a ~29 min failure cache now suppresses the
  repeat.
- Wake `.dev/agent-wake/requests/wake-21757ebfb6a7.json`: queued 07:09:27.464Z,
  codex launch 07:09:34.077Z (exited), retry 07:18:37.730Z, opencode launch
  07:18:55.784Z; `attempts=2`. One wake request per occurrence.

## Findings

1. Not a new fault. Same signature/root cause as issue-8aae0621645f (60.019s) and
   issue-ae248907f1f1 (failing optional presentation-DLL hidden inside EXE
   `setup`). The wait is flat-60s-budget noise over real, unlabeled setup work.
2. Acceptance passes for the wait record: issue carries wait_id, stage, reason,
   elapsed, generation, evidence; readiness ready=true, missing=[]; exactly one
   reviewer wake was queued for this occurrence.
3. Attribution defect persists: evidence build_id is
   `2026-11-22T02:37:06.972018Z`, the previous published artifact, not the
   in-flight attempt; `request_ids` is empty.
4. Journal gap: the declared root dev-loop journal
   `logs/2026-11-22/20261122_011031/dev-loop/events.jsonl` does not exist (only
   `diagnostics/`, `server-01/`, `run.json`, host-resource CSVs). No on-disk
   `dev-loop.wait.over_budget` for this wait_id; matches open issue-0498cbef4ef3.
5. Policy noise/duplication: 110 open build-friction issues, 83
   `game-dll-build at game_build`, 6 `exe-build at setup` all within ~1.4s of the
   flat 60s budget. `build_friction.py:131-169` keys uniqueness on wait_id, so
   each occurrence mints a new issue and wake.

## Claims

- Source evidence: none (review-only).
- Build evidence: no-op EXE attempt 2026-11-22T07:09:58Z, 80.833s, 77.76s failed
  presentation-DLL phase.
- Runtime evidence: none required for this wait-policy issue.
- Human acceptance: not performed.

## Handoff

- Appended review verdict `confirmed-wait-policy-noise` to issue-34969db31bf6.
- Appended a recurrence occurrence to issue-8aae0621645f.
- No code fix claimed. Owners: issue-ae248907f1f1 (stop the failing optional
  presentation-DLL from consuming EXE setup), issue-e044a931c54b /
  issue-7d3dcfd7433b (remove/measure avoidable waits, per-stage budgets),
  issue-136d4553ff92 (stage accounting), issue-0498cbef4ef3 (restore dev-loop
  journal events).
