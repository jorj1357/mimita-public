// 2026-10-01T20:02:12Z (display: 2026-10-01 16:02:12 EDT)
/* purpose
* Record the pass that stopped the profiler from logging every frame (the
* perf_report inaccurate/FPS-drop cause), replaced the special settling with a
* configurable "has not moved" freeze, exposed restitution/friction, and added
* destruction/moving-crate perf measurement.
*/

# Task

- Summary: profiler cadence + honest overlay; natural + configurable settling
  freeze; destruction timing; projectile-impact logging; moving-crate perf probe.
- Status: PASS_WITH_HUMAN_REVIEW
- Branch: `afad20a-rebuild`.
- Result states: build PASS; 5 selftests PASS; runtime FPS still pending.

# Evidence

`logs/10-01-2026/20261001_153220/events.jsonl` (259 MB): 280,871
`PERFORMANCE_FRAME` rows. `PERFORMANCE_FRAME` was gated on `deepProfiling`, and
`perf_report` sets `deepProfiling=true`, so it logged once per frame. This is why
`perf_report` disagreed with the felt FPS: the profiler was a large part of the
frame and its `CPU` total is the timer-scope sum including that logging.

`--physical-perf-selftest` (new moving phase): 10 crates x 48 holes = 43,340
triangles. Settled 0.026 ms/tick, **moving 21.6 ms/tick** — the quantified
target for the local-space refactor.

# Changes

## P0 — Profiler/logging cadence and honest overlay

- `src/perf/perf.cpp`: `PERFORMANCE_FRAME` is aggregated over ~1 s and emitted
  once per second (avg + max per subsystem, plus max npcs/effects/draw calls),
  never per frame and not gated on `deepProfiling`. The spike report is
  rate-limited to once per second. Added `PerfState.maxFrameTimeMs`,
  `maxPhysicsMs`, `maxEntityPhysicsMs`, `maxRenderingMs`, reset when
  `perf_report` opens.
- `src/perf/perf-overlay.cpp`: shows `MAX` frame + per-subsystem worsts and an
  `UNACCOUNTED` line (frame minus named timers), so a single stall and an
  incomplete breakdown are both visible.
- `config/debuglogger.json`: `network` `verbose` -> `important` (66k records/run).

## P1 — Settling redesign + config expansion

- `src/physics/physical-entity.cpp`: removed the special settle block
  (`supportedFrictionRetain` velocity kill, artificial righting torque,
  `angularVelocity *= 0.55`, `isRestingOnFace` sleep gate). Added
  `updateSettling`: an object that stays within `sleepMoveThresholdMeters` of an
  anchor for `sleepRequiredTicks` fixed ticks freezes (zero velocity, sleeping)
  until disturbed; it never freezes while penetrating > 0.05 m. Waking resets the
  anchor. Ran in the early-skip, no-contact, and contact paths.
- `src/physics/physical-entity.h`: `sleepAnchorPos`, `sleepAnchorValid`.
- `src/impact/destructible-world-config.{h,cpp}` + `config/destructible-world.json`:
  new `sleepMoveThresholdMeters` (0.1), `sleepRequiredTicks` (20),
  `objectRestitution` (0.1), `objectFriction` (0.6). New dynamic entities take
  restitution/friction from config.
- Effect: settled crates are effectively free (0.026 ms/tick in the probe),
  and settling is a natural result of collisions plus the freeze rule.

## P3/P5 — Destruction timing + projectile-impact log

- `src/impact/impact-system.cpp`: `Perf::ScopedTimer("Destruction")` wraps
  `flushPendingCuts` (feeds `PerfTimes.destruction`), and `submit` emits a
  throttled `PROJECTILE_IMPACT` event (entity, mass, speed, energy, impulse,
  velocity/angular before+after) to `events.jsonl`. `config/debuglogger.json`
  `physics` -> `important`.

# Reasoning

- `docs/specs/20261001plan.md` and the user's playtest notes: no FPS drops,
  natural settling, config-driven physics constants, measured cost.
- A profiler that logs every frame changes the thing it measures; aggregate and
  rate-limit.

# Files changed

- `src/perf/perf.h`, `src/perf/perf.cpp`, `src/perf/perf-overlay.cpp`
- `src/physics/physical-entity.{h,cpp}`
- `src/impact/destructible-world-config.{h,cpp}`, `src/impact/impact-system.cpp`
- `config/destructible-world.json`, `config/debuglogger.json`
- `docs/regressions/2026-10-01/physical-objects-collision-REG.md` (Attempt 8)

# Pre-existing (not authored this session)

`config/accounts/default.json`, `config/analytics.json`,
`config/movement/movement-source.json`, `docs/specs/20261001plan.md`,
`src/devtools/dev-log-commands.cpp` were already modified; left untouched.

# Validation

- Build: `python build.py build-only` -> success.
- `--physical-perf-selftest` PASS (settled 0.026 ms/tick; moving 21.6 ms/tick
  reported as TARGET MISS).
- `--destructible-selftest`, `--moving-crate-selftest`,
  `--destruction-replication-selftest`, `--destruction-stress-selftest` PASS.
- Cold-build debt: appended `Cold-build occurrence 42`.

# Human review still needed

- Open `perf_report` and confirm the numbers now track the felt FPS and that the
  frame no longer inflates from profiling. Read the 1/s `PERFORMANCE_FRAME`
  records (avg/max, `max_destruction_ms`, `max_entity_physics_ms`).
- Confirm crates now freeze when still and settle naturally at any density.

# Not done (open)

- Local-space entity surfaces + entity broadphase (the 21.6 ms moving target),
  deep depenetration (stuck/through-floor), config-triangle weapon hitboxes.
