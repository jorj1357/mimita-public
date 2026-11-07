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
overrides currently persist position only. This follows the 2026-11-01 user
direction to tune rotations in the editor without repeatedly editing JSON.

Blocks and chunk parts also have editable `size [x] [y] [z]` fields. Press Enter
to apply positive dimensions, or drag the negative axis endpoint handles to
resize in the 3D viewport. The positive axis handles continue to move the
selection. A resize participates in undo/redo.

### Chunk primitive authoring (2026-11-05)

Map-authored primitive chunks appear in the outliner as
`chunk_part <chunkId>/<partId>` and
`chunk_connector <chunkId>/<connectorId>` rows. Selecting a row places a
non-colliding preview at the editor camera when that chunk has no visible
instance. The world preview draws part wire boxes and connector side markers,
and the existing gizmo moves the selected part or connector in chunk-local
space.

The properties panel edits part position, material, and UV scale. Material and
UV changes refresh already-loaded streamed templates without rebuilding chunk
collision. Structural changes (position, size, collision, connectors, or part
add/remove) mark the generation signature dirty so streamed chunks and their
per-chunk collision proxies are regenerated. Terminal equivalents:

```text
mapeditor chunk_preview <chunkId>
mapeditor chunk_part_material <chunkId>/<partId> <material>
mapeditor chunk_part_uv <chunkId>/<partId> <u> <v>
mapeditor chunk_connector_field <chunkId>/<connectorId> <side|role|orientation|tag> <value>
```

`worldchunk face|material|uv` and `worldconnector connect|disconnect|preview`
use the same owners. Primitive face UVs default to one repeat per world unit;
`uv_scale` and `face_uv_scale` provide per-part and per-face overrides.

The editor toolbar exposes Add Block, Add Chunk, Add Connector, Preview
Connections, Save, and Reload. Add Chunk creates a 10 × 10 m floor and four
directional connectors. Add Block creates a primitive chunk part while a chunk
preview is active, otherwise a map-level block. Preview Connections reports
every connector in the active chunk, including missing matches. Terminal
equivalents include `mapeditor add chunk <id>`, `mapeditor add chunk_part
<box|floor|wall>`, `mapeditor add connector <east|west|north|south>`,
`mapeditor preview_connections`, and `mapeditor set_size <x> <y> <z>`.

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

### GLB material assignment (future; data layer exists)

Map destruction is currently disabled by default; see
`docs/specs/destructible-world/destructible-world.md` section 72. This section
records the Map Editor work needed before it is re-enabled.

Destructible map materials must be authored without editing the source GLB in
Blender. The Map Editor extends the existing GLB override layer with material
and walkability assignment.

Current data layer:

- `MapGLBObject.materialName` exists.
- `MapGLBObjectOverride.material` exists and `config/maps/<mapId>.json`
  `glb_objects[].material` is parsed and applied.
- The editor does not yet expose material in its properties panel.
- The editor save path does not yet write `glb_objects[].material`.
- The editor does not yet classify or visually mark walkable floors/slopes.

Desired editor behavior:

- material selector for selected GLB nodes,
- multi-select apply from the outliner,
- persist `glb_objects[].material` through the same undo/autosave path as
  position edits,
- preserve material when a node is moved,
- "mark selected as walkable floor/slope/foundation" action,
- material/walkability colors (for example foundation green, concrete gray),
- bulk filters: horizontal, vertical, walkable surface, bottom-height range,
  size range, name contains, current material, spatial bounds,
- later: orthographic top/front/side views, marquee/box selection, user folders,
  and "select all similar".

Resolution order:

```text
per-node glb_objects[].material
    -> optional future material_rules
    -> destructible-world default heuristic
```

Floor and slope classification must reuse the shared walkable-slope rule
(`config/collision.json` `walkableSlopeDot`) so the editor and runtime agree on
what counts as walkable ground. Walkable floors and ramps default to
`foundation`; explicit per-node material wins.

The client and dedicated server resolve the same material from the same map
JSON. Removing an override restores the next resolution level.

