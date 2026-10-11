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
```

`entity_add` places the entity five world units in front of the player camera.
The new spawn point is immediately present in memory and is automatically
written to `config/maps/zombietower4.json`. The debug marker is visible after
`entity_visibility on`.

To move it to the current camera placement:

```text
entity_move_here tower4_start
```

To place it at an exact position:

```text
entity_set_position tower4_start 12.5 4.0 -31.0
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
| `entity_save` | `entity_save` | Manually write the current registry to the active map JSON; normal editor mutations already auto-save. |
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

### What each type actually means today

An entity is an authored position plus a trigger/placement shape and a set of
optional fields. The type name does not automatically make every field
gameplay-active. The current consumers are:

| Type | Meaning in the current code | Shape used | Current gameplay result |
|---|---|---|---|
| `monster_zone` | A player-proximity monster activation region. | The active monster-zone path currently checks `radius` only. `shape: "box"` does not yet change monster-zone activation. | When an active player is inside the radius, the server spawns up to `spawnCount` NPCs, applies `monsterRole` such as `zombie`, and respects `maxAlive`, `spawnCooldownTicks`, and `oneShot`. The current implementation places each NPC at the zone center; it does not yet choose a random point inside the zone. |
| `checkpoint` | A one-time Zombie Tower progress marker. | Explicit `sphere` or `box` containment. Empty `shape` keeps the legacy inference. | The first active player who enters records this checkpoint as the party's latest respawn position, subject to its monotonic `checkpointIndex`. A later party wipe respawns the party there. |
| `pickup` | A future/general pickup location. | Debug geometry only at present. | The entity is saved, reloaded, and drawn. `pickupId` is stored, but this map-entity loop does not yet grant health, ammo, weapons, or other items. |
| `damage_volume` | An authored environmental hazard volume. | Explicit `sphere` or `box` containment. Empty `shape` keeps the legacy inference. | Every `damageIntervalTicks` while players or living NPCs are inside, the server applies `damage` through the environment damage path and emits a `damage-volume.damage` event. |
| `boss_trigger` | A one-time Zombie Tower boss encounter trigger. | Same shared containment test as `checkpoint`: a larger box is accepted, and the sphere is also accepted. | Entering it starts the boss encounter, locks progression, and spawns `bossId` at the entity position. The lock is released when that spawned boss dies. Set `bossId`; an empty `bossId` can activate the lock without creating a boss actor. |
| `spawnpoint` | A selectable player spawn location. | Point placement; `radius`/`size` are not used to select the location. | Enabled spawnpoints are candidates for player spawning. The optional `tag` can separate groups, for example `CT` and `T`; the server randomly selects among matching authored points. |

The shape rule is explicit for volume consumers:

```text
shape = sphere  -> use radius; ignore size for containment
shape = box     -> use size; ignore radius for containment
shape omitted   -> legacy inference: box when size is larger than 1 1 1,
                   otherwise sphere
```

The box is centered at `position`, uses full dimensions `size x y z`, is
axis-aligned, and cannot currently be rotated. `checkpoint`, `boss_trigger`,
and `damage_volume` use this shape rule. `monster_zone` is a special case in
the active server path and currently checks only its spherical `radius`, even
if `shape: "box"` is authored.

The colored debug markers are only an authoring aid. They prove that the
entity is loaded and visible, not that its gameplay consumer exists or has
activated.

### How to think about the six types

Use the types according to the event you want:

```text
spawnpoint     = where an actor may be placed at spawn time
monster_zone   = where entering should start/replenish a monster wave
checkpoint     = where entering records a later party respawn location
pickup         = where a future item interaction will be authored
damage_volume  = where an environmental hazard applies damage
boss_trigger   = where entering should start a boss encounter
```

They are not interchangeable. A `spawnpoint` does not mean “spawn monsters
here,” and a `monster_zone` does not currently mean “randomly distribute
monsters throughout this box.” For a random monster origin, the current
editor can author the activation zone, but the random interior spawn policy
still needs a gameplay implementation.

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
10 seconds. `radius` is in world units. `size x y z` is an axis-aligned box
size in world units for entity types that use the shared containment helper.
Successful editor mutations automatically write the current registry to the
active map JSON. If the write fails, the terminal reports that the change is
only in memory. `entity_save` remains available as a manual retry/explicit
save command.

