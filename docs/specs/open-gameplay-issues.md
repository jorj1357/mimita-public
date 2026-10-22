2026 10 21 0355 jorj  updateed this to just be  open gamepla issues bc its sick to have just 1 doc to  spawm all the ideas
C:\mimita-v9\docs\jorj-docs\misc-bugs.md
it was this but no not this dont use this  
use the open gamepla issues doc  that we are in right now 

2026 10 17 1211 jorj todo add: the  blood and world imapct effects dont show if the npc hittin gme
needs to be automatic all the itme
and no like
square issues or
idk what the problem is 
its like 
the images ![alt text](image-5.png)
the pngs are sqaure cut off nto good 

2026 10 17 1317  npcs only the blood only spawns when i kill a npc
not good
need to be like 
npc shoots me = blood happens every time 
not just if i kill npc 
also the aim in  arm , tweaking and jtitering around not good, needs to just be  astalbe aiming in arm, cant be moving around
also, weapon is auto equipped when spawend in, liek starting gaem for first time, not good, need to be able to  jsut have the weapon eb weeuipped and have weapon ollisions actually happen bc right now its like i just spawn adn the weapon is equpped but then it doenst even have the pose or collisiosn enabled 

ALSO TODO EVERY MAP NEEDS TO AHVE SPAWNPOINTS IN THE JSON AND I CAN SET WHATS ACTIVE/INACTIVE
LIKE EACH SPAWNPOINT SO U DOTN HAVE TO  MKAE NEW SPAWNPOITNS IN BELNDER NEED DAT 
USE SPAWNPOITNS NOT THE ONES MADE IN BLENDER OK ok im done 

2026 10 17 1331 collisiosn or wepaons are not goodo liek i jsut got stuckinsidne a car in the map trainkinda, weaponcollisiosn overalll like even on maps like hospital5 i was getting stcuk in things, also  the juggernaut4 map? the smaller version i would get stuck in walls as juggernaut i wouldl get stuck ovtne, 
also not sure when this happened but  heads for humanoid avatars are rolling much too far back, it looks wierd , heads need to just be consistnetnly like leaning in teh direction ur moving  

2026 10 17 1335 NO EULER ROTATIONS FOR ANIMATIONS, WE NEED A SMOOTH EULER TO QUATERNION CONVERSION BC ITS LIKE XYZ TO WXYZ AND WE NEED LIEKA  NROMALIZATION FRMO BELNDER AND STUFF SO WE DONT GET CONFUSED DOING ANIMATIONS AND WORLD POSITIOSN ETC

# Open gameplay issue inbox — started 2026-10-16

This living inbox collects gameplay observations from any task or play session
and routes them to the relevant behavior owner. Reports have not all been
reproduced. An entry is not proof of root cause or a claim that a fix is
implemented. Keep settled requirements and investigation evidence in the owner
document, then update this inbox when an issue is reproduced or resolved.

This document does not replace the linked specifications, confirm a bug, or
authorize implementation choices that are still marked open.

## First priority — decide what gameplay presentation should do

The player wants presentation to be the first design conversation, starting
with Duel. The open question is how the game should make important information
and feedback understandable at a glance, especially during fast fights and for
spectators. This is a discussion starter, not a decided HUD design.

**Possible Duel starting point:** keep the two competitors visually balanced
on screen, with each side showing team color, actor name/avatar, kills out of
20, and progress toward the win. Put the current phase and remaining time
somewhere easy to find; make the winner or stalemate unmistakable during
RESULTS. Decide what stays visible during active play versus what can appear
on a scoreboard or spectator view. Check the existing graphics-presentation,
GUI, and Duel owners before turning this sketch into a requirement.

Questions to settle in that discussion: What must a player understand in one
glance while aiming? What extra context does a spectator need? Which combat
effects build confidence that a hit counted, and which only add clutter? What
visual hierarchy makes the current objective obvious without covering the
fight?

## New intake — 2026-10-21

These are player reports from a play session. They are recorded as observed or
requested behavior, not as confirmed causes. The six attached screenshots
(`codex-clipboard-63358e38-80a0-4b99-aa7e-97e1d38bbb0c.png`,
`codex-clipboard-b16badb8-8ceb-493a-978f-d3f90dc1c125.png`,
`codex-clipboard-9d0cc63b-bd0d-47fb-8adb-63f1f399f40b.png`,
`codex-clipboard-5204e8e3-66ee-4680-bcac-3f8335af5b04.png`,
`codex-clipboard-1d93ad7b-2665-4f18-8c7b-420e912524e6.png`, and
`codex-clipboard-cec704f2-58e0-454c-9d53-8cb2415c12e9.png`) show blood and
world-impact marks appearing as clipped, blocky bars, stretched shapes, or
shards with heavy overlap. The original shape defects were traced to randomized blood-sprite stretching and crack chains that mapped the full PNG onto every narrow segment. The fix is implemented and built as 2741: blood spray uses equal axes, and each world crack draws the complete square PNG once with uniform size and in-plane rotation. The render summary now reports visible blood and crack aspect ratios. The PNG alpha path remains enabled. This does not identify every large red/transparent shape visible in the screenshots; non-textured hit effects and tracers also have separate render paths. The active Duel session remains on build 2733, so gameplay visual acceptance is pending.

