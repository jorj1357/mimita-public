# issue-b18e63894585: Elite NPC objective navigation regresses into corner traps and underuses legal weapons

## Human report (verbatim, immutable)

> canu also add a issue that npc naviagation issue is like  going ointo corners adn not rushing toward objective, the objective in juggernaut modes should be go to the enemy as fast as possible adn kill them as fast as posslbe, for all npcs in elite mode, can u make that the default nad make it like that live  so npcs all attack with all waepons as much as they can live  adn creata n issue for ther agents to investgaete and get evidenc or propose solutiosn or suggestiongs on how tomake it better and less regressions


## User quote

### 2026-11-21T15:31:35.824Z — codex-melee-and-tdm-issues

Via codex-melee-and-tdm-issues; supports unspecified:

> also add to npc issue that npcs in team daethmatch over time get bored ? or they start running into conrers, arent aggressively chasing the enemy team the whole time, npcs should be aggresive max aggression in elite difficulty the entire time in team deathamtch mode, no wiaitng no searching no going into corenrs over and over, need stronge rnpc logic , u add suggestion/evidence etc for this issue and hwo to fix

## Agent suggestion

### 2026-11-21T15:31:38.431Z — codex-melee-and-tdm-issues

New related report explicitly covers Team Deathmatch: NPCs initially act then become passive, repeatedly route into corners, and stop pursuing the enemy team. Existing issue title/acceptance primarily covers Juggernaut/objectives; extend investigation and acceptance to prolonged TDM waves and both NPC teams. Proposed evidence: in a pinned TDM run, log per-NPC target/team, target-age, last-seen age, route progress/stuck duration, chosen tactical state, weapon/range and attack cadence at fixed intervals from spawn through repeated corner approaches. Compare elite difficulty over time against other difficulties. Identify the first state transition that leaves pursuit or fails to make route progress. Proposed fix: retain continuous enemy acquisition/retargeting, prevent friendly targets, treat repeated low route progress/corner reversals as stuck and select a new reachable enemy route/position, with no idle/search state while a valid enemy is reachable in elite TDM; preserve LOS, collision and 60 Hz fixed-tick owners. Acceptance: runtime journal proves elite NPCs stay hostile and pursue/attack valid enemies across the full scenario without repeated corner traps; human visual review confirms.

## Agent suggestion

### 2026-11-22T14:35:34.318Z — codex-networking-stall-fix

Related shared-navigation finding from FCS run 20261122_134637 (gen495): actor 100011's navigator spent 7032.7936 ms on one replan while destination_distance was zero; npc.nav.plan-cost seq 95522 attributes 7030.6396 ms to Recast for to_goal_m=0.0, and npc.slow-update seq 95534 confirms the total. This is not proof of the Juggernaut elite corner-trap symptom, but identifies a shared planner hotspot and unnecessary zero-distance route query that can worsen navigation responsiveness across modes. A reached-goal skip was added at src/npc/npc-navigator.cpp; validation with a fresh pinned runtime is pending.

## Agent suggestion

### 2026-11-22T15:44:06.449Z — codex-networking-stall-fix

Runtime proof of the zero-distance Recast guard in run 20261122_150906, gen500: npc.nav.goal-reached events for actor 100012 report destination_distance_m=0.0, tolerance=1.5, recast_query_skipped=true (first seq12894/tick1690; repeated). Across 230 npc.nav.plan-cost records, max recast_ms=533.1483, none exceeded 1000ms, and zero recorded zero-distance goals entered a nonzero Recast query. This proves this specific guard reached the live owner; the run still had 1.057s average NPC tick work under severe load, so it does not prove overall FCS networking fixed.

## Agent suggestion

### 2026-11-22T20:38:50.402Z — codex-networking-stall-fix

Additional finding from pinned runtime 20261122_203657 (EXE build 2026-11-22T19:53:22.116562Z, game DLL generation 515, FCS, 19 NPCs): server NPC actor 100018 planned a 6-node route to an objective 326.865m away in tick1. Event npc.nav.plan-cost seq1116 at 20:37:07.137Z reports total_ms=1492.6602, of which recast_ms=1492.4011; local_plan_ms=0.259, global_graph_ms=0.0001. In the same tick, NPC AI cost was 1526.898ms and server fixed tick 1545.5ms, producing a snapshot cadence stall. Source: src/npc/npc-navigator.cpp:1466 reports this query; Recast call chain in src/npc/recast-navigation.cpp:1413-1417 nearestPolyEscalated (start/dest), then :1442 findPath/:1456 straightPath. This is not the earlier zero-distance/7.03s case, but it corroborates that Recast can consume >1.49s on an ordinary long-goal query and starve server snapshots. The current log does not split nearest-poly vs findPath time; fix not yet proven. Please correlate with existing slow/zero-distance Recast evidence.

## Scheduled independent review

### 2026-11-24T11:58:02.496Z — resource-profiler

A bounded independent review was queued as wake wake-b82da7cc7f7e after 548 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (reviewed-with-findings: recent gen587 Horde window reproduces no-target/stuck; fix and proof absent; original cited journal missing)

### 2026-11-24T12:39:29.359Z — issue-review-sweeper

Reviewer: issue-review-sweeper

INDEPENDENT BOUNDED REVIEW (issue-review-sweeper, wake wake-b82da7cc7f7e, 2026-11-24T12:0xZ). Read-only: no source edit, build, test run, lease, close, proof claim, or owner change.

