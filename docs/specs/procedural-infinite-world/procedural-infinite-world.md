9 7 2026 1712 jorj todo - explain the inf dng slr  infinite world gneration , and like
maek it like minecraft
or 
how it emerges naturallt from simple rules undernaeath
but we can also make  specific cells in blender and then repeat those over and over 

AND ENSURE THIS MODE WOkrs with all the other imeplemtnations like portal gun, works with networking everhting must be client/server sided there cant be somethign that onl works for me as the  client that is bad etc.

and perofmance focus on perofmance , what if i have 999 plauers inthis server and all are generating chunks. need chunk queue? or each client themselves gets the seed for the world, thats cool, each client can generate the world itself and predict the world's generation instnatly like that, some kinda authorization or validation, but then u can hav a zillionplauerrs all conencted to 1 server chatting with each other and then each clieint sends the new chunks the made to the servr like in a queue, and even cooler: display what chunk the  sever is workign on right now

## Vehicle Arena sequencing (2026-11-04)

Procedural maps are not a prerequisite for the first Vehicle Arena. First
deliver and human-test one stable, authored arena with kickoff positions, ball
spawn, walls, and goal volumes. Once that mode is reliable across clients,
prioritize live map editing and connected authored modules, then reuse those
tools for duels, Payload routes, and other generated courses. Any later
procedural arena must preserve server/client authority and the performance,
networking, and deterministic chunk requirements in this specification. The
arena itself is specified in `../gamemodes/vehicle-arena.md`.

## Server authority, connector contact, and spawn anchors — 2026-11-07

- The worldgen profile is resolved from the active map ID. A map named
  `infinite_cramped` loads `config/worldgen/infinite_cramped.json`; it must not
  be silently replaced by the legacy `connected_world` radius.
- `infiniteworld list` lists named worlds from the gamemode map list.
  `infiniteworld status` reports map, seed, anchor, radius, desired/pending/
  loaded/rejected/resolved counts, and rendered chunks. `infiniteworld load
  <world>` requests the existing host-controlled map change; a request is not
  proof of server acceptance.
- Neighboring chunks connect only when opposite connectors are on the expected
  side and their positions are within the sum of their per-connector radii
  (default 0.5 m). The journal records distance and radii for every match or
  miss.
- Chunk templates may author typed spawn anchors: `human`, `npc`, or `any`.
  Loading a generated cell registers those anchors for player/NPC spawn and
  respawn. Spawn anchors are separate from monster-zone wave spawning.
- The server sends a canonical world descriptor (map, seed, generator version,
  dependency identity, descriptor hash) with map changes. Clients log the
  received descriptor as `world-stream.identity`.
- The server builds an initial authoritative chunk-collision and spawn-anchor
  footprint from the authored templates. Live per-cell server streaming around
  moving actors, full wall collision authority beyond the initial footprint,
  and measured 240 FPS acceptance remain required follow-ups.
- A map/GLB with no authored spawn point must still join: the server finds the
  highest walkable surface under the map-bounds center and logs
  `map.spawn-fallback`; an ungrounded fallback is reported, not hidden.

## RPG world editor: authored chunks and infinite zones — 2026-11-05

World projects may combine authored finite regions with procedurally generated
zones. The author chooses whether generation applies to the whole world or only
to named zones. The in-game project browser and viewport editor both expose
zone rules; the author need not travel to a zone to change its generation data.

Reusable chunks and structures have stable IDs and explicit connection points.
Doorway/corridor connectors declare the shape, orientation, and compatibility
tags a neighboring chunk must satisfy. The generator may only connect
compatible points; it must report a missing compatible neighbor instead of
creating impassable or disconnected geometry. Blender-authored structures may
be repeated as packages without duplicating their source asset.

Every zone version pins a seed, generator version, rule tree, and chunk
dependencies. Clients may quickly preview nearby geometry from those inputs,
but the authoritative server confirms the canonical chunk/layer identity and
accepted world edits. A mismatched generator version cannot be treated as
deterministic agreement. Generation work uses bounded queues/budgets and must
not run unbounded work in a fixed simulation tick.

Generation can be activated by entering a configured region, including a room
whose contents recursively contain more rooms. Recursive/fractal or
non-Euclidean designs must assign stable room/layer/portal identities so
players can rendezvous in the same apparent space and leave it deterministically.
The current editor phase does not claim arbitrary-player-count throughput or
unbounded generation performance; those need measured server/client budgets.