| Area | Player report / desired result | Status and open detail | Owner / route |
|---|---|---|---|
| **Presentation — first priority** | Make the game's presentation clearer overall. For Duel, spectators should see both sides, team and actor names, each side's kills, kills remaining to win, and who is ahead. | Design conversation requested. The possible layout above is only a starting sketch; active-player and spectator information priorities still need agreement. | [`graphics/graphics-presentation.md`](graphics/graphics-presentation.md), [`gui/guiv2.md`](gui/guiv2.md), [`gamemodes/duel.md`](gamemodes/duel.md) |
| **Blocker — Zombie Tower map authoring and spawn-volume editing** | The player believes the tower may not be long enough and reports that it needs many specifically placed zombie spawn volumes. Adding them by repeating `entity_add` commands and editing JSON to change them is too cumbersome. They want a better map editor where spawn volumes can be selected, copied/pasted, and moved by clicking and dragging. | Human-reported blocker to continuing Zombie Tower map work; tower length has not been measured or confirmed as the cause. Current Entity Editor documents command-based creation and JSON save/edit flows, including `monster_zone`; it does not document drag handles or copy/paste. Investigate the actual map length and authoring workflow separately. Define editing behavior for selecting, dragging, duplicating, assigning unique IDs, and saving volumes before implementing the editor improvement. | [`entity-editor/entity-editor.md`](entity-editor/entity-editor.md), [`gamemodes/zombie-tower/zombie-tower.md`](gamemodes/zombie-tower/zombie-tower.md) |
| Live Blender-to-game map editing | The player wants to edit a map in Blender and see the game update without repeatedly exporting/loading by hand. A possible first step is a Python watcher that observes the selected map's Blender source or exported `.glb` and asks the game to reload the changed map; the longer-term goal is doing more of the map editing inside the game. | Early idea; no file-watching, Blender export, reload protocol, safety checks, or editor ownership has been selected or verified. Write a focused workflow/architecture spec before implementation: define whether the watched source is `.blend`, exported `.glb`, or both; how Blender produces a valid new asset; how the game detects a complete save, validates and safely reloads it, and reports errors; and how the in-game editor grows toward the same workflow. Keep this connected to the Zombie Tower spawn-volume authoring blocker, but treat it as a reusable map-authoring workflow rather than a Zombie Tower-only feature. | [`entity-editor/entity-editor.md`](entity-editor/entity-editor.md), [`../features/live-code-development/live-code-development.md`](../features/live-code-development/live-code-development.md), [`../operations/asset-management/asset-management.md`](../operations/asset-management/asset-management.md) |
| NPC combat | NPCs should use both shotgun and revolver, change weapons much faster, and begin shooting promptly after spawning. NPCs should normally deal more damage, move more visibly, and have fewer barriers to reaching or hitting an opponent. NPCs should be able to use melee. | User-reported; not reproduced. Exact damage, switching cadence, movement style, and allowed obstacles need owner-level decisions and evidence. | [`npc-behavior/npc-behavior.md`](npc-behavior/npc-behavior.md), [`../architecture/player-npc-systems/npc-movement.md`](../architecture/player-npc-systems/npc-movement.md), [`weapons/melee-weapons.md`](weapons/melee-weapons.md) |
| Shot and cooldown reliability | Some shots seem to be rejected or lost because of a cooldown; firing does not always count. | User-reported; not reproduced. Trace input, cooldown, and authoritative shot result before naming the failing step. | [`weapons/weapons.md`](weapons/weapons.md), [`networking/networking.md`](networking/networking.md) |
| Blood and world-impact marks | Effects should appear consistently when any actor hits another actor, including NPC-to-player hits. Blood, bullet holes, and world cracks should preserve the complete authored PNG shape and alpha, with only uniform size changes and in-plane rotation. | Shape fix implemented and built as 2741: blood spray has equal billboard axes, and a world crack renders once as the complete square PNG (configured diameter 2 world units), with uniform sizing and in-plane rotation. The render summary records visible blood/crack aspect ratios. Gameplay visual acceptance remains pending because the active Duel session is still build 2733. Large red/transparent geometry in the screenshots is a separate unidentified artifact. | [`visuals/blood-bulletholes-viscera.md`](visuals/blood-bulletholes-viscera.md), [`weapons/weapons.md`](weapons/weapons.md); code owners: `src/effects/hit-effects.cpp::HitEffects::onHit()`, `src/effects/effect-part-blood.cpp::EffectPartSystem::spawnBloodEffect()`, `src/ragdoll/ragdoll-mode.cpp::RagdollModeSystem::sprayCorpseBlood()`, `src/effects/effect-part.cpp::EffectPartSystem::spawnWorldCracks()`, `src/effects/effect-part-render.cpp` crack decal batching |
| Shotgun pellet direction | Each pellet from an NPC shotgun shot should travel along its own shot direction; the player reports pellets appearing aligned to world axes. | User-reported; not reproduced. | [`weapons/weapons.md`](weapons/weapons.md), [`networking/networking.md`](networking/networking.md) |
| Performance / frame pacing | Play felt somewhat choppy; turning OBS off seemed to help, but the player is unsure whether the difference was real. | Unconfirmed impression; no measured frame-time or controlled comparison yet. Do not treat OBS as the cause without measurement. | [`performance/performance.md`](performance/performance.md) |
| Rocket Launcher presentation | The rocket needs a more complete explosion animation and presentation. | Desired improvement; exact animation/effect and timing are open. | [`weapons/weapons.md`](weapons/weapons.md), [`visuals/visuals.md`](visuals/visuals.md) |
| World-hit sound | World-hit sounds should scale with impact force and choose from multiple sounds instead of always using one. The player pointed to `assets/sound/hitworld2.mp3` and said ricochet already has a related precedent. | User-reported desired behavior. Current ricochet behavior and how force should affect loudness, pitch, or sample choice were not verified or specified. | [`../architecture/live-development/hot-audio-contract.md`](../architecture/live-development/hot-audio-contract.md), [`weapons/weapons.md`](weapons/weapons.md), `config/audio-recipes.json` |
| Sound asset organization | Make sound files easier to find using meaningful folders and short event names, for example `assets/sound/entity/general/death1.wav`, instead of loose, long descriptive filenames. | Organization proposal; confirm naming, variant numbering, and how recipes refer to files before migrating assets. | [`../operations/asset-management/asset-management.md`](../operations/asset-management/asset-management.md), [`../architecture/live-development/hot-audio-contract.md`](../architecture/live-development/hot-audio-contract.md) |
| Replay export | Replay export reportedly broke after the fullscreen modes were added. | User-reported regression; not reproduced here. | [`replay/replay.md`](replay/replay.md) |
| Spy Knife | The equipped Spy Knife model is not visible. Hits can appear client-side without changing the target's authoritative health. | User-reported; not reproduced. Trace model presentation and authoritative damage as separate paths. | [`weapons/melee-weapons.md`](weapons/melee-weapons.md), [`networking/networking.md`](networking/networking.md) |
| Duel avatars | Duel needs an avatar limit. | Need a product decision describing which avatars are allowed or how the limit is chosen. | [`gamemodes/duel.md`](gamemodes/duel.md), [`avatar/avatar-editor.md`](avatar/avatar-editor.md) |
| Legacy Duel cleanup | Remove old offline/local Duel gameplay paths. Duel and other gameplay modes must be client/server; there is no supported or maintained offline gameplay implementation. Keep the current networked mode path and remove obsolete local Duel code after tracing its callers. | Requested for future cleanup; exact legacy owners and safe deletion boundary need a repository-wide call-site audit. | [`gamemodes/duel.md`](gamemodes/duel.md), [`gamemodes/gamemodes.md`](gamemodes/gamemodes.md), [`networking/networking.md`](networking/networking.md) |
| Juggernaut avatars and outlines | Limit both Juggernaut roles to the JSON allowlist of normal boy/girl body avatars; exclude oversized and monster avatars. Friendly outlines are off. Enemy outlines are faint red. | Requirements now recorded in the mode and role JSON; runtime behavior still needs live Juggernaut verification. | [`gamemodes/juggernaut.md`](gamemodes/juggernaut.md), `config/gamemodes/juggernaut.json`, `config/actor-presets/juggernaut_fighter.json`, `config/actor-presets/juggernaut_arcade.json` |
| Invisible avatars | Avatars sometimes become invisible again; the exact affected actor and session are unclear. Trace identity selection, asset/model loading, replication, entity binding, and render submission separately. | **Source/journal trace (2026-10-21), likely network identity failure found; visual cause not confirmed:** live Duel run `20261021_084608` recorded 62 avatar-load queue rejections (`worker_queue_full`, queue depth 3 / limit 2), all marked retryable; 58 NPC rebinds later reported an avatar instance and loaded model. More importantly, remote avatar cache folders use `remote_` plus 24 hash characters (31 characters total), but snapshot avatar fields are 24 bytes including the terminator (23 usable characters). The server copies names into that field with `strncpy(..., sizeof(field)-1)`, so a 31-character remote ID is truncated on the wire. The journal contains a failed lookup for `remote_568d80ddb4e934de` (23 characters) because its `avatar.json` was missing or invalid, matching that truncation shape. Failed cache entries also count as `isAvatarLoadPending`, preventing the render-side retry path from reapplying the failed identity. This is a strong mechanism for a remote avatar to remain on fallback presentation, but the journal does not identify it as the avatar the player saw disappear, and normal live-actor render submission is not recorded. Next reproduction should capture mode/map, actor kind/name/avatar ID and exact first failing event. | [`avatar/avatar-editor.md`](avatar/avatar-editor.md), [`gamemodes/gamemodes.md`](gamemodes/gamemodes.md), [`networking/networking.md`](networking/networking.md), `src/avatar/avatar-atlas.cpp`, `src/network/avatar-network-client.cpp`, `src/network/server-players.cpp`, `src/network/multiplayer-tick.cpp`, `src/render/render-player.cpp`; journal: `logs/2026-10-21/20261021_084608/events.jsonl` |
| Aim feel and shot confidence | Crosshair aim in `gameplay.json` feels more reliable but less fun. Physical gun aiming feels more immersive, but the player is unsure shots line up with what they saw. | Open feel report; no aim-mode change requested or decided. Compare visible aim, shot direction, and hit result before choosing. | [`weapons/weapons.md`](weapons/weapons.md), [`gui/gui-general.md`](gui/gui-general.md) |
| Avatar head movement | Avatar heads seem to lean too far backward while moving forward. The player remembers them behaving more normally before ragdoll edits, possibly about a week earlier, but is unsure when the change began or what the intended motion was. | User-reported and uncertain; no cause or prior behavior established. | [`ragdoll-retrograd/ragdoll-retrograd.md`](ragdoll-retrograd/ragdoll-retrograd.md), [`avatar/avatar-editor.md`](avatar/avatar-editor.md) |
| Player jitter and NPC geometry recovery | The player reports jitter when far from map geometry and NPCs remaining stuck inside geometry after recent player-world collision and networking improvements; NPCs should be pushed back out into clear space. | User-reported; not reproduced. The link between the symptoms and collision, movement correction, or networking is unconfirmed. Trace the first embedded actor position/contact and later position writers before choosing a recovery path. | [`../architecture/collision/collision.md`](../architecture/collision/collision.md), [`movement/movement.md`](movement/movement.md), [`networking/networking.md`](networking/networking.md), [`../architecture/player-npc-systems/npc-movement.md`](../architecture/player-npc-systems/npc-movement.md) |

