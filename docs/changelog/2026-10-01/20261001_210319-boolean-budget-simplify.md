// 2026-10-01T21:03:19Z (display: 2026-10-01 17:03:19 EDT)
/* purpose
* Record the pass that fixed the measured FPS-dip cause: the boolean cut
* rebuild. Decompose() was removed from the per-rebuild path, cutters are much
* coarser with Manifold Simplify, the triangle cap is 4096 with one cut per
* flush, and an over-cap cut no longer drops the cached session. Also adds
* object min-bounce-speed / max-speed config.
*/

# Task

- Summary: make hole cutting cheap (no FPS dip) and bound triangle growth;
  configurable object response.
- Status: PASS_WITH_HUMAN_REVIEW
- Branch: `afad20a-rebuild`.
- Result states: build PASS; 5 selftests PASS; moving holey crates now under the
  4 ms target in the probe.

# Evidence (from `logs/10-01-2026/20261001_163224/events.jsonl`)

1-second aggregated `PERFORMANCE_FRAME` windows showed `max_entity_physics` ≈
`max_destruction` up to 300-380 ms in one tick; actor physics <= 12.5 ms and
rendering <= 2.8 ms. So the FPS dips when shooting were the boolean rebuild.
The log itself was 1.2 MB (the earlier profiler/logging fix held).

# Changes

## Boolean rebuild (the FPS dip)

- `src/impact/boolean-mesh.cpp`:
  - `fillResult` no longer calls `difference.Decompose()` (it ran on every
    rebuild). Shell/component default to 1; the fracture path still uses
    `booleanDecomposePieces`.
  - `booleanSubtractIncremental(sessionId, base, cutters, maxTriangles,
    simplifyTolerance)`: after the subtraction it calls `Manifold::Simplify`
    when `simplifyTolerance > 0`, then rejects (restoring the pre-cut running
    solid, NOT committing) when over `maxTriangles`.
  - `circularSegmentsForRadius` now targets a large edge length and clamps to
    `[4, 8]` (low-poly rim ~ tens of triangles per hole, not hundreds).
- `src/impact/destructible-geometry.{h,cpp}`:
  - `maxTrianglesPerEntity` default 4096, new `meshSimplifyTolerance` member.
  - `rebuild` passes the cap + tolerance to the wrapper and computes a cheap
    `countSurfaceComponents` (vertex weld + union-find) for `componentCount`.
  - `discardPending` keeps the cached session on `ResultTooLarge` (the wrapper
    did not commit), so the next cut stays incremental instead of replaying.
- `config/destructible-world.json` `destruction`:
  `maxCutsPerEntityPerTick` 8 -> 1, `maxTrianglesPerEntity` 120000 -> 4096,
  `meshSimplifyTolerance` 0.02.

## Object response config

- `src/impact/destructible-world-config.{h,cpp}` + JSON `physics`:
  `objectMinBounceSpeed` (1.0; below it contacts project instead of bouncing)
  and `objectMaxSpeed` (60; linear speed cap).
- `src/physics/physical-entity.cpp`: applied in `resolveWorldContactVelocity`,
  `resolveEntityPairContact`, and the integration step.
- `src/impact/destructible-geometry.h` + JSON: `comOffsetFraction` 0.12 -> 0.08
  (keep the unbalanced-support trigger sensitive after the coarser cutters).
- `src/perf/perf-overlay.cpp`: `UNACCOUNTED` clamped at 0.

# Reasoning

- `docs/specs/20261001plan.md`: no FPS dips when cutting holes; bounded triangles
  ("combine triangles"); physics constants in `config/destructible-world.json`.
- Manifold `Decompose()` and high-segment cutters were the measured cost; the
  fracture-only path still needs the real decomposition.

# Measured effect (`--physical-perf-selftest`: 10 crates x 48 holes)

- Boolean build 1400 ms -> ~230-290 ms for 480 cuts.
- Triangles 43,340 -> 6,560.
- Moving holey crates 21.6 -> 2.6-3.1 ms/tick (TARGET 4 ms: MET).
- Settled 0.024 ms/tick.

# Files changed

- `src/impact/boolean-mesh.cpp`
- `src/impact/destructible-geometry.{h,cpp}`, `impact-system.cpp`
- `src/impact/destructible-world-config.{h,cpp}`
- `src/physics/physical-entity.cpp`
- `src/perf/perf-overlay.cpp`
- `config/destructible-world.json`
- `docs/regressions/2026-10-01/physical-objects-collision-REG.md` (Attempt 9)

# Pre-existing (not authored this session)

`config/accounts/default.json`, `config/analytics.json`,
`config/movement/movement-source.json`, `docs/specs/20261001plan.md`,
`src/devtools/dev-log-commands.cpp` were already modified; left untouched.

# Validation

- Build: `python build.py build-only` -> success.
- `--destructible-selftest`, `--moving-crate-selftest`,
  `--destruction-replication-selftest`, `--destruction-stress-selftest` PASS.
- `--physical-perf-selftest` PASS; moving 2.9 ms/tick (TARGET MET).
- Cold-build debt: appended `Cold-build occurrence 43`.

# Human review still needed

- Confirm shooting holes no longer dips FPS; holes look acceptable (coarse rim).
- Confirm the holey crate/triangle triangle budget (4096) is enough for the
  intended hole counts; raise `maxTrianglesPerEntity` if holes stop appearing.
- Retest the crate on a cylinder and player phasing in corners.

# Not done (open)

- Deep depenetration (stuck/through-floor, player phasing), heavy-cut COM
  verification, config-triangle weapon hitboxes, overlay word wrap, two-client.
