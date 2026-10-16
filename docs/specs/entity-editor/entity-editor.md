// 2026-10-08
/* purpose
* document the current Map Entity Editor v0 command surface
* show how authored entities are edited in the active map JSON
* explain live visibility, save, reload, and last-valid behavior
* this document DOES NOT claim that every entity type already has gameplay behavior
* this document DOES NOT replace the Zombie Tower gameplay specification
* this document DOES NOT make commands a bypass around server authority
*/

# Map Entity Editor v0

## What this is

The Map Entity Editor is the first map-authoring slice of the future unified
MiMITA editor. It edits authored map entities stored in:

```text
config/maps/<active-map-id>.json
```

For Zombie Tower 4, the active map ID is `zombietower4`, so the file is:

```text
C:\mimita-v9\config\maps\zombietower4.json
```

The editor has one registry owner, `MapConfigRegistry`. The map ID comes from
the active server map when available, otherwise from the loaded map path. The
editor does not silently edit an unrelated map file.

## Fast answer: add a spawn point

Open the in-game terminal while Zombie Tower 4 is loaded and run:

```text
entity_help
entity_visibility on
entity_add spawnpoint tower4_start
entity_save
```

`entity_add` places the entity five world units in front of the player camera.
The new spawn point is immediately present in memory. The debug marker is
visible after `entity_visibility on`. `entity_save` writes it to
`config/maps/zombietower4.json`.

To move it to the current camera placement:

```text
entity_move_here tower4_start
entity_save
```

To place it at an exact position:

```text
entity_set_position tower4_start 12.5 4.0 -31.0
entity_save
```

To inspect the running registry:

```text
entity_list
entity_info tower4_start
```

## Command reference

Run `entity_help` in the game for the compact in-game list.

| Command | Usage | Purpose |
|---|---|---|
| `entity_help` | `entity_help` | Print all Map Entity Editor commands. |
| `entity_add` | `entity_add <type> [id]` | Create an entity at the camera-forward placement. |
| `entity_list` | `entity_list` | List every entity in the active map registry. |
| `entity_info` | `entity_info [id]` | Inspect an entity; omitted ID uses the selected entity. |
| `entity_select` | `entity_select <id>` | Select an entity. |
| `entity_move_here` | `entity_move_here [id]` | Move an entity to the current camera-forward placement. |
| `entity_set_position` | `entity_set_position <id> <x> <y> <z>` | Set an exact world position. |
| `entity_set` | `entity_set <id> <property> <value>` | Change an authored property; `size` takes three values. |
| `entity_delete` | `entity_delete [id]` | Delete an entity from memory. |
| `entity_visibility` | `entity_visibility <on\|off>` | Show or hide authored debug markers. |
| `entity_save` | `entity_save` | Write the current registry to the active map JSON. |
| `entity_reload` | `entity_reload` | Reload the active map JSON; invalid JSON keeps the last valid state. |

### Entity types

`entity_add` accepts:

```text
monster_zone
checkpoint
pickup
damage_volume
boss_trigger
spawnpoint
```

Examples:

```text
entity_add spawnpoint tower4_start
entity_add checkpoint tower4_floor_02
entity_add pickup tower4_health_01
entity_add damage_volume tower4_lava_01
entity_add boss_trigger tower4_boss_door
entity_add monster_zone tower4_zone_a
```

IDs are stable authoring IDs. If omitted, one is generated, such as
`spawnpoint_1` or `monster_zone_1`.

### Editable properties

```text
entity_set tower4_zone_a radius 20
entity_set tower4_zone_a size 10 4 10
entity_set tower4_zone_a spawnCount 5
entity_set tower4_zone_a maxAlive 12
entity_set tower4_zone_a spawnCooldownTicks 600
entity_set tower4_zone_a monsterPool basic_zombie
entity_set tower4_zone_a oneShot true
entity_set tower4_zone_a enabled true
```

`spawnCooldownTicks` uses the fixed 60 Hz simulation tick; `600` ticks is
10 seconds. Use `entity_save` after property changes. Without it, changes
remain only in the current process.

## Complete Zombie Tower 4 example

```text
entity_visibility on
entity_add monster_zone tower4_zone_a
entity_set tower4_zone_a radius 20
entity_set tower4_zone_a spawnCount 5
entity_set tower4_zone_a maxAlive 12
entity_set tower4_zone_a spawnCooldownTicks 600
entity_set tower4_zone_a monsterPool basic_zombie
entity_set tower4_zone_a oneShot false
entity_save
entity_list
```

The saved entity is shaped like:

