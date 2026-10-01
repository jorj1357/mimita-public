// 2026-09-30T21:56:34Z (display: 2026-09-30 17:56:34 EDT)
/* purpose
* Record the second destructible pass after human gameplay feedback: repeated
* shots now keep cutting/deepening, a burst of cuts is queued and applied under
* a per-tick budget instead of synchronously per shot, no-op cuts skip all
* conversion work, and a config-driven penetration count exists for tunneling.
*/

# Task

- Summary: Make shooting an existing hole keep removing material, batch rapid
  cuts into one rebuild per entity per tick under a budget, skip work for cuts
  that remove nothing, and add a config-driven projectile penetration count.
- Status: CODE_COMPLETE / BUILD_VERIFIED / SELFTEST_PASS /
  RUNTIME_VISUAL_VALIDATION_REQUIRED
- Branch: `afad20a-rebuild`; base commit `61cd8e92`.

# Human feedback driving this pass

- Repeated shots at the same hole did not cut again.
- Rapid fire into one crate caused severe lag (one synchronous rebuild per shot).
- Wanted: repeated shots deepen; a queue/budget so a burst cannot stall a frame;
  optional penetration; simplification later.

# Changes

## Queue + budget (the main performance fix)

- `DestructibleGeometry` gains `pendingCutCount`; `DestructibleGeometrySystem`
  gains `enqueueCut` and `flushQueuedCuts(geometry, maxCutsThisFlush)`.
  `rebuild` now applies only the not-yet-applied cutters (bounded by a cap) and
  skips the entire conversion when nothing changed.
- `ImpactSystem::submit` now **only enqueues** the cut into the authoritative
  history and returns `pending = true`; it no longer rebuilds synchronously.
- New `ImpactSystem::flushPendingCuts(maxCutsPerEntity, budgetMs)` runs once per
  fixed tick from `PhysicalEntitySystem::advanceKinematics`. It applies one
  batched rebuild per entity, capped by `kMaxCutsPerEntityPerTick` (8) and
  `kCutBudgetMsPerTick` (2.0 ms); leftovers drain on later ticks. It owns
  publishing `localTriangles`, mass refresh, and fracture.
- A burst of N shots now costs ~O(triangles) per tick instead of O(N ×
  triangles).

## No-op cut detection

- `BooleanCutResult` gains `changed`. `booleanSubtractIncremental` compares the
  running solid's volume before/after; if a cutter removed no material it sets
  `changed = false` and returns **without** the O(triangle) `Decompose` +
  `GetMeshGL` + conversion. This also stops a repeat shot into already-empty
  space from reporting a false "created" cut and from doing wasted work.

## Repeat-shot deepening

- The deferred queue plus no-op detection means a shot that reaches new material
  keeps cutting; a shot into empty space is a cheap no-op instead of a stall.
  New self-test 22 shoots the exact same ray repeatedly (finding the current
  first surface each time) and asserts cuts keep registering and volume keeps
  dropping.

## Config-driven penetration

- `WeaponDefinition::penetrationCount` (+ `penetration_count` JSON, default 1)
  and `penetrationsRemaining` on `ServerProjectile`/`NetworkProjectile`.
  On an entity impact with penetrations left, the bolt cuts the surface and
  advances past it instead of exploding, in both the server and client
  prediction loops. Default 1 preserves the previous single-surface behavior.

## Docs

- `docs/specs/manifold-destructible-integration-plan.md`: 10.1 penetration note
  and an 11 "destruction queue + budget" subsection.
- `config/weapons.json`: `projectile_rifle` gains `"penetration_count": 1`.

# Reasoning

- Spec alignment: `destructible-world.md` 44 (explicit work budgets, split heavy
  operations into incremental jobs) and 17/19 (penetration as a physical
  projectile property). The queue is the "handle as much as you can this tick"
  behavior the human asked for.
- The per-cut O(triangle) conversion (Decompose + GetMeshGL + fillSurface + mass)
  was the dominant, growing burst cost; batching makes it once per tick and
  no-op detection removes it for void contained cutters.
- Tests required immediate geometry, so `submitRifle` flushes explicitly. The
  production path stays deferred.

# Validation

- Build: `python build.py build-only` -> BUILD SUCCESS.
- `mimita.exe --destructible-selftest` -> PASS, incl. new
  "shooting the same ray repeatedly keeps registering cuts", "repeated shots on
  one ray keep removing material", "the same hole deepens instead of staying
  fixed", and the existing one-shot/force/size/tunnel checks.
- `mimita.exe --moving-crate-selftest` -> PASS.
- `mimita.exe --destruction-replication-selftest` -> PASS.
- NOT performed: live burst FPS measurement and two-client visual confirmation.

# Pre-existing work

- All prior Manifold/destructible/replication/fracture/hole-size work is
  uncommitted from earlier sessions and was extended, not re-authored.
  Pre-existing uncommitted edits across `config/*`, `devscripts/*`, `src/*`
  (other files), and dated docs were left untouched.

# Documents and skills reviewed

- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/specs/destructible-world/destructible-world.md` (17, 19, 44)
- `docs/specs/manifold-destructible-integration-plan.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/efficiency-checker-v1.md`

# Human review still needed

- Rapid-fire into a crate: confirm no frame spike and that holes appear within a
  frame or two (the queue drains).
- Shoot an existing hole: confirm it keeps deepening.
- Set `"penetration_count": 3` in `weapons.json` for `projectile_rifle` and
  confirm a single shot bores through several surfaces.
- Triangle simplification / lower cutter tessellation is still pending and is
  the next lever if triangle growth is still a problem.