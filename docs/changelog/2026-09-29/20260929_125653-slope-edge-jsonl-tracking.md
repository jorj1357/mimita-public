# Task

- Task ID: slope-edge-jsonl-tracking
- Summary: Port v9 central structured logging toward the v8 events.jsonl model and instrument the active actor-triangle collision response.
- Status: build verified; live JSONL reproduction still required
- Date, time, timezone: 2026-09-29T12:56:53Z, ISO 8601 UTC; display timezone America/New_York
- Branch: current working branch
- Base commit: not captured; pre-existing worktree changes preserved
- Final commit: not applicable

# Pre-existing changes

- The worktree already contained unrelated gameplay, collision, network,
  configuration, and documentation edits.
- No pre-existing source changes were reverted.

# Requested behavior

Understand the likely slope/edge snag in code and replace ad-hoc text logging
with the v8-style append-only `events.jsonl` event stream so a live run gives
usable collision updates.

# Specification alignment

- `docs/specs/debug-logging/canonical-jsonl.md` requires one authoritative
  append-only `logs/<yyyy-mm-dd>/<yyyymmdd_hhmmss>/events.jsonl` stream.
- `docs/specs/debug-logging/debug-logging.md` requires structured fields,
  correlation, fixed-tick movement/collision evidence, central ownership, and
  bounded output.
- `docs/architecture/collision/collision.md` requires fixed 60 Hz collision
  and one authoritative contact/response path.
- The v8 reference implementation was inspected at
  `C:\mimita-priv-v8\src\debug\structured-log.{h,cpp}` and
  `src\hot-reload\packages\collision\collision-log.h`.

# Most likely code seam

The walkable-edge change in `physics-collision-glb-sweep.cpp` is not the entire
active player path. The player path enters `solveActorTriangleCollision()` and
then performs contact collection, raw-normal merging, close-feature collapse,
response-normal selection, velocity response, and remaining-movement sliding.
The most likely class of failure is therefore a later manifold/response-stage
decision or overwrite, not necessarily the original sweep edge normal. The new
events distinguish those possibilities without claiming a root cause early.

# Exact implementation changes

## `src/debug/structured-log.h` / `src/debug/structured-log.cpp`

- Added typed `StructuredLogger::writeEvent(...)`.
- Replaced central structured category-file output with one append-only JSONL
  stream at `logs/yyyy-mm-dd/yyyymmdd_hhmmss/events.jsonl`.
- Added `MIMITA_EVENTS_FILE` support so a launcher can point client and server
  at one shared file.
- Added universal wall time, monotonic time, sequence, run ID, PID, process,
  level, category, event name, event ID, tick, source, line, function, and
  typed fields.
- Added `logger.started` and `logger.stopped` records.
- Retired central logger creation of per-category `.txt` files.

## `src/physics/movement/actor-triangle-solver.cpp`

Added events around the active response boundary:

- `collision.contact.before_response`
- `collision.contact.after_response`
- `collision.solve.summary`

Fields include position/velocity before and after, intended and remaining
movement, triangle index, actor part, entity ID, contact point, depenetration,
response and surface normals, penetration, time of impact, walkable/near-feet
classification, contact count, iteration count, and `movementSimulationTick`.

## `config/debuglogger.json`

Enabled the collision category at `trace` with JSONL output so the next run
actually records the investigation. This is hot-reloadable and can be returned
to `off` after the reproduction.

# Validation

- `python build_agent.py`: first attempt failed only because the new probe used
  nonexistent `Player::tick`; corrected to `Player::movementSimulationTick`.
- Corrected `python build_agent.py`: `Status: SUCCESS`.
- `.dev/builds/0381/mimita.exe --collision-selftest`: PASS, including
  `walkable floor edge keeps face normal` and the complete collision self-test.
- `git diff --check`: passed for the changed source/config paths.

# Runtime evidence

Not yet collected. The next live run must verify the actual printed
`events.jsonl` path and capture the same-tick before/after/summary sequence
while reproducing the slope snag.

# Human acceptance

Still required: launch the new build, walk into the reported slope edge, then
inspect the JSONL records. Build and deterministic self-test success do not
prove the live movement feel or identify the final root cause.

# Related regression

`docs/regressions/2026-09-29/slope-edge-snag-REG.md`