```json
{
  "id": "tower4_zone_a",
  "type": "monster_zone",
  // "position": [12.5, 4.0, -31.0],
  // "size": [1.0, 1.0, 1.0],
  "radius": 20.0,
  "enabled": true,
  "visible": true,
  "oneShot": false,
  "monsterPool": "basic_zombie",
  "spawnCount": 5,
  "maxAlive": 12,
  "spawnCooldownTicks": 600
}
```

## Editing JSON while the game is running

The live workflow is:

1. Load Zombie Tower 4.
2. Run `entity_visibility on`.
3. Create or adjust an entity with commands.
4. Run `entity_save` to write `config/maps/zombietower4.json`.
5. Edit that JSON externally and save it.
6. The client and dedicated server poll the active map config and reload a
   valid change without restarting. The server poll is bounded to at most
   once per 250 milliseconds; the client uses its normal config polling loop.
7. The debug marker updates from the newly loaded entity data.

Force a reload with:

```text
entity_reload
entity_list
```

Malformed JSON emits a reload-failure diagnostic and retains the last valid
entity state. It does not apply half of a broken edit.

The directions are explicit:

```text
terminal command -> in-memory registry immediately
entity_save      -> JSON file
JSON file edit   -> live registry after polling, or entity_reload
```

The editor does not overwrite JSON after every command. Use `entity_save` when
the current in-game state should become the authored file.

## Current gameplay scope

All six entity types can currently be authored, saved, reloaded, and shown as
debug geometry. `monster_zone` is the first type with authoritative gameplay:
when a live player enters its radius, the server can create a configured NPC
wave and apply its cooldown, maximum-alive, and one-shot rules.

`checkpoint`, `pickup`, `damage_volume`, and `boss_trigger` can currently be
authored, saved, reloaded, and shown as debug geometry, but their full gameplay
consumers are future slices. `spawnpoint` is different: the server already
consumes enabled authored spawnpoint entities in some gamemode-managed spawn
paths, with optional team spawn-group tag matching. That consumption is
incomplete and does not cover every spawn entry path.

## Open issue: one authored spawn point is not used by every spawn path

User-reported desired behavior: after placing a spawn point, it should be
usable by the next player or NPC spawn in the current Sandbox session. It
should also be saved so the same point is available after a restart.

Source review found three different paths today:

- Gamemode-managed respawn/map-change selection in
  `src/network/server-gamemode.cpp` reads enabled `MapConfigRegistry` spawnpoint
  entities and filters their `tag` against the team's `spawn_group`.
- Initial player join in `src/network/server-packets.cpp` selects from
  `HeadlessWorld::spawnPoints`, which are loaded from spawn nodes in the map
  GLB; it does not consult authored map-config spawnpoint entities.
- The `npc_spawn` terminal command in `src/terminal/npc-commands.cpp` chooses
  from the client/local `World::spawnPoints` through
  `src/game/spawn-utils.cpp`. In multiplayer it sends that chosen position in
  `SpawnNpcRequestPacket`, rather than asking the server to select an authored
  spawnpoint.

The intended design is one server-owned spawn-selection policy, specified in
[`gamemodes.md`](../gamemodes/gamemodes.md), used by initial player join,
respawn/map changes, and NPC spawn commands. It should prefer enabled authored
map-config spawnpoints that match the active mode/team/role rules, then use
legacy GLB spawnpoints as a compatibility fallback, then the existing safe
fallback. The server must validate the selected position and apply it through
the existing authoritative player/NPC spawn lifecycle. Existing checkpoint,
explicit teleport, and mode-specific placement rules keep their documented
precedence.

`entity_add` / `entity_set_position` make the point available in the current
authoring session; `entity_save` persists it. A newly placed point affects the
next spawn, not actors already alive. For a remote server, the editor must send
an authorized edit request to the server; saving only the client's local map
JSON cannot change a different machine's spawn registry.

Acceptance must cover initial player join, player respawn, `npc_spawn`, NPC
wave/role spawns, team-tag filtering, disabled points, map-config reload,
legacy-GLB fallback, safe-position rejection, and a remote client/server edit.
This is a spec and implementation gap; runtime behavior was not reproduced in
this review.

## Safe editing checklist

```text
entity_visibility on
entity_list
entity_select <id>
entity_info
entity_set_position <id> <x> <y> <z>
entity_save
entity_reload
entity_list
```

Keep IDs stable, edit only the active map file, and validate JSON before a
multiplayer test. Build success does not prove that the command, reload,
server activation, or visible marker was observed in a live session.
