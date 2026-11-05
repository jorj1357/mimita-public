# Open Gameplay Issues

## Purpose and audience

This document is the short working index for current gameplay reports, verification work, and ideas that should not be lost.

**Audience:** the player and contributors triaging gameplay and product work.

**Does not:** replace feature-owner specifications, prove a fix, or turn an exploratory idea into a committed implementation requirement. The linked owner documents define behavior; runtime evidence and human review remain separate.

## Status and evidence

- **Active:** a current report or agreed work remains open.
- **Needs recheck:** the user reports that something may be fixed, but it has not been confirmed here.
- **Resolved (user report):** the user says the issue is fixed or no longer wanted; this is not independent runtime evidence.
- **Deferred:** retained for later prioritization.
- **Idea:** a dated design direction, not a delivery commitment.

Priorities: **P0** is the current top project goal; **P1** is high-impact active work; **P2** is follow-up or recheck; **P3** is deferred. Build, runtime, and human-acceptance evidence are distinct.

## Active

| ID | Priority | Issue | Owner / next step |
|---|---:|---|---|
| OGI-001 | P0 | Complete Zombie Tower as a playable mode | [Zombie Tower](gamemodes/zombie-tower/zombie-tower.md); track full-run acceptance there. Floor-count choice remains open in that spec. |
| OGI-002 | P0 | NPC navigation still fails on multi-floor and complex routes | [NPC movement](../architecture/player-npc-systems/npc-movement.md); reproduce Hospital pursuit and validate traversal in Zombie Tower. |
| OGI-004 | P1 | Physical impacts can snag instead of producing the desired bounce | [Movement](movement/movement.md), [collision architecture](../architecture/collision/collision.md); retain the reported Dust2 Cyberia snag and replay evidence. |
| OGI-007 | P1 | Map Editor crash and unreliable bomb-site dragging | [Entity Editor](entity-editor/entity-editor.md); reproduce crash and drag failure separately. The user says the editor remains a separate issue. |
| OGI-009 | P1 | Interaction range and prompt behavior are inconsistent; bomb defuse feels too hard to reach | [Counter-Strike](gamemodes/counterstrike.md), [Fun Counter Strike](gamemodes/funcounterstrike.md); consolidate interaction eligibility/range and one reusable popup, then validate plant, defuse, pickup, and interruption. Exact revised range is not decided. |
| OGI-010 | P1 | Scenario recording and replay are not reliably usable | [Scenario recording](dev-loop/scenario-input-recording.md); investigate incomplete/broken recordings, missing metadata, replay checkpoints, and reported performance impact. |
| OGI-011 | P1 | JSON field inventory and stale cross-file references remain incomplete | [JSON configuration](../architecture/json-configuration/json-configuration.md); align runtime loading and validation across configuration families. |
| OGI-012 | P1 | Automatic validation runner should finish from expected scenario events | [Quality gates](dev-loop/quality-gates.md); status retained as previously recorded per user request. |
| OGI-022 | P1 | Freeze/slowdown is followed by reconnect, disconnect, or a failed return to the same server | [Networking](networking/networking.md), [performance](performance/performance.md); capture the exact server/client state and first divergence in a fresh report. User says leaving and rejoining can recover, but the slowdown/freeze still occurs. |
| OGI-023 | P1 | Rejoining can leave team identity and friendly/enemy outlines wrong | [Counter-Strike](gamemodes/counterstrike.md), [gamemodes](gamemodes/gamemodes.md); apply the server-assigned mode team before client outline/hostility presentation. Whether team choice persists across disconnect remains unspecified. |
| OGI-024 | P2 | Server creation should be easier to find and work for local play without coordinator access | [Networking](networking/networking.md); keep local hosting usable when offline and clearly state that the room is local-only and not discoverable to remote players. |

## Needs recheck

