# Swordsword contact and equipment behavior

- Result: `PASS_WITH_HUMAN_REVIEW`. Source and configuration are aligned and build 2720 published. Live visual/combat acceptance remains pending.
- Local time: 2026-10-20 22:15:07 EDT.
- Canonical time: 2026-10-21T02:15:07Z.
- Branch: `20261011zt`.
- HEAD: `f6d6eeb2ce409153c8d23933b32c77eee39a5b48`.
- The active server was kept running on build 2718 (PID 20772); latest successful build is 2720.

## User report and cause

The Swordsword definition uses slot 4. Its entry retained the existing model but had `pose_id: hafs`, no `animation_profile`, no animation actions, and no configured contact sound/force values. The current actor contact owner requires a nonempty `animationProfile`; the legacy Swordsword geometry path had already been retired in favor of the equipped-model blade. That mismatch left Swordsword without an authoritative blade shape. Ordinary NPC damage through hitscan was present in the active journal, so this diagnosis is specific to the Swordsword configuration.

## Change

Restored the Swordsword configuration for the existing shared system: weapon-selected animation profile, four action clips, double-click window, held defensive pose, no action hitboxes or impulses, contact damage/knockback/force settings, and the existing Swordsword impact sound. The existing GLB and slot remain unchanged. The force, damage, knockback, audio, and guard values are initial playable tuning; the specification leaves their final balance open.

This file records the implementation completed in this AI session: an equipped-weapon animation profile path; primary and secondary double-click action handling; fixed-tick model blade attachment and swept body-part contact; actor-neutral server damage, confirmation, death, and audio; stable equipped weapon/action replication; NPC action selection; an offline adapter using the shared contact owner; and owner-level contact diagnostics. Weapon/profile identity and action phase use the existing animation, weapon, damage, and snapshot owners. No `weaponcollisions.json` entry was added.

## Changed-file excerpts

Each quote is verbatim. “No prior text at this insertion” means that the quoted addition had no corresponding line before this session; “No previous file” means the file was created in this session. Line numbers identify the recorded excerpt.