## New intake — 2026-10-22

This entry records a configuration-usability report. It does not decide that
roles and actor presets should be merged or that their current ownership should
change.

| Area | Player report / desired result | Status and open detail | Owner / route |
|---|---|---|---|
| Role and actor-preset setup | The player has trouble understanding `config/roles.json` and actor presets and wants this setup to be simpler. | User-reported clarity issue. Add a plain-language guide showing what each file controls and the exact Sandbox NPC example. Whether the file structure itself should change remains open. | [`actor-presets/actor-presets.md`](actor-presets/actor-presets.md), [`gamemodes/gamemodes.md`](gamemodes/gamemodes.md), [`npc-behavior/npc-behavior.md`](npc-behavior/npc-behavior.md) |
| Enemy readability and hit confirmation | Make enemies easy to locate, especially the Juggernaut, and make it clear whether attacks actually damage them. The player is considering more realistic hit effects, blood, or in-world damage numbers, and wonders whether map or movement changes could help. | Open presentation/design issue. First make enemy location and confirmed damage understandable; decide which effects communicate a registered hit without obscuring play. Blood, damage numbers, other effects, and map or movement changes are options to evaluate, not selected solutions. The existing blood/world-impact issue separately tracks effect consistency and image shape. | [`graphics/graphics-presentation.md`](graphics/graphics-presentation.md), [`gamemodes/juggernaut.md`](gamemodes/juggernaut.md), [`weapons/weapons.md`](weapons/weapons.md), [`visuals/blood-bulletholes-viscera.md`](visuals/blood-bulletholes-viscera.md) |
| Parallel human creation alongside AI coding | While AI works on code, the player wants productive work such as writing specifications, creating Blender assets, making sounds, and other content. | Workflow/productivity request. Define how the player and AI can identify independent work, keep ownership and feedback clear, and bring the results together. The workflow, tools, and conflict handling remain open; this request does not assume simultaneous edits to the same files are safe. | [`../features/live-code-development/live-collaboration-feature.md`](../features/live-code-development/live-collaboration-feature.md), [`../operations/asset-management/asset-management.md`](../operations/asset-management/asset-management.md), [`animations/blender-animation-authoring.md`](animations/blender-animation-authoring.md), [`../architecture/live-development/hot-audio-contract.md`](../architecture/live-development/hot-audio-contract.md) |
| JSON configuration range and error feedback | The player wants JSON settings to accept very large values (example: `-9999999999999999999999999999` through `999999999999999999999999999999999`) and wants invalid or unsupported values to be obvious, including which file and setting caused the problem. | Open design issue. Decide safe per-setting numeric ranges and whether out-of-range values are accepted, clamped, or rejected while preserving the last valid configuration. Record a categorized error in `events.jsonl` with the config path, field, and reason, then surface a clear, deduplicated in-game tip through the existing tips presentation path. Exact wording, severity, and repeat/throttle behavior remain open. | [`../architecture/json-configuration/json-configuration.md`](../architecture/json-configuration/json-configuration.md), [`debug-logging/debug-logging.md`](debug-logging/debug-logging.md), [`gui/gui-general.md`](gui/gui-general.md) |
| Memory use grows sharply during reconnect attempts | The player reports RAM use rising by roughly 100 MB per second while reconnecting, then staying around 4,180 (exact value and units uncertain); the player says the figures are approximate and is concerned about the size of the increase. | User-reported, not reproduced; cause and exact memory units/measurements are unknown. Measure process memory across a bounded reconnect sequence, identify which owner retains or allocates the growing memory, and verify whether usage is released or remains elevated after reconnect succeeds or stops. Do not infer a leak or blame a component from the estimate alone. | [`networking/networking.md`](networking/networking.md), [`performance/performance.md`](performance/performance.md), [`../architecture/accessibility/accessibility.md`](../architecture/accessibility/accessibility.md) |
| Juggernaut physics time spikes while aiming for 240 FPS with all actors visible | Reported at 2026-10-22 13:49 (time as provided by the player): while working toward 240 FPS with all actors on screen, the reported physics-ms value jumps substantially. The player was not connected to a server at the time. | User-reported, not measured or reproduced. Exact actor count, frame/physics timings, metric definition, build, and local run mode are unknown. Profile the same non-server-connected Juggernaut scenario with actor count and frame/physics timings recorded, then trace the first expensive physics owner before proposing an optimization. The separate connected 68-actor measurement in the NPC behavior spec does not establish this report's cause or result. | [`performance/performance.md`](performance/performance.md), [`gamemodes/juggernaut.md`](gamemodes/juggernaut.md), [`npc-behavior/npc-behavior.md`](npc-behavior/npc-behavior.md), [`../architecture/collision/collision.md`](../architecture/collision/collision.md) |