The properties are interpreted by type as follows:

| Property | Used by | Explanation |
|---|---|---|
| `shape` | `checkpoint`, `boss_trigger`, `damage_volume` | Exact supported values are `sphere` or `box`. Empty keeps legacy inference. It is saved for every entity type, but `monster_zone` currently still uses radius-only activation. |
| `radius` | `monster_zone`, `checkpoint`, `boss_trigger`, `damage_volume` | Sphere radius when `shape` is `sphere`; also the current activation radius for `monster_zone`. |
| `size` | `checkpoint`, `boss_trigger`, `damage_volume` | Full axis-aligned box dimensions when `shape` is `box`. |
| `spawnCount` | `monster_zone` | Maximum number requested in one activation, limited by available `maxAlive` room. |
| `maxAlive` | `monster_zone` | Maximum living NPCs associated with that zone. |
| `spawnCooldownTicks` | `monster_zone` | Minimum fixed-tick delay between activations. |
| `monsterPool` | `monster_zone` | String used as the spawned NPC's pool/name prefix by the current path. It is not a documented catalog lookup here. |
| `oneShot` | `monster_zone` | Prevents that zone from activating again after its first activation. |
| `pickupId` | `pickup` future consumer | Identifier stored for the item/effect that should be granted later. |
| `damage`, `damageType`, `damageIntervalTicks` | `damage_volume` | Environment damage amount, label, and fixed-tick interval. Applies to players and living NPCs inside the volume. |
| `bossId` | `boss_trigger` | Identifier/name used to create the boss NPC when the trigger is entered. |
| `tag` | `spawnpoint` | Optional spawn group filter, such as `CT` or `T`. |
| `checkpointRequirement` | reserved/future | Loaded and saved, but no current map-entity runtime check consumes it. |
| `checkpointIndex` | `checkpoint` | Monotonic progress number; a checkpoint only advances the run when its index is greater than the current checkpoint index. |
| `monsterRole` | `monster_zone` | Shared NPC role, such as `zombie`, used when the zone creates NPCs. |
| `enabled` | all types | Disabled entities are ignored by the active consumers and hidden from gameplay selection. |
| `visible` | editor/debug drawing | Controls the authored debug marker, not gameplay activation. |

### Shape examples: exactly what to type

Use lowercase `sphere` or `box` as the value. These are the complete explicit
shape values currently supported by the entity system.

#### Sphere

A sphere is centered at the entity's `position` and uses `radius`:

```text
entity_add damage_volume lava_sphere
entity_set lava_sphere shape sphere
entity_set lava_sphere radius 5
entity_set lava_sphere damage 8
entity_set lava_sphere damageType lava
entity_set lava_sphere damageIntervalTicks 30
```

This damages players and living NPCs while they are inside a 5-unit sphere.
The `size` field does not define the sphere's containment.

#### Box

A box is centered at the entity's `position` and uses full dimensions from
`size`:

```text
entity_add damage_volume lava_box
entity_set lava_box shape box
entity_set lava_box size 20 2 12
entity_set lava_box damage 8
entity_set lava_box damageType lava
entity_set lava_box damageIntervalTicks 30
```

This creates a box that is 20 units wide, 2 units tall, and 12 units deep.
The box is axis-aligned; there is currently no `rotation` property. The
`radius` field does not define a box's containment.

For an existing entity, changing from sphere to box is just:

```text
entity_set lava_1 shape box
entity_set lava_1 size 20 2 12
```

Changing it back is:

```text
entity_set lava_1 shape sphere
entity_set lava_1 radius 5
```

Every successful command auto-saves, so no separate `entity_save` command is
needed. `entity_info lava_1` and `entity_visibility on` help confirm the
authored values and debug marker.

#### Empty shape / legacy inference