| ID | Priority | Issue | Current user update / next step |
|---|---:|---|---|
| OGI-003 | P2 | Actors may sink, pin, or enter closed geometry under pressure | User thinks NPC collision may be fixed. Recheck the reported `atdm`, `tdmtest3`, and `dust2cyberiav4` cases; include player ground failure. |
| OGI-005 | P2 | NPC mesh may disappear while the NPC remains active | User is unsure whether this still occurs and says NPCs seem fine. Confirm a current Fun Counter Strike run before closing. |
| OGI-014 | P2 | NPCs may stop attacking, fail to acquire targets, or ignore live profile edits | User thinks NPC combat may be fine. Recheck in a confirmed TDM run. |
| OGI-015 | P2 | Authored spawn points may not be used by every player/NPC spawn path | User thinks NPC spawn behavior may be fixed. Verify initial join, respawn, NPC spawn, team filtering, and remote edits. |
| OGI-025 | P2 | Fire and Deathball hit damage may not use normal damage presentation | An older note says this may be missing; the user was unsure whether Deathball showed damage. Recheck current impact feedback before deciding whether this remains a defect. [Weapons](weapons/weapons.md), [visuals](visuals/visuals.md). |
| OGI-026 | P2 | Player avatar identity/appearance and saved avatar state may not agree across clients | The 2026-11-04 note says avatars are not replicating/saving reliably. Confirm whether the same selected actor/avatar is visible to self and remote clients and whether saved selection returns after rejoin. [Networking](networking/networking.md), [avatar editor](avatar/avatar-editor.md), [data saving](data-saving/data-saving.md). |
| OGI-027 | P2 | Some female-presenting avatars may not use the expected animations | The 2026-11-03 note says these avatars do not animate like the Jason avatar. Recheck current supported avatar sets against the shared animation contract. [Animations](animations/animations.md). |

## Resolved or no longer wanted

These statuses reflect the user's 2026-11-05 update. They do not claim independent runtime verification.

| ID | Status | Issue | Evidence / owner |
|---|---|---|---|
| OGI-006 | Resolved (user report) | Unexpected Normal Zombie at the player's Fun Counter Strike spawn | User says Fun Counter Strike spawns are fine. [Fun Counter Strike](gamemodes/funcounterstrike.md) |
| OGI-008 | Resolved (user report) | Bomb-site visibility when Entity Visibility is enabled | User says bomb-site visibility is fine. [Entity Editor](entity-editor/entity-editor.md) |
| OGI-013 | Resolved (user report) | Split the dev-loop entry point into focused modules | User says this is done; `devscripts/dev_loop/` exists. [Quality gates](dev-loop/quality-gates.md) |
| OGI-016 | Resolved (user report) | VS Jason preset correction and visual/feel check | User says this is done; no new human review claim is made here. [VS Jason changelog](../changelog/2026-11-01/20261101_155817-vs-jason-role-preset.md) |
| OGI-020 | No longer wanted | Make Fun Counter Strike NPCs easier | User says to remove this request. [Fun Counter Strike](gamemodes/funcounterstrike.md) |
| OGI-021 | Resolved (user report) | Weapon-set override concerns | User says the behavior is fine and to leave it alone. [Gamemodes](gamemodes/gamemodes.md) |

## Deferred

| ID | Priority | Idea | Owner |
|---|---:|---|---|
| OGI-017 | P3 | General melee improvements | [Melee weapons](weapons/melee/melee-weapons.md), [weapons](weapons/weapons.md) |
| OGI-018 | P3 | Focused behavior specifications for Godball and HAFS | [Weapons](weapons/weapons.md) |
| OGI-019 | P3 | Easier ways to equip weapons beyond slot 10 | [Weapons](weapons/weapons.md) |

## Dated idea index

These summaries preserve the product meaning and capture dates of the informal notes. They are ideas unless a linked owner specification says otherwise; personal storytelling and transcription noise are omitted.

