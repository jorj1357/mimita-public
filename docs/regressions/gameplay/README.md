# Gameplay and Systems regressions

Domain regression index. Each linked record owns its chronology and evidence.

Entries: 46
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
- Status: RECOVERED AFTER APPROVED LOG CLEANUP — PostgreSQL accepts connections and auth reaches it; prevention, health route, and email alerts remain pending; exact stop cause unverified
- Summary: The 2026-11-21 recurrence had the API online while PostgreSQL 14/main was down and root disk full. After approved truncation of the runaway rotated syslog, PostgreSQL started and public invalid-credentials auth returned HTTP 401. Readiness remains 404; watchdog and email alerting are not live.
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

## Entry 34 — Healthall left active NPC maximum health stale

- Recorded: 2026-11-06T23:37:00Z (2026-11-06 18:37 EST, America/New_York)
- Status: FIXED IN SOURCE; LIVE COMMAND AND 500/500 HUD VERIFIED; DAMAGE-SOAK REVIEW PENDING
- Summary: The host command raised the server NPC mirror's current health but left each live NPC body's `maxHp` at its prior value. The command now updates the authoritative body and mirror together and records healed player/NPC counts.
- Detail: [healthall-live-npc-REG-2026-11-06.md](healthall-live-npc-REG-2026-11-06.md)

## Entry 35 — healthall 500 did not raise lethal body-part health

- Recorded: 2026-11-07T15:44:30Z
- Status: MODE-SCOPED FIX IN SOURCE; BUILD/RUNTIME VALIDATION PENDING
- Summary: `healthall 500` sets actor current/max HP to 500, but damage also drains each body part's independent 100 HP pool. Destroying the torso/head forces death while global HP remains above zero. The command does not restore body-part health.
- Detail: [healthall-body-part-lethal-REG-2026-11-07.md](healthall-body-part-lethal-REG-2026-11-07.md)


## Entry 34 — Unlit cosmetic GLBs render black

- Recorded: 2026-11-15T02:20:00Z
- Status: OPEN — source fix and logging implemented; visible Jason-mask acceptance pending
- Summary: Cosmetic GLBs contain geometry but no materials/textures. The cosmetic renderer sampled the default/missing texture and had no solid-color fallback. Remote cosmetic texture paths also used the local avatar name.
- Detail: [black-cosmetic-glb-REG-2026-11-15.md](black-cosmetic-glb-REG-2026-11-15.md)

## Entry 35 — NPC avatar names truncated on the wire

- Recorded: 2026-11-15T02:20:00Z
- Status: OPEN — wire capacity and hot prefix resolver implemented; runtime acceptance pending
- Summary: `MAX_AVATAR_NAME_BYTES` was 24, while `remote_<24 hex>` names are 31 characters. NPC avatar names were truncated into nonexistent avatar folders.
- Detail: [npc-avatar-name-truncation-REG-2026-11-15.md](npc-avatar-name-truncation-REG-2026-11-15.md)

## Entry 36 — Hot-client world and text sampled black

- Recorded: 2026-11-15T09:05:00-05:00
- Status: FIXED — generation 182/183 runtime probe and visible world verified
- Summary: The DLL texture-parameter bridge forwarded semantic `TextureValue` numbers (`Linear=2`) instead of GL tokens (`0x2601`). Min filters became invalid, level-0-only textures sampled black, and the hot client showed a black world with dark HUD text. The bridge now maps texture values before forwarding.
- Detail: [black-world-texture-filter-REG-2026-11-15.md](black-world-texture-filter-REG-2026-11-15.md)

### Follow-up resolution notes

- Entry 34: the shared texture-filter bug was a second root cause for black
  cosmetics; see Entry 36.
- Entry 35: the generation 183 run had no `no_worker` or `worker_queue_full`
  avatar failures; normal pending loads completed.

## Entry 37 — Black boxes around font glyphs

- Recorded: 2026-11-15T16:20:00-05:00
- Status: RESOLVED / not currently reproducing — documentation-only record
- Summary: The DLL texture-parameter bridge forwarded semantic `TextureValue` numbers instead of GL tokens. Invalid min filters left level-0-only font pages incomplete, so glyph quads sampled black. The texture-token fix also resolved the black world/cosmetic symptoms.
- Detail: [black-font-glyph-boxes-REG-2026-11-15.md](black-font-glyph-boxes-REG-2026-11-15.md)
- Changelog: [20261115_090500-hot-client-texture-filter-world-visible.md](../../changelog/2026-11-15/20261115_090500-hot-client-texture-filter-world-visible.md), [20261114_222000-hot-client-live-default-cosmetic-avatar-fixes.md](../../changelog/2026-11-14/20261114_222000-hot-client-live-default-cosmetic-avatar-fixes.md)

