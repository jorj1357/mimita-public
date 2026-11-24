# Handoff: live evidence, session snapshots, time/order, and emergent primitives

Status: documentation + prototype handoff. The only implemented piece is the
read-only `devscripts/evidence-watch.py` observer and the gold evidence note.
The session snapshot and time/order/emergence work are specified, not
implemented.

Related specs:

- `docs/specs/dev-loop/live-evidence-observer.md`
- `docs/specs/simulation/session-snapshot.md`
- `docs/specs/simulation/time-and-ordering.md`
- `docs/specs/simulation/quantum-inspired-and-literal-quantum-research.md`
- `docs/gold/2026-11-24-juggernaut-horde-overwhelming-human-evidence.md`

## User decisions that constrain the work

1. **Session snapshot first.** Capture actors, round/objective, weapons/ammo,
   projectiles, area effects, and RNG. Declare destructible-world and vehicle
   limits until they are covered.
2. **Time/tick overrides are dev/test only.** Default gameplay remains 60 Hz.
   120/240/999 Hz and slow motion are experiments, not a shipped mode.
3. **Live evidence observer is on-demand** (`list/show/tail`) with an optional
   background registry index.
4. **999 Hz is a stress benchmark**, not a promised playable rate.
5. **The bigger direction is primitive-state emergence**: simple states
   (spin/up/down, position, velocity, health, team, owner) propagate through
   local rules into NPC behavior, rockets, collisions, vehicles, and GUI.
6. **Discrete/event-driven model first; literal-quantum research stays a
   separate branch.** Do not mix literal quantum amplitudes into authoritative
   gameplay.

## What already exists

### Gold evidence note

`docs/gold/2026-11-24-juggernaut-horde-overwhelming-human-evidence.md` records
the user’s report that Juggernaut Horde NPCs felt overwhelming and gold. It
links the live Juggernaut Horde runtime and the measured tick/callsite context.
This is human acceptance, not numeric proof.

Live candidate: run `20261124_022440_01`, Juggernaut Horde, 67 NPCs / 1 player,
server PID 33892, journal
`logs/2026-11-24/20261124_022440_01/server-01/events.jsonl`.

### Evidence observer prototype

`devscripts/evidence-watch.py` is implemented and exercised:

```text
python devscripts/evidence-watch.py list
python devscripts/evidence-watch.py show 33892
python devscripts/evidence-watch.py tail 33892 --event network.mode.start --lines 3
python devscripts/evidence-watch.py watch --once
python devscripts/evidence-watch.py capture 33892 --out <bundle>
```

It discovers MiMITA processes through `windows_process_inventory`, maps PIDs to
run manifests and `logger.started` records, and reads journals read-only. It
never adopts, stops, attaches to, or edits a process. `capture` currently copies
full journal segments; keep the output bounded before using it on very large
journals.

Known limitations to fix before calling the tool complete:

- full journal scan on first `list`; add a bounded index/cache;
- journal mapping for older human-started processes without `run.json`;
- `tail` re-reads from the start on each poll; add per-file offsets;
- role verification for external processes;
- capture size cap/compression;
- optional dashboard/coordination integration.

### Existing foundations for snapshots

- `src/devtools/scenario-server-recorder.cpp` already emits
  `scenario.record.checkpoint` with actors, NPC AI, projectiles, RNG, and a
  completeness flag.
- `src/devtools/scenario-server-playback.cpp` restores actors, NPC AI, and RNG,
  and explicitly records unrestored components (`world_mutable`, some
  projectile/state cases).
- `scenario_play <path.jsonl>` restores a recorded start state.
- `agent.snapshot` is a diagnostic client snapshot, not a save-state.

### Existing time/tick facts

- `GAMEPLAY_SIMULATION_HZ = 60` in `src/network/simulation-constants.h:16`.
- Hardcoded 1/60 remains in:
  - `src/sim/simulate-tick.cpp:89`
  - `src/engine/fixed-tick-clock.h:14`
  - `src/gui/hud/ui-tick-clock.h:17`
  - `src/network/server.cpp:2501` (listen server)
- Server accumulator: `SERVER_DT`, `MAX_STEPS = 5` at
  `src/network/server.cpp:1254,1530`.
- `docs/specs/networking/networking.md:400-540` already records the 60 Hz
  contract and future 120/240/serverless direction.
- `docs/specs/tas/tas.md` already describes reset, slow motion, and future save
  states.
- `docs/specs/architecture/nodes-and-connections.md` defines time and space as
  projections of node/connection/event facts. Use it; do not invent a second
  ontology.
- `docs/architecture/ecs-entity-etc/ecs.md` and
  `docs/architecture/live-development/hot-kernel.md` define the ECS/event
  direction and the kernel/hot-gameplay boundary.

## Workstream 1 — live evidence observer