Map destruction rules, material profiles, foundation values, and reset behavior
are owned by `docs/specs/destructible-world/destructible-world.md` sections 71
and 72.

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

### Authored surface behavior — design intake, not implemented

The editor needs a future surface-properties slice for authored blocks and
supported loaded map objects. The requested controls are:

- **Step-up / climbable:** allow walking forward up very steep authored faces,
  including an 89-degree example. Whether the option can climb vertical or
  overhanging faces (90 degrees or greater), and whether it is per object, per
  face, or both, remains undecided.
- **Pass-through:** let selected authored objects permit actors or other
  gameplay objects through them. This is a gameplay-collision property and
  remains independent from visible alpha.
- **Visual alpha:** use `1.0` for fully opaque and `0.0` for fully transparent.
  This concerns rendered geometry/materials; it is independent from collision.

These are desired editor capabilities, not current runtime behavior. Existing
`block_wire_alpha` only controls editor wireframe presentation. Keep the global
movement step-up and slope rules in the movement spec as the default behavior;
define any per-surface override there and in the collision contract before
implementation. Client and server behavior must match. The related design
questions and initial acceptance cases are tracked in
[`open-gameplay-issues.md`](../open-gameplay-issues.md#deferred-design-work).

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
| `mapeditor` | `mapeditor route_bonus <se`c>` | Set checkpoint time bonus. |
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
| `entity_visibility` | `entity_visibility <on\|off>` | Local master switch for all debug visibility. Off by default. |
| `entity_visibility_actor_capsules` | `entity_visibility_actor_capsules <on\|off>` | Show actor-pair solver capsules through walls, with an accurate capsule body and hemispherical caps. |
| `entity_visibility_actor_hitboxes` | `entity_visibility_actor_hitboxes <on\|off>` | Show final body-part and held-weapon collision hitboxes through walls. |
| `entity_visibility_weapon_hitboxes` | `entity_visibility_weapon_hitboxes <on\|off>` | Show active weapon hitboxes through walls. |
| `entity_visibility_npc_positions` | `entity_visibility_npc_positions <on\|off>` | Show NPC position/navigation markers through walls. |
| `entity_visibility_map_objects` | `entity_visibility_map_objects <on\|off>` | Show authored map entities and constraints. |
| `entity_save` | `entity_save` | Write the current registry to the active map JSON. |
| `entity_reload` | `entity_reload` | Reload the active map JSON; invalid JSON keeps the last valid state. |

### Local viewer visibility

`entity_visibility 1|0` is the local viewer’s master switch. It starts off and
is not saved across restarts. The category commands start enabled, so turning
the master on shows every category until a category is turned off. Categories
are independent and may be toggled while the master is off; they take effect
when the master is on.

Visibility is presentation-only. It never mutates `entity.visible`, saves map
JSON, sends a packet, or changes another client’s view. Collision shapes are
drawn with depth testing disabled so the requested capsules and hitboxes are
visible through walls. `entity_set <id> visible <0|1>` remains the separate
authored-data control for one map entity.

### Collision debug shapes

The collision categories answer different questions and must be labeled:

- `entity_visibility_actor_capsules` shows the actor-pair solver shape from
  `config/actor-collision.json`. It is not the actor’s static-world hitbox.
- `entity_visibility_actor_hitboxes` shows the final body-part and held-weapon
  collision meshes used against static world geometry.
- `entity_visibility_weapon_hitboxes` shows active weapon contact shapes.
- `entity_visibility_npc_positions` shows NPC roots and navigation markers.

`drawCapsuleWire()` and `drawCapsuleApprox()` are legacy, separate
visualizations. They must be consolidated into one accurate capsule renderer
that draws the cylinder body and both hemispherical caps. A capsule debug draw
must show the complete shape the solver tests, not a cylinder approximation.
The actor-hitbox view must show final animated body/weapon shapes so a tall
monster's legs, arms, and held weapon can be compared directly with world
geometry.

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

## Map Editor interaction overhaul (2026-11-07)

- Viewport selection uses a click threshold: a press/release without 3-4 px
  movement selects (or Shift-toggles) without starting a transform. Selected
  gizmo handles take priority over objects underneath them: pressing a handle
  starts a transform of the current selection even when another object is under
  the cursor.
- Left-drag on empty viewport space is a selection box. Shift adds and Ctrl
  toggles. Dragging an object still moves it.
- F2 starts display-name editing for the selected outliner object. Stable IDs
  do not change. `name` is stored in map JSON; search matches both name and ID.
- The properties panel exposes editable position, rotation, and scale for
  blocks, chunk parts, entities, vehicles, connectors, spawn points, bomb
  sites, routes, and the payload cart where those values exist.
- Outliner scrollbars support thumb drag and track paging in addition to wheel
  scrolling.
- The toolbar/panel layout is defined in `config/gui/map-editor.json`; button
  rects and text scale come from that file rather than hardcoded positions.
- `config/mapeditor.json` defines per-category overlay alpha for entities,
  blocks, chunk parts, connectors, spawn points, GLB objects, and bomb sites.
- Right-clicking an outliner object opens a context menu with Add Attachment,
  Anchor, Unanchor, and Remove Attachment. Attachments are authored records with
  stable IDs, parent/child references, anchored state, mass, density, and
  material strength. Anchored children follow parent moves in the editor.
- Selecting a chunk spawn point shows its allowed actor class
  (`human`, `npc`, `any`), radius, owning chunk, and preview state.

Each interaction writes a bounded `events.jsonl` event:
`map-editor.pick`, `map-editor.click.action`, `map-editor.box_select`,
`map-editor.rename.apply`, `map-editor.transform.apply`,
`map-editor.scroll`/`ui.scroll.drag`, `map-editor.context_menu`,
`map-editor.attachment.added`, and `map-editor.spawn.inspect`.

## Live module boundary and editor evidence (2026-11-07)

Map editor behavior is owned by the reloadable `map-editor` module specified in
[`../hotreload/hotreload.md`](../hotreload/hotreload.md) section 5. The host keeps
the `MapConfigRegistry` document, dirty/saved revisions, JSON save/backup,
collision/geometry refresh, window/input, renderer/device lifetimes, and stable
object handles. The module receives versioned plain-data inputs and asks host
services to apply edits; no `MapEditor*`, registry reference, GLFW window,
camera, STL container, or module-owned pointer crosses the module ABI.

The first implemented module path handles property-panel position edits. A
successful edit emits `map-editor.module.position` from the module and
`map-editor.transform.apply` from the host, then the existing debounced
`map-editor.autosaved`. Reload emits `map-editor.reload.result`. Every record
identifies the module generation where applicable. Saving `config/maps/<map>.json`
externally and reloading still works as data; reload failure keeps the previous
valid document.

Authored blocks support per-face materials through `faces[6]`. Empty entries
fall back to `material`. The controlled map
`config/maps/hot_render_proof.json` provides a six-face box. Use
`worldrender audit_faces proof_box` to record each face's normal, winding,
material, submitted batch range, and culling expectation. The audit is
diagnostic evidence; it does not replace human visual review, and a face or
operation that was not reached is `NOT_COVERED`.

Map-editor panel, toolbar, outliner, properties, context menu, box selection,
row colors, and help presentation are defined in
`config/gui/map-editor.json`; `config/mapeditor.json` remains the behavior and
overlay-alpha owner. Successful module or shader hot reloads show a
dark-green/green notification with the full edited path and generation; failures
show a red notification with the failing path and reason. These notices are
client/editor presentation only.

The `map-editor` module now owns drag-delta projection and gizmo axis
hit-distance math. `mapeditor drag_probe` is a bounded diagnostic that records
`map-editor.drag-probe` with the computed delta, picked axis, and module
generation. Host code still polls input, mutates the document, records undo, and
refreshes collision/geometry. Selection command behavior for `select`,
`select_kind`, `select_route`, and `selectall` is parsed by the module and
applied through a host selection service; selection storage remains host-owned.
Clipboard, undo/redo, save/reload/autosave, focus, nudge, transform fields,
bomb-site radius, and rename command parsing also live in the module and call
host-owned document services. `mapeditor drag_probe` is diagnostics-only; real
mouse-drag acceptance remains separate.

### Cursor-space and click-priority diagnostics

Editor input now maps the raw GLFW cursor into the renderer scene space once and
uses that mapped cursor for box selection, outliner/UI hit tests, click
thresholds, and context-menu placement. Mode 6
(`BorderlessFullFrameStretched`, 1024×768 scene on a desktop-sized window) must
behave like windowed mode. `map-editor.cursor.map` records raw/window/framebuffer/
scene/UI sizes and the mapped cursor on mode change or at most once per second.
`map-editor.handle.pick` records cursor, object under cursor, handle axis,
selection count, and the chosen action on click. Selected gizmo handles take
priority over objects underneath them so a handle press reliably starts a
selection transform. Gizmo endpoint projection for interaction uses unclamped
screen projection so a long handle remains pickable and draggable even when its
far endpoint is off-screen. `map-editor.gizmo.hover` records hover-axis changes.

When the `map-editor` module is loaded, cursor mapping and click resolution are
requested from the module through `EVENT_EDITOR_INPUT` and
`EVENT_EDITOR_CLICK`. The module returns the mapped scene cursor and a click
action (`handle drag`, `select`, `toggle`, `click candidate`, or `box select`);
the host still performs picking and applies the action. Selection IDs are
serialized in the module state blob and restored on activation through host
selection services. `map-editor.state.export`/`map-editor.state.import` record
the selection count and blob size.

## Bomb-site zones

Bomb sites are authored map data owned by `MapConfigRegistry` and persisted in
`config/maps/<map>.json` as `bomb_sites`. They are not a second entity type.
The map editor can create, select, move, and delete sites:

- `mapeditor add bombsite [id] [radius]` creates a site at the editor camera
  look point and selects it.
- Sites appear in the outliner and respond to the shared select/nudge/
  set_position/set_radius/delete actions.
- The editor draws each site as a labeled circular usable area and highlights
  the selected site.
- `mapeditor save` persists the site list; invalid JSON keeps the last valid
  config.
- The terminal `site_debug` command remains available for `show`, `hide`,
  `select`, `move`, `print`, and `save`.

The current authored shape is:

```json
"bomb_sites": [
  { "id": "A", "position": [0, 0, 0], "radius": 4.0, "visible_debug": true }
]
```

When the local viewer enables Entity Visibility and the map-object category is
enabled, bomb-site zones are visible by default as labeled circular regions.
Their visibility does not require selecting a site or enabling a second
bomb-site-specific debug command. The master visibility switch remains off by
default; this requirement applies when the viewer turns it on.

Entering a site circle makes the bomb interaction available; it never plants
automatically. Planting and defusing require the mode's selected interaction
(hold or repeated taps) while the actor remains inside the site or interaction
range. The gamemode objective's `site_group` label is not a second lookup
authority.

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

## Reusable Blender object library and assemblies

The editor should let a creator place objects from the shared runtime library
under `assets/objects/library/<asset-id>/`. Asset packaging and provenance are
defined by the [Asset Management](../../operations/asset-management/asset-management.md)
specification. A map stores an asset ID and instance settings; it does not
copy the mesh into map JSON.

For a placed object, the properties panel should support:

- position, rotation, and scale;
- static or dynamic physical behavior;
- collision enabled/disabled and the asset's supported collision choices;
- mass or material density using the shared physical-object rules;
- stable selection and an undoable transform/property edit.

Creators should be able to select multiple objects and connect them with a
shared physical constraint. A rigid welded attachment uses the shared
`Fixed` constraint. Connections have stable IDs and can be inspected, edited,
or removed from the editor. The editor must not implement its own physics or
attachment solver.

The map save stores asset references, per-instance physical overrides, and
constraint records. On reload, the editor reports a missing asset or
constraint reference and preserves the authored data for repair rather than
silently deleting it. Server-owned live map changes use the existing
authorized editor request path; writing a client's local JSON alone does not
modify a remote server's map.

This capability is a later editor slice. The current viewport's displayed
GLB scale/rotation values are not evidence that those overrides can already be
edited or saved. Acceptance requires placing an asset, editing its transform
and physical properties, attaching it to another object, saving/reloading,
and confirming the same assembly is used by the live server.

## Future workflow: live Blender-to-game map editing

Creators want to edit a selected map in Blender and see validated changes in
the game without manually repeating export, asset selection, and reload steps.
The intended workflow may watch the Blender source, the exported GLB, or both,
then request a safe map reload after a complete valid export. This is a deferred
workflow idea, not a current capability or implementation authorization.

Before implementation, define how Blender produces the game asset, how a
complete save/export is detected, how geometry and map data are validated, how
the game avoids loading a partial asset, how live reload errors are shown, and
how local edits reach an authorized remote server. Keep this workflow connected
to the in-game editor and shared asset-management rules.

## Vehicle placement and the vehicle builder (2026-11-03)

The map editor places vehicle instances. It does not author arbitrary vehicle
geometry as map blocks.

Map-authored vehicle data lives in the map JSON `vehicles` array:

```json
"vehicles": [
  {
    "id": "vehicle_1",
    "template": "car_sedan",
    "position": [12.0, 4.0, -31.0],
    "yaw": 0.0,
    "enabled": true
  }
]
```

The viewport editor can add, select, move, delete, and spawn these placements
through the shared selection, gizmo, undo, and save paths. A vehicle placement
is authored data; a spawned vehicle is a runtime object and is never serialized
as a second map entry.

The vehicle builder edits a reusable template in `config/vehicles/<id>.json`.
It is a later slice on the same editor tools, after the first Vehicle Arena
milestone:

- primitive parts (`box`, `slope`, `sphere`, `cylinder`),
- mass, density, and center of mass,
- wheel hub, radius, width, spin/steer/suspension axes,
- seats and named attachments.

Wheels stay on the analytic vehicle path and are edited as wheel records, not
ordinary body parts. The editor shows wheel hubs and dimensions so authors can
see what they are tuning. Arbitrary vertex, edge, or face editing is out of
scope.

The first editor slice exposes wheel editing through commands:

```text
mapeditor vehicle_wheel <index> hub <x> <y> <z>
mapeditor vehicle_wheel <index> radius <m>
mapeditor vehicle_wheel <index> width <m>
```

### Vehicle Arena map priority

The first Vehicle Arena milestone uses a stable, authored arena map with
authored kickoff positions, ball spawn, and goal volumes. Do not make live
map editing, connected modules, or procedural generation a dependency of the
first playable mode. After the authored arena passes multiplayer and human
playtesting acceptance, extend map editing for arena iteration and connected
modules; see `../procedural-infinite-world/procedural-infinite-world.md` for
the later procedural-world direction. Arena rules and acceptance are owned by
`../gamemodes/vehicle-arena.md`.

### Arena objects, boost pads, and curve authoring (2026-11-05)

The editor must author a complete playable arena without Blender. New authored
object kinds:

- `BallSpawn` with position and ball physics overrides,
- `GoalVolume` with team, center, half extents or radius, facing, and enabled,
- `KickoffPoint` with team, position, and yaw,
- `BoostPad` with kind (`small`/`big`), position, radius, recharge ticks, and
  enabled. Pad recharge is shown as a world-space timer.

Curves use cubic Bezier control points with editable tangent handles, matching a
Blender-style curve workflow. Commands add, move, delete, insert, and close
points and handles; edits use the shared undo/autosave path and are logged as
`mapeditor.curve.*` events. A curve stores radius, cross-section mode (`wall`,
`ribbon`, or `tube`), and segment count.

Curve and primitive geometry is sampled into the shared triangle render and
collision meshes. The first implementation is triangle-only: there is no
analytic quarter-sphere collider. Wheel raycasts may use the existing smooth
seam/vertex normals so dense tessellation does not feel faceted. Collision
triangle counts and validation are logged.

`mapeditor bake_glb` optionally writes `assets/maps/<mapId>.glb` from authored
geometry for tooling and sharing. `config/maps/<mapId>.json` remains
authoritative; baking never overwrites creator data automatically.

## World projects and collaborative live editing — 2026-11-05

The RPG editor extends the existing map/entity editor rather than creating a
second editing owner. Today the editor mutates the process-local
`MapConfigRegistry` and saves `config/maps/<mapId>.json`. That local path is not
the collaborative project/version system specified by
[`../world-projects/world-projects.md`](../world-projects/world-projects.md).
Do not treat editor-local JSON as a write to a remote host.

The creator project browser must open, fork, save, and select immutable world
versions. A public project is forkable; a private project remains private until
its owner changes it. Project versions include map data, chunk templates,
generation zones and rules, NPC placements and presets, interaction data,
world overrides, and pinned dependencies. Project and asset licenses remain
visible in the browser and attribution travels with every fork.

The shared editor runs against one authoritative host session. Clients submit
edit operations; the host validates and sequences them, then replicates accepted
changes to all editors. Each accepted edit has a revision/sequence and stable
operation identity. Clients detect gaps through acknowledgements/heartbeats,
request retained operations, and fall back to a host snapshot if needed. Merge
operations on independent properties; surface same-property conflicts for
author resolution. Each editor can undo their own last accepted operation;
the project owner can undo any operation. Undo appends a new inverse action.

The owner has all editor capabilities. The owner may grant or revoke named
capabilities for account IDs: geometry, chunks/generation, NPC authoring,
weapon overrides, interactions, undo, and publish. Validate each operation on
the host. The local client may preview its request but cannot save it as
authoritative before the host accepts it.

All clients keep rolling local recovery copies of the latest host-confirmed
state and their pending operations. A disconnected client may resume at the
last confirmed revision or continue as its own fork. The host saves the live
session as a recoverable draft/version at shutdown. Disconnects must not erase
confirmed edits.

### Chunks and infinite-generation zones

Creators can make reusable chunks/structures with explicit connection points
and compatibility tags such as doorway, corridor, or open edge. Validation
reports missing or incompatible required connections before publication.
Generation may apply to the whole world or only to authored zones. A zone stores
its seed, generator version, rule set, chunk/structure dependencies, and
boundary connections. Authors can edit a zone's generation rules from the
project browser without travelling to it, or while standing in the zone.

Clients may generate nearby geometry immediately from the host-approved seed
and generator version. The host remains authoritative for accepted generated
chunk identity and world mutations. A seed alone is insufficient if generator
code/data versions differ; mismatch requires the host's canonical chunk data
or a compatible generator package. Generation work must be queued/budgeted so
high movement speed cannot stall the fixed simulation tick.

Recursive rooms, fractals, portals, and non-Euclidean topology are future
extensions. Every generated location needs a stable world/zone/chunk/layer
identity so two clients can refer to the same recursive room and rejoin it.
Large-scale player-count and infinite-throughput claims require measured
performance evidence before acceptance.

### NPC, weapon, and interaction authoring

The editor places NPC spawns and edits reusable NPC presets, role/loadout,
movement/combat profile, appearance, animation, sounds, and world-specific
reward values. Editing a live NPC preset offers an explicit choice to apply to
existing NPCs or only future spawns. Existing actors retain identity; applying
changes occurs at a safe simulation boundary.

World weapon/NPC definitions inherit shared defaults and may pin or override
supported values in a world version. Editing an override must not silently
change other modes. NPC stats remain fixed across group sizes; more players
increase the number of attackers. NPC implementation continues to use the
shared player/NPC actor, movement, combat, animation, and damage owners.

World objects may declare a named shared interaction action, its prompt data,
range, and input mode. The initial input modes are `press` and `hold`; a hold
interaction also declares a tick duration and exposes progress. Press continues
to mean one activation on the F key-down edge. The shared action registry owns
the behavior; a map file cannot name arbitrary native code. The in-game editor
must let authors select these properties without editing raw JSON.

### Extensibility and live source workflow

Gameplay/editor presentation remains data-driven and hot-reloadable where the
existing GUI and animation specs require it. First specify the authoring
workflow and stable action/capability API; only then decide whether a Python
tool should author data or present editor controls. Do not embed Python
gameplay execution as a shortcut. Document sandboxed Lua as a future option,
including capability restrictions, server authority, resource quotas, and
deterministic failure behavior. Do not ship a script editor in the first
editor slice. C++ source inspection/forks are an advanced development path,
not executable content downloaded by joining a public world.

For native editor code, the target is one reloadable `map-editor` module that
owns editor-specific input interpretation, selection, editing tools, commands,
undo/redo policy, viewport/UI behavior, and editor mode policy. The module and
its first position-edit/save/reload proof are specified in
[`docs/specs/hotreload/hotreload.md`](../hotreload/hotreload.md), section 5.
`MapConfigRegistry`, the authored map/project document, file persistence,
window/input, renderer, geometry, and collision mechanisms remain host-owned
services. This describes the migration target; it does not claim the current
editor C++ source already reloads as a module.

### Infinite-world authoring quick start (2026-11-05)

Use the normal in-game editor and terminal surface. No separate tool is
required for the first loop:

1. Enter the editor with `mapeditor 1`.
2. Add built-in connector-aware chunk variants with `worldchunk defaults`.
   They are `chunk_straight`, `chunk_corner`, `chunk_cross`, and
   `chunk_deadend`, using the existing procedural room GLB.
3. To add your own primitive chunk:
   `worldchunk add <id>`
   `worldchunk part <id> floor floor 0 0 -0.25 10 10 0.5 floorv2`
   `worldchunk save <id>`
4. Add connection points to a chunk:
   `worldconnector add <chunkId> <connectorId> <side> <orientation> <x> <y> <z> <role> [tags...]`
   Sides are `east`, `west`, `north`, `south`, `up`, `down`; orientation
   describes the opening type, for example `doorway`. Empty tags are
   unconstrained. Connect or preview matches with:
   `worldconnector connect <chunkA> <connA> <chunkB> <connB>`
   `worldconnector preview <chunk> <connector>`
5. Create a generation zone:
   `worldzone add <id> <seed> <generatorVersion> <minX minY minZ> <maxX maxY maxZ> [chunkIds...]`
   If chunk IDs are omitted, the zone uses every authored chunk.
6. Verify deterministic identity:
   `worldzone hash <seed> <generatorVersion> <dependencyIdentity> <chunkX> <chunkZ>`
   The same inputs must produce the same hash; changing the generator version
   or dependencies must change it.
7. Walk into the zone. The client streams chunks asynchronously around the
   player and commits a bounded number per frame. The journal reports
   `world-stream.template.loaded`, `world-stream.chunk.applied`,
   `world-stream.commit`, and `world-stream.evict`.

The generator chooses chunk variants deterministically from
`seed + generatorVersion + dependencies + chunk coordinate`, then prefers
variants whose connectors match already-committed neighbors. Missing or
incompatible connections are reported rather than silently creating an
impassable result.

To persist the authored world, use `worldproject save` or the normal map
editor save path. Authored chunks, connectors, zones, NPC presets, weapon
overrides, and interaction definitions are part of the map/project data.

### Authoring acceptance

The initial collaborative editor slice must show two clients connected to one
host. One edits a block, NPC placement/preset, interaction mode, and chunk or
zone rule. Both clients must render the host-accepted values and report the
same revision. Force a sequence gap and verify replay, then exceed retained
history and verify snapshot recovery. Verify unauthorized edits are rejected,
same-property conflicts remain visible, own-edit undo works, and each process
recovers its last confirmed state after disconnect. These are runtime and
human-visible acceptance requirements; a build or local JSON save alone does
not satisfy them.
