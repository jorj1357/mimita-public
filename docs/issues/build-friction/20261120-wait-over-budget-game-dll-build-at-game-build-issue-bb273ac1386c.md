# issue-bb273ac1386c: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-expected-build-latency)

### 2026-11-20T17:25:13.690Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed with dev-loop journal and build-timings. The 60.549s wait at stage game_build maps to build attempt game-dll-20261120T170256138061Z-c77c9e (started 17:02:56.138Z, completed 17:04:44.368Z, total 108.23s, success=false, exit 1, error 'codegen check failed: packet_registry_codegen.py'). The wait was a build genuinely in progress, not a stuck stage. Evidence that the 60s budget is below normal DLL build latency: generation 405 reported total_seconds 2134.073 (source scan/parse ~500-360s, compile 1360.694s, link 136.181s); the follow-on attempt game-dll-20261120T171733871137Z-338310 started 17:17:33.874Z and was still emitting translation_unit.started events at 17:24:15 (elapsed_ms ~401s). Therefore fixed-60s wait issues will fire on every non-cached DLL build. Acceptance check: wait_id, stage, reason, elapsed, and build ID are present in the issue; exactly one reviewer wake was queued for this occurrence (wake-1362ef073454). Defect found: recurrence handling is wrong - 12 near-identical 'Wait over budget: game-dll-build at game_build' issues exist (issue-63dccbd8f8b6, 2008e743c710, 6413477d361d, 87f1017235e5, e84cfa28c7f7, 6bc05eadfb7f, dd54a79216ad, 25b14f436a3e, 40dfaa2a803c, b0ba8698acd7, bb273ac1386c, 9a9807fa0128) instead of occurrences on one canonical issue. Recommendation: suppress/annotate the wait issue while the pinned build attempt is actively producing TU events or derive the budget from rolling build duration, and attach repeat occurrences to a single issue. The c77c9e codegen failure is a separate build defect that needs its own triage. No source edits made; bounded review only.

## Scheduled independent review

### 2026-11-23T14:55:36.257Z — mode3-latency-goal

A bounded independent review was queued as wake wake-87b6ce898286 after 412 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (confirmed-expected-build-latency-no-new-defect)

### 2026-11-23T15:03:17.480Z — issue-review-sweeper

Reviewer: issue-review-sweeper

## Independent review — issue-review-sweeper (bounded, read-only) — 2026-11-23T14:58Z

**Verdict:** confirmed-expected-build-latency; no new behavior defect in this occurrence, but recurrence handling is still unresolved.

**Observed facts (direct):**
- This occurrence is historical: wait_id `game-dll-build:2026-11-20T17:02:55.229Z`, budget_seconds=60, elapsed=60.549s, stage=game_build, build_id=2026-11-20T07:47:09.981634Z, source_generation=23. The prior review already tied it to attempt `game-dll-20261120T170256138061Z-c77c9e` (failed `codegen check failed: packet_registry_codegen.py`), i.e. a build genuinely in progress plus a separate codegen failure.
- The wait policy is still a fixed 60s: `devscripts/dev-loop.py:113` `WAIT_ISSUE_SECONDS` defaults to 60 (env `MIMITA_WAIT_ISSUE_SECONDS`), tested at :3819/:3838/:3856, each calling `build_friction.record_wait_incident` + `wake_reviewer`.
- Uniqueness is keyed on the exact `wait_id`, which embeds the occurrence timestamp, so `_wait_issue(...)` cannot match a prior row for a new wait; the "append occurrence" branch (`build_friction.py:164-169`) is unreachable for distinct waits. The docstring (`build_friction.py:143-148`) makes this intentional: "a new wait gets another issue even when its stage and reason are identical."
- Current inventory: 152 `Wait over budget*` build-friction issues, of which 130 share the exact title `Wait over budget: game-dll-build at game_build` (135 in_review / 16 new / 1 assigned for the whole group). No canonical/consolidation issue exists for the signature.
- Wake artifacts: 326 files reference `build-wait-reviewer`; wake requests by day 2026-11-20=75, 11-21=109, 11-22=186, 11-23=44; `build-wait-reviewer` requests 26 / 35 / 98 / 4.
- Faster path now quiescent: this run's artifact (gen 560) `total_seconds=33.783` (compile 18.620, link 5.365, 623 skipped); dev-loop status `build=published_stale`, `game-build status=idle`. Last 12 `.dev/build-timings` DLL builds are 15.1-58.6s, none >60s. Current journal `logs/2026-11-23/20261123_144429/dev-loop/events.jsonl` shows no over-budget wait; launch completed 22.18s and build identity matched.
- Still a latent trigger: 182 of 350 recorded `game-dll-*.json` builds exceed 60s, and the most recent >60s build was 267.2s at 2026-11-22T23:56 (right before the last duplicate, issue-cd97259fdc20 at 23:53).

**Inference / uncertainty:**
- The specific Nov-20 wait was expected build latency (build in progress + codegen failure), confirming the prior verdict. Confidence: high (journal and build-timings agree).
- The duplicate/wake storm (130 near-identical issues, 326 reviewer wake artifacts) is a workflow problem, not this occurrence's defect: fixed 60s budget + per-occurrence uniqueness under frequent >60s builds. Under the faster gen-560 path the flood is currently dormant (no new game-dll-build duplicate since 2026-11-22T23:53; 4 reviewer wakes today), so the risk is latent, not eliminated. Confidence duplicates are inherent to current code: high; confidence the flood stays quiet: medium (one cold/header-fanout build restarts it).
- I did not reproduce a new over-budget wait (would require forcing a >60s root build) — out of scope for a bounded read-only review.

**Contrary evidence considered:**
- Recent sub-60s builds suggest the budget is now adequate in the common path. Counter: 182/350 builds exceeded 60s and cold/full builds still do, so the policy still fires on legitimate multi-minute builds.
- The earlier recommendation to consolidate conflicts with issue-9a4503f6ed68's accepted design ("one issue per wait occurrence"), so it is a design change for that owner, not a bug fix here. No source edited.

**Next discriminating check:**
- Drive one cold/full game-DLL build through the root dev-loop (e.g. a header-fanout change) and observe whether a new `game-dll-build` wait issue plus reviewer wake is created and whether it duplicates an existing signature instead of appending an occurrence. If it stays under 60s, the trigger is currently dormant. Do not close this issue on the strength of faster builds alone.

**Status:** read-only; no source edit, no build, no close, no fix claim.
