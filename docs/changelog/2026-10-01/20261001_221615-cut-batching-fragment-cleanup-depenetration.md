// 2026-10-01T22:16:15Z (display: 2026-10-01 18:16:15 EDT)
/* purpose
* Record the pass that aggregated cut batches, fixed multiplayer mesh
* determinism (removed per-batch Simplify), added fragment size/idle deletion,
* and added a deep-depenetration recovery pass.
*/

# Task

- Summary: near-instant but batched holes, deterministic multiplayer geometry,
  fragment cleanup, and no falling through/sticking in geometry.
- Status: PASS_WITH_HUMAN_REVIEW
- Branch: `afad20a-rebuild`.
- Result states: build PASS; 5 selftests PASS; full-auto max < 4 ms.

# Baseline

Attempt 10 is human-confirmed good for performance/queue (sustained 2 fps gone).
This pass keeps that and adds the requested items.

# Changes

- **Aggregate-then-batch cuts (G1).** `config/destructible-world.json`
  `destruction.cutBatchIntervalTicks` (8) + `cutBatchMax` (24) replace the fixed
  per-tick cap. `ImpactSystem::flushPendingCuts` collects cut intents and applies
  ALL of an object's queued cuts in one boolean op once the interval passes (or
  the queue hits the cap). `DestructibleGeometry::lastCutFlushTick` tracks the
  cadence; tests flushing at tick 0 bypass it.
- **Multiplayer determinism (removed per-batch Simplify).**
  `meshSimplifyTolerance` default/JSON 0. Per-batch `Manifold::Simplify` made the
  final mesh depend on batching, so a client applying cuts one at a time
  diverged. The coarse cutter already bounds growth.
- **Fragment cleanup (G2).** `fragmentInstantDeleteMaxDimMeters` (0.25),
  `fragmentDeleteMaxDimMeters` (0.5), `fragmentIdleDeleteSeconds` (5). A fragment
  whose largest AABB dimension is below the instant threshold is deleted now;
  below the delete threshold it is deleted once undisturbed for the idle time
  (`PhysicalEntity::lastInteractionTick`, set on any impulse). Long thin shards
  are kept.
- **Deep-depenetration recovery (G3).** Added `pointInsideWorldTriangle` and
  `recoverDeepPenetration`; a Dynamic body with no contacts whose vertices are
  inside a nearby world triangle is pushed out along its normal, bounded by
  `physics.recoveryFeatureRadius` (0.5). Gated on a genuine inside test so a
  crate merely falling toward a floor is not lifted. `collectActorMeshContactsInto`
  gained an optional `featureRadius` (default unchanged).
- Config comments for every new field; new selftest for the embedded-body push.

# Reasoning

- The user wants aggregation + batching (collect intent, then do the work) rather
  than tuning single numbers; determinism is required for multiplayer; fragments
  get no special logic, just better (deep) contacts; the 4 ms / 240 fps floor.

# Files changed

- `src/impact/impact-system.cpp`, `destructible-geometry.{h,cpp}`,
  `destructible-world-config.{h,cpp}`
- `src/physics/physical-entity.{h,cpp}`
- `src/physics/movement/physics-collision-shared.h`, `physics-collision-mesh.cpp`
- `config/destructible-world.json`
- `docs/regressions/2026-10-01/physical-objects-collision-REG.md` (Attempt 11)

# Pre-existing (not authored this session)

`config/accounts/default.json`, `config/analytics.json`,
`config/movement/movement-source.json`, `docs/specs/20261001plan.md`,
`src/devtools/dev-log-commands.cpp` were already modified; left untouched.

# Validation

- Build: `python build.py build-only` -> success.
- `--destructible-selftest`, `--moving-crate-selftest` (incl. new embedded-body
  push), `--destruction-replication-selftest`, `--destruction-stress-selftest`,
  `--physical-perf-selftest` PASS.
- Full-auto 200 cuts: avg ~1.4-2.0 ms, max < 4 ms per flush; moving holey crates
  ~2.9 ms/tick.
- Cold-build debt: appended `Cold-build occurrence 45`.

# Human review still needed

- Full-auto into a crate: confirm dips are gone and holes appear promptly.
- Fragments: small chunks disappear; long shards remain; no falling through the
  floor / sticking in walls.
- Cylinder: hole-side contacts no longer bounce up (G4 not done this pass).

# Not done (open)

- G4 irregular-geometry contact normals; near-cylinder broadphase; config-triangle
  weapon hitboxes; overlay word wrap.
