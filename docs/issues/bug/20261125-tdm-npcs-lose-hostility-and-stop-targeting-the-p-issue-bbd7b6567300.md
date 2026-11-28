# issue-bbd7b6567300: TDM NPCs lose hostility and stop targeting the player during a round

## Human report (verbatim, immutable)

> but tdm npcs stp treating me like a enemy stop attacking/targetting me idk why at randmo too, at teh start of a round they will be super aggressive whihc is what is intended but then they stop caring after some time


## Assignment

### 2026-11-25T20:55:44.301Z — opencode-deepseek-v4-1

Owner: opencode-deepseek-v4-1

## Agent suggestion

### 2026-11-25T21:01:18.656Z — tdm-npc-target-retention-diagnosis

Source/config review (2026-11-25; code inspection only, not TDM runtime proof): `config/gamemodes/tdm.json` selects `elite_squad` with `npc_targeting.mode=opposite_team`, `include_players=true`, and `include_npcs=true`. In `src/network/server-npcs.cpp`, the `player` mode has a player-first branch, but `opposite_team` goes through scored selection with hostile player and NPC candidates together. The score weighs distance, low health, weapon threat, current-target stickiness, and a switch threshold. TDM has no special human retaliation bonus; the additional `playerThreat` bonus is gated to Fun Counter Strike. The elite tier has finite stickiness (0.85) and switch threshold (0.05). `src/npc/npc.cpp` resets `targetMemory` whenever `serverTargetId` changes, so a switch from the human to an NPC discards the prior human pursuit memory. This is a plausible explanation for the reported loss of interest when another hostile NPC becomes the higher-scoring candidate; no TDM journal from the reported failure confirms that this happened.

Fix direction: add/reuse one generic target-selection policy field that gives eligible enemy humans priority in TDM while retaining the `opposite_team` hostility predicate and NPC fallback when no live hostile player is eligible. Do not simply change TDM mode to `player`: `src/npc/npc-targeting.cpp` treats unknown/teamless actors as hostile in that legacy mode, while the TDM spec says unknown team is not hostile. Keep pursuit memory for a valid hidden human target, and add bounded target diagnostics showing candidate type/team/score, current target, switch reason, and invalidation reason. Validate in a pinned multi-round TDM run: log target changes, teams, roster/liveness, pursuit-memory transitions, and server damage; confirm enemy NPCs keep/reacquire a valid human through the same interval. A past FCS run in related `issue-cd492e4faa08` showed an analogous target switch to another hostile NPC after a player kill, but it is not proof of this TDM report.

## Agent suggestion

### 2026-11-25T21:12:42.156Z — tdm-npc-postkill-followup

User follow-up (context `ctx-0b5a1ae6738f`, 2026-11-25): the user says the loss of hostility may happen after they kill NPCs. Mode, exact victim/attacker identities, whether the dead NPC respawns or surviving NPCs change target, and a matching run are unknown. This increases the relevance of a recent-killer threat/retaliation path, beyond ordinary target competition, but is not runtime confirmation. Related FCS issue `issue-cd492e4faa08` contains a 2026-11-22 isolated run where after player 1 killed NPC 100010, a target-change selected hostile NPC 100008 at 228.04 m instead of the killer. The FCS-specific `playerThreat` bonus is enabled only for Fun Counter Strike; TDM has no equivalent bonus in the inspected selector. Distinguish kill-triggered threat loss from candidate score switching, team identity invalidation, and the killed NPC's respawn/reset in a pinned TDM run.

## Agent discussion and suggestions

### 2026-11-28T17:23:02.447Z — tdm-npc-focus-fire

Message: `msg-13b4c307e8f2` (message)

Subject: server-gamemode.cpp focus region lease

I am editing src/network/server-gamemode.cpp (TeamBrain focus block + CT site split) under lease 68008f73c416. Your activity lists server-gamemode.cpp. I am not touching player-ai, engine-tick-combat, or melee. Please keep your changes out of the team-brain/focus region (serverTeamBrainTick focus block and the per-NPC objective context) or reply if you have an active edit there.

## Agent suggestion

### 2026-11-28T17:25:56.769Z — tdm-npc-focus-fire

IMPLEMENTATION (2026-11-28). Live evidence confirms the failure is target competition, not team loss. Journal logs/2026-11-28/20261128_092602/client-01/events-0010.jsonl (TDM, dust2cyberiav4, server PID 31144, 9 roster NPCs, human actor 1, team 0): at 09:31:29 all five BLUE NPCs acquire actor 1 at 268-295 m (target_reason=module_scored); at 09:34:39.710, the tick the human dies, NPCs 200006 and 200005 switch target 1 -> 200001/200000 (module_scored); after the 09:34:39.758 respawn no NPC targets actor 1 again through 09:38:38, while 163 NPC-vs-NPC damage events continue. Fix implemented: (1) team squad focus is now the shared aim target for elite soldiers in src/network/server-npcs.cpp; (2) src/network/server-gamemode.cpp resolves focus only to a living hostile, uses the nearest living enemy with an 8s/150m fair rotation when TDM has no report, and never keeps a dead report actor; (3) generic damage retaliation marks any attacker (human or NPC) as stale-priority in the victim team's TeamBrain; (4) no human-priority flag is enabled; a human and an NPC are identical candidates; (5) elite maxAttackersPerTarget raised 3 -> 16 so the whole squad commits; (6) FCS defenders split round-robin across usable sites until one team report pulls all CTs to the sighted enemy area, with no chosen-site sharing. Specs updated: npc-behavior.md section 15, teamdeathmatch.md, funcounterstrike.md. Build pending. Runtime proof still required.

## Agent suggestion

### 2026-11-28T18:19:40.084Z — tdm-npc-focus-fire

RUNTIME PROOF (2026-11-28, generation 826, run 20261128_181614, map dust2cyberiav4, profile tdm-validation, server-01/events.jsonl). Verified proof task-proof-a52f080c35e5. Results: 274 npc.target-changed events with target_reason=squad_focus and only 20 module_scored; 42/42 one-second samples with >=2 sampled team members had 100% of sampled members on the same server_target_id; team 1 (enemy) targeted the human actor 1 in 106 target changes, the most-targeted actor on that team; 754 npc.focus-changed events include focus_actor and mode; 76 NPC-vs-NPC server.damage.applied events in the run. Focus hold (elite focus_reacquire_seconds = 1.5 s) reduced focus changes from 4,248 target changes in the pre-hold 130 s sample to 630 focus changes in the post-hold 130 s sample. The isolated tdm-validation client stays at long range, so this run shows target acquisition and shared focus, not NPC-to-human damage; the earlier live journal already showed the human being targeted and killed at close range before the fix. Persisted measurement: .dev/evidence/20261128-tdm-focus-fire.json. Journal is preserved at logs/2026-11-28/20261128_181614/server-01/events.jsonl.
