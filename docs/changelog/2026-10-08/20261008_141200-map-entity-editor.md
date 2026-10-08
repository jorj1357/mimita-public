# Map Entity Editor v0

Time: 2026-10-08 14:12:00 EST
Branch: `afad20a-rebuild`
Commit: none created; local changes remain uncommitted.

## Scope and diagnosis

Implemented the first generalized Map Entity Editor slice for Zombie Tower after tracing the existing owners. `MapConfigRegistry` owned bomb-site JSON only; terminal registration was split between `src/main-systems.cpp`, `src/terminal/debug-commands.cpp`, and `src/terminal/npc-commands.cpp`; static GLB spawn points were owned by `src/world`; persistent NPC activation was owned by `src/network/server-gamemode.cpp`; and debug visualization was owned by `src/debug/debug-visuals-scene.cpp`. No active generalized authored-entity registry existed.

The Zombie Tower specification requires map-authored entities, editor commands, debug visibility, hot reload, stable IDs, and last-valid configuration behavior. The implementation keeps bomb-site data and adds a single generalized entity owner rather than introducing parallel per-entity registries.

## Changes made

- `src/gamemode/map-config.h:27-103` adds `MapEntity`, entity lookup/mutation, visibility, creation/deletion, and mutable access for the server owner.
- `src/gamemode/map-config.cpp:70-360` parses and saves `entities`, accepts camelCase/snake_case fields, emits categorized map-entity events, and applies parsed data only after a complete valid parse. Invalid JSON retains the last valid state and advances the invalid-file timestamp so polling does not spam repeated failures.
- `src/terminal/map-entity-commands.cpp:72-226` adds `entity_add`, `entity_list`, `entity_info`, `entity_select`, `entity_move_here`, `entity_set_position`, `entity_set`, `entity_delete`, `entity_visibility`, `entity_save`, and `entity_reload`. `entity_set` covers radius, size, spawn count, max alive, cooldown, monster pool, one-shot, and enabled state.
- `src/main-systems.cpp:421` registers the command owner.
- `src/debug/debug-visuals-scene.cpp:116,347` renders authored entities with type colors, labels, boxes/spheres, and spawnpoint markers when entity visibility is enabled.
- `src/engine/engine-tick-setup.cpp:152` keeps the client map registry aligned with the active map.
- `src/network/server.h:635` records the authored zone owner on spawned NPCs.
- `src/network/server-gamemode.cpp:2777-2817,3540` activates `monster_zone` entities from the authoritative server when a live player enters the radius, applies count/max-alive/cooldown/one-shot rules, and emits `monster-zone.activated` through the existing NPC wave path.
- `config/maps/zombietower1.json` adds the first active Zombie Tower map-config file with an empty entity list and preserved bomb defaults.

## Validation evidence

BUILD: `python build_agent.py` completed successfully at 2026-10-08 14:10:20. The final named executable is `mimita-20261008T1415-map-entity-v0.exe`; 1 source compiled, 548 skipped, return code 0.

COMPONENT: `mimita-20261008T1415-map-entity-v0.exe --versioninfo` completed and emitted `EVENTS_JSONL_PATH=logs/10-08-2026/20261008_141020/events.jsonl`. The existing `--map-config-selftest` also passed (`sites_checked=2`, `PASS`) on the earlier named build. `config/maps/zombietower1.json` parsed successfully with PowerShell JSON parsing.

JSONL: `logs/10-08-2026/20261008_141020/events.jsonl` contains the exact final executable identity, `logger.started`, `versioninfo.executed`, and `logger.stopped`. It contains no map-entity or monster-zone events because no in-game command or map scenario was reached.

RUNTIME: A normal timestamped executable was launched earlier (`mimita-20261008T1408-map-entity-v0.exe`, journal `logs/10-08-2026/20261008_140831/events.jsonl`) and remained responsive. Native computer-use could enumerate browser surfaces but could not access the Windows game window, so terminal commands, visible markers, hot reload, NPC activation, and multiplayer behavior were not exercised. The running process was left untouched.

HUMAN ACCEPTANCE: still required. In Zombie Tower, verify `entity_visibility on`; add and configure `monster_zone zt_zone_a`; save, edit, reload, and confirm visible stable-ID markers; approach with a player and confirm one authoritative wave, max-alive, cooldown, and one-shot behavior; then verify invalid JSON retains the last valid state. Pickup/checkpoint/damage-volume/boss-trigger gameplay effects are not implemented by this v0 slice; they are authored and visualized only.

## Limits and specification notes

Only `zombietower1.json` was added because it is the active proof target; `zombietower2` through `zombietower5` still need authored files. The placement command uses the existing player-plus-camera-forward placement path; no shared collision-aware editor raycast owner was found during diagnosis. Saving rewrites the supported map-config fields and does not preserve unknown future top-level fields.

The following specification TODOs were observed and intentionally not edited: `docs/specs/entity-editor/entity-editor.md:1` (`todo explain entity editor`), `docs/specs/gamemodes/zombie-tower.md:1172` (`todo explain better enshittification ?`), and `docs/specs/gamemodes/zombie-tower.md:5293` (`todo - explain zombie tower`).

## Repository hygiene

Pre-existing edits in configuration and documentation, including the modified Zombie Tower specification, were preserved and are not attributed to this work. No regression record was created because no human-confirmed regression was established. Focused reviews used the router-selected JSON configuration, terminal-command, logging, runtime-scenario, build/EXE, task-completion, ECS, player/NPC, and regression guidance; the entity-editor document remains an acknowledged TODO rather than an authority.
