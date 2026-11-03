# Gameplay and Systems regressions

Append-only master ledger. Each entry links to the detailed dated record that owns the full evidence.

Entries: 26

---

## Entry 1 — Duel-Specific Ownership Breaks General Gamemodes

- Recorded: 2026-09-09T12:20:00Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/gamemodes/gamemodes.md
- Detail: `docs/regressions/2026-09-09/duel-specific-gamemode-ownership-REG.md`

## Entry 2 — Gamemode Session and Actor Lifecycle Regression

- Recorded: 2026-09-09T14:30:00Z
- Status: ATTEMPTED FIX (1)
- Summary: During human FFA playtesting on 2026-09-09, an NPC was counted inconsistently as a killer or victim. After a map change or a second countdown, NPCs could remain at the previous map position, while the visible countdown remained at
- Detail: `docs/regressions/2026-09-09/gamemode-session-actors-REG.md`

## Entry 3 — Camera sway and recoil coupling

- Recorded: 2026-09-09T18:20:00Z
- Status: ATTEMPTED FIX (1)
- Summary: Changing camera sway settings changes only presentation sway. Weapon recoil, explosion shake, firing direction, and collision behavior retain their existing owners and timing. Landing roll tilts the view instead of changing yaw.
- Detail: `docs/regressions/2026-09-09/camera-sway-recoil-coupling-REG.md`

## Entry 4 — Hybrid arm camera aim regression

- Recorded: 2026-09-30T12:03:55-04:00
- Status: ATTEMPTED FIX (5)
- Summary: This tracker covers the hybrid ragdoll arm behavior when physical.hybrid.armscamerafollow is greater than 1.0. The desired result is general weapon behavior, not a revolver-only or rocket-launcher-only offset.
- Detail: `docs/regressions/2026-09-30/hybrid-arm-camera-aim-REG.md`

## Entry 5 — Destruction Uncaught Exception

- Recorded: 2026-10-01T16:55:00Z
- Status: UNRESOLVED
- Summary: docs/specs/destructible-world/destructible-world.md
- Detail: `docs/regressions/2026-10-01/destruction-uncaught-exception-REG.md`

## Entry 6 — Repeat-key weapon unequip

- Recorded: 2026-10-07T13:50:50-04:00
- Status: ATTEMPTED FIX (1)
- Summary: For every weapon-selection key or key binding:
- Detail: `docs/regressions/2026-10-07/repeat-key-unequip-REG.md`

## Entry 7 — Avatar Limb Render Origin Regression

- Recorded: 2026-10-14T00:30:56Z
- Status: ATTEMPTED FIX (3)
- Summary: docs/specs/debug-logging/AI-fix-live.md (avatar transform evidence profile), docs/gold/2026-10-06-avatar-bind-anchors-and-jsonl-evidence.md
- Detail: `docs/regressions/2026-10-14/avatar-limb-render-origin-REG.md`

## Entry 8 — Avatar Node-Name Suffix Detection Regression

- Recorded: 2026-10-14T01:03:37Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/debug-logging/AI-fix-live.md, docs/gold/2026-10-06-avatar-bind-anchors-and-jsonl-evidence.md
- Detail: `docs/regressions/2026-10-14/avatar-node-suffix-detection-REG.md`

## Entry 9 — Melee Action ID Use After Erase

- Recorded: 2026-10-18T16:45:01Z
- Status: ATTEMPTED FIX (1)
- Summary: - docs/specs/gamemodes/supersmashbrosmelee.md - docs/specs/debug-logging/debug-logging.md
- Detail: `docs/regressions/2026-10-18/melee-action-id-use-after-erase-REG.md`

## Entry 10 — Player-to-NPC Melee Percent Reset

- Recorded: 2026-10-18T17:23:10Z
- Status: ATTEMPTED FIX (1)
- Summary: - docs/specs/gamemodes/supersmashbrosmelee.md, section 58
- Detail: `docs/regressions/2026-10-18/melee-npc-percent-reset-REG.md`

## Entry 11 — Melee Hit Feedback and Hitbox Accuracy

- Recorded: 2026-10-18T17:42:50Z
- Status: ATTEMPTED FIX (2)
- Summary: - docs/specs/gamemodes/supersmashbrosmelee.md, sections 58–59
- Detail: `docs/regressions/2026-10-18/melee-hit-feedback-and-hitbox-accuracy-REG.md`

## Entry 12 — Melee NPC Death Server Crash From Stale Dependency

- Recorded: 2026-10-19T21:24:32Z (2026-10-19 17:24:32 EDT, America/NewYork)
- Status: FIXED (source/build-owner fix; runtime recheck pending)
- Summary: - docs/specs/gamemodes/supersmashbrosmelee.md, sections 24, 31, 32, 60
- Detail: `docs/regressions/2026-10-19/melee-npc-death-stale-dependency-REG.md`

## Entry 13 — Melee Local Hit Result And Knockback Overwrite