The RPG editor and collaboration contracts are owned by
[`../entity-editor/entity-editor.md`](../entity-editor/entity-editor.md) and
[`../world-projects/world-projects.md`](../world-projects/world-projects.md).

## Infinite World: Cramped authoring trial (2026-11-06)

The `connected_world` mode is displayed as **Infinite World**. Its first named
map is `infinite_cramped`, selected as **Cramped**. Dev launch mode 52 joins this
map. In the game terminal, use `infiniteworld list`, `infiniteworld status`, and
`infiniteworld load cramped`. Loading requests the existing host-controlled map
change; the request is not proof that the server accepted it. New named worlds
must be added to the mode's `maps` list, with matching map asset, map config,
and worldgen profile under the `infinite_<name>` ID.

Cramped's map config contains five editor-authored chunks named `chunk1` through
`chunk5`, and its worldgen profile sets 10 m cells, X/Y repetition, no Z
repetition, and a radius of 10 cells in each horizontal direction. The
neighborhood therefore has up to 441 loaded cells. Installs are limited to
two chunks per render setup pass. All five current chunks have matching flat
floors and four horizontal doorway connectors; wall collision and server
authoritative per-cell identity are not yet implemented.

In the native Map Editor, use the toolbar's **Add Chunk**, **Add Block**,
**Add Connector**, **Preview Connections**, **Save**, and **Reload** buttons.
With a chunk preview active, Add Block creates a chunk part. The outliner lists
parts and connectors; selecting one previews the containing chunk. Positive
axis handles move objects, and negative axis endpoint handles resize selected
blocks or chunk parts. The properties panel has exact position and size fields;
press Enter to apply a field. `Ctrl+Z` and `Ctrl+Y` undo and redo edits.
Equivalent terminal commands are:

```text
mapeditor 1
mapeditor add chunk chunk6
mapeditor chunk_preview chunk6
mapeditor add chunk_part box
mapeditor set_size 2 0.25 2
mapeditor add connector east
mapeditor preview_connections
mapeditor save
mapeditor reload
mapeditor 0
```

`mapeditor add block` creates a map-level block; the toolbar Add Block targets
the active chunk preview. Use `worldconnector preview <chunk> <connector>` for
one connector's compatibility report and `worldconnector connect` for explicit
links. A missing-compatible-neighbor report must be resolved before treating
the authored chunk as connected. The map editor's save/reload remains subject
to live runtime verification.

### Earlier connected-world trial evidence

The earlier dev mode 52 trial launched with `python devscripts/dev-loop.py --launch-mode 52 --request launch`.
Its `connected_world` map enables whole-world generation and names the
`connected_world` generator profile. The profile uses 10 m X/Y cells, a
10 × 10 m floor chunk, X/Y repetition with Z fixed, and a two-cell loaded
radius. The grid and spawn start at X/Y origin (0, 0), with Z=0 as the
floor surface. The small base-map floor supports initial spawning. The map
contains `chunk_a`, `chunk2`, and `chunk3` as three 10 m floor variants. Four
connectors on each tile sit at local (±5, 0, 0) and (0, ±5, 0),
so neighboring floors meet at their edges. The client removes cells outside
the loaded radius and installs at most two new cells per render setup pass.

Use `worldstream status` to inspect the loaded chunk count and `worldstream
clear` to clear and regenerate it. For a real movement trial, aim with
`agent.aim <x> <y> <z>`, then use `agent.move 1 0 <ticks>`; this holds ordinary
forward input for fixed simulation ticks. For example, the dev loop accepts
`python devscripts/dev-loop.py --command "agent.aim 300 0 2"` and
`python devscripts/dev-loop.py --command "agent.move 1 0 800"`. One move
command accepts at most 3600 ticks.
Read the running client's `world-stream.anchor`, `world-stream.update`,
`world-stream.connector.matched`, and `world-stream.chunk.applied` events in
`events.jsonl`. A trial must report player X, cell X, loaded/pending counts,
installation stalls, server tick windows, and client frame timing separately.

