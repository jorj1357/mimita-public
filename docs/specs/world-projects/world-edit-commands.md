# World edit command reference

Terminal commands for the world/editor authoring surface. GUI/editor actions
that exist share these commands where practical; scenario and dev-loop
validation uses the same paths.

Coordinate note: X = right, Y = forward, Z = up. Chunk grids tile X/Y by
default (`plane: xy`); Z repetition is opt-in.

## `worldproject`

| Command | Effect |
|---|---|
| `worldproject list` | List local projects. |
| `worldproject status` | Show store root and current project. |
| `worldproject create <id> [title]` | Create a project from the current map. |
| `worldproject open <id> [version]` | Open a project working copy or version. |
| `worldproject save [title]` | Save working map as an immutable version. |
| `worldproject fork <source> <new-id> [version]` | Fork a version with lineage. |
| `worldproject versions <id>` | List versions. |
| `worldproject export <id> <dir> [version]` | Export a verified package. |
| `worldproject import <dir> [new-id]` | Import and verify a package. |
| `worldproject drafts <id>` | List recovery drafts. |
| `worldproject recover <id> <draft>` | Promote a draft to a version. |
| `worldproject migrate` | Migrate format-1 projects. |

Journal: `world-project.created`, `opened`, `version-saved`, `forked`,
`exported`, `imported`, `recovered`, `migrated`.

## `worldedit`

Local host-authoritative edit session (not yet networked).

| Command | Effect |
|---|---|
| `worldedit begin <project> <version>` | Start a session. |
| `worldedit grant <account> <cap>` | Grant a capability. |
| `worldedit revoke <account> <cap>` | Revoke a capability. |
| `worldedit set <account> <target> <id> <property> <type> <value...> [base=<rev>]` | Submit a typed set. |
| `worldedit get <target> <id> <property>` | Read current property. |
| `worldedit undo <account> <seq>` | Undo an accepted op. |
| `worldedit ack <account> <seq>` | Acknowledge a sequence. |
| `worldedit replay <after>` | Replay retained ops. |
| `worldedit snapshot` | Build a snapshot. |
| `worldedit status` | Session status. |

Capabilities: `geometry`, `chunks`, `npcs`, `weapons`, `interactions`, `undo`,
`publish`, `all`.

Targets: `entity`, `block`, `vehicle`, `bombsite`, `payload`, `npc_preset`,
`zone`, `interaction`, `world`.

Journal: `world-edit.begin`, `accepted`, `applied`, `rejected`, `conflict`,
`read`, `snapshot`.

## `worldmode`

Per-mode generator JSON in `config/worldgen/<mode>.json`.

| Command | Effect |
|---|---|
| `worldmode list` | List modes. |
| `worldmode ensure` | Create built-in modes and chunks. |
| `worldmode load <id>` | Load a mode. |
| `worldmode save <id>` | Persist after edits. |
| `worldmode grid <id> <xy|xz|yz|all>` | Set the tiling plane. |
| `worldmode cell <id> <x> <y> <z>` | Center-to-center cell size. |
| `worldmode size <id> <x> <y> <z>` | Chunk content size. |
| `worldmode radius <id> <x> <y> <z>` | Stream radius. |
| `worldmode spacing <id> <center|connector>` | Spacing mode. |
| `worldmode chunks <id> [chunk...]` | Set chunk library references. |
| `worldmode branch <id> <depth>` | Max branch depth. |
| `worldmode cycles <id> <0|1>` | Allow cycles. |

## `worldchunk`

Reusable chunk library in `config/worldgen/chunks/<id>.json`.

| Command | Effect |
|---|---|
| `worldchunk add <id>` | Add a chunk to the current map. |
| `worldchunk defaults` | Create built-in tunnel chunks. |
| `worldchunk list` | List chunk library files. |
| `worldchunk save <id>` | Save a chunk to the library. |
| `worldchunk part <chunk> <partId> <kind> <x> <y> <z> <sx> <sy> <sz> [material]` | Add a primitive part. |
| `worldchunk face <chunk> <partId> <+x|-x|+y|-y|+z|-z> <material>` | Set a per-face material. |

Part kinds: `box`, `wall`, `floor`, `ceiling`, `tunnel`, `doorway`, `prop`.

## `worldconnector`

| Command | Effect |
|---|---|
| `worldconnector add <chunk> <id> <side> <orientation> <x> <y> <z> <role> [tag...]` | Add a connector. |
| `worldconnector list <chunk>` | List connectors. |

Sides: `east`, `west`, `north`, `south`, `up`, `down`. Roles: `entry`, `exit`,
`bidirectional`, `none`.

## `worldzone`

| Command | Effect |
|---|---|
| `worldzone list` | List zones. |
| `worldzone add <id> <seed> <version>` | Add a zone. |
| `worldzone grid <id> <xy|xz|yz|all>` | Set tiling plane. |
| `worldzone axes <id> <x> <y> <z>` | Repeat axes (0/1). |
| `worldzone origin <id> <x> <y> <z>` | Grid origin. |
| `worldzone cell <id> <x> <y> <z>` | Cell size. |
| `worldzone size <id> <x> <y> <z>` | Chunk size. |
| `worldzone radius <id> <x> <y> <z>` | Stream radius. |
| `worldzone spacing <id> <center|connector>` | Spacing mode. |
| `worldzone mode <id> <modeId>` | Reference a mode profile. |

## `worldstream`

| Command | Effect |
|---|---|
| `worldstream status` | Show backend and chunk count. |
| `worldstream render instancing` | GPU-instancing backend. |
| `worldstream render vbo` | Per-chunk VBO backend. |
| `worldstream clear` | Remove all streamed chunks. |

## `worldinteraction`

| Command | Effect |
|---|---|
| `worldinteraction add <id> <action> <targetKind> <targetId> <prompt> <range> <press|hold> [holdTicks]` | Author an interaction. |
| `worldinteraction hold <id>` | Start a hold. |
| `worldinteraction release` | Release the active hold. |
| `worldinteraction list` | List authored interactions. |

## `mapeditor` world-authoring subset

- `mapeditor add chunk <geometry>` - add a chunk (GLB or primitive).
- `mapeditor add zone <seed> [id]` - add a zone around the player.
- `mapeditor chunk_connector <chunkId> <id> <side> <orientation> [tags...]`
- `mapeditor save`, `reload`, `undo`, `redo`.

Existing object commands (`add entity/block/vehicle/bombsite/route/cart`,
selection, nudge, set_position) continue to work for non-chunk authored data.
