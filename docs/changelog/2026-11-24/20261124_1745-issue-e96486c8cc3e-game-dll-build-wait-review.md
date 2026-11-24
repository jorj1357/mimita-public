# game-dll-build wait review (issue-e96486c8cc3e)

Read-only bounded independent review by `issue-review-sweeper` (wake-24722249f116), 2026-11-24T17:44Z. No source edit, no build or test run, no runtime process control, no close, no fix claim.

## Occurrence

Wait `game-dll-build:2026-11-21T03:10:13.609Z` fired at 60.281 s against attempt `game-dll-20261121T031013951147Z-cdd78c`, source_generation 4, request `auto-4`. At issuance the attempt was ~28.9 s into `codegen:packet_registry_codegen.py` after `codegen:resident_state_codegen.py` completed in 26.80 s; it was progressing, not stalled. The attempt later failed at 03:15:55.536Z with `failed_phase=codegen_timeout` (packet_registry_codegen exit_code 124 after 308.94 s), a different and later event than the 60 s wait.

## Acceptance findings

- Field presence: pass (wait_id, stage, reason, elapsed, budget, source_generation, request_ids, evidence).
- Reviewer wake exactly once: pass. One `wake.queued`, `wake-6b00f2b0af56`, at 03:11:23.488Z for this wait_id; the 2026-11-24T17:08:28Z wake is the scheduled independent review.
- Build/run identity: fail. `generation=null`, `build_id=2026-11-21T02:26:50.306117Z` is the previous EXE build, `build_attempt_id` exists only inside the free-text reason, `run_id` absent. Third instance of the defect recorded on issue-9a4503f6ed68 and issue-3472692a57c2.
- Proof expectation not re-checkable: `logs/11-21-2026` exists but is empty, so the run's `dev-loop.wait.over_budget` / `dev-loop.wait.reviewer-wake` events are pruned.
- Dispatch failed separately: 36m44s to first codex claim, codex model-cache failure 03:49:23Z, retry to opencode only at 13:50:41Z, opencode session crashed 15:18:08Z (`Unexpected server error`, `err_9dbc78ec`), `wake.failed` 15:20:49Z. Net 12h09m with no review written.

## Recurrence (why the class stays open)

Coordinator store holds 193 `Wait over budget` build-friction issues (175 in_review, 17 new, 1 assigned); newest game-dll occurrence is issue-d82b02478dff at 2026-11-24T17:13:14Z (63.935 s, "starting whole-game build"). `devscripts/dev-loop.py:114-115` still defaults every kind to 60 s and `:3884-3900` flags game_build on total stage elapsed, with `:3896-3897` still using `latest_build` / `game_build_generation`.

## Cross-links

- Review appended to issue-e96486c8cc3e (state now in_review), verdict `reviewed-policy-unchanged-identity-gap`.
- Process proposal appended to issue-9a4503f6ed68 (aggregate per (kind,stage), stage-aware game_build budget, in-flight attempt identity) with a measurable target and 3-build experiment; issue-linked question sent to wake-proof-wait.
- Dispatch occurrence appended to issue-84c584957e65 and shared with build-wait-reviewer; no duplicate issue created.
- brain_check `build_resource_gaps` (44a127, 968a7e, 8723b2) are unrelated resource-sampling gaps already tracked on issue-136d4553ff92 / issue-ed3ac775283d.

## Next discriminating check

After a stage-aware/waived game_build budget lands, run one real game-DLL build and confirm zero new game_build wait issues while the timeline advances, in-flight attempt identity in the evidence, and exactly one issue plus one wake for a genuine >N s no-advance stall.
