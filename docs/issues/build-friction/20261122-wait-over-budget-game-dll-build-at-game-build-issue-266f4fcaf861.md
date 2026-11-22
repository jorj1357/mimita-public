# issue-266f4fcaf861: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-build-friction)

### 2026-11-22T04:34:16.380Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed: over-budget game-dll-build wait game-dll-build:2026-11-22T04:19:39.819Z (generation 452, build attempt game-dll-20261122T041939996065Z-df61cc).

FINDING 1 - no stall; the 60s budget is below the normal build critical path.
- Wait fired 2026-11-22T04:20:42.393Z at elapsed 61.085s; the build started 04:19:39.996Z and SUCCEEDED at 04:21:05.556Z in 85.547s (.dev/build-timings/game-dll-20261122T041939996065Z-df61cc.json).
- Phase timeline in the same attempt JSONL: changed_source_preflight 19.967s, object_cache_scan 3.099s (ended 04:20:36.604Z), vendor 0.014s, compile 1.842s (ended 04:20:38.460Z), link 24.982s (gnu_linker 24.622s, ended 04:21:03.441Z), coherence_validation 0.627s, publish 0.801s. At the 60s check the attempt was still in object_cache_scan/pre-link, i.e. normal work, not a hang.
- Recent attempts: 451=157.422s, 452=85.547s, 453=90.022s, 454=80.846s. Every one exceeds WAIT_ISSUE_SECONDS=60 (devscripts/dev-loop.py:109-110), so every game-DLL build now files an issue and wakes a reviewer.

FINDING 2 - recurring waits create duplicate open issues, contrary to the recurrence rule.
- Uniqueness key is the exact wait_id, and a new wait_id always creates a new issue: devscripts/build_friction.py:131-140 (_wait_issue), :143-170 (record_wait_incident; docstring :145-148). The title dedupe at :125-128 is bypassed for waits. Title is identical for all occurrences (:172).
- Observed identical-title issues today: bc45be2f32a3 (04:10:48), 266f4fcaf861 (04:19:39), 6202f0e8d9f4 (04:23:20), ce4c245a8a70 (04:24:51), 628187de3d6d (04:29:08); the board shows about 28 open/in_review 'Wait over budget: game-dll-build at game_build' issues. Today's journal has 24 dev-loop.wait.over_budget events and 24 reviewer wakes (1:1, no mismatched wait_id).
- This contradicts the reviewer instruction 'If the wait recurs, add the occurrence instead of creating a duplicate issue' and the risk recorded in this issue; it is the same wait class recurring, not a new report.

FINDING 3 - acceptance fields exist but reason is uninformative (truncated raw stdout).
- stage is hardcoded game_build (devscripts/dev-loop.py:3527). reason is game_build_detail (dev-loop.py:3528), which is set to the last 240 chars of any worker stdout line (dev-loop.py:2829-2830). In this issue evidence, reason starts mid-number (58445.797...), the 240-char tail of the module-identity-provider GAME_FILE JSON. It cannot identify the active phase (preflight/scan/compile/link), so 'carries stage, reason' is only formally met from devscripts/build_friction.py:187-190.

FINDING 4 - wake delivery is correct, exactly once per occurrence.
- For this wait_id there is exactly one dev-loop.wait.reviewer-wake and one dev-loop.wait.over_budget with created=true; wake-79c8de08d15e targets issue-266f4fcaf861. Across today's journal, over_budget=24 and reviewer-wake=24 with identical wait_ids.

Suggested fix (reported, NOT applied - shared build path; needs owner/human decision):
1) derive the wait budget from observed attempt durations (stage-aware or p95 of recent attempts) so normal builds do not trip it;
2) populate stage/reason from the live build phase/subphase in .dev/build-timings instead of the raw stdout tail;
3) collapse recurring identical waits into one issue with occurrences (or a recurrence window) while still waking once per occurrence, satisfying the recurrence rule.
Generic owners that already cover this: issue-136d4553ff92 (build lifecycle observability, build-coordinator), issue-e044a931c54b (remove avoidable waits), issue-7d3dcfd7433b (measure wait stages). Do not spawn more duplicate wait issues.

Evidence basis: dev-loop events.jsonl for run 20261122_011031, .dev/build-timings/game-dll-20261122T041939996065Z-df61cc.json and .jsonl, attempt 451/453/454 summaries, issue records. Confidence: high for findings 1/2/4 (direct timing + journal), medium for finding 3 intent (needs policy choice). No source edited; no behavior proof claimed.