## Issue index

| Issue | Current status | Owner / details |
|---|---|---|
| Role and actor-preset setup is hard to understand | User wants a simpler explanation of `roles.json`, actor presets, and the Sandbox NPC setup path; structural changes remain undecided | [`actor-presets/actor-presets.md`](actor-presets/actor-presets.md), [`gamemodes/gamemodes.md`](gamemodes/gamemodes.md), [`npc-behavior/npc-behavior.md`](npc-behavior/npc-behavior.md) |
| Zombie Tower map authoring is a reported blocker | The player believes the tower may be too short and says many specific zombie spawn volumes must be placed. Repeating commands and editing JSON makes this slow; desired editor improvements are click-and-drag movement and copy/paste for spawn volumes. Map length and the exact editor interaction remain to be verified/defined. | [`entity-editor/entity-editor.md`](entity-editor/entity-editor.md), [`gamemodes/zombie-tower/zombie-tower.md`](gamemodes/zombie-tower/zombie-tower.md) |
| Live Blender-to-game map editing workflow | User wants a selected map opened in Blender and changes reflected in-game, initially possibly through a Python watcher for the Blender source or exported `.glb`, with a longer-term goal of a complete in-game map editor | No watcher/export/reload architecture chosen. Define safe save detection, asset validation, reload/error behavior, and the relationship between Blender and in-game editing in a dedicated spec before implementation. | [`entity-editor/entity-editor.md`](entity-editor/entity-editor.md), [`../features/live-code-development/live-code-development.md`](../features/live-code-development/live-code-development.md), [`../operations/asset-management/asset-management.md`](../operations/asset-management/asset-management.md) |
| JSON settings reject or obscure very large or invalid values | User wants broad numeric support, a precise logged error, and a visible in-game tip naming the file and setting; safe per-setting limits and reject/clamp behavior are undecided | [`../architecture/json-configuration/json-configuration.md`](../architecture/json-configuration/json-configuration.md), [`debug-logging/debug-logging.md`](debug-logging/debug-logging.md), [`gui/gui-general.md`](gui/gui-general.md) |
| RAM use reportedly climbs rapidly during reconnect attempts and remains high | Approximate report: about 100 MB/s growth while reconnecting and around 4,180 afterward; exact amount and units are uncertain | Needs a bounded live reconnect measurement and memory-owner trace before identifying cause or remedy | [`networking/networking.md`](networking/networking.md), [`performance/performance.md`](performance/performance.md), [`../architecture/accessibility/accessibility.md`](../architecture/accessibility/accessibility.md) |
| Juggernaut physics time reportedly spikes while aiming for 240 FPS with all actors visible | Player-reported at 2026-10-22 13:49 while not connected to a server; exact frame/physics timings and actor count are unknown | Reproduce and profile this local run separately from the connected 68-actor measurement already recorded in the NPC behavior specification | [`performance/performance.md`](performance/performance.md), [`gamemodes/juggernaut.md`](gamemodes/juggernaut.md), [`npc-behavior/npc-behavior.md`](npc-behavior/npc-behavior.md), [`../architecture/collision/collision.md`](../architecture/collision/collision.md) |
| Enemies are hard to locate and confirmed damage is unclear, especially against the Juggernaut | User wants clearer enemy presentation and visible proof when damage registers; blood, in-world numbers, other effects, and map or movement changes are options for discussion | Open design issue; the feedback approach and whether map or movement changes are needed remain undecided. Existing blood/world-impact consistency and shape tracking stays a separate issue. | [`graphics/graphics-presentation.md`](graphics/graphics-presentation.md), [`gamemodes/juggernaut.md`](gamemodes/juggernaut.md), [`weapons/weapons.md`](weapons/weapons.md), [`visuals/blood-bulletholes-viscera.md`](visuals/blood-bulletholes-viscera.md) |
| Player wants to create specs, Blender assets, sounds, and other content while AI codes | Workflow request for useful parallel human/AI work with clear task ownership and a way to integrate results | Workflow, tools, and conflict handling remain open; no implementation or collaboration mechanism is claimed here. | [`../features/live-code-development/live-collaboration-feature.md`](../features/live-code-development/live-collaboration-feature.md), [`../operations/asset-management/asset-management.md`](../operations/asset-management/asset-management.md), [`animations/blender-animation-authoring.md`](animations/blender-animation-authoring.md), [`../architecture/live-development/hot-audio-contract.md`](../architecture/live-development/hot-audio-contract.md) |
| Player jitters far from map geometry and NPCs remain embedded in geometry | User reports both symptoms; cause and whether they share an owner are unconfirmed. NPCs should be moved back out to clear space. | [`../architecture/collision/collision.md`](../architecture/collision/collision.md), [`movement/movement.md`](movement/movement.md), [`networking/networking.md`](networking/networking.md), [`../architecture/player-npc-systems/npc-movement.md`](../architecture/player-npc-systems/npc-movement.md) |
| Griever behavior still does not fully match intent: server position errors, hovering, and a report that it rolls but does not bounce enough | Prior reports and an attempt note exist; current root cause and latest runtime result are not established in this document | [`griever-monster/griever-monster.md`](griever-monster/griever-monster.md), [`movement/movement.md`](movement/movement.md), [`../architecture/collision/collision.md`](../architecture/collision/collision.md) |
| Custom animation authoring for monsters and attacks | Runtime animation system is specified; Blender export remains a proposal with open format/authoring decisions | [`animations/animations.md`](animations/animations.md), [`animations/blender-animation-authoring.md`](animations/blender-animation-authoring.md), [`gamemodes/zombie-tower.md`](gamemodes/zombie-tower.md) |
| Spy Knife shows predicted hit effects but NPC health does not change; PvP is reported absent/nonfunctional | NPC mismatch already documented; PvP report not reproduced | [`weapons/melee-weapons.md`](weapons/melee-weapons.md), [`networking/networking.md`](networking/networking.md) |
| Godball and HAFS need further investigation | Existing Godball and HAFS definitions/specs found; exact failing behavior unspecified | [`weapons/weapons.md`](weapons/weapons.md), `config/weapons.json` (`godball`, `hafs`) |
| SwordSword needs its own model, attacks, animation, damage, and collision coverage | Informal notes now linked to explicit open issues; no runtime proof here | [`weapons/swordsword.md`](weapons/swordsword.md), [`animations/blender-animation-authoring.md`](animations/blender-animation-authoring.md) |
| Ragdoll/limb collision is inconsistent; long-arm grabbing should not tunnel | Existing `RAG-011` already forbids free tunneling; reported runtime gap needs reproduction | [`ragdoll-retrograd/ragdoll-retrograd.md`](ragdoll-retrograd/ragdoll-retrograd.md), [`../architecture/collision/collision.md`](../architecture/collision/collision.md) |
| Weapon collision can snag or launch the Juggernaut out of Juggernaut4 | User-reported; collision shape/bevel proposal is undecided | [`moving-physical-objects/moving-physical-objects.md`](moving-physical-objects/moving-physical-objects.md), [`../architecture/collision/collision.md`](../architecture/collision/collision.md) |
| Entity Editor spawn points should work immediately in Sandbox for players and NPCs | Authored points already serve some gamemode respawn/map-change paths; initial join and `npc_spawn` use separate GLB/local-world paths | [`entity-editor/entity-editor.md`](entity-editor/entity-editor.md), [`gamemodes/gamemodes.md`](gamemodes/gamemodes.md) |
| Hospital NPC pursuit can stall across floors | User-reported; multi-floor navigation is already a stated capability | [`../architecture/player-npc-systems/npc-movement.md`](../architecture/player-npc-systems/npc-movement.md) |
| Client/server player position can disagree by about 25 m and rubber-band toward stale state | User-reported; cause unconfirmed; do not blindly trust client transforms | [`movement/movement.md`](movement/movement.md), [`networking/networking.md`](networking/networking.md) |
| Melee-inspired 3D arena mode and Blender-authored attacks | Mode proposal clarified; core gameplay choices remain open | [`gamemodes/supersmashbrosmelee.md`](gamemodes/supersmashbrosmelee.md), [`animations/blender-animation-authoring.md`](animations/blender-animation-authoring.md) |

