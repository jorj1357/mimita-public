# NPC dossier, readiness fix, and first header split

## Scope

This session consolidated the NPC behavior knowledge, fixed the isolated-run
readiness design, and started the NPC-only header fan-out tranche. Phase 2
runtime measurement is still pending because another isolated juggernaut horde
run remained active; no concurrent isolated test was started.

## NPC knowledge consolidation

- Added `docs/features/npc-behavior.md` as the NPC feature dossier: underlying
  goal, canonical issue index, evidence table with run IDs/journals, problem to
  owner `file:line` map, decisions, solution frontier, and open questions.
- Added `issue-196c3045b4cb` for header fan-out, with the full dependency-graph
  baseline and the NPC-first tranche.
- Recorded decision `decision-2c2ee150dc3a` for FCS elite difficulty:
  200 ms reaction, true elite aim/turn, 3-tick accepted-shot floor,
  memory-only information, nearly unbeatable starting point; supersedes the
  older `issue-0a2a3fc00768` aim-error recommendation.
- Routed NPC work through the dossier in `docs/ROUTER.md` and added the header
  fan-out rule to `AGENTS.md`.

## Readiness fix

The gen568 readiness failure was reproduced as a real state, not just a label
bug: failed run `20261123_182722` had the client in `DUEL_PHASE_INTERMISSION`
(phase 4) and server tick stalls up to 4,494 ms. The runner's 45 s wall-clock
wait was invalid.

Changes:

- `src/network/community-match-client.cpp`: emit canonical
  `network.mode.active` when the authoritative active phase is applied.
- `devscripts/dev_loop/parallel_test_runs.py`: wait for
  `network.mode.active` (legacy `network.mode.start` `stage=active` fallback)
  with a simulation-time budget and server-health gate. Slow tick progress or
  a tick stall over budget now reports `server_stalled` with tick range,
  elapsed time, max stall, and over-100 ms percentage.
- `docs/specs/debug-logging/canonical-jsonl.md`: documented the readiness event
  contract.
- `issue-fa63aa4e218f`: records the readiness bug and acceptance.

## NPC header tranche

- Added `docs/architecture/headers/header-hygiene.md` with classification,
  rules, fan-out budgets, tranche order, and acceptance.
- Moved NPC-only debug flags from `src/config.h` to
  `src/debug/config-npc.h`; updated the six consumers and `debug.reset`.
- Measured: `dependency_graph.py --affected src/debug/config-npc.h` = 6 sources
  versus 284 for `src/config.h`.
- Build: game DLL generation 571, success, 829.795 s, SHA-256
  `11c1f7b7af6b1df5577246877de9f23efb96deca02850032aeb0d63271271005`.

## Evidence and limits

- Build success proves compilation only.
- The header split is behavior-neutral by design; no behavior improvement is
  claimed from it.
- The canonical readiness event and health gate are source/build proven on
  gen571; a healthy pinned run and a controlled stalled-run check remain open
  in `issue-fa63aa4e218f`.
- Phase 2 gen568/gen571 comparison is unproven. Proof task
  `task-proof-0549561ef512` was added. The blocker was the active isolated run
  `20261123_195307` (juggernaut horde); starting another isolated test would
  have violated the no-concurrent-heavy-run rule.
- No human visual acceptance was performed.

## Next steps

1. When the machine has no other heavy test run, start a pinned FCS gen571 run
   and compare TTK, kills/round, dash/bhop/air, wall, and stuck against
   gen564/gen567 with `devscripts/quality/npc-match-report.py`.
2. Continue the NPC header tranche: `npc.h` metrics/state/system split,
   `physics/config.h` player dimensions, then the `player.h` hybrid boundary.
3. Use the new readiness evidence to classify future test failures as
   `server_stalled` versus `client_not_active`.

## Phase 2 runtime result (added after the pinned run)

Run `20261123_205333` was started on generation 571 and reached online using the
new `network.mode.active` readiness path. Over 256.2 s and 3 completed rounds:

- 3 T plants, 3 CT NPC defuses, CT winners 3/3.
- NPC-vs-NPC TTK mean 1.08 s, median 0.45 s (gen564: 2.40 / 0.84 s); kills per
  round 5.00 (gen564: 4.33).
- CT: 11.92 m/s, dash 25.43/min, down-dash 8.45/min, bhop 3.76/min, air 26.1%.
- T: 11.53 m/s, dash 22.04/min, down-dash 7.45/min, bhop 3.21/min, air 22.8%.
- Versus gen564: CT dash 17.52→25.43/min, T 10.85→22.04/min; CT bhop
  2.03→3.76/min, T 1.30→3.21/min; air CT 24.8→26.1%, T 17.0→22.8%.
- Not met: air target 30%; CT stuck flat and T stuck worse; CT near-wall and
  blocked motion increased. Defuse starts still restart 18 times with 15
  interruptions before 3 completions.
- Proof task `task-proof-5a4c3efb911a` verified `objective.defused`. The generic
  metric proof verifier did not match the nested metric field, so the measured
  numbers are recorded in the issue review and regression record.
- The run was stopped by its own `--stop-test-run` request, failures=[], PIDs
  3124/30004 only. The concurrent juggernaut horde run `20261123_195307` was
  left untouched.