- Recorded: 2026-10-19T22:36:00Z (2026-10-19 18:36:00 EDT, America/NewYork)
- Status: FIXED (source and runtime journal; human feel acceptance pending)
- Summary: - docs/specs/gamemodes/supersmashbrosmelee.md, sections 24, 26, 27, 28, 59, 60
- Detail: `docs/regressions/2026-10-19/melee-local-hit-result-and-knockback-overwrite-REG.md`

## Entry 14 — Melee Percent And Knockback Scaling

- Recorded: 2026-10-19T22:36:00Z (2026-10-19 18:36:00 EDT, America/NewYork)
- Status: FIXED (source and runtime journal; human feel acceptance pending)
- Summary: - docs/specs/gamemodes/supersmashbrosmelee.md, sections 25, 26, 29, 60
- Detail: `docs/regressions/2026-10-19/melee-percent-and-knockback-scaling-REG.md`

## Entry 15 — Duel NPC Shared Weapon Set

- Recorded: 2026-10-26
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/gamemodes/duel.md
- Detail: `docs/regressions/2026-10-26/duel-npc-shared-weapon-set-REG.md`

## Entry 16 — Player Trail Camera Visibility

- Recorded: 2026-10-26 (reported in chat; exact wall time unavailable)
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/graphics/graphics-presentation.md
- Detail: `docs/regressions/2026-10-26/player-trail-camera-visibility-REG.md`

## Entry 17 — Duel Outline Team Identity

- Recorded: 2026-10-26T00:32:25Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/graphics/graphics-presentation.md
- Detail: `docs/regressions/2026-10-26/duel-outline-team-identity-REG.md`

## Entry 18 — Stretched teleport outline

- Recorded: 2026-10-28T01:52:53Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/gamemodes/teamdeathmatch.md
- Detail: `docs/regressions/2026-10-28/stretched-teleport-outline-REG.md`

## Entry 19 — Deathball world pass-through suppressed actor damage

- Recorded: 2026-11-01T01:54:30Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/weapons/deathball.md, docs/specs/gamemodes/funcounterstrike.md, docs/specs/networking/contact-authority.md
- Detail: `docs/regressions/2026-11-01/deathball-world-pass-suppresses-damage-REG.md`

## Entry 20 — Avatar outlines missing on fallback and local NPC bodies

- Recorded: 2026-11-01T03:22:00Z
- Status: ATTEMPTED FIX (1)
- Summary: config/playervisuals.json, docs/specs/gui/gui.md
- Detail: `docs/regressions/2026-11-01/avatar-outlines-missing-fallback-REG.md`

## Entry 21 — Grenade timing defined in multiple competing places

- Recorded: 2026-11-01T20:50:32Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/weapons/weapons.md, docs/specs/gamemodes/funcounterstrike.md, docs/specs/npc-behavior/npc-behavior.md
- Detail: `docs/regressions/2026-11-01/grenade-timing-split-REG.md`

## Entry 22 — Regression: website auth database outage on 2026-09-16

- Recorded: unknown
- Status: see detail
- Summary: Sign-in and sign-up failed while the website/API still appeared online. The Node process was healthy from PM2's perspective, but PostgreSQL 14/main was down and port 5432 had no listener.
- Detail: `docs/regressions/2026-09-16/2026-09-16-website-auth-database-outage.md`

## Entry 23 — Ragdoll per-death presentation regression — Attempt 1

- Recorded: unknown
- Status: Attempt 1 implemented; live gameplay confirmation pending.
- Summary: Date: 2026-10-06
- Detail: `docs/regressions/2026-10-06/ragdoll-per-death-presentation-ATTEMPT-1-REG.md`

## Entry 24 — Avatar atlas reused a pre-atlas vertex buffer

- Recorded: unknown
- Status: FIXED IN SOURCE; HUMAN VISUAL REVIEW PENDING
- Summary: - Recorded: 2026-10-25T00:14:05Z - Status: FIXED IN SOURCE; HUMAN VISUAL REVIEW PENDING
- Detail: `docs/regressions/2026-10-25/avatar-atlas-upload-cache-REG.md`

## Entry 25 — Sandbox RESET repeated the explode death forever

- Recorded: unknown
- Status: FIXED IN SOURCE; RUNTIME RECHECK PASSED
- Summary: - Recorded: 2026-10-25T00:14:05Z - Status: FIXED IN SOURCE; RUNTIME RECHECK PASSED
- Detail: `docs/regressions/2026-10-25/sandbox-explode-respawn-loop-REG.md`

## Entry 26 — Payload cart keeps the previous map route

- Recorded: unknown
- Status: see detail
- Summary: When Payload changes maps, the cart uses that map's authored origin, route, dimensions, facing, and interaction box. It must not travel along a route from the map that was active before the change.
- Detail: `docs/regressions/2026-10-26/payload-cart-keeps-previous-map-route-REG.md`
