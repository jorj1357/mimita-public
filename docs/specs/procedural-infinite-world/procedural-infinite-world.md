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
