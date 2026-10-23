# Payload and Zombie Tower map editor blocker

- Result: PASS_WITH_HUMAN_REVIEW
- Canonical timestamp: 2026-10-23T04:13:13Z
- Display time: 2026-10-23 00:13:13 EDT, America/New_York
- Branch: `20261011zt`
- HEAD: `cbfa4b0df4e56b6f217c45dbe986b49df599b50a` (no commit created)

## Task

Explain current open gameplay issues and recommend priorities, while recording
the player's map-authoring blocker for Payload and Zombie Tower.

## Pre-existing edits

The working tree already contained unrelated changes in source, configuration,
runtime diagnostics, changelogs, regressions, and `docs/specs/open-gameplay-issues.md`.
Those changes were preserved. The issue document had extensive prior edits; this
session changed only its existing map-authoring blocker row and matching index
row. No source, map, or configuration files were changed.

## Documents and focused review

Read `AGENTS.md`, `docs/ROUTER.md`, `docs/skills/documentation-checker-v1.md`,
`docs/skills/spec-behavior-review-v1.md`,
`docs/architecture/time-and-formatting/time-and-formatting.md`,
`docs/operations/task-completion/task-completion.md`,
`docs/specs/open-gameplay-issues.md`,
`docs/specs/entity-editor/entity-editor.md`,
`docs/specs/gamemodes/payload.md`, and
`docs/specs/gamemodes/zombie-tower/zombie-tower.md`.

The router-listed `docs/doc-review-09-03-2026.md` is absent in this checkout.
Documentation review found the editor currently saves authored map entities to
map JSON, while drag selection, copy/paste, static block editing, and Payload
route editing are not yet described as supported editor features. Payload's
route/map-data contract remains incomplete. Zombie Tower map length has not
been independently measured. Lua or a Roblox Studio-like scripting environment
is a longer-term idea, not a prerequisite for a useful first editor slice.

## Change

### `docs/specs/open-gameplay-issues.md:80`

Before:

> `| **Blocker — Zombie Tower map authoring and spawn-volume editing** | The player believes the tower may not be long enough and reports that it needs many specifically placed zombie spawn volumes. Adding them by repeating \`entity_add\` commands and editing JSON to change them is too cumbersome. They want a better map editor where spawn volumes can be selected, copied/pasted, and moved by clicking and dragging. | Human-reported blocker to continuing Zombie Tower map work; tower length has not been measured or confirmed as the cause. Current Entity Editor documents command-based creation and JSON save/edit flows, including \`monster_zone\`; it does not document drag handles or copy/paste. Investigate the actual map length and authoring workflow separately. Define editing behavior for selecting, dragging, duplicating, assigning unique IDs, and saving volumes before implementing the editor improvement. | [\`entity-editor/entity-editor.md\`](entity-editor/entity-editor.md), [\`gamemodes/zombie-tower/zombie-tower.md\`](gamemodes/zombie-tower/zombie-tower.md) |`

After:

> `| **Blocker — map authoring for Payload and Zombie Tower** | The player considers the current authoring workflow a blocker: placing Payload cart routes, Zombie Tower monster spawn volumes, and map blocks is too hard through repeated commands and JSON edits. They want a better in-game map/entity editor to place, select, move, duplicate, and remove map content, then save those edits to the map. A future Lua interpreter or Roblox Studio-like creation environment is an idea they may want to start toward. | Human-reported blocker to building these maps and modes. The current Entity Editor documents command-based map-entity creation/editing and JSON save/reload, but not viewport drag editing, copy/paste, or editing static map blocks/geometry. Payload's cart route/map-data contract is not fully specified. First define and build a small editor slice for persistent map entities and route points; investigate the actual Zombie Tower playable length and authoring flow separately. Treat general block editing and Lua/studio-like scripting as later steps after the saveable editor foundation works. | [\`entity-editor/entity-editor.md\`](entity-editor/entity-editor.md), [\`gamemodes/zombie-tower/zombie-tower.md\`](gamemodes/zombie-tower/zombie-tower.md), [\`gamemodes/payload.md\`](gamemodes/payload.md) |`

### `docs/specs/open-gameplay-issues.md:130`

Before:

> `| Zombie Tower map authoring is a reported blocker | The player believes the tower may be too short and says many specific zombie spawn volumes must be placed. Repeating commands and editing JSON makes this slow; desired editor improvements are click-and-drag movement and copy/paste for spawn volumes. Map length and the exact editor interaction remain to be verified/defined. | [\`entity-editor/entity-editor.md\`](entity-editor/entity-editor.md), [\`gamemodes/zombie-tower/zombie-tower.md\`](gamemodes/zombie-tower/zombie-tower.md) |`

After:

> `| Map authoring for Payload and Zombie Tower is a reported blocker | Placing cart routes, monster spawn volumes, and map blocks through commands and JSON is too difficult. The player wants an in-game editor to place, move, duplicate, remove, and save map content. A Lua/Studio-like environment is a longer-term idea; map length and exact editing behavior still need investigation/definition. | [\`entity-editor/entity-editor.md\`](entity-editor/entity-editor.md), [\`gamemodes/zombie-tower/zombie-tower.md\`](gamemodes/zombie-tower/zombie-tower.md), [\`gamemodes/payload.md\`](gamemodes/payload.md) |`

Reason: capture the user's stated blocker across both modes and record a staged
direction that starts with saving/editing map entities and route points, leaving
general geometry editing and scripting for later.

## Validation and review

- `git diff --check -- docs/specs/open-gameplay-issues.md`: passed; Git emitted
  its existing LF-to-CRLF working-copy warning only.
- Documentation-only task: no build, test, or runtime scenario was run.
- The blocker is recorded as a human report, not a confirmed map-length or
  editor-root-cause diagnosis. Human review is still needed to define the first
  editor interaction slice and measure Zombie Tower's playable route.