STATUS / READINESS
- readiness: ready=true, missing=[], state=new before this review, 2 tasks. elite-npc-aggression-implementation (owner fcs-npc-live-fix) is open; elite-npc-regression-review is unowned and open. Both have latest_proof=null (no task_proofs rows). There were no prior issue_reviews; the only prior review text is the 2026-11-24T11:58:02Z scheduling note.
- The issue's only cited journal, logs/2026-11-20/20261120_232200/client-01/events.jsonl, does not exist in this checkout (Test-Path False; logs/2026-11-20 is empty). This evidence-availability gap is already recorded under issue-3f776ce60511. The number "548 completed work sessions" in the scheduling note is not proof of this issue.

OBSERVED FACTS (existing artifacts only; no run started)
- Most recent jugger_horde run on disk: 20261124_110111 (game generation 590, dust2cyberiav4, --npcs 5, 11:01-11:03Z). Server journal: 20 npc.target-sense, 90 npc.attack-gate (28 with target_visible=true), 87 npc.target-changed, 9 npc.shot with fired=true, 17 server.damage.applied. So a small gen590 Horde control does acquire targets and fire.
- Longest recent Horde run: 20261124_081518 (server process 08:15-10:17Z, game generation 587, dust2cyberiav4). server-01 produced 14 events segments totaling 1,292.1 MB. Over the whole run it contains 228 server.damage.applied with victim_is_npc=true (6 false) and 5,988 npc.shot with hit_entity=true, so NPC-vs-NPC combat did happen earlier.
- Final window (server-01/events.jsonl, 2026-11-24T10:10:37.751Z to 10:17:06.209Z, ~6.5 min): 8,128 npc.attack-gate, all with suppression_reason=no_visible_target and target_id=0; 0 npc.shot; 0 server.damage.applied; 0 npc.target-changed; 1,799 npc.stuck-episode; 342 npc.movement-progress-failed; 2,171 npc.target-sense with target_visible=false. Note npc.target-sense candidate_valid/target_visible are computed against the human player only (src/npc/npc.cpp:548), so this signal does not itself describe NPC-vs-NPC perception.
- Same window phase telemetry: performance.server-phase-budget reports npcs=67, npc_over=true, max_npc_ms=61.89 vs npc_budget_ms=10, 26 over-budget ticks in one 1.03 s window; server.world-phase-window reports living_npcs=63 and npc_sim_ms=605.1. The AI/target phase reports target_npc_candidates=31,691, target_player_candidates=473, target_scans=473, target_scan_skipped=1,417, target_forced_refreshes=0. So the host loop enumerated NPCs but selected no target.
- npc.trace at 10:17:05.776Z samples 8 actors: 200059-200062 (juggernaut_fighter, team 0) all PATROL; 200064-200066 (juggernaut_arcade, team 1) RETREAT; 200063 (team 1) CHASE. server.dead.npc.snapshot.sanitized reports only actor 200063 at health=0; 200064-200066 are not reported dead and still carry movement state. At least 3 live hostile NPCs were therefore present while the team-0 fighters held target_id=0.
- Last npc.shot in the preceding segment was 2026-11-24T10:07:16.086Z (events-0018 had 89 shots and 6 damage); shots then ceased through the final 6.5-min window.

INFERENCE / UNCERTAINTY
- This is a recent, on-disk manifestation of the reported prolonged-match symptom (fighters PATROL/stuck, no engagement) on gen587. It is not proof of the elite default fix, which has not been implemented.
- Candidate first-failure area (hypothesis, not proven): target candidates are enumerated (31,691 NPC checks) but no hostile NPC is selected (target_id 0, 0 target-changed). Possible causes: npcTeamOf() reading a stale/non-hostile team from the npcs map copy (src/network/server-npcs.cpp:2212-2215), a divergence between the module targeting policy and gamemode npc_targeting, or target-scan starvation under the measured NPC CPU over-budget. The available journals do not distinguish these.
- Whether the final window was inside a non-combat round/intermission phase is unknown. No round-end/phase events appear in the final window (only 3 match.countdown.* events earlier in the run). But combat AI kept running and >=3 hostile NPCs appear alive, so an innocent "no targets" round state is not established.
- The gen590 5-NPC control does not prove NPC->NPC targeting under a full 67-NPC Horde; its shots may be player-directed.

CONTRARY EVIDENCE
- NPC-vs-NPC damage and hits did occur earlier in the same 081518 run (228 NPC victims; 5,988 hits), so this is not a constant "NPCs never shoot" defect.
- The 110111 gen590 control fired 9 shots and applied 17 damage with target_visible=true gates, so the fire path can work in some states.
- Task-1 (implementation) is still open and no candidate fix is recorded for this issue, so the missing proof is expected rather than a regression of a previously proven fix.

NEXT DISCRIMINATING CHECK (smallest, owner-runnable)
- In one pinned current-generation (gen593) jugger_horde run, hold >=1 live hostile per team, or use 1 fighter NPC + 1 live enemy juggernaut NPC with no human, for 3-5 min. Correlate per fighter: myTeam, resolved hostile NPC team, target_npc_candidates, the target_id in any npc.target-changed, attack-gate target_id/suppression_reason, npc.shot fired, and server.damage.applied victim_is_npc. Discriminator: (a) live hostile in sight + target_id stays 0 and no target-changed -> first failure is target acquisition (record the exact team comparison); (b) target_id nonzero but attack-gate suppresses -> gate/LOS/weapon owner; (c) intermission/round-end state -> scenario is invalid, re-run in active phase.
- Capture a fresh 3-5 min process window so the journal stays small; the 081518 process wrote 1.29 GB and is slow to analyze.

No source, build, runtime behavior, fix, or proof is claimed. Owner remains fcs-npc-live-fix; the unowned elite-npc-regression-review task remains claimable. Cold handoff: the final-window counts above are reproducible from the exact paths and wall-clock window.
