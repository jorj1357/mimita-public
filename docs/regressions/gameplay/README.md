# Gameplay and Systems regressions

Domain regression index. Each linked record owns its chronology and evidence.

Entries: 33
---

## Evidence-backed recurring problems and fixes

| Recurring pattern | Evidence-backed response |
|---|---|
| Gameplay state, authority, identity, or lifecycle differs by mode/actor. | Fix the established authoritative owner and trace the state to visible outcome. |
| A presentation/input symptom masks a shared-system failure. | Record exact mode and actor; validate the actual runtime path before closure. |

---

## Entry 1 — Duel-Specific Ownership Breaks General Gamemodes

- Recorded: 2026-09-09T12:20:00Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/gamemodes/gamemodes.md
- Detail: [duel-specific-gamemode-REG-2026-09-09.md](duel-specific-gamemode-REG-2026-09-09.md)

## Entry 2 — Gamemode Session and Actor Lifecycle Regression

- Recorded: 2026-09-09T14:30:00Z
- Status: ATTEMPTED FIX (1)
- Summary: During human FFA playtesting on 2026-09-09, an NPC was counted inconsistently as a killer or victim. After a map change or a second countdown, NPCs could remain at the previous map position, while the visible countdown remained at
- Detail: [gamemode-session-actors-REG-2026-09-09.md](gamemode-session-actors-REG-2026-09-09.md)

## Entry 3 — Camera sway and recoil coupling

- Recorded: 2026-09-09T18:20:00Z
- Status: ATTEMPTED FIX (1)
- Summary: Changing camera sway settings changes only presentation sway. Weapon recoil, explosion shake, firing direction, and collision behavior retain their existing owners and timing. Landing roll tilts the view instead of changing yaw.
- Detail: [camera-sway-recoil-REG-2026-09-09.md](camera-sway-recoil-REG-2026-09-09.md)

## Entry 4 — Hybrid arm camera aim regression

- Recorded: 2026-09-30T12:03:55-04:00
- Status: ATTEMPTED FIX (5)
- Summary: This tracker covers the hybrid ragdoll arm behavior when physical.hybrid.armscamerafollow is greater than 1.0. The desired result is general weapon behavior, not a revolver-only or rocket-launcher-only offset.
- Detail: [hybrid-arm-camera-REG-2026-09-30.md](hybrid-arm-camera-REG-2026-09-30.md)

## Entry 5 — Destruction Uncaught Exception

- Recorded: 2026-10-01T16:55:00Z
- Status: UNRESOLVED
- Summary: docs/specs/destructible-world/destructible-world.md
- Detail: [destruction-uncaught-exception-REG-2026-10-01.md](destruction-uncaught-exception-REG-2026-10-01.md)

## Entry 6 — Repeat-key weapon unequip

- Recorded: 2026-10-07T13:50:50-04:00
- Status: ATTEMPTED FIX (1)
- Summary: For every weapon-selection key or key binding:
- Detail: [repeat-key-unequip-REG-2026-10-07.md](repeat-key-unequip-REG-2026-10-07.md)

## Entry 7 — Avatar Limb Render Origin Regression

- Recorded: 2026-10-14T00:30:56Z
- Status: ATTEMPTED FIX (3)
- Summary: docs/specs/debug-logging/AI-fix-live.md (avatar transform evidence profile), docs/gold/2026-10-06-avatar-bind-anchors-and-jsonl-evidence.md
- Detail: [avatar-limb-render-REG-2026-10-14.md](avatar-limb-render-REG-2026-10-14.md)

## Entry 8 — Avatar Node-Name Suffix Detection Regression

- Recorded: 2026-10-14T01:03:37Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/debug-logging/AI-fix-live.md, docs/gold/2026-10-06-avatar-bind-anchors-and-jsonl-evidence.md
- Detail: [avatar-node-suffix-REG-2026-10-14.md](avatar-node-suffix-REG-2026-10-14.md)

## Entry 9 — Melee Action ID Use After Erase

- Recorded: 2026-10-18T16:45:01Z
- Status: ATTEMPTED FIX (1)
- Summary: - docs/specs/gamemodes/supersmashbrosmelee.md - docs/specs/debug-logging/debug-logging.md
- Detail: [melee-action-id-REG-2026-10-18.md](melee-action-id-REG-2026-10-18.md)

## Entry 10 — Player-to-NPC Melee Percent Reset

- Recorded: 2026-10-18T17:23:10Z
- Status: ATTEMPTED FIX (1)
- Summary: - docs/specs/gamemodes/supersmashbrosmelee.md, section 58
- Detail: [melee-npc-percent-REG-2026-10-18.md](melee-npc-percent-REG-2026-10-18.md)

## Entry 11 — Melee Hit Feedback and Hitbox Accuracy