If `shape` is omitted or set to an empty string in JSON, the old rule remains:
an entity with any `size` component larger than `1` is treated as a box;
otherwise it is treated as a sphere. New JSON should use explicit `shape` so
the intended geometry is obvious.

Important limitation: `checkpoint`, `boss_trigger`, and `damage_volume` honor
the explicit shape. `monster_zone` currently uses only its radius to decide
whether a player activates it, so writing `entity_set tower4_zone_a shape box`
does not make its activation region rectangular yet.

## Complete Zombie Tower 4 example: tower encounter

For the zombie-tower behavior you described—“when the player reaches this
part of the tower, start a zombie encounter”—author one `monster_zone` at the
entrance to the encounter. The player does not need to touch the exact center;
being within `radius` is enough.

```text
entity_visibility on
entity_add monster_zone tower4_zone_a
entity_set tower4_zone_a radius 20
entity_set tower4_zone_a spawnCount 5
entity_set tower4_zone_a maxAlive 12
entity_set tower4_zone_a spawnCooldownTicks 600
entity_set tower4_zone_a monsterPool basic_zombie
entity_set tower4_zone_a oneShot false
entity_list
```

The saved entity is shaped like this (other entity types use the same common
fields and add the type-specific fields described above):

```json
{
  "id": "tower4_zone_a",
  "type": "monster_zone",
  "position": [12.5, 4.0, -31.0],
  "size": [1.0, 1.0, 1.0],
  "radius": 20.0,
  "shape": "sphere",
  "enabled": true,
  "visible": true,
  "oneShot": false,
  "monsterPool": "basic_zombie",
  "spawnCount": 5,
  "maxAlive": 12,
  "spawnCooldownTicks": 600
}
```

With the current implementation, the result is:

```text
player enters tower4_zone_a radius
        -> server activates the zone
        -> up to 5 NPCs are created
        -> each NPC starts at tower4_zone_a.position
        -> the zone may activate again after 600 ticks if oneShot is false
          and living NPCs are still below maxAlive
```

The requested “random area inside the zone” behavior would instead be:

```text
player enters tower4_zone_a radius
        -> server activates the zone
        -> choose a valid random point inside the authored zone
        -> spawn each zombie at that point
```

That second flow is not what the current code does yet. Do not expect changing
`size` to create random spawn locations: `monster_zone` currently uses only
the spherical `radius` check and then assigns `npc.pos = zone.position`.
Until that gameplay change is implemented, a practical authoring workaround
is to use several separate `monster_zone` entities around the tower and tune
their activation radii, but that gives multiple centers—not a random interior
selection from one zone.

### Example: checkpoint before the next tower section

This makes a spherical checkpoint at the current camera-forward placement:

```text
entity_add checkpoint tower4_floor_02_checkpoint
entity_set tower4_floor_02_checkpoint shape sphere
entity_set tower4_floor_02_checkpoint radius 5
```

To make the checkpoint cover a doorway or a short hallway instead of a sphere:

```text
entity_set tower4_floor_02_checkpoint shape box
entity_set tower4_floor_02_checkpoint size 8 4 12
```

When an active player enters the sphere/box, the server records the entity's
`position` as the latest party respawn position. The box is centered on that
position; it is not rotated. A party wipe later returns the party to the
checkpoint position, and the same checkpoint is not counted twice during that
run.

### Example: pickup location (authored, not gameplay-active yet)

This authors the future location and item identifier for a large health pickup:

```text
entity_add pickup tower4_health_01
entity_set tower4_health_01 pickupId health_large
entity_set tower4_health_01 tag floor_02
```

Today this gives the editor a saved entity and a yellow debug marker. It does
not yet make walking over the marker grant health, because no current
map-entity consumer reads `pickupId`. The `tag` is also just authored metadata
for this type at present.

### Example: damage volume

This describes a lava-like hazard region:

```text
entity_add damage_volume tower4_lava_01
entity_set tower4_lava_01 shape box
entity_set tower4_lava_01 size 20 2 12
entity_set tower4_lava_01 damage 25
entity_set tower4_lava_01 damageType lava
entity_set tower4_lava_01 damageIntervalTicks 30
```