| Topic and dates | Preserved direction | Owner documents |
|---|---|---|
| Avatars and replication — 2026-11-03 to 2026-11-04 | Show the selected actor consistently to self and other clients; make avatar replication/saving dependable; give all applicable avatars, including female-presenting avatars, their intended animations. Independent client accounts would help test distinct saved data. | [Networking](networking/networking.md), [animations](animations/animations.md), [avatar editor](avatar/avatar-editor.md), [data saving](data-saving/data-saving.md) |
| Controls, interactions, and inventory — 2026-11-03 to 2026-11-04 | Separate gameplay, chat, editor, and other input contexts; support cross-platform remapping and a shared action surface usable from the game and developer tools. Use one shared interaction path and consistent popup. An `inventory_open` action should open an editable inventory GUI; selecting an owned item should use the normal equip path. Python-based GUI presentation was floated as an option, but no implementation language was selected. | [Counter-Strike](gamemodes/counterstrike.md), [gamemodes](gamemodes/gamemodes.md), [GUI](gui/guiv2.md), [accessibility](../architecture/accessibility/accessibility.md), [weapons](weapons/weapons.md), [JSON configuration](../architecture/json-configuration/json-configuration.md) |
| Editors and creation tools — 2026-11-01 to 2026-11-04 | Make map/entity authoring easier inside the game, including live rotation tuning and a Blender-to-game workflow without repeated direct JSON edits; connect map, video, and future sound creation workflows. | [Entity Editor](entity-editor/entity-editor.md), [Video Editor](creator-tools/video-editor.md), [Music](music/music.md), [project vision](../architecture/vision-mentality/vision.md) |
| Performance and diagnostics — 2026-11-01 to 2026-11-05 | Keep authoritative simulation at 60 Hz and rendering responsive; expose server Hz in F3 as the concrete overlay request. Older notes cite a 240 FPS target, but the archive below records that number as not a current goal. Use detailed, event-linked scenario evidence to find the first bad tick while preserving runtime performance, including earlier concerns about low server tick rate under high NPC counts. Explore camera-near hard/soft culling as an optional presentation technique, without choosing distances yet. The notes also ask for shorter edit-to-feedback cycles and a later development/stable executable distinction, without weakening required validation. | [Performance](performance/performance.md), [Networking](networking/networking.md), [debug logging](debug-logging/debug-logging.md), [scenario recording](dev-loop/scenario-input-recording.md) |
| Weapons and effects — 2026-11-03 to 2026-11-04 | Explore more visible, distinct grenade presentation and stronger bounce/throw distance; continue darkbang experiments; use a large initial blood mark with smaller outward marks and more visible body gore. Exact tuning and presentation remain open. | [Weapons](weapons/weapons.md), [effects](effects/effects.md), [blood and viscera](visuals/blood-bulletholes-viscera.md) |
| Persistence, networking, and sharing — 2026-11-04 to 2026-11-05 | Explore durable RPG progress across sessions/devices and storage that can serve player-created games; the local-versus-centralized/distributed model remains undecided. Make local hosting understandable and support optional sharing. | [Data saving](data-saving/data-saving.md), [Networking](networking/networking.md), [project vision](../architecture/vision-mentality/vision.md) |
| Future modes and worlds — 2026-11-03 to 2026-11-04 | Explore Retrograd/Juggernaut play, infinite generated and non-Euclidean worlds, VR, and social play across devices. One Juggernaut idea is that NPC opponents rush the Juggernaut and make that target easy to recognize. The same design notes question how to make the Juggernaut visually distinct and readable. [Juggernaut](gamemodes/juggernaut.md). Prioritize reusable features over adding content for its own sake. These are not current delivery commitments. | [Gamemodes](gamemodes/gamemodes.md), [RPG](gamemodes/rpg.md), [procedural worlds](procedural-infinite-world/procedural-infinite-world.md), [project vision](../architecture/vision-mentality/vision.md) |

An incomplete 2026-11-03 fragment says Rocket Launcher explosion decals should not “...”; the rest of the request is missing, so no behavior is inferred. Clarify before adding it to the weapon/effects contract.

