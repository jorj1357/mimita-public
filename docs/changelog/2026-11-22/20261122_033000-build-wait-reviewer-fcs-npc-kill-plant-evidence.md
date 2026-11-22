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
  so no single latest-generation run captured both a plant and kills.
- CT net travel away from `ct_spawn` is not established: gen443 CT deaths clustered
  within ~40 units of the ct spawn and 672 team0 `npc.stuck-episode` records were
  observed, so the original CT-spawn-trap symptom is not disproven.
- Runtime journals are a rolling window (the gen441 file now starts at seq 233590),
  so early events can be lost. Record proof evidence from exact lines promptly.

## Claims

- Source evidence: none added (review-only).
- Build evidence: generation 441 and 443 existed and were pinned by the observed runs.
- Runtime evidence: NPC-on-NPC kills on gen443 and a bomb plant on gen441 as above.
- Human visual acceptance: not performed.

## Handoff

- Posted issue review 99 and proof evidence records
  `task-proof-e188cb212353` (kill.reward, gen443) and
  `task-proof-2cee3967700d` (objective.planted, gen441).
- Sent message to issue owner fun-cs-npc-objective correcting the earlier
  "no objective.planted" snapshot and noting the journal rolling window.
- A fresh pinned run on the current generation remains the final proof step.
