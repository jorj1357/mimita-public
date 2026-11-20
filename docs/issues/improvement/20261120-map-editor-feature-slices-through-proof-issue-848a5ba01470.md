# issue-848a5ba01470: Map editor feature slices through proof

## Human report (verbatim, immutable)

> 


## User quote

### 2026-11-20T14:30:40.548Z — context-bus

Via context-bus; supports Map editor environment and traversal slice:

> skybox/fog/lighting editing; teleporters/area effects; ladders - can we do this

## Context

### 2026-11-20T14:30:40.930Z — context-bus

reproduction: Open a creator map in the editor and exercise environment controls, teleporter/area triggers, ladder volumes, and face-drag editing.
expected_behavior: Each authored control persists in the project and applies to the live map without a separate tool or restart.
acceptance: Pinned editor and Play Test events prove environment, teleporter/area, ladder/climb, face-drag, and rendered/collision results.
paths: ['src/mapeditor', 'src/gamemode', 'src/render', 'src/input', 'docs/specs/entity-editor', 'docs/specs/procedural-infinite-world']
dependencies: []
risk: Environment and traversal edits can desynchronize render, collision, and server authority.
proof_expectation: Editor apply events plus live Play Test render/collision/physics evidence.

## Context

### 2026-11-20T14:30:55.806Z — context-bus

evidence: {'source': 'user request 2026-11-20', 'known': 'LightingConfig and map fog exist; teleporter/ladder/face-drag are not implemented'}