## Triage for the 2026-10-16 play report

Priority is based on whether the issue blocks joining or causes authoritative
gameplay rejection, then on how broadly it affects play, and finally on
whether the behavior is sufficiently specified to implement. “Do now” means
investigate and reproduce first; it is not permission to bypass the required
runtime evidence or make a guessed code change.

### Do now: reproduce and trace blockers

| Rank | Report / evidence | Why this priority | Owner |
|---|---|---|---|
| 1 | Below-world spawn and map disagreement. The selected journal contains conflicting server/client map loads, repeated missing-map load failures, and a local actor/corpse presentation around Z=-501 below loaded map bounds. | A bad spawn makes the session unplayable and can invalidate later combat evidence. The journal narrows the trace but does not identify the spawn writer. | [`gamemodes/gamemodes.md`](gamemodes/gamemodes.md), [`networking/networking.md`](networking/networking.md) |
| 2 | Rocket Launcher and Force Punch attacks rejected as not equipped; 26 rejections in the journal, including five Rocket Launcher rejects with logical/native slot disagreement. | Authoritative combat rejection directly invalidates shots and is recurring across lives/weapons. Validate without manual re-equip and attach actor-generation identity. | [`networking/networking.md`](networking/networking.md), [`weapons/weapons.md`](weapons/weapons.md) |
| 3 | Dev-loop repeats status blocks and can show `NOT RUNNING` with old uptime and empty process/journal identity; launch also feels slow. | Confusing lifecycle output obscures build/run evidence and delays all play scenarios. Fixing attribution and status clarity precedes performance claims. | [`dev-loop/dev-loop.md`](dev-loop/dev-loop.md) |
| 4 | Leave-room / `ss` / reconnect sequence appears to require repeated GUI actions and may return to the sandbox server browser unexpectedly. | This is a core session lifecycle failure, but the owner docs already define the intended zero-extra-action join/rejoin contract; reproduce against that contract before expanding it. | [`dev-loop/dev-loop.md`](dev-loop/dev-loop.md), [`networking/networking.md`](networking/networking.md) |

