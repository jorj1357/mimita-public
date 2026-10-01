// 2026-10-01T19:28:50Z (display: 2026-10-01 15:28:50 EDT)
/* purpose
* Record the pass that found and fixed a large self-inflicted frame cost: the
* actor-triangle solver built and wrote per-contact debug JSON every fixed tick,
* and the collision category was at trace, producing 74 MB - 4 GB events.jsonl
* files. Also adds a headless entity-physics perf self-test.
*/

# Task

- Summary: Stop per-contact collision debug JSON/logging in the fixed tick; add
  a headless perf guard for the entity/destruction physics path.
- Status: PASS_WITH_HUMAN_REVIEW
- Branch: `afad20a-rebuild`.
- Result states: build PASS; all selftests + new perf selftest PASS; runtime
  FPS acceptance still pending.

# Evidence from the user's logs

`logs/10-01-2026/20261001_152118/events.jsonl` (74 MB, ~112k records) was
**99,715 COLLISION events**; previous runs were 2.2 GB / 4.1 GB. The spill was
`src/physics/movement/actor-triangle-solver.cpp` emitting
`collision.contact.before_response` / `after_response` per contact per solve
iteration and `collision.solve.summary` per solve, all `correlation_id
"slope-edge-investigation"`, with `config/debuglogger.json` `collision: trace`.

Numeric `PERFORMANCE_FRAME` (added last pass) confirmed the breakdown
incompleteness: a 42.4 ms frame had `simulation_ms` 13.5, `rendering_ms` 1.2,
`networking_ms` 0.56, `entity_physics_ms` ~0 — the rest is unaccounted
(`MIMITA_PERF_SCOPE` stages, swap/sleep, and the collision logging itself).

# Changes

## Stop the per-contact debug JSON in the fixed tick

- `src/physics/movement/actor-triangle-solver.cpp`: the `contactFields` JSON and
  the `before_response`/`after_response` writes are now built only when
  `shouldLog(Collision, Trace)`; the `solve.summary` JSON is built only when
  `shouldLog(Collision, Verbose)`. Previously the `nlohmann::json` object was
  constructed unconditionally for every contact (the logger's early-out came
  after construction), so the cost was paid even with logging disabled.
- `config/debuglogger.json`: `collision.level` `trace` -> `off` and
  `file_output` false (the slope-edge investigation is complete).

## Headless entity-physics perf self-test

- `src/physics/physical-entity.{h,cpp}` + `src/game/game-cli.cpp`: added
  `--physical-perf-selftest`. It builds a floor world, spawns 10 dynamic
  destructible crates each riddled with 48 holes (43,340 collision triangles
  total), settles them, and times a fixed tick. No window required, so it is an
  automated guard for the <4 ms frame work.
- Measured: **~1.2-1.4 ms per fixed tick** for 10 holey crates, stable over
  runs, PASS under the 4 ms budget.

# Reasoning

- `AGENTS.md`: use centralized categorized logging; do not create unmanaged
  debug cost. The per-contact trace logging was a completed investigation left
  enabled, and its JSON construction dominated the fixed tick.
- Making the measurement (perf self-test) repeatable lets these changes be
  verified without a human run.

# Files changed

- `src/physics/movement/actor-triangle-solver.cpp`
- `config/debuglogger.json`
- `src/physics/physical-entity.{h,cpp}`
- `src/game/game-cli.cpp`
- `docs/regressions/2026-10-01/physical-objects-collision-REG.md` (Attempt 7)

# Pre-existing work (not authored this session)

`config/accounts/default.json`, `config/analytics.json`,
`config/movement/movement-source.json`, and `docs/specs/20261001plan.md` were
already modified and were left untouched.

# Documents and skills reviewed

- `AGENTS.md`, `docs/ROUTER.md`
- `docs/specs/debug-logging/debug-logging.md` (centralized logging intent)
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/skills/efficiency-checker-v1.md`
- `docs/regressions/README.md`

# Validation

- Build: `python build.py build-only` -> success; relinked `mimita.exe`.
- `mimita.exe --physical-perf-selftest` PASS, `perTick` 1.17-1.40 ms over 3 runs.
- `--destructible-selftest`, `--moving-crate-selftest`,
  `--destruction-replication-selftest`, `--destruction-stress-selftest` PASS.
- Cold-build debt: appended `Cold-build occurrence 41`.

# Human review still needed

- Re-run a holey-crate session and confirm the FPS drops/spikes are gone and
  `events.jsonl` is small. If not, check `network.level` (still `verbose`) and
  read the numeric `PERFORMANCE_FRAME` records for the next dominant term.

# Not done this pass (open)

- Full local-space entity surface refactor, entity broadphase, deep
  depenetration (no tunneling/phasing/sticking, fragment-through-floor), and
  config-triangle weapon hitboxes. Tracked in the regression record.
