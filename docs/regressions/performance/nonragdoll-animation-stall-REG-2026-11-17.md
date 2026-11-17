# Non-ragdoll player animation stall

Date: 2026-11-17
Time last updated: 2026-11-17T16:00:00Z
Status: OPEN — ragdoll mode bypasses a costly non-ragdoll path
Owner: local player animation / final-pose containment

Related:

- `docs/specs/animations/animations.md`
- `docs/specs/performance/performance.md`
- `docs/features/performance/fps-improvement-ledger.md`
- `docs/regressions/performance/fps-frame-budget-REG-2026-11-15.md`

## Symptom

The user reports about 298 FPS while ragdoll mode is active and about 2–3 FPS
after leaving ragdoll mode, in the same session.

## Expected behavior

Exiting ragdoll must not multiply the player's frame cost. Non-ragdoll movement,
animation, collision, and final-pose containment should each have a bounded
owner cost that fits the 4.17 ms frame target.

## Actual behavior

Ragdoll mode skips the expensive non-ragdoll path:

- `src/entities/player-animation.cpp:330` returns early from
  `updateProceduralAnimation()` when `ragdollModeActive` is true.
- `src/sim/simulate-tick.cpp:469-470` skips normal player final-pose
  containment in ragdoll mode.

In the measured non-ragdoll path, `PlayerAnimation` reached about 169 ms and
`Physics::CollisionSteps` reached about 29–32 ms in the pathological run.
Collision contributes, but animation/final-pose work is the dominant reason
ragdoll is dramatically faster.

## Evidence

- `perf_report 2` `performance.report` top scopes.
- `PERFORMANCE_SPIKE` child breakdown.
- Server was simultaneously overloaded in the same run, so the absolute values
  are not a clean baseline. The ragdoll/non-ragdoll relative difference is the
  reported evidence.
- Clean run `20261117_170234` confirmed `ActorPair::FinalPose` ~26–51 ms and
  `Simulation::FinalPose` ~26–47 ms. After skipping the actor-pair final-pose
  sample on unchanged ticks, `ActorPair::FinalPose` left the top scopes and the
  client improved from about 7–14 FPS to about 23 FPS. `Simulation::FinalPose`
  remained up to ~38 ms.

## Required next evidence

- Clean connected payload funworld3 window, no concurrent build.
- Toggle ragdoll on/off in the same scene and same PID.
- Compare `PlayerAnimation`, `Simulation::FinalPose`, `Physics::CollisionSteps`,
  `ActorPair::FinalPose`, and frame p50/p95.
- Profile inside `updateProceduralAnimation` before optimizing.

## Acceptance

- Non-ragdoll frame cost is bounded and close to ragdoll for the same scene.
- The fix is not "keep ragdoll on."

## Rules

- Do not treat ragdoll mode as a performance fix.
- Do not remove the ragdoll early return until the non-ragdoll path is measured
  and bounded.