### Handoff readiness for priorities 1 and 2

These entries are ready to hand off as trace-first investigation and repair
tasks, with acceptance criteria below. The journal identifies concrete paths
and rejects a blind fix; it does not name the final root cause. The agent must
reproduce or instrument the first missing state transition before changing an
owner.

#### Priority 1 — below-world spawn / map disagreement

- **Known evidence:** run `20261016_134002`, server PID 22216 loaded
  `mimita-aabb-only-interior-small-v4.glb` with zero spawn points; client PID
  32808 loaded `dust2cyberiav4.glb` with two spawn points; the client then
  retried missing `mimita-aabb-only-interior-small.glb` 30 times; later server
  PID 32276 and client PID 32808 reported `funworld3.glb`. A local
  actor/corpse-presentation event reported Z≈−501, below loaded map bounds.
  This supports map/session inconsistency and an out-of-world actor position,
  but does not identify the exact spawn writer.
- **First trace:** reconstruct command/GUI selection, server preset and
  authoritative map ID, client join/room sequence, every map load request,
  map-config resolution, spawn selection/application, and replication around
  the first bad position. Identify PID 32276's launch request and ownership
  from dev-loop/server records; do not assume it is a duplicate server or
  blame a stale executable.
- **Required outcome:** one canonical server-selected map/content revision;
  matching client load readiness before join/spawn; all player and NPC spawn
  routes use the authoritative safe-spawn policy; no stale-map coordinates;
  invalid map or spawn fails visibly.
- **Required scenario:** fresh dev-loop run; select the reported map/mode;
  record server start, room registration, client join, initial spawn,
  death/respawn, and NPC spawn. Repeat `ss` leave/rejoin if that is the entry
  path. Confirm map ID/revision, server/client readiness, spawn source and
  position, actor generation, and in-bounds collision contact. Inspect the
  exact new `EVENTS_JSONL_PATH` and first divergence.
- **Handoff status:** ready for investigation and repair without more product
  clarification; not enough evidence for a guessed one-line fix.

#### Priority 2 — equipped weapon disagreement across lives

- **Known evidence:** run `20261016_134002` has 26 server
  `weapon.attack.weapon_not_equipped` events (21 Force Punch, 5 Rocket
  Launcher). Rocket events show client slot 3, weapon native slot 7, server
  equipped slot 0, weapon set 1; a later accepted equip maps logical slot 3
  to native slot 7. Rejections occur near death events. The rejection event
  lacks actor generation and authoritative equipped definition ID, so the
  precise race/stale-state cause remains open.
- **First trace:** follow one weapon from active weapon-set data through client
  hotbar selection, local runtime/equipped identity, equip request encoding,
  server logical/native resolution, authoritative equip state, respawn
  inventory initialization, attack identity, validation, and result
  replication. Emit a bounded event chain with request ID, actor generation,
  definition/instance ID, hotbar index, equip revision, weapon set, and
  validated attack tick.
- **Required outcome:** stable item identity authorizes equip and attack;
  hotbar index is UI mapping only. The server owns inventory and
  equipped-item history, including respawn initialization. Legal shots after
  respawn work without manual re-equip; stale-generation, unowned, disallowed,
  or genuinely unequipped requests still reject with a reason.
- **Architecture outcome:** migrate toward the data-driven item/tool
  definition with reusable components in
  [`weapons/weapons.md`](weapons/weapons.md). Do not add or align another
  hard-coded slot. First repair the current lifecycle at its identity owner;
  then migrate `create...Definition()` registrations through parity-checked
  slices. Do not combine a full rewrite with the immediate respawn fix unless
  tracing proves the current model prevents a safe repair.
- **Required scenario:** using the real dev-loop executable, select a loadout
  where Rocket Launcher hotbar position differs from its legacy native slot;
  equip/fire, die, respawn without manual re-equip, then fire again. Repeat
  several lives and with two sets placing the same weapon at different
  positions. Confirm server/client identity, ammo/cooldown policy, equip
  acknowledgement, attack acceptance, and no false disagreement effects.
  Also confirm invalid item identity is rejected.
- **Scenario spec:** the full setup, event chain, and assertions are in
  [`networking/networking.md`](networking/networking.md), under “Reproducible
  scenario: equip, fire, die, respawn, fire again.” The current runner queues
  steps but does not yet evaluate those assertions or launch/join the session.
- **Handoff status:** ready for an agent to trace and repair respawn/equip.
  The component-based weapon migration is a separately staged architecture
  task, not an implied one-shot rewrite.

### Execution order and agent handoff for priorities 1 and 2

The work is split into evidence gates so the first agent can make the test
surface reliable without guessing at either gameplay root cause:

1. **Prove the existing `agent.*` command adapters.** Follow the command
   registry through normal owners and record dispatch, acceptance, application,
   and resulting state in the fresh executable's exact `EVENTS_JSONL_PATH`.
   The adapter-specific owners and minimum journal evidence are in
   [`dev-loop.md`](dev-loop/dev-loop.md), “Agent command proof before scenario
   automation.” A command being listed in help or returning queued is not
   proof it moved, aimed, acted, shot, or captured the intended state.
