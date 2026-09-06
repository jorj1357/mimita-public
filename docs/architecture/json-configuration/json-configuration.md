// 09 06 2026, 00 00
/* purpose
* define the JSON configuration architecture for MiMITA
* identify commonly used configuration families and their owners
* make the vast majority of gameplay and presentation behavior editable
* this file DOES NOT claim that every listed file is hot-reloaded today
* this file DOES NOT replace feature specifications
* this file DOES NOT permit unsafe live changes to a running network match
*/

# JSON Configuration Architecture

MiMITA should be configuration-led. The vast majority of values that control
gameplay, presentation, accessibility, testing, and networking should live in
named JSON files rather than being compiled into C++ code.

The goal is to change behavior by editing configuration and reloading it, not
by rebuilding the executable for every tuning change. A value is a good JSON
candidate when it is a policy, constant, balance value, layout value, visual
choice, timing value, or test setting rather than engine plumbing.

This is a target architecture. The repository currently contains many JSON
files, but not every file is proven to be hot-reloaded. Each file must be
verified against its loader and reload path before being described as live.

## Reload contract

Every configuration file should have:

1. one obvious owner and one schema;
2. a safe default when the file is missing or invalid;
3. validation with a warning that names the file and rejected field;
4. atomic replacement of the last valid configuration;
5. a reload signal or timestamp check that does not require a process restart;
6. a documented application boundary: immediately, on respawn, at round start,
   or on the next connection;
7. a terminal command that can reload, print, validate, or inspect it.

Invalid JSON must not partially replace a working configuration. Match-critical
values should be staged and applied at a safe boundary instead of changing
halfway through a tick.

## Configuration families

These are the hot-reload families that are already heavily used or should be
made hot-reloadable over time.

| Family | Current files in this repository | Values that belong here |
|---|---|---|
| Movement | `config/movement.json`, `config/movement/` | acceleration, speed, gravity, jump, dash, air control, friction, movement mode |
| Collision and physics | `config/collision.json`, `config/collision-lod.json`, `config/weaponcollisions.json` | body dimensions, layers, tolerances, solver limits, weapon-world collision |
| Networking | `config/networkingconfig.json`, `config/network/`, `config/networking/` | tick and snapshot policy, interpolation, retry timing, timeouts, packet limits, ICE behavior |
| Gameplay | `config/gameplay.json`, `config/gameplay/`, `config/size_scaling.json` | damage, health, respawn, void bounds, scaling, knockback, interaction rules |
| Weapons | `config/weapons.json`, `config/weaponsets.json`, `config/duel-weapons.json` | definitions, cooldowns, projectiles, ammo, slots, sets, duel restrictions |
| Weapon presentation | `config/weapon_hitfx.json`, `config/weapon-cool-shot-line.json`, `config/weapon-tracers.json` | tracers, shot lines, impact visuals, timing, colors, visibility |
| Effects | `config/hitfx.json`, `config/impact_decals.json`, `config/effects/` | particles, decals, lifetimes, sizes, colors, sound references |
| Game modes | `config/gamemodes/`, `config/duel-maps.json`, `config/onlinemodes.json` | goals, timers, countdowns, maps, queues, team and duel rules |
| GUI and HUD | `config/gui/`, `config/killfeed.json`, `config/healthbar.json`, `config/rewards-hud.json`, `config/chatgui.json` | text, fonts, colors, positions, sizes, visibility, layout, notifications |
| Input and profiles | `config/current-profile.json`, `config/profiles.json`, future input/accessibility files | bindings, sensitivity, remapping, color-blind modes, text scale, contrast |
| Video and lighting | `config/video-settings.json`, `config/shadows.json`, `config/lighting.json`, `config/postfx.json`, `config/skybox.json` | resolution, quality, shadows, lighting, post-processing, frame limits |
| Audio | `config/audio/`, `config/notifications.json`, `config/tipsconfig.json` | volumes, routing, subtitles, notification behavior, music |
| Avatars and actors | `config/bodyparts.json`, `config/player-procedural.json`, `config/npc-avatar.json`, `config/npc-difficulty.json`, `config/npcpresets/` | body layout, cosmetics, NPC difficulty, actor presentation, procedural values |
| Replay | `config/replay/`, `config/replayexport.json` | recording, playback, export, camera, event and effect capture |
| Debugging and testing | `config/debug/`, `config/debuglogger.json`, `config/hotreload.json`, `config/analytics.json` | debug categories, logging, profiling, reload diagnostics, test switches |

The inventory is a starting map, not proof that every file currently has the
same loader or reload behavior. Check the owning loader before changing one.

## What should remain compiled

Keep platform integration, memory ownership, serialization machinery, security
boundaries, packet framing, cryptographic operations, and low-level rendering
plumbing in code. Their policies and tunable inputs should still be exposed as
configuration where safe.

Never put secrets, passwords, private keys, or account credentials in shipped,
committed, or reported JSON.

## Hot-reload priority

Implement and verify reload support in this order:

1. GUI, HUD, text, colors, fonts, and layout;
2. effects, audio routing, crosshair, shadows, lighting, and post-processing;
3. movement, weapon balance, damage, collision tolerances, and game-mode values;
4. NPC difficulty, avatar presentation, replay, and profiles;
5. networking timing and transport policy at safe session boundaries.

Movement constants and networking values should be editable, but their scope
must be explicit. A local presentation value may apply immediately; a shared
match or protocol value should be negotiated, applied on restart, or applied at
the next round so clients and servers cannot silently disagree.

## Verification

For every new or migrated JSON value, document the loader, owner, schema,
default, reload trigger, application boundary, validation command, and expected
diagnostic. A focused terminal test should prove the resulting state without
requiring a visual test for every edit.
