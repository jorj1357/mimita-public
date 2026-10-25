2026 10 21 1602 
"Cart area: The cart gets a tiny origin point, and its push/block area is placed using settings measured from that point. Your starting size works out to about 3 m × 2.25 m × 1.5 m. If a defender overlaps that area, the cart stops. The exact shape and how it attaches to the origin are still open choices." - https://chatgpt.com/s/cx_6ad91a78bd7081918412eda904a64348  jorj convo  i was lik etop 1% or top 0.1% chat gpt users in  2025 i think  tht is crazy 

2026 10 21 1551 jorj todo o make it so funworld3 has  the spawns defined in the entitt enditor, not in the .glb  
dotn need to edit the.glb to make the map diffrnt

also we nede to make a map eidotr!!!!!!!!!!!!!!
so we 

can make it so like

psuedeo: 

maps\funworld3\funworld3.json
map.glb

the .json has like
what .glb or other files is used here
where are they 
what is rotation
is it destructbiel or not

so u can edit this sutff live too like
add remvove blocks
etc

but 
for now just make it so spawn works like in the json  eidting 
so i can override the spawn ebhaivor right now 

2026 10 20 1712 todo make it so 
spawn volumes
mosner spawn voluems hav ae max
also
a specific set of monsters that the spawn
todo define those paths? not sure whe or where or how right now but hmmm
ueah 
so that a spawn volume is done and then it doenst spawn antthing else just spawns the amount it spawns then thats done 

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

The Map Entity Editor is the terminal and command-level subset of the unified
map editor. The in-game Map Editor v1 viewport is the visual superset: it lists
and edits authored entities, blocks, payload route points, the payload cart
origin, and loaded GLB nodes. The `entity_*` commands remain available for
bounded scripting and for editing the entity subset without opening the
viewport.

All of it edits authored map data stored in:

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

## Map Editor v1 viewport mode (2026-10-23)

The focused viewport editor is entered and left with:

```text
mapeditor 1
mapeditor 0
mapeditor status
mapeditor save
mapeditor reload
mapeditor freeze <0|1>
```

`mapeditor 1` detaches the gameplay camera through the existing freecam path.
The player and NPC simulation are frozen by default. `mapeditor freeze 0`
leaves the detached editor camera active while the world simulation continues.

Controls:

```text
WASD + Q/E       freecam move (Shift fast, Ctrl slow)
RMB held         mouse look
RMB released     free cursor
LMB              select / drag
Shift+LMB        add or remove from selection
Ctrl+A           select all
Ctrl+C/V/X       copy / paste / cut
Ctrl+D           duplicate
Delete or X      delete selection
Ctrl+Z           undo
Ctrl+Shift+Z/Y   redo
Ctrl+S           save active map JSON (writes a backup)
autosave         debounced map JSON write after edits (no backup)
V                toggle vertex snap
G                toggle 1 m grid snap
F                frame selection from its bounds
Esc              clear selection/drag, then leave editor
PgUp/PgDn        scroll the outliner
wheel            scroll the outliner under the cursor
Ctrl+click row   add or remove that row from the selection
Shift+click row  select the range from the anchor row
Enter            apply the focused position field
```

The bottom help strip is intentionally removed. The only persistent hint is:

```text
Commands are in docs/specs/entity-editor/entity-editor.md
```

The editor selects entities, authored blocks, payload route points, the payload
cart origin, and loaded GLB nodes. Selection uses stable IDs, not pointers.
Runtime actors such as NPCs, players, projectiles, and physics props are not in
this list yet; the list covers authored map data and loaded GLB nodes. Dragging
mutates the in-memory map data through the viewport. Route, entity, and
cart-origin edits do not rebuild collision. GLB translations update collision
incrementally when the affected chunk set is small; map-spanning objects defer
the full rebuild until editor exit or `mapeditor rebuild_collision` so the drag
stays responsive. Explicit Ctrl+S writes a backup; debounced autosave updates
the active map JSON without creating a backup.

### Outliner, search, and properties (2026-10-25)

The outliner occupies the right side of the screen and is drawn in framebuffer
pixels. It has:

- an `OBJECTS <count>` header and a search field;
- a scrollbar inside the outliner, 14 px wide by default, inset 6 px from the
  panel edge;
