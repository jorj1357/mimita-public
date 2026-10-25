# Triangle sweep snag and distant-contact fix

- Recorded: `2026-10-24T15:12:14Z` (display: `2026-10-24 11:12:14 EDT`, `America/New_York`)
- Branch: `20261011zt`
- HEAD before and after: `0472121baef8d96342508d82aef5598de97daff7`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Request and source trace

Fix ordinary ground movement catching at block seams/corners and collision
reports from geometry that is far from the actor, while retaining
triangle-versus-triangle collision.

The active actor path is
`doGLBTriangleCollisions()` → `runActorTriangleCollisionStep()` →
`solveActorTriangleCollision()` → `collectActorMeshContactsInto()` in
`src/physics/movement/physics-collision-mesh.cpp`. It keeps finite triangle
intersection and continuous translation sweeps. The suspect behavior was the
pose sweep's shell: a whole tick's limb deformation was used as padding for the
entire translation sweep. Swept crossings also used the deepest actor vertex
against the world's infinite triangle plane to estimate penetration, even
when the finite triangles no longer overlapped at the final pose. Either can
make a contact look much larger than the actual nearby feature.

## Exact source changes

### `src/physics/movement/physics-collision-mesh.cpp:649-701`

Before, a translating triangle used its entire frame deformation as sweep
padding:

```cpp
} else if (glm::dot(sweep, sweep) > 1e-8f) {
    const float sweepPadding = deformationRadius +
        (roundedFeatures ? featureRadius : 0.0f);
    if (sweptTriangleTriangleIntersect(
            pa, pb, pc, sweep,
            wt.a, wt.b, wt.c, sweepPadding,
            timeOfImpact, sweepNormal)) {
```

Now translation remains continuous while pose deformation is split into
segments no larger than `0.08f`; each sweep uses only that segment's deformation
as padding. The existing triangle-test cap fails closed if the work budget
cannot verify the complete path:

```cpp
} else if (glm::dot(sweep, sweep) > 1e-8f ||
           deformationRadius > 1e-5f) {
    constexpr float kMaximumPoseDeformationPerSweep = 0.08f;
    constexpr int kMaximumPoseSweepSegments = 64;
    const int poseSegments = std::max(
        1, (int)std::ceil(
            deformationRadius / kMaximumPoseDeformationPerSweep));
    if (poseSegments > kMaximumPoseSweepSegments) {
        gActorNarrowphase.budgetExceeded = true;
        return;
    }
```

Each segment interpolates both triangle poses and runs the continuous
triangle-versus-triangle sweep with only local deformation padding. It does not
add simulation ticks.

Before, plane penetration used the deepest vertex even for a sweep-only
crossing:

```cpp
const float rawPenetration =
    std::max(0.0f, -std::min({s0, s1, s2}));
```

Now plane depth is used only when the final finite triangles still overlap;
sweep-only recovery continues to use travel after the time of impact:

```cpp
const float rawPenetration = currentOverlap
    ? std::max(0.0f, -std::min({s0, s1, s2}))
    : 0.0f;
```

### `src/physics/movement/actor-triangle-solver.cpp:1002-1048`

Before, the unresolved-overlap fallback cleared the rejected pose's contacts
before emitting a rollback record. The event had no contact identity, so it
could not show which body part or map triangle produced the rollback.

Now, before clearing those contacts, the existing rate-limited rollback event
includes the contact count and up to four leading contacts: actor part, map
triangle/entity id, contact point and distance from player origin, normal,
penetration, time of impact, and part sweep delta. Contact-derived state is
still cleared immediately after logging, preserving the existing rollback
behavior.

## Other worktree changes

The following modified files were present but were not changed as part of this
collision fix: `config/accounts/default.json`, `config/analytics.json`,
`src/gui/gui-main.cpp`, `src/network/multiplayer-packets.cpp`,
`src/network/multiplayer-tick.cpp`, and `src/network/server.cpp`.
`src/main.cpp` also appeared as modified during final checks; it was left
untouched.

## Reviewed documents and skills

- `docs/ROUTER.md`
- `docs/specs/movement/movement.md`
- `docs/architecture/collision/collision.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/specs/performance/performance.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
- `docs/regressions/2026-09-29/slope-edge-snag-REG.md`
- `docs/regressions/2026-10-01/physical-objects-collision-REG.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md` — `PASS_WITH_HUMAN_REVIEW`; no
  specification conflict found; live feel acceptance remains open.
- `docs/skills/efficiency-checker-v1.md` — reviewed; no new broadphase gather
  or fixed tick was added. Pose sweep work is bounded by 64 segments per
  triangle and the existing 200,000 triangle-test cap.
- `docs/skills/logging-checker-v1.md` — reviewed; rollback detail is owned by
  the actor solver, includes the decision context, and is bounded to four
  contacts at the existing 250 ms event interval.

## Validation and evidence

- `git diff --check`: passed for the worktree.
- Focused tests: not run; none were requested. No synthetic gameplay test was
  added.
- Dev-loop build: `SUCCESS`, build `3026`, source generation `19`, compiled 1
  source and skipped 567 unchanged sources. The published executable is
  `C:\mimita-v9\.dev\builds\3026\mimita.exe`, SHA-256
  `ed614a2957ad77ff1482b0da4036ff93ba233e25ad2adddd79fe62d2d1ee4b01`.
- Exact executable `--versioninfo` identified the binary and reported
  `EVENTS_JSONL_PATH=logs/10-24-2026/20261024_111102/events.jsonl` for that
  preflight process. This is build identity evidence, not gameplay evidence.
- At validation time, dev-loop run `20261024_150801` remained live on build
  `3023` with client PID `28108` and server PID `28276`; the new build was
  published without replacing those processes. Its journal is
  `logs/2026-10-24/20261024_150801/events.jsonl`.
- A later status check showed dev-loop launching run `20261024_151350` on build
  `3027` after `src/main.cpp` appeared modified. That launch was already in
  progress and was left untouched; it is not evidence of a collision scenario
  on the changed code.
- A prior journal from this investigation,
  `logs/2026-10-24/20261024_142856/events.jsonl`, contained 69
  `collision.last_valid.rollback` events and reported residual penetrations
  around 5–25 m. This supports the reported rollback symptom, but does not by
  itself identify the offending contact. The enhanced contact details have not
  yet been observed during a reproduction on build 3026.
- Runtime scenario and human acceptance: pending. Walking straight across
  block seams/corners and checking near-body contacts require a fresh run on
  build 3026. The active server/client were preserved rather than replaced.

## Remaining review

Run build 3026 and walk straight across block seams, diagonally across corners,
and into walls/floors while moving sideways. Confirm no distant contact or
repeated rollback, and confirm normal triangle contacts still stop the body
outside solid geometry. Inspect `collision.last_valid.rollback` and
`collision.solve.summary` in that run's `EVENTS_JSONL_PATH`; accept only after
the contact identity and visible movement agree.
