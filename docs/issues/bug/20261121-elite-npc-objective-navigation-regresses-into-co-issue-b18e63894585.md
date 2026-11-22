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