- PgUp/PgDn and mouse-wheel scrolling;
- row colors by kind and a highlighted selected row with an outline;
- rows for cart origin, payload route points, GLB nodes, entities, and blocks.

Search matches case-insensitive ID, GLB node name, entity type, and kind words
such as `route`, `block`, `glb`, `entity`, and `cart`. Search results are the
same list rows used for clicking, Shift-range selection, and Ctrl-multi-select.

The properties panel below the list shows the primary selection:

```text
SELECTED  <kind> <id>
pos       [x] [y] [z]
rot       <x> <y> <z>       scale <x> <y> <z>
<type, name, material, radius, route flags, or GLB details>
```

The three position fields are editable. Click a field, type a number, and press
Enter to apply the position. The edit uses the same undo, collision refresh, and
autosave path as a handle drag. `Esc` cancels the text focus. Rotation and scale
are displayed for blocks, entities, routes, the cart origin, and GLB nodes;
editable rotation/scale overrides for GLB nodes are the next slice. GLB
overrides currently persist position only.

### Drag handles

The selection handle is a filled, camera-facing axis gizmo. The default values
live in `config/mapeditor.json`:

- `handle_length: 12.0` (about 5x the old 2.4 m)
- `handle_pick_pixels: 60.0`
- `handle_radius: 0.80` for the end sphere
- `handle_thickness: 0.48` for the shaft

All object types use the same handle size. Axis and plane drags are projected
through screen space: dragging toward the visible handle tip moves the object in
that axis's positive direction, regardless of camera pitch or roll. The
selection center moves all selected objects together; Ctrl-multi-select is the
intended way to move a whole payload route.

### Block wireframes

Authored block wireframes use `block_wire_alpha` from `config/mapeditor.json`,
default `0.5`. Selected blocks use full opacity. Set it to `1.0` to return to
the old fully opaque look.

Copy and paste are internal to the editor and report the affected IDs in the
terminal:

```text
[MAPEDIT] copied 2 objects: entity foo, block bar
[MAPEDIT] pasted 2 objects: entity foo_2, block bar_2
```

Undo/redo is an in-memory authored-data history. Ctrl+S is still the explicit
persistence action. Saves use a temporary file, validate the generated JSON,
copy the previous valid file to:

```text
config/maps/backups/<mapId>/<mapId>_<yyyymmdd_hhmmss>_save.json
```

and keep the newest 50 backups. Generated backups and editor autosaves are
untracked.

### Editor settings and autosave

`config/mapeditor.json` owns editor-only presentation and interaction values and
hot reloads:

```json
{
  "title_scale": 0.42,
  "body_scale": 0.30,
  "row_scale": 0.28,
  "help_scale": 0.24,
  "outliner_width": 380.0,
  "outliner_top": 112.0,
  "outliner_bottom_padding": 32.0,
  "row_height": 26.0,
  "properties_height": 156.0,
  "scrollbar_width": 14.0,
  "scrollbar_inset": 6.0,
  "handle_length": 12.0,
  "handle_pick_pixels": 60.0,
  "handle_radius": 0.80,
  "handle_thickness": 0.48,
  "block_wire_alpha": 0.5,
  "focus_min_distance": 1.5,
  "focus_padding": 1.35,
  "camera_speed_multiplier": 1.0,
  "autosave_delay_ms": 400,
  "label_mode": "selected"
}
```

`label_mode` is `all`, `selected`, `hovered`, or `off`. Font scales are
`title_scale`, `body_scale`, `row_scale`, and `help_scale`; the last one now
sizes the small documentation-path line. The editor also exposes
`mapeditor_camspeed <multiplier>`, `mapeditor select <id>`,
`mapeditor select_kind <kind>`, `mapeditor set_position <x> <y> <z>`,
`mapeditor nudge <dx> <dy> <dz>`, `mapeditor autosave`,
`mapeditor rebuild_collision`, and the cart/route authoring commands.

Debounced autosave writes the active map JSON after an idle delay; it does not
create a backup. Ctrl+S is the explicit save and still creates a backup. The
JSON write is atomic and the server's map-config poll reads it.

### GLB object overrides

Loaded GLB nodes can be translated and persisted without editing the GLB:

```json
"glb_objects": [
  { "id": "node:12", "name": "Cube.6450", "position": [12.0, 4.0, -31.0] }
]
```

