# FCS NPC kill and bomb-plant evidence review (build-wait-reviewer)

Date: 2026-11-22 ~03:30Z
Agent: build-wait-reviewer
Context: ctx-c00ad9484fa1 (high priority) "NPC-on-NPC kill or bomb-plant evidence ... with events proof"
Issue: issue-be1adb51b6ac (Fun Counter Strike NPC objective behavior regressed: CTs stuck at spawn)

## Scope

Read-only evidence review. No source, config, or build-script edits were made. No
compiler, build.py, module worker, or second dev-loop was invoked. No new runtime
was started because the issue owner and issue-review-sweeper were already active.

## Runtime evidence observed

### NPC-on-NPC kills (generation 443)

- Run `20261122_025700_fcs-objective-progress`, launch mode 45 (fun counter strike),
  map dust2cyberiav4, game DLL `build/game/generation-443/game-g443.dll`, EXE build
  `2026-11-22T02:37:06.972018Z`.
- Server journal: `logs/2026-11-22/20261122_025700_fcs-objective-progress/server-01/events.jsonl`.
- 4 authoritative `server.damage.applied` events with attacker and target both npc and
  `killed:true`: 2 team0-to-team1 and 2 team1-to-team0. First at t=108.436 tick=5042,
  attacker 100008 (team 0) killed target 100009 (team 1), source `physical_contact`.
- 7 `kill.reward` events with `victim_is_npc:true` in the retained window.
- Client journal `logs/2026-11-22/20261122_025700_fcs-objective-progress/client-01/events.jsonl`
  has `network.mode.start` mode=funcounterstrike phase=4 participant_count=20.

### Bomb plant (generation 441)

- Run `20261122_023327_fcs-proof`, game DLL `build/game/generation-441/game-g441.dll`.
- Server journal: `logs/2026-11-22/20261122_023327_fcs-proof/server-01/events.jsonl`.
- `objective.carried` (carrier 100018, team 1) then `objective.planted` at t=882.927
  tick=50265, operator 100018 (npc), site B, position [-903.267,-102.953,2353.251].
- issue-review-sweeper independently recorded the full chain (plant-start, planted,
  exploded, round.result) and 22 cross-team NPC-on-NPC `kill.reward` events as
  `evidence-8572793982d1` with result=proven.

## Findings and caveats

- The user-requested "NPC-on-NPC kill or bomb-plant evidence" exists in pinned
  runtime journals for generation 441 and 443.
- The gen443 runtime was stopped after ~5.5 minutes (4 `objective.carried`, 0 plant),
  so that run alone did not capture both a plant and kills; the later gen446
  `fcs-proof-bwr` run does contain both.
- CT net travel away from `ct_spawn` is not established: gen443 CT deaths clustered
  within ~40 units of the ct spawn and 672 team0 `npc.stuck-episode` records were
  observed, so the original CT-spawn-trap symptom is not disproven.
- Runtime journals are a rolling window (the gen441 file now starts at seq 233590),
  so early events can be lost. Record proof evidence from exact lines promptly.

### Combined proof on generation 446 (latest published at run start)

The agent started its own pinned FCS runtime after discovering the owner and
sweeper were between runs, capturing both behaviors in one journal:

- Run `20261122_033318_fcs-proof-bwr`, game DLL `build/game/generation-446/game-g446.dll`,
  EXE build `2026-11-22T02:37:06.972018Z`, started 2026-11-22T03:33:18.193Z.
- Server journal: `logs/2026-11-22/20261122_033318_fcs-proof-bwr/server-01/events.jsonl`.
- Bomb chain: `objective.plant-start` t=918.165 operator 100016 (npc) site B;
  `objective.planted` t=925.149 site B; `objective.exploded` t=1020.755 site B;
  `round.result` t=1020.755 round=5 winner_team=1 reason=2 (bomb detonation).
  A second `objective.plant-start` for site A at t=1033.624 operator 100010 (npc).
- NPC-on-NPC combat: 208 authoritative `server.damage.applied` events with both
  attacker and target npc and `killed:true` (155 team0-to-team1, 53 team1-to-team0);
  214 `kill.reward` events with `victim_is_npc:true`; example t=902.443 attacker
  100004 (team 0) killed target 100011 (team 1) via `physical_contact`.
- Client `network.mode.start` mode=funcounterstrike server_tick=30063.
- Runtime stopped after capture; shared server PID 26748 preserved.

## Claims

- Source evidence: none added (review-only).
- Build evidence: generations 441, 443, and 446 existed and were pinned by the observed runs.
- Runtime evidence: NPC-on-NPC kills on gen443 and gen446; bomb plant/explosion/round
  result on gen441 and gen446.
- Human visual acceptance: not performed.

## Handoff

- Posted issue reviews 99 and a follow-up with verdict proven.
- Proof evidence records: `task-proof-e188cb212353` (kill.reward, gen443),
  `task-proof-2cee3967700d` (objective.planted, gen441),
  `task-proof-d08c8ddf3821` (kill.reward, gen446),
  `task-proof-4380993cf802` (objective.planted, gen446).
- Sent messages to issue owner fun-cs-npc-objective with the corrected plant
  evidence and the gen446 combined proof.
- Source advanced to generation 449 while the gen446 run executed, so the newest
  source still needs its own pinned long run; the owner started `fcs-route-proof`
  for that.