- Recorded: 2026-10-18T17:42:50Z
- Status: ATTEMPTED FIX (2)
- Summary: - docs/specs/gamemodes/supersmashbrosmelee.md, sections 58–59
- Detail: [melee-hit-feedback-REG-2026-10-18.md](melee-hit-feedback-REG-2026-10-18.md)

## Entry 12 — Melee NPC Death Server Crash From Stale Dependency

- Recorded: 2026-10-19T21:24:32Z (2026-10-19 17:24:32 EDT, America/NewYork)
- Status: FIXED (source/build-owner fix; runtime recheck pending)
- Summary: - docs/specs/gamemodes/supersmashbrosmelee.md, sections 24, 31, 32, 60
- Detail: [melee-npc-death-REG-2026-10-19.md](melee-npc-death-REG-2026-10-19.md)

## Entry 13 — Melee Local Hit Result And Knockback Overwrite

- Recorded: 2026-10-19T22:36:00Z (2026-10-19 18:36:00 EDT, America/NewYork)
- Status: FIXED (source and runtime journal; human feel acceptance pending)
- Summary: - docs/specs/gamemodes/supersmashbrosmelee.md, sections 24, 26, 27, 28, 59, 60
- Detail: [melee-local-hit-REG-2026-10-19.md](melee-local-hit-REG-2026-10-19.md)

## Entry 14 — Melee Percent And Knockback Scaling

- Recorded: 2026-10-19T22:36:00Z (2026-10-19 18:36:00 EDT, America/NewYork)
- Status: FIXED (source and runtime journal; human feel acceptance pending)
- Summary: - docs/specs/gamemodes/supersmashbrosmelee.md, sections 25, 26, 29, 60
- Detail: [melee-percent-and-REG-2026-10-19.md](melee-percent-and-REG-2026-10-19.md)

## Entry 15 — Duel NPC Shared Weapon Set

- Recorded: 2026-10-26
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/gamemodes/duel.md
- Detail: [duel-npc-shared-REG-2026-10-26.md](duel-npc-shared-REG-2026-10-26.md)

## Entry 16 — Player Trail Camera Visibility

- Recorded: 2026-10-26 (reported in chat; exact wall time unavailable)
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/graphics/graphics-presentation.md
- Detail: [player-trail-camera-REG-2026-10-26.md](player-trail-camera-REG-2026-10-26.md)

## Entry 17 — Duel Outline Team Identity

- Recorded: 2026-10-26T00:32:25Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/graphics/graphics-presentation.md
- Detail: [duel-outline-team-REG-2026-10-26.md](duel-outline-team-REG-2026-10-26.md)

## Entry 18 — Stretched teleport outline

- Recorded: 2026-10-28T01:52:53Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/gamemodes/teamdeathmatch.md
- Detail: [stretched-teleport-outline-REG-2026-10-28.md](stretched-teleport-outline-REG-2026-10-28.md)

## Entry 19 — Deathball world pass-through suppressed actor damage

- Recorded: 2026-11-01T01:54:30Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/weapons/deathball.md, docs/specs/gamemodes/funcounterstrike.md, docs/specs/networking/contact-authority.md
- Detail: [deathball-world-pass-REG-2026-11-01.md](deathball-world-pass-REG-2026-11-01.md)

## Entry 20 — Avatar outlines missing on fallback and local NPC bodies

- Recorded: 2026-11-01T03:22:00Z
- Status: ATTEMPTED FIX (1)
- Summary: config/playervisuals.json, docs/specs/gui/gui.md
- Detail: [avatar-outlines-missing-REG-2026-11-01.md](avatar-outlines-missing-REG-2026-11-01.md)

## Entry 21 — Grenade timing defined in multiple competing places

- Recorded: 2026-11-01T20:50:32Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/weapons/weapons.md, docs/specs/gamemodes/funcounterstrike.md, docs/specs/npc-behavior/npc-behavior.md
- Detail: [grenade-timing-split-REG-2026-11-01.md](grenade-timing-split-REG-2026-11-01.md)

## Entry 22 — Regression: website auth database outage on 2026-09-16

- Recorded: unknown
- Status: see detail
- Summary: Sign-in and sign-up failed while the website/API still appeared online. The Node process was healthy from PM2's perspective, but PostgreSQL 14/main was down and port 5432 had no listener.
- Detail: [website-auth-database-REG-2026-09-16.md](website-auth-database-REG-2026-09-16.md)

## Entry 23 — Ragdoll per-death presentation regression — Attempt 1

- Recorded: unknown
- Status: Attempt 1 implemented; live gameplay confirmation pending.
- Summary: Date: 2026-10-06
- Detail: [ragdoll-per-death-REG-2026-10-06.md](ragdoll-per-death-REG-2026-10-06.md)

## Entry 24 — Avatar atlas reused a pre-atlas vertex buffer

- Recorded: unknown
- Status: FIXED IN SOURCE; HUMAN VISUAL REVIEW PENDING
- Summary: - Recorded: 2026-10-25T00:14:05Z - Status: FIXED IN SOURCE; HUMAN VISUAL REVIEW PENDING
- Detail: [avatar-atlas-upload-REG-2026-10-25.md](avatar-atlas-upload-REG-2026-10-25.md)