To author another floor variant in the live 3D Map Editor, use its chunk part
and connector controls, save the map, reload it, and verify the new chunk ID
appears in streamed cell events. The corresponding terminal commands are
`mapeditor 1`, `mapeditor chunk_preview <id>`, `mapeditor save`, and
`mapeditor reload`. The viewport exposes each part and connector as a
selectable object whose position can be changed with the editor gizmo.
For a new library chunk, use
`worldchunk add <id>`, `worldchunk part <id> floor floor 0 0 -0.25 10 10 0.5
<material>`, and four `worldconnector add` calls with east/west/north/south
positions at ±5 m. `worldconnector preview <id> <connector>` reports
compatible neighbors. `worldchunk save <id>` writes the reusable library
entry; `worldmode chunks connected_world <id> ...` adds library references.
See [world-edit-commands.md](../world-projects/world-edit-commands.md) for
exact command signatures and map save/reload controls.

For another tile that connects on every horizontal edge, enter these commands
in the game terminal while the connected-world map is loaded:

```text
worldchunk add chunk4
worldchunk part chunk4 floor floor 0 0 -0.25 10 10 0.5 stonev2
worldconnector add chunk4 east_4 east doorway 5 0 0 bidirectional
worldconnector add chunk4 west_4 west doorway -5 0 0 bidirectional
worldconnector add chunk4 north_4 north doorway 0 5 0 bidirectional
worldconnector add chunk4 south_4 south doorway 0 -5 0 bidirectional
worldconnector preview chunk4 east_4
mapeditor 1
mapeditor chunk_preview chunk4
mapeditor save
mapeditor reload
mapeditor 0
```

Matching all four edge positions and compatible connector roles allows the
stream to pick each cell by its seed and coordinates without recursing back
to the origin. It keeps the resolved-cell cache bounded to the loaded radius.
Restricted links, mismatched tags, and non-edge connector positions use the
graph resolver and require a separate long-distance budget before indefinite
travel can be claimed for those designs.

The performance target is a fixed 60 Hz server tick and at least 240 rendered
frames per second during movement. Record one-second frame windows and maximum
frame times; a high average does not prove every frame met 4.17 ms.

The flat-grid trial gives the server a fixed-tick support plane when all
authored chunks are matching, unrotated 10 m floors at the same height. It
does not yet authorize per-cell chunk identity or non-floor collision. Server
authoritative chunk identity and general generated-chunk collision, background generation, strict
240 FPS per-frame performance, and visual editor acceptance still require
implementation and runtime proof. The mode must not be described as a
multiplayer-authoritative infinite world until those paths are complete.

## Infinite World quick reference (2026-11-07)

### Enter and inspect

```text
python devscripts/dev-loop.py --request launch --launch-mode 52
infiniteworld list
infiniteworld status
infiniteworld load cramped
worldstream status
worldstream render instancing
worldstream render vbo
worldstream clear
```

`infiniteworld load <name>` requests a host map change. It does not prove the
server accepted the request.

### Files

```text
config/gamemodes/connected_world.json      named world list / display name
config/maps/infinite_cramped.json          authored chunks, connectors, spawns
config/worldgen/infinite_cramped.json      seed-independent grid/profile
assets/maps/infinite_cramped.glb           optional chunk GLB source
config/mapeditor.json                      editor scales, layout, alpha
config/gui/map-editor.json                 toolbar/button rects
```

### Create a 10 x 10 m floor chunk

Terminal:

```text
worldchunk add chunk4
worldchunk part chunk4 floor floor 0 0 -0.25 10 10 0.5 floorv2
worldconnector add chunk4 east_4 east doorway 5 0 0 bidirectional
worldconnector add chunk4 west_4 west doorway -5 0 0 bidirectional
worldconnector add chunk4 north_4 north doorway 0 5 0 bidirectional
worldconnector add chunk4 south_4 south doorway 0 -5 0 bidirectional
worldconnector preview chunk4 east_4
mapeditor 1
mapeditor chunk_preview chunk4
mapeditor save
mapeditor reload
mapeditor 0
```

`worldchunk part` arguments are `<chunk> <partId> <kind> <x> <y> <z> <sx> <sy>
<sz> [material]`. Sizes are full meters, not half extents.

In the Map Editor:

```text
mapeditor add chunk chunk6
mapeditor chunk_preview chunk6
mapeditor add chunk_part floor
mapeditor select chunk6/floor2
mapeditor set_position 0 0 -0.25
mapeditor set_size 10 10 0.5
```

