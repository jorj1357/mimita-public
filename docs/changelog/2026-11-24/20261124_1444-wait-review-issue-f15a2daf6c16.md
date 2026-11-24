# Build-wait review: game-dll-build over budget (issue-f15a2daf6c16)

**Purpose:** Record reviewer evidence for over-budget wait `game-dll-build:2026-11-24T19:08:17.245Z` and the build attempt it covered.

**Audience:** MiMITA dev-loop/build-friction owners and coordination reviewers.

**Does not:** Change wait policy, edit source, or prove runtime/gameplay behavior.

## Task

- Session: session-2c61b4150bd2 / work-d7f6f85b62b3 (agent build-wait-reviewer)
- Focus: Review a dev-loop build/test wait issue; append journal-backed evidence.
- Status: review complete, no source change.
- Branch: checkpoint/one-checkout-condensation-20261111
- Base/final commit: c6eb8e5e8b7c58ab9b02a3764e36693ef4df7874 (working tree already dirty before this session)

## Findings

- Wait fired at 60.318s (+0.318s over the flat 60s budget). One `dev-loop.wait.over_budget` and one `dev-loop.wait.reviewer-wake` (`wake-afa5d52a1438`, codex) exist for this `wait_id`; per-wait dedupe held. The record carries wait_id, kind, stage, elapsed, budget, build_id, generation, source_generation, and request_ids.
- The wait was **not a stall**. The Mode 3 batch sealed at 19:08:17.158Z (`-94fd8c25`, 11 changed inputs) included changed headers `src/combat/weapon-types.h` and `src/vehicles/vehicle-types.h`. Attempt `game-dll-20261124T190817284712Z-0003b3` (run `20261124_170535`) then ran 411.7s and **failed at compile** (404.599s) with `src/vehicles/vehicle-system.cpp:700: 'collisionHalf' was not declared`.
- Cache invalidation shows `vehicle-system.cpp` with `reason: object file missing`, i.e. the sealed batch captured an in-progress edit. Current source declares `collisionHalf` at line 699 and generation 606 built cleanly, so this compile break is already resolved.
- Resource pressure was extreme: 353 samples @1s, mean host CPU 98.755% / peak 100% across 12 logical CPUs, minimum available memory 25,145,344 B (~24 MiB) of 15.65 GiB; jobs=10, source_count=644, compiled=266, skipped=378.
- Known defect: `evidence.reason` is the last 240 chars of a worker line (mid-JSON tail), not a readable stage summary (`dev-loop.py:3112/3142` setting `game_build_detail`, read at `dev-loop.py:3892`). Tracked by `issue-6e08d89b55e3`.

## Actions

- Appended one review (verdict confirmed) to `issue-f15a2daf6c16`.
- Appended one occurrence to `issue-6e08d89b55e3` (reason-field defect) instead of creating a duplicate issue.
- Appended one build-pressure occurrence to `issue-2a5c5da53565`.
- Added one evidence-backed suggestion to `issue-136d4553ff92`: prevent a Mode 3 batch from sealing a source whose object is missing and whose edit is still in progress.
- No code changed and no build/test requested.

## Evidence

- `logs/2026-11-24/20261124_170535/dev-loop/events.jsonl` (`dev-loop.wait.over_budget`, `dev-loop.wait.reviewer-wake`, `dev-loop.game-build.failed`)
- `.dev/build-timings/game-dll-20261124T190817284712Z-0003b3.json`
- `.dev/build-timings/game-dll-20261124T190817284712Z-0003b3.resource.json`
- `.dev/agent-wake/logs/2026-11-24T19-09-18.599Z-build-wait-reviewer.prompt.txt`
- `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-f15a2daf6c16.md`
- `docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md`
- `docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md`
- `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md`

## Human acceptance

- Not applicable: coordination/review-only session; no gameplay or visual claim.
