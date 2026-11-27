# issue-81d40f5f20b3 independent review (2026-11-27)

Read-only review, no source, build, test-run, close, or fix.

## What changed
- Appended one independent review to `issue-81d40f5f20b3` (verdict
  `confirmed-recurring-wait-policy-noise-with-real-latency-deficit`).
- Appended one process proposal (with reconciliation note) to
  `issue-9a4503f6ed68`.
- Appended one consolidated build-resource-sampling gap report to
  `issue-2a5c5da53565` and asked `cpu-investigation` to confirm ownership.

## Evidence used
- `issue-003b412c0836` wait evidence: `game-dll-build:2026-11-27T18:41:02.539Z`,
  elapsed 60.845s, budget 60.0s.
- `logs/2026-11-27/20261127_174537/dev-loop/events.jsonl`:
  `dev-loop.wait.over_budget` + `dev-loop.wait.reviewer-wake` for that wait_id.
- `.dev/build-timings/game-dll-20261127T184102743581Z-55292b.json`: gen 805,
  total 427.963s, preflight 286.041s, compile 290.658s, link 119.636s.
- `.dev/build-timings` for `...d5be13` (191.959s) and `...d61781` (32.858s).
- `docs/specs/dev-loop/build-performance-budget.md`: one changed leaf `.cpp`
  hard ceiling 30s.
- `devscripts/dev-loop.py:114-115,3885-3901`;
  `devscripts/build_friction.py:131-169`.

## Finding
The wait alarm is working as designed but fires on healthy, multi-minute
builds; per-attempt `wait_id` minting produces roughly 90 open duplicate
wait issues. The build itself is far over the performance budget, so this is
both alarm noise and a real latency signal. No fix was implemented.

## Residuals
- Existing proposals on `issue-9a4503f6ed68` already cover coalescing; the
  recommendation is to pick one canonical variant.
- Build-resource telemetry ownership is unconfirmed (`cpu-investigation`).
