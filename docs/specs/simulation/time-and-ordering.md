# Time and ordering

Status: target research direction; current gameplay is fixed 60 Hz.
Scope: one simulation clock, dev-only time scaling, tick-rate benchmarks, and
event-order semantics.

## Current facts

- `GAMEPLAY_SIMULATION_HZ = 60` in `src/network/simulation-constants.h:16`.
- Hardcoded 1/60 remains in:
  - `src/sim/simulate-tick.cpp:89`
  - `src/engine/fixed-tick-clock.h:14`
  - `src/gui/hud/ui-tick-clock.h:17`
  - `src/network/server.cpp:2501`
- Server accumulator uses `SERVER_DT` and `MAX_STEPS = 5`
  (`src/network/server.cpp:1254,1530`).
- `docs/specs/networking/networking.md:400-540` records the 60 Hz contract and
  the future 120/240/serverless direction.
- `docs/specs/architecture/nodes-and-connections.md` defines time and space as
  projections of facts, not inherent dimensions.

## T0 — one clock owner

Add one owner, for example `src/sim/simulation-clock.h/.cpp`:

```text
hz
dt
ticksToSeconds
secondsToTicks
logical tick
dev-only wall-time scale
```

Replace every hardcoded 1/60 site. Audit all tick-to-seconds and
seconds-to-ticks conversions: weapons, cooldowns, NPC timers, perception
memory, network validation, interpolation, input sampling, AI decisions.

Default remains 60 Hz. Overrides are dev/test only.

## T1 — tick-rate benchmark

Run a fixed headless/pinned scenario at 60/120/240/999 Hz. Measure:

- CPU per tick and worst actor;
- server tick budget and overruns;
- snapshot size/cadence;
- input sampling and client interpolation;
- determinism at sample ticks.

999 Hz is a stress benchmark, not a promised playable mode. Do not tie
gameplay truth to frame rate or wall clock.

## T2 — slow motion and single-step

Dev-only wall-clock scale:

- 0.1x, 0.5x, 1x, 2x;
- pause and single-step;
- same fixed dt and same event order;
- server-authoritative in multiplayer; clients must agree.

This is for debugging/TAS and human observation, not normal gameplay balance.

## T3 — event order instead of wall-clock Hz

Time is event order and causal edges, not a global wall-clock tick. A tick is a
batch boundary.

Event record:

```text
logical_time
entity/actor
event_kind
causal_parents
payload
stable_sequence/tie-break
```

Rules:

- Simulation consumes events in deterministic order.
- Wall time schedules input/render only.
- Network snapshots and render frames are projections of the event/state graph.
- Same input + same initial primitive state + same event order reproduces the
  same macro outcome.
- No hidden wall-clock reads in gameplay decisions.

Use the nodes/connections ontology:
`nodes + typed connections + attributes + provenance + version`. Temporal and
spatial facts are edges/attributes; no second clock or second world.

## T4 — primitive-state emergence

A separate headless toy world first; shadow-compare with real event chains.

Minimal primitive state:

- stable ID, position, velocity, orientation, up, spin;
- mass, health, team, owner, alive/dead, held action;
- discrete bits such as grounded, up/down, hostile, supported.

Local rules produce:

input → intent → movement → contact → support/push/damage → death → NPC target
→ projectile/explosion → objective → GUI/audio projection.

Acceptance:

- one seed/impulse produces a traceable causal chain into NPC behavior, rocket
  explosion, collision, vehicle response, and GUI;
- the same primitive state and event order reproduce the macro outcome;
- no subsystem has a second hidden source of truth;
- the event DAG does not destroy the tick budget.

Do not rewrite the engine before the toy world proves propagation, determinism,
and performance.

## Related

- `docs/architecture/ecs-entity-etc/ecs.md`
- `docs/architecture/live-development/hot-kernel.md`
- `docs/specs/architecture/nodes-and-connections.md`
- `docs/specs/networking/networking.md`
- `docs/specs/tas/tas.md`