This creates a 20 by 2 by 12 box that deals 25 `lava` damage to players and
living NPCs inside it every 30 fixed ticks. The server emits a
`damage-volume.damage` event when damage is applied.

### Example: boss trigger at the top of a floor

Put the trigger before the arena and name the boss definition/actor with
`bossId`:

```text
entity_add checkpoint tower4_boss_checkpoint
entity_set tower4_boss_checkpoint shape sphere
entity_set tower4_boss_checkpoint radius 6

entity_add boss_trigger tower4_floor_05_boss
entity_set tower4_floor_05_boss shape box
entity_set tower4_floor_05_boss size 18 6 18
entity_set tower4_floor_05_boss bossId tower_guardian
```

Entering the trigger once starts the encounter, marks progression locked, and
spawns `tower_guardian` at the trigger's center position. When that spawned
boss dies, the encounter lock is released. The checkpoint immediately before
it is separate and is what gives the party a safe retry position.

### Example: player spawn points

For a team-based map, author tagged points:

```text
entity_add spawnpoint tower4_ct_start
entity_set tower4_ct_start tag CT

entity_add spawnpoint tower4_t_start
entity_set tower4_t_start tag T
```

The server chooses randomly among enabled points matching the requested spawn
group. A spawnpoint's position is the spawn location; its `radius` and `size`
do not create a spawn area. If no matching tag is requested, generic enabled
spawnpoints can be used. The `spawnpoint.CT` and `spawnpoint.T` naming style
belongs to map GLB spawn-node tags; for editor entities, use the entity's
`tag` property as shown above.

## Editing JSON while the game is running

The live workflow is:

1. Load Zombie Tower 4.
2. Run `entity_visibility on`.
3. Create or adjust an entity with commands; each successful mutation
   automatically writes `config/maps/zombietower4.json`.
4. Edit that JSON externally and save it when you want to change values
   directly.
5. The client and dedicated server poll the active map config and reload a
   valid change without restarting. The server poll is bounded to at most
   once per 250 milliseconds; the client uses its normal config polling loop.
6. The debug marker updates from the newly loaded entity data.

Force a reload with:

```text
entity_reload
entity_list
```

Malformed JSON emits a reload-failure diagnostic and retains the last valid
entity state. It does not apply half of a broken edit.

The directions are explicit:

```text
terminal mutation -> in-memory registry + JSON file automatically
entity_save       -> manual JSON retry/explicit save
JSON file edit    -> live registry after polling, or entity_reload
```

The editor now writes JSON after every successful authoring mutation. This
means `entity_add`, movement, property changes, deletion, and visibility
changes no longer require a separate save command. Direct external JSON edits
still remain supported and are applied through the normal reload path.

## Current gameplay scope

All six entity types can currently be authored, saved, reloaded, and shown as
debug geometry. The current authoritative consumers are:

- `monster_zone`: live player-radius activation and configured NPC spawning,
  including role selection, cooldown, maximum-alive, and one-shot rules.
- `checkpoint`: live one-time run-progress and party-respawn recording.
- `damage_volume`: live environment damage to players and living NPCs inside
  an explicit sphere or box.
- `boss_trigger`: live one-time boss activation, progression lock, boss spawn,
  and unlock-on-boss-death.
- `spawnpoint`: live selection of player spawn positions, including authored
  group tags.

`pickup` is currently an authoring/visualization entity. Its fields are loaded
and saved, but its map-entity gameplay consumer is a future slice and must not
be assumed from the JSON shape alone. Random interior spawning for
`monster_zone` is also a desired behavior not yet provided by the current
implementation.

## Safe editing checklist

```text
entity_visibility on
entity_list
entity_select <id>
entity_info
entity_set_position <id> <x> <y> <z>
entity_save                 (optional manual retry/explicit save)
entity_reload
entity_list
```

Keep IDs stable, edit only the active map file, and validate JSON before a
multiplayer test. Build success does not prove that the command, reload,
server activation, or visible marker was observed in a live session.