`id` is the loader's stable `node:<index>` id and `name` is diagnostic. The
client and dedicated server apply the translation to that node's vertex/triangle
range. Removing the override restores the original GLB translation; undo,
cancel, and reload use the same path.

Position is the only persisted GLB property in this slice. The properties panel
also displays the GLB base rotation and scale for inspection. Editable GLB
rotation/scale overrides require storing base vertex positions and a delta
transform, and are the next slice. The old override matrix path remains intact
for loading existing files.

### Authored blocks

The viewport editor authors static box blocks in the same map JSON:

```json
"blocks": [
  {
    "id": "block_1",
    "shape": "box",
    "position": [12.0, 4.0, -31.0],
    "size": [4.0, 4.0, 2.0],
    "rotationEuler": [0.0, 0.0, 15.0],
    "material": "",
    "enabled": true,
    "tags": ["blockout"]
  }
]
```

These are appended to the loaded GLB world as render geometry and collision
triangles. The GLB remains the base map. The dedicated server applies the same
blocks to its headless world so player and NPC collision/navigation see them.
Arbitrary baked GLB triangles are not individually editable in v1.

### Payload route authoring

The editor can author ordered route points and a cart origin in map JSON:

```json
"payload": {
  "cart_origin": { "position": [0, 0, 0], "yaw": 0.0 },
  "interaction_box": {
    "size": [3.0, 2.25, 1.5],
    "offset": [0, 0, 0],
    "rotationEuler": [0, 0, 0]
  },
  "route": [
    {
      "id": "route_1",
      "position": [0, 0, 0],
      "radius": 2.0,
      "order": 0,
      "waitTicks": 0,
      "enabled": true,
      "checkpoint": true,
      "final": false,
      "time_bonus_seconds": 0.0
    }
  ]
}
```

Route points are selectable and draggable, and their world label shows `CP`,
`FINAL`, `OFF`, radius, wait ticks, and time bonus. With a route point selected,
the terminal authors the remaining fields:

```text
mapeditor route_checkpoint <0|1>
mapeditor route_final <0|1>
mapeditor route_enabled <0|1>
mapeditor route_radius <meters>
mapeditor route_bonus <seconds>
mapeditor route_wait <ticks>
mapeditor route_move <up|down>
mapeditor route_validate
```

### Select and move the whole payload path

The easiest path-editing workflow is:

```text
mapeditor select_route
mapeditor nudge 5 0 0
```

`select_route` is an alias for `select_kind route`; both select every route
point and nothing else. With the whole route selected, drag the single
selection-center handle, or use `mapeditor nudge <dx> <dy> <dz>` /
`mapeditor set_position <x> <y> <z>` for exact movement. Ctrl+click individual
outliner rows to build a custom subset, then drag or nudge the same way.

Add and remove points:

```text
mapeditor add route [id]     append a route point at the camera-forward point
mapeditor duplicate          duplicate the selected route point(s)
mapeditor delete             delete the selected route point(s)
mapeditor route_move up      move a selected point earlier in the route
mapeditor route_move down    move a selected point later in the route
```

Adding a route point renumbers the route and keeps the JSON order field in sync.
Deleting a route point renumbers the remaining points. Both operations use the
same undo/autosave path as a drag.

### Direct JSON authoring

`config/maps/<map>.json` remains authoritative and is safe to edit directly.
The live workflow is:

1. Save pending editor changes, or leave the editor with a clean state.
2. Stop editing in-game so autosave cannot overwrite the external file.
3. Edit `payload.route[]` or `payload.cart_origin` in the map JSON.
4. Save the file.
5. Run `mapeditor reload` or let the map-config poll pick it up.
6. Run `mapeditor route_validate` and inspect the route in the editor.

If the editor has unsaved changes and the file changes externally, reload first.
Otherwise the next autosave can replace the external edit with the in-memory
editor state. Autosave writes without a backup; Ctrl+S is the explicit save and
writes a backup.

`route_validate` reports duplicate ids, too few points, a cart-origin/route-start
mismatch, long or steep segments, a missing final flag, zero checkpoint bonuses,
and tiny radii. Ctrl+S saves and re-runs the same validation.