| File | Line | Before | After |
|---|---:|---|---|
| [`config/animations/library/weapons/swordsword.json`](../../../config/animations/library/weapons/swordsword.json) | 3 | No previous file. | `"poseComposition": "authored_only",` |
| [`config/weapons.json`](../../../config/weapons.json) | 366 | `"pose_id": "hafs",` | `"pose_id": "swordsword",` |
| [`src/combat/melee-client.cpp`](../../../src/combat/melee-client.cpp) | 368 | `g_shieldHeld = input.shieldHeld || g_debugShield;` | `g_shieldHeld = !moveSet->animationOnly && (input.shieldHeld || g_debugShield);` |
| [`src/combat/melee-client.h`](../../../src/combat/melee-client.h) | 29 | No prior text at this insertion. | `bool secondaryHeld = false;` |
| [`src/combat/weapon-attachment.cpp`](../../../src/combat/weapon-attachment.cpp) | 23 | No previous file. | `bool resolveWeaponAttachment(const WeaponDefinition& def,` |
| [`src/combat/weapon-attachment.h`](../../../src/combat/weapon-attachment.h) | 19 | No previous file. | `bool resolveWeaponAttachment(const WeaponDefinition& definition,` |
| [`src/combat/weapon-audio.cpp`](../../../src/combat/weapon-audio.cpp) | 15 | No prior text at this insertion. | `void playContactSound(const WeaponDefinition& def, const glm::vec3& position, float force)` |
| [`src/combat/weapon-audio.h`](../../../src/combat/weapon-audio.h) | 17 | No prior text at this insertion. | `void playContactSound(const WeaponDefinition& def, const glm::vec3& position, float force);` |
| [`src/combat/weapon-contact-local.cpp`](../../../src/combat/weapon-contact-local.cpp) | 11 | No previous file. | `void tickLocalPhysicalContactWeapons(Player& player, NpcSystem& npcSystem, uint32_t tick)` |
| [`src/combat/weapon-contact-local.h`](../../../src/combat/weapon-contact-local.h) | 7 | No previous file. | `void tickLocalPhysicalContactWeapons(Player& player, NpcSystem& npcs, uint32_t tick);` |
| [`src/combat/weapon-execution.cpp`](../../../src/combat/weapon-execution.cpp) | 82 | No prior text at this insertion. | `if (!target.bodyParts.empty()) {` |
| [`src/combat/weapon-json-config.cpp`](../../../src/combat/weapon-json-config.cpp) | 261 | No prior text at this insertion. | `weaponJsonString(root, "animation_profile", def.animationProfile);` |
| [`src/combat/weapon-melee-config.cpp`](../../../src/combat/weapon-melee-config.cpp) | 305 | No prior text at this insertion. | `out.holdWhileButton = j.value("hold_while_button", false);` |
| [`src/combat/weapon-melee-config.h`](../../../src/combat/weapon-melee-config.h) | 161 | No prior text at this insertion. | `bool animationOnly = false; // continuous-contact weapons reuse actions for presentation` |
| [`src/combat/weapon-swordsword.cpp`](../../../src/combat/weapon-swordsword.cpp) | 465 | No prior text at this insertion. | `if (!def.animationProfile.empty()) return;` |
| [`src/combat/weapon-types.h`](../../../src/combat/weapon-types.h) | 186 | No prior text at this insertion. | `std::string animationProfile; // shared clip profile selected by equipped identity` |
| [`src/combat/weapon-viewmodel.cpp`](../../../src/combat/weapon-viewmodel.cpp) | 496 | No prior text at this insertion. | `weaponTransform = attachment.world;` |
| [`src/engine/engine-tick-camera.cpp`](../../../src/engine/engine-tick-camera.cpp) | 1067 | No prior text at this insertion. | `if (player.usesMeleeAuthoredPose()) held = false;` |
| [`src/engine/engine-tick-combat.cpp`](../../../src/engine/engine-tick-combat.cpp) | 338 | No prior text at this insertion. | `meleeInput.secondaryHeld = rightMouseDown;` |
| [`src/entities/player-animation.cpp`](../../../src/entities/player-animation.cpp) | 205 | No prior text at this insertion. | `return def && (!def->animationProfile.empty() ||` |
| [`src/entities/player-avatar-animations.cpp`](../../../src/entities/player-avatar-animations.cpp) | 196 | No prior text at this insertion. | `const bool sharedProfile = avatarName.rfind("profile:", 0) == 0;` |
| [`src/main-systems.cpp`](../../../src/main-systems.cpp) | 337 | No prior text at this insertion. | `"agent.weaponbutton", "Press/release a shared weapon-action input.",` |
| [`src/network/multiplayer-context.h`](../../../src/network/multiplayer-context.h) | 120 | No prior text at this insertion. | `uint16_t equippedWeaponDefNetworkId = 0;` |
| [`src/network/multiplayer-interpolation.cpp`](../../../src/network/multiplayer-interpolation.cpp) | 77 | No prior text at this insertion. | `transform.weaponActionIndex = entity.weaponActionIndex;` |
| [`src/network/multiplayer-projectiles.cpp`](../../../src/network/multiplayer-projectiles.cpp) | 1266 | No prior text at this insertion. | `if (definition) WeaponAudio::playContactSound(*definition,` |
| [`src/network/multiplayer-shots.cpp`](../../../src/network/multiplayer-shots.cpp) | 244 | No prior text at this insertion. | `if (definition) WeaponAudio::playContactSound(*definition,` |
| [`src/network/multiplayer-tick.cpp`](../../../src/network/multiplayer-tick.cpp) | 1016 | `char buffer[16384];` | `char buffer[MAX_RECEIVE_PACKET_BYTES];` |
| [`src/network/packets.h`](../../../src/network/packets.h) | 26 | `constexpr uint16_t PROTOCOL_VERSION = 42;` | `constexpr uint16_t PROTOCOL_VERSION = 43;` |
| [`src/network/server-attack.cpp`](../../../src/network/server-attack.cpp) | 1269 | No prior text at this insertion. | `if (meleeSet->animationOnly && req->attackVariant == 255) {` |
| [`src/network/server-damage.cpp`](../../../src/network/server-damage.cpp) | 484 | No prior text at this insertion. | `event.contactForce = contactForce;` |
| [`src/network/server-melee-actions.cpp`](../../../src/network/server-melee-actions.cpp) | 768 | No prior text at this insertion. | `void tickServerWeaponAnimationActions(std::unordered_map<uint32_t, ServerPlayer>& players,` |
| [`src/network/server-npcs.cpp`](../../../src/network/server-npcs.cpp) | 844 | No prior text at this insertion. | `sn.equippedWeaponId = n.body.equippedWeaponId;` |
| [`src/network/server-physical-contact-actors.cpp`](../../../src/network/server-physical-contact-actors.cpp) | 200 | No previous file. | `void tickServerAnimatedPhysicalContactWeapons(SOCKET sock,` |
| [`src/network/server-physical-contact.cpp`](../../../src/network/server-physical-contact.cpp) | 535 | No prior text at this insertion. | `tickServerAnimatedPhysicalContactWeapons(sock, players, npcs, tick, totalPacketsOut);` |
| [`src/network/server-players.cpp`](../../../src/network/server-players.cpp) | 1675 | No prior text at this insertion. | `serverWriteWeaponActionSnapshot(player.equippedWeaponId, player.meleeActionActive,` |
| [`src/network/server.cpp`](../../../src/network/server.cpp) | 910 | `tickServerPhysicalContactWeapons(sock, players, npcs, world, SERVER_DT, tick, totalPacketsOut);` | `tickServerMeleeActions(sock, players, npcs, tick, totalPacketsOut);` |
| [`src/network/server.h`](../../../src/network/server.h) | 776 | No prior text at this insertion. | `struct ServerPhysicalContactResult {` |
| [`src/network/snapshot-chunks.cpp`](../../../src/network/snapshot-chunks.cpp) | 154 | No prior text at this insertion. | `out.weaponActionIndex = entity.weaponActionIndex;` |
| [`src/network/snapshot-chunks.h`](../../../src/network/snapshot-chunks.h) | 13 | `constexpr uint32_t SNAPSHOT_CHUNK_MAX_ENTITIES = 7;` | `constexpr uint32_t SNAPSHOT_CHUNK_MAX_ENTITIES = 6;` |
| [`src/npc/npc-combat.cpp`](../../../src/npc/npc-combat.cpp) | 459 | No prior text at this insertion. | `// Continuous blade contact and animation actions are server-owned;` |
| [`src/physics/movement/actor-collision-mesh.cpp`](../../../src/physics/movement/actor-collision-mesh.cpp) | 307 | No prior text at this insertion. | `const std::lock_guard<std::mutex> cacheLock(cacheMutex);` |
| [`src/sim/simulate-tick.cpp`](../../../src/sim/simulate-tick.cpp) | 412 | No prior text at this insertion. | `tickLocalPhysicalContactWeapons(*sim.player, *sim.npcSystem, (uint32_t)sim.tick);` |
| [`src/terminal/weapon-commands.cpp`](../../../src/terminal/weapon-commands.cpp) | 215 | No prior text at this insertion. | `MimitaMelee::requestButton(false, true);` |

