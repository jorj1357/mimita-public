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