`radius` is the cart's world-space arrival sphere: entering it triggers the
point/checkpoint. `waitTicks` pauses the cart for that many server ticks when
the point is first entered. The Payload cart runtime contract remains in
[`../gamemodes/payload.md`](../gamemodes/payload.md).

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

## Map editor command reference

Run `mapeditor help` in the game for the compact list.

| Command | Usage | Purpose |
|---|---|---|
| `mapeditor` | `mapeditor` | Toggle the viewport editor. |
| `mapeditor` | `mapeditor <0\|1\|on\|off>` | Leave or enter the editor. |
| `mapeditor` | `mapeditor status` | Print active, map, dirty, freeze, and selection count. |
| `mapeditor` | `mapeditor save` | Save the active map JSON with a backup. |
| `mapeditor` | `mapeditor reload` | Reload the active map JSON. |
| `mapeditor` | `mapeditor freeze <0\|1>` | Keep the editor camera but continue world simulation. |
| `mapeditor` | `mapeditor autosave` | Force the debounced no-backup map save. |
| `mapeditor` | `mapeditor rebuild_collision` | Flush a deferred collision rebuild. |
| `mapeditor` | `mapeditor focus` | Frame the current selection. |
| `mapeditor` | `mapeditor add entity <type> [id]` | Add an authored entity. |
| `mapeditor` | `mapeditor add block [id]` | Add a 1 m authored block. |
| `mapeditor` | `mapeditor add route [id]` | Append a payload route point. |
| `mapeditor` | `mapeditor add cart` | Add the payload cart origin. |
| `mapeditor` | `mapeditor select <id>` | Select the first object with that ID. |
| `mapeditor` | `mapeditor select_kind <entity\|block\|route\|cart\|glb\|all>` | Replace the selection with one object kind. |
| `mapeditor` | `mapeditor select_route` | Select every payload route point. |
| `mapeditor` | `mapeditor selectall` | Select all authored entities, blocks, routes, and GLB nodes. |
| `mapeditor` | `mapeditor set_position <x> <y> <z>` | Move the primary selection to an exact position and apply the delta to the rest. |
| `mapeditor` | `mapeditor nudge <dx> <dy> <dz>` | Move the whole selection by a delta. |
| `mapeditor` | `mapeditor route_checkpoint <0\|1>` | Set the checkpoint flag on selected route points. |
| `mapeditor` | `mapeditor route_final <0\|1>` | Set the final flag. |
| `mapeditor` | `mapeditor route_enabled <0\|1>` | Enable or disable selected route points. |
| `mapeditor` | `mapeditor route_radius <m>` | Set arrival radius. |
| `mapeditor` | `mapeditor route_bonus <sec>` | Set checkpoint time bonus. |
| `mapeditor` | `mapeditor route_wait <ticks>` | Set arrival wait in 60 Hz ticks. |
| `mapeditor` | `mapeditor route_move <up\|down>` | Reorder selected route points. |
| `mapeditor` | `mapeditor route_validate` | Validate route ids, order, flags, distances, radii, and bonuses. |
| `mapeditor` | `mapeditor cart_origin_to_route_start` | Move the cart origin to the first enabled route point. |
| `mapeditor` | `mapeditor route_start_to_cart_origin` | Translate the whole route to the cart origin. |
| `mapeditor` | `mapeditor copy\|paste\|cut\|duplicate\|delete\|undo\|redo` | Clipboard and history actions. |
| `mapeditor_camspeed` | `mapeditor_camspeed <multiplier>` | Set camera speed relative to the configured default. |

`mapeditor selectall` does not select the cart origin; use `select_kind cart`
when the cart origin must move with the selection.

