# Independent build-wait review — issue-4a0450674f5f

- **When:** 2026-11-26T01:09Z
- **Agent:** build-wait-reviewer (wake-37aa85e5edcf, opencode)
- **Issue:** issue-4a0450674f5f — Wait over budget: game-dll-build at game_build
- **Wake task:** review over-budget game-dll-build wait at stage game_build, elapsed 62.51s
- **Wait / attempt / run:** game-dll-build:2026-11-26T00:29:50.339Z / game-dll-20261126T002950581148Z-fff09d / 20261126_002257
- **Scope:** read-only review. No source edit, no build/test/runtime started by this agent, no fix or closure claimed.

## Evidence

- `logs/2026-11-26/20261126_002257/dev-loop/events.jsonl`: exactly one `dev-loop.wait.over_budget` (created issue-4a0450674f5f) and exactly one `dev-loop.wait.reviewer-wake` (wake-37aa85e5edcf) for this wait.
- `.dev/build-timings/game-dll-20261126T002950581148Z-fff09d.json`: success, no_op=false, generation 660, total 212.641s, compile 188.812s, link 4.703s, 36 compiled / 609 skipped from a physical-entity.h fanout-33 change. Slowest TU server-gamemode.cpp 132.082s.
- `.dev/build-timings/game-dll-20261126T002950581148Z-fff09d.resource.json`: mean host CPU 98.911%, peak 100%, minimum available memory ~170 MiB, 140 samples @1s.
- Review body appended to issue-4a0450674f5f (review id 457, verdict `confirmed-build-friction-real-build-over-60s-budget-no-duplicate-filed`), with `related_build`/`related_run` set to the authoritative attempt and run.
- Occurrence noted on the owning tech-debt issue-6e08d89b55e3 (reason field stores truncated worker JSON; this occurrence starts `:52563.0,`).

## Outcome

- Confirmed real (non-stalled) build over the flat 60s budget; first divergence is the budget versus natural compile cost, not build progress.
- Acceptance holds on the queue side (fields present; one reviewer wake), but the wait snapshot is not self-identifying: `build_id` and `generation` do not match the attempt, and `reason` is a truncated JSON fragment.
- Reviewer wake lifecycle repeated the known dispatch defect: codex launch at 00:30:59 then opencode fallback at 00:57:03 for the same wake, queue-to-final-launch ~26m07s (>5min target), two `wake.launched` rows.
- Recurrence appended to the existing issue; no duplicate issue filed. 78 of the first 100 `issue list --kind build-friction` rows share the same title.

## Follow-up (unowned)

- Owner decision on scaled/deduped game-dll-build wait budget and self-identifying wait evidence (see issue-136d4553ff92, issue-7d3dcfd7433b, issue-6e08d89b55e3).