## Entry 25 — Sandbox RESET repeated the explode death forever

- Recorded: unknown
- Status: FIXED IN SOURCE; RUNTIME RECHECK PASSED
- Summary: - Recorded: 2026-10-25T00:14:05Z - Status: FIXED IN SOURCE; RUNTIME RECHECK PASSED
- Detail: [sandbox-explode-respawn-REG-2026-10-25.md](sandbox-explode-respawn-REG-2026-10-25.md)

## Entry 26 — Payload cart keeps the previous map route

- Recorded: unknown
- Status: see detail
- Summary: When Payload changes maps, the cart uses that map's authored origin, route, dimensions, facing, and interaction box. It must not travel along a route from the map that was active before the change.
- Detail: [payload-cart-keeps-REG-2026-10-26.md](payload-cart-keeps-REG-2026-10-26.md)

## Entry 27 — GUI weapon-set override was replaced by the gamemode default

- Recorded: 2026-11-03T23:04:29Z
- Status: ATTEMPTED FIX (1) — explicit TDM/SSBM runtime override evidence recorded; Auto and GUI review pending
- Summary: docs/specs/gamemodes/gamemodes.md, GUI server-launch settings
- Detail: [gui-weapon-set-REG-2026-11-03.md](gui-weapon-set-REG-2026-11-03.md)

## Entry 28 — Holding F did not defuse for non-designated human CTs

- Recorded: 2026-11-04T11:10:00Z
- Status: ATTEMPTED FIX (1) — live journal proof recorded; human play confirmation pending
- Summary: docs/specs/gamemodes/funcounterstrike.md — the NPC designated-defuser assignment was applied as an exclusive lock on human players.
- Detail: [human-defuse-designated-REG-2026-11-04.md](human-defuse-designated-REG-2026-11-04.md)

## Entry 29 — RMB weapon aim reversal

- Recorded: 2026-11-05 (user report; exact report time unavailable)
- Status: ATTEMPTED FIX (2) — final-build live journal direction proof passed; human visual review pending
- Summary: The shared aim solver omitted the right-arm bind transform between the rendered arm node and canonical physics body when mapping the weapon barrel axis.
- Detail: [rmb-weapon-aim-REG-2026-11-05.md](rmb-weapon-aim-REG-2026-11-05.md)

## Entry 30 — Fun Counter Strike inherited the dev profile's Captain Falcon loadout

- Recorded: 2026-11-05T17:56:10Z
- Status: FIXED IN SOURCE; LIVE JOURNAL PASS; HUMAN VISUAL REVIEW PENDING
- Summary: Launch mode 45 appended Fun Counter Strike mode flags while retaining the `npc-navigation` profile's explicit `--weapon-set 12`, forcing Captain Falcon weapons over the mode and role loadouts.
- Detail: [funcounterstrike-launch-loadout-REG-2026-11-05.md](funcounterstrike-launch-loadout-REG-2026-11-05.md)

## Entry 31 — Terminal-spawned car did not enter with F

- Recorded: 2026-11-04 (time not recorded)
- Status: fixed
- Summary: The terminal-spawned vehicle was client-local, but the interaction path treated any multiplayer context as server-authoritative; the seat request was rejected because the server had no matching vehicle.
- Detail: [vehicle-spawn-local-REG-2026-11-04.md](vehicle-spawn-local-REG-2026-11-04.md)

## Entry 32 — Fun Counter Strike spawned an unrostered startup NPC

- Recorded: 2026-11-05 (captured run started at 2026-11-05T20:06:15Z)
- Status: ATTEMPTED FIX — build passed; fresh FCS runtime verification pending; reported Boomer identity unresolved
- Summary: The generic startup `--npcs` path created actor 1000 with avatar `demongirl` before FCS had assigned a team and role, independently of the persistent NPC-spawn policy.
- Detail: [funcounterstrike-unrostered-startup-npc-REG-2026-11-05.md](funcounterstrike-unrostered-startup-npc-REG-2026-11-05.md)

## Entry 33 — Avatar model fallback stayed as a green capsule

- Recorded: 2026-11-06T07:17:03Z
- Status: FIXED IN SOURCE — runtime model load and movement verified; visual review pending
- Summary: A bound avatar instance could leave `model_loaded=false` forever because `render-player.cpp` set `lazy_load_requested=true` without queueing a model load. The new procedural meat/bone layer generation also called a GL texture lookup from the async model-load worker, preventing model completion. Failed avatar loads were not retried. Runtime run `20261106_071418` now reports `model_loaded=true`, `body_mesh_count=6`, populated body parts, and successful `agent.move`.
- Detail: [avatar-model-fallback-REG-2026-11-06.md](avatar-model-fallback-REG-2026-11-06.md)
