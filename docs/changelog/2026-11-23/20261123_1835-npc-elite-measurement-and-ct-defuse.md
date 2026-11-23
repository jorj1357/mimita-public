# NPC elite measurement and CT defuse repair

## Scope

This session started the Fun Counter Strike elite-NPC work with a measurement
pass, then repaired the concrete CT-defuse defect that the measurement exposed,
and began the elite difficulty/movement-tech pass. The work stayed on the shared
checkout and used the root dev-loop only.

The user's answers fixed the target: maximum legal elite difficulty, a 200 ms
acquisition reaction, a 3-tick (50 ms) accepted-shot floor, no through-wall
radar, rocket jumps allowed in combat, and a designated NPC defuser that can be
quickly reassigned.

## Phase 0 — unit calibration and measurement instrument

- Recorded the human's stated intention as **one world unit equals one metre**,
  while current engine evidence indicates a 2:1 convention: the default capsule
  is 3.6 world units for a 1.8 m human, and a 20-unit authored block crosses in
  one second at the configured 20 unit/s speed. Metrics therefore report raw
  world units plus calibrated metres at a provisional 0.5 m per world unit. Raw
  values remain in the journal so a later player probe can correct the scale
  without losing data.
- Extended `npc.movement-metric` with per-life dash, air-dash, down-dash, jump,
  bhop, freeze, air-time, air-movement, maximum planar speed, and final-pose
  wall-contact counters. The instantaneous-speed field now uses velocity plus
  external impulse instead of displacement over a short interval.
- Added `devscripts/quality/npc-match-report.py`: a read-only journal analyzer
  for per-NPC/per-team movement, dash/jump/air/wall rates, NPC-vs-NPC kills,
  TTK, kill intervals, round objective timing, and diagnostics. It honours the
  `meters_per_world_unit` recorded in each event.
- Captured a pinned generation-564 baseline: CT 11.83 m/s average, dash
  17.52/min, down-dash 9.15/min, bhop 2.03/min, air 24.8%, near-wall avoidance
  11.6%, stuck 21.90/min; T 10.15 m/s, dash 10.85/min, down-dash 6.22/min,
  bhop 1.30/min, air 17.0%, near-wall avoidance 9.5%, stuck 15.46/min. Four
  plants, no completed defuse.

## Phase 1 — CT defuse repair (runtime proven)

The baseline showed the designated defuser was often the human player, so all
NPC CTs were gated out of defusing. Source changes:

- `src/network/server-gamemode.cpp`: prefer a living NPC as the designated
  defuser; keep it stable while defuse progress is active; only swap to another
  NPC when the stable one is dead or another is at least 5 m closer and no
  defuse is in progress. Human defuse remains ungated.
- `src/npc/npc.cpp`: lock `DefuseObjective` for the designated CT and
  `PlantObjective` for a carrier inside a site; hold position during the
  interaction, releasing only for a real hazard.
- `npc.defuse-assigned` now records actor kind, stable/nearest NPC and human
  candidates, distances, and the selection reason.

Pinned proof: run `20261123_172947`, game DLL generation 567 SHA-256
`3ce3673f656373f1c5b1c8b5206cb40434143bdd09336c91021f828ef95e092b`, EXE
`97b78d2ae01ed2dd665e39c9f5ac97ebf4acef801ee3597c4ecb0f7a584f7a8d`, mode 45 on
`dust2cyberiav4`. Journal:
`logs/2026-11-23/20261123_172947/server-01/events.jsonl`.

Four T plants and four CT defuses completed. Every sampled
`objective.defuse-start` used `operator_kind: npc`; operators 100005, 100007,
and 100001 completed at t=69.83, 110.95, 175.16, and 239.54 s. The run was
stopped by its own `--stop-test-run` request with failures=[].

## Phase 2 — elite profile and movement tech (source/config built, proof pending)

- `config/behavior-profiles.json` `fun_cs_rush`: true elite aim (`0.06°`),
  `turn_speed_deg_per_sec: 1800`, `reaction_delay: 0.2`,
  `fire_interval_ticks: 3`, `dash_chance: 0.85`, focus/swarm, target-relative
  approach sectors, and `information_mode: memory` retained.
- `config/npc-difficulty.json`: `downDashChance: 1.0`.
- `src/npc/npc.h`, `src/npc/npc.cpp`, `src/network/server-npcs.cpp`: elite
  objective-travel bhop holds the shared jump input for a short window on a
  clear, unblocked route so the preset auto-bhop chains. A visible target,
  hazard, stuck episode, failed route, or local correction ends the hold.
- Game DLL generation 568 built successfully, SHA-256
  `72ddbf4a81f99ba34e1bba4cb2eb403afa3744791b3f4172f5f27b6fb8f276d5`.
- Runtime proof for Phase 2 is **not yet captured**. Two isolated gen568 test
  runs failed readiness: one client did not observe active mode, one did not
  connect. Those failures occurred under current shared-machine load and are not
  behavior evidence. The user can launch mode 45 from the root dev-loop to test
  the published generation.

## Documentation

- `docs/regressions/npc-movement/fcs-objective-speed-defuse-REG-2026-11-22.md`:
  added the unit-calibration decision, the gen564 baseline, and the gen567 CT
  defuse proof.
- `docs/specs/npc-behavior/npc-behavior.md`: added the difficulty policy
  (start at maximum legal difficulty and tune down), the FCS elite profile
  values, plant/defuse priority, the measurement contract, and the provisional
  unit scale.
- `docs/specs/gamemodes/funcounterstrike.md`: updated the FCS elite profile,
  movement-tech use, and designated-defuser/plant-lock behavior.

## Validation and evidence

- Game DLL generation 564: Phase 0 build success and pinned baseline.
- Game DLL generation 567: Phase 1 build success and pinned CT-defuse proof.
- Game DLL generation 568: Phase 2 build success.
- `npc-match-report.py` was run against generation-564, generation-567, and the
  earlier live diagnostic journals; it parses both the legacy 1.0 scale and the
  new 0.5 scale.
- Packet registry codegen was regenerated after `server-gamemode.cpp` line
  shifts; `python devscripts/packet_registry_codegen.py --check` returned
  `ok packets=78`.
- No human visual acceptance was performed. Build success is not behavior
  acceptance; the defuse proof is runtime-journal evidence, while elite
  difficulty and movement-tech feel remain pending live review.

## Follow-up

- Capture a pinned Phase 2 run and compare TTK, kills/round, dash/bhop/air
  rates, wall avoidance, and stuck time against the gen564/gen567 baseline.
- Reduce the remaining repeated `objective.defuse-start` restarts within a
  round (operator/range churn is reduced but not eliminated).
- Add explicit air-strafe and rocket-jump/deathball-knockback technique
  execution once the Phase 2 baseline is measured.
- Investigate why isolated test-run readiness is failing under current shared
  load; the failures blocked Phase 2 runtime proof.
