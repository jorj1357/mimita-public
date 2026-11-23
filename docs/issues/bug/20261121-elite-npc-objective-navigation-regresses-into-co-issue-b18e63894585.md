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
