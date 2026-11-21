# issue-b95574b63f29: Juggernaut Horde Fighters fail to consistently engage nearby human Juggernauts

## Human report (verbatim, immutable)

> also npcs dont attack me in juggernaut horde mode, make that an issue and alos get evidnece investiagte why, right no win live run im in juggernaut horde and npcs not atacking me shooting at me, at all, even tho mi right there, all npcs in juggernaut the enemeis fighters should swarm around juggernauts and shoot at them over nad over and over like be super elite ultra aaggrssive


## Triage

### 2026-11-21T00:02:21.828Z — fcs-npc-live-fix

Summary: Investigate active Juggernaut Horde evidence: Fighters never selected human player ID 1 in sampled target records after team_pick 2 changed player to juggernaut_mode_juggernaut; correlate targeting/perception and first cause.
Owner: fcs-npc-live-fix
Priority: high

## User quote

### 2026-11-21T22:48:55.780Z — reconnect-horde-consistency

Via reconnect-horde-consistency; supports user-message-2026-11-21:

> alos npcs stop fighitn me/choosing me as a arget in juggernaut horde that needs to be a issue and also fixe dhow to fix that

## Agent suggestion

### 2026-11-21T22:52:57.054Z — reconnect-horde-consistency

Evidence correction (2026-11-21): the cited run_id 20261120_232200 is NOT a Juggernaut Horde run. Its logs/2026-11-20/20261120_232200/run.json manifest records mode "fun counter strike", map funworld3, and build 2026-11-20T20:20:51.788061Z:64cd9304582622039486e6a1fe369e2f7d466a2dc0eac7975241f98f0ee0df55. Therefore evidence aggregates currently attached to this issue cannot establish Horde targeting or shooting behavior and should be treated as misattributed until corrected. Source/config inspection (not runtime proof): config/gamemodes/jugger_horde.json declares team_roster with 64 Fighter + 4 Juggernaut capacities, and src/network/server-gamemode.cpp::reconcileTeamRosterNpc is expected to reconcile those capacities and emit gamemode.team-roster-npc.created plus gamemode.team-roster.resolved. Next discriminating run must log selected launch mode, gamemode.match.configured, team-roster resolution/counts, per-actor team/role, target-sense, fire gate, shots, and damage on same pinned Horde server/client journals.

## Agent discussion and suggestions

### 2026-11-21T23:02:32.936Z — live-build-gui-timer-resource-audit

Message: `msg-1870b70b424e` (evidence)

Subject: Evidence: Juggernaut Horde Fighters do target the human after team_pick 2; loss is state-dependent

Additional read-only evidence from the same user session, run 20261121_224058 child server PID 20908 (map badhouse, mode jugger_horde, generation 436). No code or process change.

Timeline: player 1 spawn acknowledged tick 9. At tick 180 the full roster existed (63 fighters + 4 juggernauts) and client identity resolved as team 0 role juggernaut_fighter. No npc.target-changed record had target_actor=1 before tick 438, and the four juggernaut NPCs targeted fighter NPCs 200029/200063-200066 at about 100 m. At tick 438 the host ran team_pick 2; actor.team-assigned recorded player=1 team=1 team_name=Juggernauts role=juggernaut_mode_juggernaut host_override=true. After that, 14 npc.target-changed events targeted actor 1 at 1.6-19.1 m between ticks 625 and 1130, all reason module_scored, with team-0 fighter actors (for example 200021, 200016, 200031, 200050, 200022, 200036, 200033, 200047, 200009, 200004, 200019, 200050). So Fighters do acquire and keep the human Juggernaut when the human is actually the opposing Juggernaut.

This narrows the reported loss of targeting. It is not a universal target-filter failure. The two concrete states observed here that can look like NPCs ignoring the player are: (a) before any human is active on that exact server, server-gamemode.cpp:9373-9415 keeps stage=waiting, participants=0, roster absent and only the generic startup NPC exists (run 20261121_215237 server PID 29912 at tick 107694: active_players=0, npc_count=1, npc id 1000 role empty team -1 PATROL stuck); (b) before the human is the opposing role, team-0 juggernaut NPCs legitimately target team-0 fighter NPCs while the human is still a Fighter. Other plausible transient states to capture next: player connectionStale (server-npcs.cpp:2225/2291/2353 excludes stale players), p.downed, or a server return to WAITING when active_players reaches 0.

Targeting code owners confirmed: candidate lists include players and NPCs at server-npcs.cpp:2221-2244 (module path) and :2287-2315 (scored fallback); the hostile filter is npcTargetingIsHostile using playerTeamOf/npcTeamOf. No evidence in this run shows a same-team or LOS block for actor 1 after the team change.

Next discriminating check: capture the exact moment the user says NPCs stop choosing them, with player id/team, connectionStale, downed/dead/hp, server phase, and the next npc.target-sense or npc.target-changed for at least one hostile Fighter/Juggernaut. If server phase returned to WAITING with active_players=0, the first cause is the join/retention path, not the target selector.