## Entity command reference

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
entity command      -> in-memory registry immediately
entity_save         -> JSON file
map editor edit     -> in-memory registry + debounced autosave
Ctrl+S in editor    -> JSON file with a backup
JSON file edit      -> live registry after polling, or entity_reload/mapeditor reload
```

`entity_*` commands do not write JSON after every command; use `entity_save` when
the current in-game state should become the authored file. The map editor is
different: viewport edits mark the registry dirty and the map editor autosaves
without a backup after `autosave_delay_ms`. Use Ctrl+S when a backup is wanted.

When editing JSON externally while the map editor holds unsaved changes, save or
reload first. A later autosave can otherwise replace the external edit with the
in-memory editor state.

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
respawn/map changes, and NPC spawn commands. It selects from the map's resolved
JSON spawnpoint list: imported GLB points with their JSON overrides plus
map-JSON-authored points. For a complete GLB-spawn manifest, disabled imported
points are excluded and the raw GLB list is not a fallback. Legacy maps without
a complete manifest may use raw GLB points for compatibility, then the
existing safe fallback. The server validates the selected position and applies
it through the existing authoritative player/NPC spawn lifecycle. Existing
checkpoint, explicit teleport, and mode-specific placement rules keep their
documented precedence.

`entity_add` / `entity_set_position` make the point available in the current
authoring session; `entity_save` persists it. A newly placed point affects the
next spawn, not actors already alive. For a remote server, the editor must send
an authorized edit request to the server; saving only the client's local map
JSON cannot change a different machine's spawn registry.

Acceptance must cover initial player join, player respawn, `npc_spawn`, NPC
wave/role spawns, team-tag filtering, disabled points, map-config reload,
complete GLB-spawn manifest with disabled raw nodes excluded, legacy-GLB
fallback for unsynchronized maps, safe-position rejection, and a remote
client/server edit.
This is a spec and implementation gap; runtime behavior was not reproduced in
this review.

## Desired feature: edit GLB spawn points through map JSON

**DECIDED direction:** A map's spawn nodes may originate inside its GLB, but
creators must be able to disable, move, or supplement those points through the
map's JSON and Entity Editor without exporting a new GLB. The Entity Editor's
map JSON is the editable override layer; the GLB remains the source geometry
and initial spawn-node source.

Provide an explicit import/synchronize action that reads every spawn node from
the selected map GLB and writes a complete spawnpoint manifest into
`config/maps/<active-map-id>.json`. This operation is explicit so loading a map
does not silently rewrite creator data. Each imported point records its stable
map-local ID, source GLB node identity, `enabled`, editable position and
orientation, and existing team/role/tag fields. Newly authored points are
marked as map-JSON points and have no GLB source identity. The manifest records
the source map asset identity/revision and whether all GLB spawn nodes were
successfully represented.

Reconcile rules:

1. On first import, create enabled JSON entries at the GLB spawn transforms.
2. On later imports of the same GLB identity, preserve all JSON edits. Add new
   GLB nodes with defaults; do not reset disabled flags or edited transforms.
3. If a previously referenced GLB node disappears, keep its JSON entry and
   mark it unresolved/orphaned with a diagnostic rather than deleting creator
   data silently.
4. A complete manifest is authoritative: the runtime selects only enabled,
   safe resolved JSON entries. A disabled imported point must never reappear
   through the raw-GLB fallback. Legacy maps without a complete manifest may
   continue using raw GLB points until synchronized.
5. `entity_set_position`, `entity_move_here`, enable/disable editing, save, and
   live reload update the JSON override and affect future server-selected
   spawns. They do not move actors already alive. The normal server-side safe
   spawn validation still applies.
6. To use one spawnpoint, disable the other entries; the standard selection
   policy then has only that eligible candidate. Tags/groups may further limit
   candidates by mode, team, or role.

Source identity should use a stable ID authored in GLB node metadata when
available. **OPEN:** the deterministic fallback for older GLBs without stable
node IDs; whether the GUI exposes synchronization, the import is invoked from
the asset pipeline, or both; and how a source-asset revision change is shown
to the creator before accepting reconciliation. The system must not silently
bind an old edit to a different spawn node because node ordering changed.

Illustrative map JSON entry shape (field names are a schema proposal; the
existing `MapConfigRegistry` parser must be inspected before implementation):

```json
{
  "id": "funworld3_spawn_01",
  "type": "spawnpoint",
  "source": "map_glb",
  "source_node_id": "spawn_01",
  "enabled": false,
  "position": [0.0, 0.0, 0.0],
  "rotation": [0.0, 0.0, 0.0]
}
```

Acceptance must show an imported eight-point GLB list in JSON, disable seven,
move the remaining point live, save/reload it, restart and rejoin using that
point, and verify the server does not fall back to any of the seven disabled
raw GLB points. Also verify a new JSON-authored spawnpoint, an unresolved GLB
source ID, map revision reconciliation, and remote server-authorized editing.

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