Target spec: `docs/specs/dev-loop/live-evidence-observer.md`.

Next steps:

1. Add a bounded journal index at `.dev/live-evidence/registry.json` with
   process identity, role, run ID, build, mode/map, journal segments, last event
   time, and proof links.
2. Map external/human-started processes to journals by PID/start time/exe and
   `logger.started`; never infer role from liveness alone.
3. Add per-file offsets to `tail`.
4. Add `capture --max-bytes` and a small evidence excerpt mode.
5. Add an optional dashboard/coordination read-only view.
6. Acceptance: a human starts MiMITA manually, the observer lists it, names the
   journal, distinguishes two instances, and never controls a process.

## Workstream 2 — session snapshot

Target spec: `docs/specs/simulation/session-snapshot.md`.

Next steps:

1. Define `SnapshotStateV1` with a versioned manifest, component list,
   completeness flags, and checksums.
2. Reuse the scenario checkpoint owner where possible; do not create a second
   state serializer.
3. Add server-only `state_snapshot save/load/list/delete`.
4. Add one-command launch:
   `python devscripts/dev-loop.py --snapshot <id> --launch-mode 45`.
5. Restore at a fixed-tick boundary, increment state version, broadcast a full
   snapshot, reset client interpolation.
6. Start with a Juggernaut Horde/FCS session; expand to destructible world and
   vehicles later.
7. Acceptance: save mid-round, restart, load, compare actor state, objective/
   timers, weapons/ammo, and an N-tick event fingerprint. Divergence must be
   zero or an explicitly declared unsupported component.

## Workstream 3 — time, order, and emergence

Target specs:

- `docs/specs/simulation/time-and-ordering.md`
- `docs/specs/simulation/quantum-inspired-and-literal-quantum-research.md`

Sequence:

### T0 — one clock owner

- Add `src/sim/simulation-clock.h/.cpp` with hz/dt/tick conversions and a
  dev-only time scale.
- Replace the hardcoded 1/60 sites above.
- Audit every tick↔seconds conversion: weapons, cooldowns, NPC timers,
  perception memory, network validation, interpolation, input sampling, AI
  decisions.
- Default 60 Hz. Overrides are dev/test only.

### T1 — tick-rate benchmark

- Run a fixed headless/pinned scenario at 60/120/240/999 Hz.
- Measure CPU/tick, worst actor, server budget, snapshot size/cadence, client
  interpolation, and determinism.
- 999 Hz is a benchmark, not a playable promise.

### T2 — slow motion / single-step

- Dev-only wall-clock time scale while preserving fixed dt and event order.
- Pause/single-step for debugging/TAS.
- Server-authoritative in multiplayer; clients must agree.

### T3 — event order instead of wall-clock Hz

- Use node/connection/event facts: logical time, causal parents, stable
  tie-breaks.
- A tick becomes a batch boundary; wall time only schedules input/render.
- Add deterministic event ordering and replay from the event DAG.
- New spec `time-and-ordering.md` defines the vocabulary.

### T4 — primitive-state toy world

- Separate headless toy world first; shadow-compare with real event chains.
- Minimal primitive state: ID, position, velocity, orientation, up, spin, mass,
  health, team, owner, alive, held action, discrete bits.
- Local rules: input → intent → contact → support/push/damage → death → NPC
  target → projectile/explosion → objective → GUI/audio projection.
- One seed/impulse must produce a traceable causal chain into NPC behavior,
  rocket explosion, collision, vehicle response, and GUI.
- Same primitive state + event order reproduces the macro outcome.
- No engine rewrite until the toy world proves propagation, determinism, and
  performance.

### Literal-quantum branch

- Separate spec/toy only.
- Define amplitudes, measurement, decoherence, authority, determinism, cost.
- Do not integrate into live gameplay authority in this pass.

## Recommended order

1. Finish the evidence observer cleanup and registry.
2. Write/update the four specs and route them.
3. Implement session snapshot first slice.
4. Implement T0 central clock and T1 benchmark.
5. T2 slow motion, then T3 event order, then T4 toy world.
6. Keep NPC/header work in separate windows; do not mix it with time or
   snapshot builds.

## Coordination and build rules

- Root dev-loop only; one Mode 3 batch per coherent change set.
- Queue a build note before saving game compiler inputs.
- Use edit leases for shared files.
- Do not mix header/behavior changes with snapshot or time-architecture builds.
- Use `agent-signal.py` if another agent's file blocks the build.
- Record before/after numbers and journal paths; no build-success-as-proof.

## Open questions

- Exact snapshot component set for the first Juggernaut Horde proof.
- JSON-only first or JSON + optional binary later.
- Which client-side presentation state must be restored versus re-derived.
- Whether the toy world uses the real ECS/event types or a separate minimal
  schema first.
- How to measure “overwhelming” human feel without turning it into a synthetic
  score.