The original notes also mentioned live avatar/role policy, NPC behavior, grenades, blood, and performance repeatedly. Their current requirements remain in the owner specs above; this index replaces the repeated raw transcript rather than duplicating those contracts.

## Archive — not current work

“Archived” means not currently tracked as active work; it does not assert that
the behavior was fixed. Reopen an item with a current report or evidence.

| Archived item | Decision / evidence | Owner or evidence |
|---|---|---|
| Destructible world in TDM | Disabled by design because inconsistent floors/slopes were not fun; revisit for a mode built around destruction | [Decision changelog](../changelog/2026-10-31/20261031_152055-destructible-world-disabled-default.md), [destructible-world spec](destructible-world/destructible-world.md) |
| 240 FPS for 60 seconds and older frame-pacing reports | The target was not proved, but the user says play felt fine; the number is not a current goal | [Performance spec](performance/performance.md) |
| Older Juggernaut lag, startup, choppiness, and F3-pause reports | Archive unless they recur; the new 2026-11-05 freeze/reconnect report is tracked separately as OGI-022 | [Performance spec](performance/performance.md), [dev loop](dev-loop/dev-loop.md), [networking spec](networking/networking.md) |
| Camera settings overwritten in TDM | Archive unless it happens again | [JSON configuration](../architecture/json-configuration/json-configuration.md), [TDM spec](gamemodes/teamdeathmatch.md) |
| Large client/server position disagreement and ordinary movement rubber-banding | Archive unless it happens again; the cited large correction followed an explicit teleport into blocking geometry | [Movement spec](movement/movement.md), [networking spec](networking/networking.md) |
| Shot/cooldown reliability, shotgun pellet direction, Spy Knife health mismatch, and PvP report | Archive unless any symptom happens again | [Weapons](weapons/weapons.md), [melee](weapons/melee/melee-weapons.md) |
| Bullet-hole marks and broad enemy/damage presentation feedback | User says archive both for now | [Visuals](visuals/visuals.md), [effects](effects/effects.md) |
| Older Duel/Juggernaut avatar appearance, aim feel, head movement, full-screen shadow, and small visual-presentation reports | Archived as older visual/feel reports; reopen with a current example | [Visuals](visuals/visuals.md), [graphics presentation](graphics/graphics-presentation.md), [fullscreen/shadow evidence](../gold/20261024_191700-fullscreen-shadow-small-fix.md) |
| Ragdoll limb tunneling and TDM NPC-versus-NPC death ragdoll | Archive unless it happens again | [Ragdoll](ragdoll-retrograd/ragdoll-retrograd.md), [NPC behavior](npc-behavior/npc-behavior.md) |
| Repeated mode-start/reconnect reliability | Archive unless it happens again; prior fixes have runtime evidence but the full matrix was incomplete | [Networking reliability changelog](../changelog/2026-10-28/20261028_125325-networking-reliability.md) |
| Broad Payload/Zombie Tower map-authoring blocker | Archive with the positive report that the Map Editor made Payload easier; current crash/drag defects stay active above | [Editor feedback](../gold/20261027_020542-map-editor-payload-positive-feedback.md), [Entity Editor](entity-editor/entity-editor.md) |
| Dust 2 ramp “freeze” recording | The recording included freeze input and did not establish a grounded collision stall; the broader current snag report remains active above | [Collision evidence intake](../changelog/2026-10-31/20261031_140500-collision-stepup-and-scenario-evidence.md) |
| Old “presentation first” priority | Superseded by the 2026-11-01 decision that Zombie Tower is the top project priority | [Graphics presentation](graphics/graphics-presentation.md) |

## Reopen rule

Reopen an archived report when a new user observation, scenario, or journal
shows the behavior again. Link the new evidence and compare it with the owner
spec before deciding whether the requirement is missing, the implementation
violates it, or the original report was not reproduced.
