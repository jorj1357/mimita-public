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

## Connected-world flat-grid trial (2026-11-06)

Launch dev mode 52 with `python devscripts/dev-loop.py --launch-mode 52 --request launch`.
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
