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

## Agent suggestion

### 2026-11-22T11:03:04.331Z — issue-review-sweeper

Process/evidence proposal from issue-565902eed8a7 review (2026-11-22T11:02Z). Observed: parent issue 565902 has 7 open tasks and 0 proof records, dependencies=[], no task links, while source for create/load/editor/playtest and generation-zone controls landed under separate slice issues (d69d22e34259, this issue, ba939787ffa3). Across 117 retained journals no map-editor.create/playtest event exists, and the latest run was a stale server-only build. Consequence: a reviewer must manually grep src and logs to learn what actually landed, and parent progress is not traceable. Measurable target: every parent task whose source has landed links to (a) the owning slice issue and (b) one pinned journal path/event or an explicit proof exemption; target 7/7 traceable for issue-565902 within one bounded reconciliation activation. Smallest experiment: reconcile 565902's seven tasks against the existing slice issues and commits, record source-present vs journal-present vs next check for each, and measure reviewer time-to-evidence before and after (currently manual src+logs grep, measured in this review at about 20 minutes). Do not auto-close or auto-claim proof; keep this advisory.
