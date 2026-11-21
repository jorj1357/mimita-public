# NPC melee source and runtime trace

## Correction

The earlier status-only conclusion missed pinned runtime archives. Current status alone said no active managed run, but the checkout contains relevant TDM melee evidence in run 20261121_140624. The dashboard attempt 20261121_152950_01 was indeed a separate failed Infinite World launch, not the user's melee scenario.

## Source history

- Commit a065471f (2026-11-20 02:56 EST) is the relevant direct melee change: Mace action configs gained contact_damage_multiplier=10; client/server action-active predicates were aligned; Arcade server animated physical contact was changed to skip player attackers and weapon-world/clash behavior while preserving NPC damage contacts. The changed files are config/weapons/mace/config.json, src/combat/melee-client.cpp, src/combat/weapon-viewmodel.cpp, src/network/server-packet-handlers.cpp, and src/network/server-physical-contact-actors.cpp.
- Its changelog docs/changelog/2026-11-20/20261120_072300-arcade-npc-melee-and-mace-active-pose.md reports generation 398 compiled, but the two isolated launches failed before logger.started. It did not establish runtime behavior.
- Commit d651e133 (2026-11-20 18:25 EST) has a melee-sounding message but its actual source diffs are reconnect-token fingerprinting and connection-health text in src/network/server-packets.cpp and src/network/multiplayer-packets.cpp; it did not change melee damage.
- Current uncommitted diff in src/engine/engine-tick-combat.cpp gates AI melee input on the target-gated AI primary intent. The normal input branch still uses human mouse press/hold state; this diff does not disable manual melee. Current src/npc/npc.cpp/config changes affect NPC movement/profile behavior, not the server melee damage resolver.
- No current working-tree diff was found in the melee client, melee server claim handler, server physical-contact actor owner, or melee weapon configs.

## Runtime evidence

Pinned run 20261121_140624 used profile tdm-melee-validation and scenario melee_tdm_ai_v1, which explicitly switched the local player to AI melee Mace. It used game DLL generation 420 and EXE artifact 2026-11-21T02:26:50.306117Z (build_match=false), so this is not a human-controlled Fun Counter Strike reproduction.

Its client journal contains 935 Mace and 100 Chainsaw claims; 924 batches were sent. Server journal records all 924 batches received, 321 accepted (285 Mace, 36 Chainsaw) with authoritative player-attacker damage applied to NPCs, and 629 rejected: 383 invalid_contact_bounds, 82 target_generation_changed, 58 out_of_reach, 51 target_dead, 47 friendly_fire, 8 world_occluded.

For the 285 accepted Mace claims, 232 were outside the active pose at multiplier 1.0 and dealt 1–5 damage. Fifty-three were active-pose claims at multiplier 10.0; 27 dealt 250, with others at higher values. This shows melee was not globally disabled in that AI-driven TDM run. Low damage outside the active pose and rejected contacts are plausible contributors, but do not prove the reported human/Fun Counter Strike failure.

The client and server journals are logs/2026-11-21/20261121_140624/client-01/events.jsonl and server-01/events.jsonl; the pinned generation and artifact are recorded in logs/2026-11-21/20261121_140624/dev-loop/events.jsonl. Related earlier evidence: generation 393 had sent client claims but zero server receipt; generation 417 then received 145 AI claims but accepted zero because the AI targeted teammates and claims also failed bounds validation. The latter is documented in issue-503f3e410c01.

## Conclusion

The source history contains no recent change that globally turns off human melee damage. The best concrete leads are low base damage for Mace contacts outside its active phase and server rejection of many contact claims. The exact reported issue remains unproven because the available high-count run was AI-driven TDM, not human-controlled Fun Counter Strike. Follow-up needs a human-controlled pinned FCS run and correlation of input, client claim, server receipt/rejection, and NPC health change.