2. **Complete the one-command scenario runner.** Implement the existing
   [`dev-loop.md`](dev-loop/dev-loop.md) scenario-file contract: daemon/run
   identity, readiness waits, bounded event/state conditions, assertions,
   correlated final verdict, and nonzero failure status. Use a small command
   smoke scenario first. Do not claim end-to-end automation until its verdict
   is evaluated from the live journal.
3. **Reproduce and fix priority 1.** Use the runner to trace map selection,
   readiness, initial spawn, and respawn; require authoritative map identity
   and in-bounds spawn evidence. This is the gate for trustworthy gameplay
   scenarios, not a reason to block source-level priority 2 tracing.
4. **Reproduce and fix priority 2's current lifecycle.** Run equip/fire/death/
   respawn/fire without manual re-equip and capture the correlated
   client/server identity path. Repair the first demonstrated divergence.
   Keep the general weapon architecture migration staged unless that trace
   proves identity migration is necessary to remove the bug safely.
5. **Migrate the weapon architecture in parity slices.** First establish
   stable definition/instance identity and respawn correctness. Then create a
   generic validated definition loader, migrate one behavior family at a
   time, prove JSON-only addition/reload for capabilities already implemented,
   and delete old per-weapon constructors/slot compatibility only after
   equivalent behavior passes the same live scenarios. Unknown component or
   network protocol decisions remain explicit design work, not permission for
   an agent to invent arbitrary JSON scripting.

Priority 1 scenario acceptance requires the canonical map and revision to
match on server/client, map readiness before spawn, recorded spawn source and
safe position, and successful initial spawn plus respawn. Priority 2 requires
an item in different hotbar positions across sets, identity-based acceptance,
successful fire before and after at least three deaths without equip input,
and reasoned rejection of an invalid/unowned/stale identity. Both scenarios
must use normal command/game/network owners and the exact live journal; a
synthetic self-test or build result alone does not pass.

#### Copyable first-agent handoff

```text
Read AGENTS.md and docs/ROUTER.md, then follow the routed dev-loop,
networking, debug-logging, and weapon documents. Work specification-first and
preserve unrelated changes. Do not change gameplay code yet.

Audit the current implementation of agent.move, agent.aim, agent.action,
agent.shoot, and agent.snapshot. For each command, name the registration
owner, normal action/application owner, existing StructuredLogger events,
missing correlation or outcome evidence, and whether the live journal proves
accepted versus actually applied behavior. Inspect the scenario-file runner
and identify exactly which parts of the documented one-command contract are
implemented versus missing. Do not infer runtime success from source or a
queued response.

Update the existing dev-loop/networking/open-gameplay-issues specifications
with only evidence-backed corrections and concrete acceptance details. Define
the smallest live command-proof scenario and the implementation gates for the
one-command runner, including exact executable identity, EVENTS_JSONL_PATH,
bounded waits/assertions, and final pass/fail semantics. Keep priority 1
below-world spawn ahead of priority 2 integrated combat acceptance, while
allowing priority 2 source tracing in parallel. Keep the broad component
weapon migration separate from an immediate identity/respawn fix unless the
trace demonstrates it is required. Do not claim a fix or runtime proof.

Return exact source paths/functions and journal event names, spec gaps, a
sequenced implementation handoff, and any unresolved product decision. Add
the required single append-only changelog with exact before/after quotes.
```

The next implementation agent can then take the scenario-runner slice first;
after that runner has a live pass/fail proof, separate agents can implement
priority 1 and priority 2 against the same evidence contract. The current
weapon spec is sufficient to set the destination architecture, but migration
is broad enough to require its own owner-by-owner parity plan and live
acceptance rather than bundling it into the respawn bug fix by assumption.

### Plan next: high-impact gameplay and visual work

| Rank | Report | Why this priority | Owner |
|---|---|---|---|
| 5 | NPC combat needs stronger challenge, rapid legal weapon rotation, and authored movement sequences; three-NPC play reportedly drops frame rate. | Affects core match quality. First separate behavior tuning from measured performance; current selected resource windows do not establish NPC count as the bottleneck. | [`npc-behavior/npc-behavior.md`](npc-behavior/npc-behavior.md), [`../architecture/player-npc-systems/npc-movement.md`](../architecture/player-npc-systems/npc-movement.md), [`performance/performance.md`](performance/performance.md) |
| 6 | Knife/melee should be a viable option and a full melee mode should support NPCs; Blender-authored attacks should run on deterministic ticks. | Large feature set with multiple open combat decisions. NPC participation is now specified; hitstun, scoring, recovery, and balance still need design decisions. | [`weapons/melee-weapons.md`](weapons/melee-weapons.md), [`gamemodes/supersmashbrosmelee.md`](gamemodes/supersmashbrosmelee.md), [`animations/blender-animation-authoring.md`](animations/blender-animation-authoring.md) |
| 7 | Bullet-hole and blood marks appear stretched/broken; intended appearance is complete recognizable PNG stamps with state-aware variants. | Visible quality issue with screenshots, but rendering cause and exact alive/dead variant policy need a focused repro and authoring decision. | [`visuals/blood-bulletholes-viscera.md`](visuals/blood-bulletholes-viscera.md) |
| 8 | Replay export and ragdolls are reported unreliable; export should work headless and show progress every 60 ticks. | Useful creator workflow, but the current report lacks a failing export journal; first isolate recording, serialization, playback, and presentation. | [`replay/replay.md`](replay/replay.md), [`ragdoll-retrograd/ragdoll-retrograd.md`](ragdoll-retrograd/ragdoll-retrograd.md) |

### Needs planning: architecture, scale, and future capabilities

