# Ground slide collision and network diff review

- Time UTC: `2026-10-24T21:52:25Z`
- Display time: `2026-10-24 17:52:25 EDT` (`America/New_York`)
- Branch: `20261011zt`
- HEAD: `0472121baef8d96342508d82aef5598de97daff7`
- Baseline: `85d539f0bb7f7d59def2bc80552ba8b5d22aa15e` (`2026-10-21`)
- Result: `PASS_WITH_HUMAN_REVIEW`; source/runtime evidence points primarily to world collision response; online correction remains unisolated.

## Scope and working-tree boundary

Reviewed the committed changes since the Oct 21 baseline and the existing uncommitted diff. No source or configuration code was changed in this session. Existing modifications were preserved. The working tree already contained collision/network edits, gameplay issue intake, and several Oct 24 changelogs, including the triangle sweep and Dust2 slope trace records.

## Findings

1. **Confirmed collision response mismatch in the Oct 24 baseline run.** In `docs/changelog/2026-10-24/20261024_163550-dust2-slope-collision-trace.md`, build 3094 logged 80 vertical-face contacts classified as walkable/flattened. At tick 5999, a slope contact used world-up rather than its authored slope normal; vertical velocity changed from about `-18.13` to `+6.36`. This is direct evidence for the ground “jumping” symptom and a collision owner, not network inference.
2. **Partial correction, not a complete snag fix.** The current uncommitted `actor-triangle-solver.cpp` diff uses the oriented map face normal for support classification and preserves slope response. Build 3099's journal recorded zero false vertical-wall support classifications, but the uphill run still stopped at vertical map triangle 2440. Its geometry intent is unresolved, so corner snagging is not accepted as fixed.
3. **Other current diff changes are plausible edge-case risks, not proven causes.** `physics-collision-mesh.cpp` now segments pose-deformation sweeps and avoids using infinite-plane depth for sweep-only hits. The solver also widens same-triangle opposing-contact cleanup and uses vertical support direction for near-feet penetration recovery. Those edits need the same live corner path to determine feel; the existing trace does not identify them as the remaining blocker.
4. **Networking can add corrections online, but is not the best-supported cause here.** The Oct 20 movement-validation path can restore a reported position to the prior accepted position when its geometry sweep finds a wall. Client reconciliation emits `movement.position-correction.applied`. No same-route online trace was available to correlate such corrections with the reported jumps. The Oct 24 network worktree diffs inspected are mainly session setup, server identity handling, developer flight, and NPC collision-world startup.

## Relevant code and evidence

- `src/physics/movement/actor-triangle-solver.cpp`: map support/response selection, collision response, remaining-movement projection, and rollback evidence.
- `src/physics/movement/physics-collision-mesh.cpp`: segmented pose sweep and sweep-only penetration calculation.
- `config/collision.json`: bounce is enabled with strength `0.35`, minimum speed `0.01`, and minimum push `0.1`; these values match the Oct 21 baseline. `respondVelocityAgainstNormal()` remains on the ordinary actor-triangle response path, so configured bounce is a secondary source to inspect if the floor still hops after normal classification is correct.
- `src/network/movement-validation.cpp` and `src/network/multiplayer-reconcile.cpp`: server geometry correction and client correction journal path.
- Existing runtime records: baseline build 3094 at `logs/2026-10-24/20261024_200800/events.jsonl`; retest build 3099 at `logs/2026-10-24/20261024_202809/events.jsonl`.

## Review and validation

- Read `docs/ROUTER.md`, movement specification, collision architecture, networking specification, runtime-scenario workflow, regression record `docs/regressions/2026-09-29/slope-edge-snag-REG.md`, task-completion procedure, and `docs/skills/spec-behavior-review-v1.md`.
- `git diff --check` produced no whitespace errors. No new tests, build, dev-loop run, or source edits were made; the existing 3094/3099 runtime evidence is cited separately above.
- Human visual/feel review remains open. For online attribution, repeat the same route while connected and correlate `collision.contact.before_response`/`after_response` with `movement.position-correction.applied` and server `blocking-geometry` validation. Inspect triangle 2440 in the authored map before changing its blocking response.
