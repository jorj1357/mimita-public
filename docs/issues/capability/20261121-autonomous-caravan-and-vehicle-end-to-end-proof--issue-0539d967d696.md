# issue-0539d967d696: Autonomous Caravan and vehicle end-to-end proof in infinite city and drift maps

## Human report (verbatim, immutable)

> a big big goal, something huge and craz, is, ai should be able to fully impleent caravan mode and prove taht every single part of the gamemodes works, like, going from totally no mimita.exe open, to opening it, staring the gamemode thru the gui, joining it, playing, doing all objecrives, all kills work, all weapons work, all features work, vehicles work, npcs and humans can drive any vehicle type, etc, works human vs human too, bad connections bad performance too


## Scheduled independent review

### 2026-11-24T19:05:26.028Z — root-mode3-latency

A bounded independent review was queued as wake wake-6a2b3038df07 after 607 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (needs-evidence)

### 2026-11-24T19:14:12.624Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review by issue-review-sweeper (wake wake-6a2b3038df07). Read-only: no source edit, build, close, or fix claim.

OBSERVED FACTS
- Readiness: ready=true, score 8/8, state=new, owner empty, 0 tasks, 0 prior reviews. The only doc entry is the scheduling note (2026-11-24T19:05:26Z). related_build and related_run are empty. (agent-coord observe issue-0539d967d696; issue readiness; docs/issues/capability/20261121-autonomous-caravan-and-vehicle-end-to-end-proof--issue-0539d967d696.md)
- Dependencies are not verified/done: issue-c8290acca248 in_review, issue-e5a086b0a441 in_review, issue-0a64c72f60c8 needs_clarification, issue-848a5ba01470 new, issue-549680e53b26 in_review, issue-8bfc1ca56222 assigned.
- No proof record: proof status contains no task for this issue, and there is no linked build/run/journal on the record.
- Caravan is not implemented as a mode: config/gamemodes/ has no caravan.json, and a repo search for "caravan" outside logs/docs/changelog finds only specs (docs/specs/gamemodes/caravan.md) and historical plans. The closest implemented vehicle modes are config/gamemodes/vehicle_arena.json, rocket_duel.json, and payload.json.
- Latest relevant journals are five isolated runs by vehicle-rl-physics on 2026-11-24: 20261124_183009_rl-physics-1, 20261124_184012_rl-physics-2, 20261124_184356_rl-physics-3, 20261124_184501_rl-physics-4, and 20261124_184753_rl-physics-5. All pin game generation 600, game DLL sha256 53f23d0be0ccb64752a54cb11cc4f5f6d3a23cf04bf428ff47bcf62972de72cc, launch_mode 50 "[2 CLIENTS] Vehicle Arena - 1v1", profile vehicle-arena.
- All five runs contain only client-01 and server-01 directories; none contains a client-02 journal. In the newest run 20261124_184753_rl-physics-5 the server never reported more than 1 participant (max network.mode.start participants=1) and the only client journal is client-01.
- rl-physics-5 server journal does show real vehicle work: vehicle.auto_seat (vehicle_id 1), vehicle.seat.entered (3), vehicle.seat.replicated_auto (3), arena.boostpad.collected (36), vehicle.bounce (2224), vehicle.telemetry (444), and network.mode.start reached stage=active. It has zero damage/kill/weapon-fire events and sampled network.mode.start npc_count=0.
- vehicle-rl-physics recorded rl-physics-5 against issue-e5a086b0a441 (coord.activity.set phase=review, issue=issue-e5a086b0a441), not against this issue.

INFERENCE / UNCERTAINTY
- Evidence supports: one human client can auto-seat and drive/boost a vehicle in vehicle_arena on generation 600. It does NOT evidence the issue's "at least two human clients" requirement, weapons/kills against humans or NPCs, NPC driving, Caravan objectives, degraded network, win/loss, or restart/reconnect.
- Harness code explains the missing second client without blaming vehicle code: start_parallel_run launches server + client-01 only (devscripts/dev_loop/parallel_test_runs.py:639), while a second human client requires a separate dev-loop.py --start-test-client <run_id> call (start_second_client at devscripts/dev_loop/parallel_test_runs.py:882). The "[2 CLIENTS]" label in devscripts/dev-launch-modes.json mode 50 does not itself spawn client-02. This is a strong inference from code, not a captured launch transcript; no client-02 launch/failure record exists to confirm it.
- Cannot infer weapon, kill, NPC-drive, or network-degradation failures from these journals; those paths were not exercised (keyboard-only physics harness, no weapons, no NPCs).
- The absence of a caravan mode config is a scope/ownership gap, not evidence of a runtime defect. End-to-end acceptance is unreachable until Caravan itself is scoped and implemented.
- Minor tech-debt observation (not this issue's blocker): devscripts/dev-profiles/vehicle-arena.json sets max_mimita_processes=2, but dev-loop.py:1309 only assigns it and no code reads it; it does not explain the missing client-02.

CONTRARY EVIDENCE
- None found against the goal. Genuine partial progress exists in the dependency: vehicle_arena profile/auto-seat/boost/goal work and vehicle physics are recorded on issue-e5a086b0a441, and generation-600 journals are replayable.

NEXT DISCRIMINATING CHECK (smallest)
1. On the current generation-600 artifact via the root dev-loop, start one isolated vehicle_arena run (--start-test-run), then add the second human client with --start-test-client <run_id>.
2. Confirm, in that exact run, client-02 logger.started -> network.connection-state.changed to=Connected, server network.mode.start participants=2, both clients vehicle.seat.anchor has_local_driver=true, and server vehicle.input.server for two distinct actor ids.
3. If client-02 cannot connect or auto-seat, record the first failing stage as the real blocker for "two human clients". Do not attempt Caravan acceptance until a caravan config/objective owner is assigned.

Acceptance remains open. Ownership is unclaimed; the next check above is safe, bounded, and does not change source.