| Report | Why it needs planning | Existing owner |
|---|---|---|
| Multi-core execution, low-power support, and 240 FPS / aspirational 999 FPS. | The current journal is not a per-thread profile. Establish representative hardware, CPU/GPU/frame pacing measurements, determinism constraints, and a staged target before redesigning concurrency. | [`performance/performance.md`](performance/performance.md) |
| Combined adverse-network and low-render-rate testing (500 ms latency and about 23 FPS). | Existing networking docs define prediction/rewind and network scenarios; combined latency, loss/jitter, and low-FPS acceptance needs a bounded matrix and reproducible harness. The current screenshot shows about 8 ms ping, not this test. | [`networking/networking.md`](networking/networking.md) |
| Avatar-specific NPC collision, grappling hook/upward mobility, additional movement-tech library, generalized weapon/tool components, vehicles, and Juggernaut crowd readability. | These need explicit gameplay shape, ownership, performance budgets, and acceptance cases. Avoid making one bespoke subsystem for each item. | [`npc-behavior/npc-behavior.md`](npc-behavior/npc-behavior.md), [`movement/movement.md`](movement/movement.md), [`weapons/weapons.md`](weapons/weapons.md), [`gamemodes/juggernaut4.md`](gamemodes/juggernaut4.md), [`../architecture/collision/collision.md`](../architecture/collision/collision.md) |

### Already documented sufficiently; no duplicate spec added

- One-command `ss` start/rejoin, leaving a room while keeping a healthy server
  alive, and required session identity evidence are in
  [`networking/networking.md`](networking/networking.md) section 67.1 and
  [`dev-loop/dev-loop.md`](dev-loop/dev-loop.md).
- JSON polling/hot reload and live mode switching are described in the JSON
  configuration/live-development and gamemode specifications.
- Generic weapon requests and reusable hitscan/projectile behavior are in
  [`weapons/weapons.md`](weapons/weapons.md); the Blender-to-runtime proposal
  and 60 Hz animation contract are in
  [`animations/blender-animation-authoring.md`](animations/blender-animation-authoring.md)
  and [`animations/animations.md`](animations/animations.md).
- `logs/2026-10-16/20261016_134002/events.jsonl` contains 82,126
  `client.npc-avatar.snapshot-reassembled` events. This is a large repeated
  diagnostic count worth reviewing for bounded logging, but it does not prove
  that logging caused the reported frame-rate issue.
- The screenshot with `Room: ZZCV2WK` proves that a room code was visible in
  that captured later state; it does not disprove the earlier missing-code
  report or prove the leave/rejoin flow.

## Play-and-speak issue discovery

**Audience:** the player describing an issue during a live play session and
the AI/helper who records it. This section defines issue capture and routing;
it does not make an unverified report an accepted requirement, diagnose a
root cause by itself, or replace the owner specification and runtime evidence.

When an issue is spoken during play, capture the player's words as a short
verbatim quote when possible, then record the context separately:

1. **What was happening?** Map/mode, actor/avatar, weapon or ability, client
   or host role, and the command/input immediately before the issue.
2. **What did the player expect?** Keep the intended behavior in the player's
   own words; do not infer a new rule from the symptom alone.
3. **What actually happened?** Record visible result, error text, whether it
   repeated, and whether leaving/rejoining or another action changed it.
4. **Where does this belong?** Search current specs, issue notes, regressions,
   and code ownership. Link the existing owner and add the report there; do
   not create a competing spec when one already owns the behavior.
5. **What is known?** Mark each item as user-reported, source-traced,
   runtime-reproduced, or human-accepted. If the owner is unclear, preserve
   the report in this index with candidate links until it can be routed.
6. **What evidence can help reproduce it?** When available, record build and
   executable identity, exact `EVENTS_JSONL_PATH`, scenario/action sequence,
   server/client roles, and the first expected-versus-actual divergence. Do
   not interrupt play to guess a cause; capture the report first and investigate
   after or between play attempts.

The working loop is: play and speak → capture the report → find the owning
spec → clarify expected behavior → append the issue and acceptance scenario →
implement only after the requirement is clear → replay the same scenario on
the live dev-loop executable → inspect the event journal → update the issue
with the result. A transcript is useful evidence of intent, not proof that the
game behaved that way.

## When an implementation does not work in play

Do not conclude that “the code was ignored” or that the executable is stale
until the path is traced. Find the first missing step among:

1. the requested behavior is clear and has a checkable acceptance scenario;
2. the intended command/input reaches the documented owner;
3. the expected config, avatar, map, or animation asset is the one actually
   loaded;
4. the correct client/server authority accepts or rejects the action, with a
   reason;
5. simulation changes the authoritative state;
6. replication carries that state to the other side; and
7. presentation displays the resulting state.

For each attempt, record what changed, the exact executable/build identity,
the runtime scenario, journal evidence, the first divergence, and what will be
tried next. A successful compile proves only that the code compiled. A runtime
event proves that a path ran, not that the behavior felt or looked correct;
human acceptance remains its own result. Follow
[`../workflows/runtime-scenario-validation.md`](../workflows/runtime-scenario-validation.md).

### Griever example

The existing Griever specification already records a report of persistent
server-position error and slow hovering, with the first cause unconfirmed. Its
attempt log also includes the short report “good rolling but doesnt bounce,
needs to bounce a lot lot lot.” Treat these as separate observed symptoms:
rolling motion, bounce response, resting/floor behavior, and client/server
position agreement. For the next play attempt, state the intended bounce
behavior before tuning, record the exact executable and journal, and identify
whether the first divergence is capability/config selection, input, server
acceptance, collision contact/response, reconciliation, or presentation. Keep
the actual Griever requirements and attempt evidence in
[`griever-monster/griever-monster.md`](griever-monster/griever-monster.md).

## Work focus noted for 2026-10-16

The planned work focus is to improve the custom animation authoring/runtime
path, make Zombie Tower better through actual play, and speak issue reports as
they happen so they can be routed into the right specification. This is a
planning note, not a claim that these tasks were implemented or accepted. For
animation, first compare the runtime contract in `animations.md` with the
Blender proposal and decide the smallest export slice. For Zombie Tower, use
the existing mode and Griever specifications, then record real play scenarios
and outcomes in their attempt/issue sections.

## Documentation follow-up

Review each linked owner spec with the user before implementation. Lock open
choices (especially collision shape, authority/correction, PvP mode policy,
spawn selection, and SSBM rules) into acceptance scenarios first. Implementation
should then run those scenarios on the dev-loop executable and inspect live
runtime evidence; a document change or successful build alone does not prove a
fix.
