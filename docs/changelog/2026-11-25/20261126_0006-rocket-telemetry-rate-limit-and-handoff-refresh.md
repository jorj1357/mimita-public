# Rocket League telemetry rate-limit and handoff refresh

# Task

- Task ID: work-4ef3f8d0ccd1
- Session ID: session-fd8b6d32e148
- Owner: root-resource-loop
- Focus: vehicle telemetry volume, stale RL handoff, live proof audit
- Summary: `vehicle.state` was emitted unconditionally on every 120 Hz entity
  tick, making it 71.4% of a 227 MB server journal in ~6 min and rotating proof
  evidence away every ~2 minutes. Gated `vehicle.state` to 10 Hz and
  `vehicle.bounce` to the spec 10 Hz. Refreshed the stale Rocket League handoff
  with the now-proven jump/air-roll/flip/settle state and the remaining gaps.
- Status: complete for the telemetry change; behavior proofs (dodge, powerslide,
  flip reset) remain open and are recorded in the handoff and issues.
- Date, time, timezone: 2026-11-26T00:06:00Z
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: f9d94c5bb3cffd8bb15899cdda30eea3264c4a66
- Final commit: f9d94c5bb3cffd8bb15899cdda30eea3264c4a66 (no commit made)

# Requested behavior

Continue `docs/handoffs/20261125_rocket-league-mode-handoff.md` and prove the
implemented Rocket League vehicle behavior from live `events.jsonl`, while the
user's standing concerns are byte/CPU waste and "0 wasted bytes".

# Exact implementation changes

## File: `src/vehicles/vehicle-system.cpp`

- Function `VehicleSystem::updateArcadeControls`:
  - `vehicle.state` emission is now gated by `if (tick % 12u == 0u)` where
    `tick` is `PhysicalEntitySystem::simulationTick()` (120 Hz). Previously
    unconditional, i.e. 120 Hz per vehicle.
  - `vehicle.bounce` condition changed from `bounceTick % 6u == 0u` to
    `bounceTick % 12u == 0u`, restoring the 10 Hz rate that
    `docs/specs/vehicles/vehicles.md` §24 specifies.

## File: `docs/handoffs/20261125_rocket-league-mode-handoff.md`

- Replaced the stale 22:17 version. Corrected the "cannot jump, air roll, or
  rotate" claim, added the proven evidence table, the generation 657 telemetry
  change, corrected open defects, and next actions with commands.

# Changed files and reverse steps

| File | Before | After | Reverse |
|---|---|---|---|
| `src/vehicles/vehicle-system.cpp` | `vehicle.state` 120 Hz; `vehicle.bounce` 20 Hz | `vehicle.state` ~10 Hz; `vehicle.bounce` 10 Hz | revert the two rate expressions |
| `docs/handoffs/20261125_rocket-league-mode-handoff.md` | stale 22:17 handoff | refreshed handoff | restore previous content |

# Diagnostics

- Owner/category: PHYSICS, Important.
- Input: entity simulation tick.
- Decision: emit consolidated state at 10 Hz instead of every tick.
- Rate limiting: `tick % 12` and `bounceTick % 12`.
- Failure or rejection reason: not applicable.

# Validation

- Build: requested through the root dev-loop (`--request batch-build`).
  Generation 657 published (`build/game/generation-657/game-g657.dll`,
  revision `56c4855e1ec9fa3540a8479c7699d2a34777462e67b26c00a5c47c34e885e9e9`,
  sha256 `26e610066a0778ca8070ad0cac64e1f103281bef9e847e887090dcc107696bbc`;
  compile 94.891 s, link 10.485 s, total 129.5 s).
- Runtime: isolated pinned run `20261125_235847`
  (`logs/2026-11-25/20261125_235847/server-01/events.jsonl`, `run.json`
  `game_generation: 657`, pinned true). Server simulated 43.33 s.
  - `vehicle.state`: 401 events for each of 2 vehicles over 43.33 s =
    9.25 Hz/vehicle (was ~120 Hz).
  - `vehicle.bounce`: 401 each = 9.26 Hz/vehicle (was ~20 Hz).
- Not proven: steady-state byte/min reduction under a long session; the
  isolated test run failed its readiness gate before steady state
  (`client did not observe active mode after 2405 server ticks`). That gate is
  separate from the fixed dashboard launch mode; recorded on
  `issue-b5a35be7b0e0`.

# Measured evidence

- Before (generation 656, live run `20261125_233748`, all server segments):
  128,205 events / 227,235,851 bytes in ~6 min; `vehicle.state` 96,239 events /
  162,338,645 bytes = 71.4%; `vehicle.bounce` 16,039 events = 20.9%, with 8,537
  at rest (speed < 0.1, grounded 4).
- After (generation 657, run `20261125_235847`): 9.25 `vehicle.state`/s/vehicle,
  9.26 `vehicle.bounce`/s/vehicle.
- Projection: ~65% total journal-byte reduction at the same workload; not a
  measured long-session figure.

# Regression review

- Regression entry appended: no.
- Why: this is a logging-rate change, not a confirmed gameplay regression. The
  higher rate was not a spec requirement for `vehicle.state`; `vehicle.bounce`
  was above its 10 Hz spec because the entity step moved to 120 Hz.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not performed.
- Multiplayer review: not performed.
- Still unverified: dodge, powerslide effect, flip reset, ceiling/wall drive,
  landing penetration, crash root cause, prediction.

# Related issues

- `issue-3f776ce60511` (storage/log retention): journal-volume evidence + fix.
- `issue-e5a086b0a441` (RL capability): current proof state + remaining gaps.
- `issue-b5a35be7b0e0` (launch): isolated test-run readiness still fails.
- `issue-ed3ac775283d` (CPU): journal I/O note.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / f9d94c5bb3cffd8bb15899cdda30eea3264c4a66 / f9d94c5bb3cffd8bb15899cdda30eea3264c4a66
- Generation: 657
- Artifact: build/game/generation-657/game-g657.dll
- Journal paths: logs/2026-11-25/20261125_233748/server-01/ (before);
  logs/2026-11-25/20261125_235847/server-01/events.jsonl (after).