### Change a part texture/material

```text
mapeditor chunk_part_material chunk6/floor2 stonev2
mapeditor chunk_part_uv chunk6/floor2 2 2
worldchunk material chunk6 floor2 all stonev2
worldchunk face chunk6 floor2 +z floorv2
worldchunk uv chunk6 floor2 +z 2 2
```

Materials are IDs from `config/materials.json`.

### Add a block to a chunk

```text
mapeditor 1
mapeditor chunk_preview chunk6
mapeditor add chunk_part wall
mapeditor select chunk6/wall2
mapeditor set_position 0 0 1
mapeditor set_size 4 0.5 3
mapeditor set_rotation 0 0 30
mapeditor chunk_part_material chunk6/wall2 brickv2
mapeditor save
mapeditor reload
```

`mapeditor add block` creates a map-level block. `mapeditor add chunk_part`
creates a part inside the previewed chunk.

### Add/connect another chunk

```text
mapeditor add chunk chunk7
mapeditor chunk_preview chunk7
mapeditor select chunk7/north_chunk7
mapeditor set_position 0 5 0
mapeditor set_scale 0.5 0.5 0.5
mapeditor chunk_connector_field chunk7/north_chunk7 role bidirectional
mapeditor chunk_connector_field chunk7/north_chunk7 orientation doorway
mapeditor chunk_connector_field chunk7/north_chunk7 tag flat
mapeditor preview_connections
worldconnector preview chunk7 north_chunk7
worldconnector connect chunk6 east_chunk6 chunk7 north_chunk7
worldconnector disconnect chunk6 east_chunk6 chunk7 north_chunk7
mapeditor save
mapeditor reload
```

A connection requires the expected opposite side and a position within the two
connectors' combined radius. `set_scale <r> <r> <r>` on a selected connector
sets its radius from the X value. Position is chunk-local.

### Spawn anchors

```text
mapeditor chunk_preview chunk6
mapeditor add chunk_spawn human
mapeditor select chunk6/spawn1
mapeditor set_position 0 0 1
mapeditor set_scale 0.75 0.75 0.75
mapeditor save
```

The first spawn gets `spawn1`; later spawns use `spawn2`, `spawn3`, and so on.

Allowed classes are `human`, `npc`, and `any`. Selecting a spawn row shows its
class, radius, chunk, and preview state.

### Grid and radius

```text
worldmode cell infinite_cramped 10 10 10
worldmode size infinite_cramped 10 10 4
worldmode radius infinite_cramped 10 10 0
worldmode grid infinite_cramped xy
worldmode spacing infinite_cramped connector
worldmode save infinite_cramped
```

```text
worldzone grid infinite_zone xy
worldzone cell infinite_zone 10 10 10
worldzone size infinite_zone 10 10 4
worldzone axes infinite_zone 1 1 0
worldzone radius infinite_zone 10 10 0
worldzone origin infinite_zone 0 0 0
worldzone spacing infinite_zone connector
worldzone mode infinite_zone connected_world
```

### Selection, rename, transforms, attachments

```text
mapeditor select <id>
mapeditor select_kind block|entity|part|connector|spawn|all
mapeditor rename My Name
mapeditor set_position <x> <y> <z>
mapeditor set_rotation <x> <y> <z>
mapeditor set_scale <x> <y> <z>
mapeditor nudge <dx> <dy> <dz>
mapeditor attach
mapeditor attachment_anchor <0|1>
mapeditor attachment_set <mass> <density> <strength>
mapeditor attachment_remove
```

In the viewport: left-click select, left-drag empty space box-select,
Shift/Ctrl modify selection, F2 rename, right-click outliner object attachment
menu.

### Verify

```text
events: world-stream.identity
        world-stream.profile
        world-stream.update
        world-stream.connector.matched
        world-stream.connector.missing
        world-stream.chunk.applied
        world-stream.server.cells
        world-stream.spawn.registered
        map-editor.pick
        map-editor.rename.apply
        map-editor.transform.apply
        map-editor.connection.preview
```

A valid run shows identical `descriptor_hash` on server and clients, the same
chunk ID in the same cell, connected connectors within radius, and loaded cells
bounded by the configured radius.