## Entry 38 — Hot-client input and mouse look

- Recorded: 2026-11-15T17:30:00-05:00
- Status: PARTIALLY FIXED — host keys/terminal/equip/agent input verified; physical mouse-look and all UI buttons pending human acceptance
- Detail: [input-hot-client-REG-2026-11-15.md](input-hot-client-REG-2026-11-15.md)

## Entry 39 — Avatar body textures missing

- Recorded: 2026-11-15T17:30:00-05:00
- Status: OPEN — diagnostics/fallback added; root cause pending
- Detail: [avatar-textures-missing-REG-2026-11-15.md](avatar-textures-missing-REG-2026-11-15.md)

## Entry 40 — Map objects shift on generation activation

- Recorded: 2026-11-15T17:30:00-05:00
- Status: OPEN — idempotent authored-geometry fix required
- Detail: [map-object-activation-REG-2026-11-15.md](map-object-activation-REG-2026-11-15.md)

## Entry 41 — NPC collision and authored animation

- Recorded: 2026-11-15T17:30:00-05:00
- Status: OPEN — both must be fixed hot
- Detail: [npc-collision-stuck-REG-2026-11-15.md](npc-collision-stuck-REG-2026-11-15.md), [npc-authored-animation-REG-2026-11-15.md](npc-authored-animation-REG-2026-11-15.md)

## Entry 42 — Duel NPC weapon cycle escaped its rocket-only loadout

- Recorded: 2026-11-18T06:36:49Z
- Status: SOURCE FIX SAVED — hot build observed in progress; runtime behavior pending
- Summary: The elite NPC weapon cycle could choose profile-listed melee weapons even when the Duel host override resolved that NPC to Rocket Launcher plus `nothing`.
- Detail: [duel-npc-weapon-cycle-REG-2026-11-18.md](duel-npc-weapon-cycle-REG-2026-11-18.md)

## Entry 43 — Map editor keyboard and text-field shortcuts

- Recorded: 2026-11-18 (user report; exact report time unavailable)
- Status: SOURCE FIX SAVED — live input validation pending
- Summary: The host-dispatched hot-client key path skipped the Map Editor key owners, and shared text fields had no local undo/redo history.
- Detail: [map-editor-keyboard-routing-REG-2026-11-18.md](map-editor-keyboard-routing-REG-2026-11-18.md)

## Entry 44 — Opening the terminal typed the grave key

- Recorded: 2026-11-18 (user report; exact report time unavailable)
- Status: SOURCE FIX SAVED — live input validation pending
- Summary: The hot-client key callback toggled the terminal on grave, then GLFW's character callback delivered the same press as text after the terminal had opened.
- Detail: [console-grave-toggle-REG-2026-11-18.md](console-grave-toggle-REG-2026-11-18.md)
- Changelog: [Console grave toggle input](../../changelog/2026-11-18/20261118_093329-console-grave-toggle-input.md)

## Entry 45 — Keyboard and controller eaten by repeated ALT presses

- Recorded: 2026-11-27T07:04:00Z
- Status: FIXED IN GENERATION 783 — physical keyboard/pad acceptance pending
- Summary: The uncommitted focus fix put `keybd_event(VK_MENU)` inside `cachedWindow()`, which runs from every key/mouse/focus query. When the game window handle was temporarily unresolved it synthesized ALT down/up on every call, spamming the menu key and swallowing keyboard and pad input. HEAD's `cachedWindow()` had no ALT call. ALT now lives only in the explicit `focus_game_window` command, throttled; the query path only rescans the window once per second.
- Detail: [keyboard-alt-spam-REG-2026-11-27.md](keyboard-alt-spam-REG-2026-11-27.md)

## Entry 46 — Bluetooth DualShock 4 detected but all input axes zero

- Recorded: 2026-11-27T08:30Z
- Status: FIXED IN GENERATION 794 with the raw-HID fallback — human-confirmed
- Summary: GLFW 3.4.0 enumerated the Bluetooth DS4 (VID 054C PID 09CC) and reported a standard mapping, but both the gamepad state and the raw joystick state returned zero axes. Windows and Roblox received the pad; `joy.cpl`/DirectInput did not. The game had no alternate backend, and same-day focus/capture/ALT changes masked it. A raw-HID fallback now decodes Bluetooth report 0x11 (and USB 0x01) into the existing bindings, with a read-only probe and live-data logging. Rule: a connected all-zero device is not healthy; a controller is proven only by changing axes/buttons.
- Detail: [ds4-bluetooth-zero-input-REG-2026-11-27.md](ds4-bluetooth-zero-input-REG-2026-11-27.md)