## Evidence

- Routed Swordsword, shared weapon-contact, animation, NPC actor, collision, networking, config, build/runtime, logging, and task-completion documentation was reviewed. Focused reviews: `docs/skills/spec-behavior-review-v1.md`, `docs/skills/logging-checker-v1.md`, `docs/skills/efficiency-checker-v1.md`, and `docs/skills/asset-checker-v1.md`.
- Configuration check passed: slot 4; profile and all four referenced clips resolve; clip markers and six-part authored poses exist; action hitboxes and impulses are empty; existing model file exists.
- Build 2712 and final build 2720 succeeded through the canonical dev loop. `--snapshot-chunk-selftest` passed on build 2720. `--versioninfo` identified `C:\\mimita-v9\\.dev\\builds\\2720\\mimita.exe` and its journal path; it is executable metadata, not gameplay evidence.
- The active dev server was not restarted. At final review, the managed server remained build 2718 while build 2720 was the latest published build. The weapon configuration owner checks for edits during server ticks, so the restored JSON can reload in the running process; the journal had not shown a Swordsword contact event during this review.
- No real four-pair Swordsword kill run, human animation review, or visual blade-to-contact comparison was performed. Do not treat build success or the component/config checks as proof of those outcomes. No regression was confirmed.

## Preserved work

Existing unrelated edits in account, collision, analytics, animation, audio/video, NPC, gamemode, graphics, map, role, and actor-pair-collision files were preserved. Earlier Swordsword specification/contact changelogs present before this record were not edited or replaced.
