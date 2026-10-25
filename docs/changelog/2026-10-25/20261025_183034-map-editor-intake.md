# Map editor usability and Payload cart placement intake

- Session time: 2026-10-25T18:30:34Z (2026-10-25 14:30:34 EDT,
  America/New_York).
- Branch: `20261011zt`.
- HEAD at session start: `cfae20a5` (`pre colision merging changes`).
- Result: `PASS_WITH_HUMAN_REVIEW` — documentation intake recorded; no
  implementation or runtime acceptance was requested or performed.

## Change

Updated `docs/specs/open-gameplay-issues.md`:

- Added “New intake — Payload map editor usability and cart placement” at line
  293 with the user-supplied 2026-10-25 14:28 EDT timestamp and screenshot
  context.
- Recorded object-list scrollbar/search, selected-row highlight, `F` focus at
  about one meter (noting that the current help strip already says `F frame`),
  larger editable font sizes, and the proposed `mapeditor_camspeed` multiplier
  command.
- Recorded the reported one-second-or-longer freezes for save, undo, and move
  separately from measured evidence; none was measured in this documentation
  task.
- Updated the existing “Blocker — map authoring for Payload and Zombie Tower”
  issue index row at line 364 to point to the specific intake.

There was no prior dedicated intake section. The prior open-issue row described
map authoring as a broad blocker; it now links the concrete editor details.
The modified document already contained an unrelated, uncommitted “Gamemode
settings masterlist and validation handoff” section at lines 180–292 before this
session. That content was preserved.

## Cart placement evidence

The intake records the cart placement confusion as an open product/editor
question, alongside source evidence:

- `src/gamemode/payload-runtime.cpp::payloadRuntimeReset` (line 147) resets
  route distance to zero.
- `payloadRoutePositionAtDistance` (line 177) returns `cartOrigin` at route
  distance zero; route points are sampled relative to the first route point.
- `src/gamemode/map-config.cpp::validatePayloadAuthoring` (line 863) warns to
  move the origin with the route start when the first point differs.
- The working-tree `config/maps/payload2.json` has `cart_origin.position` at
  line 114 and first route point `cp1` at line 150, with positions
  `[-234, -22, 1429]` and `[-326, -34, 1138]` respectively.

No runtime code or map configuration was changed. The player’s question about
whether the editor should align the origin automatically or initialize from
the first route point remains a decision for follow-up. Source inspection does
not establish live visual acceptance.

## Instructions, review, and validation

- Read `docs/ROUTER.md`, the Payload and Entity Editor specifications,
  `docs/architecture/time-and-formatting/time-and-formatting.md`,
  `docs/operations/task-completion/task-completion.md`,
  `docs/skills/documentation-checker-v1.md`, and
  `docs/skills/spec-behavior-review-v1.md`.
- The router names `docs/doc-review-09-03-2026.md` for documentation tasks,
  but that file is absent from the checkout; no substitute was assumed.
- Documentation check: new details are explicitly identified as reported,
  source-derived, or undecided; existing linked owner documents were checked
  and exist. The focused TODO phrase scan found only instruction examples and
  historical changelog references, not an actionable documentation TODO.
- `git diff --check -- docs/specs/open-gameplay-issues.md`: passed.
- Tests, build, dev-loop run, and visual/playtest acceptance: not applicable to
  this documentation-only request; no tests were added or run.

## Pre-existing working-tree changes

The following unrelated modified/untracked paths were present and left
untouched: `config/accounts/default.json`, `config/analytics.json`,
`config/collision-lod.json`, `config/maps/payload2.json`,
`src/config/collision-lod-config.cpp`, `src/config/collision-lod-config.h`,
`src/game/game-cli.cpp`, `src/map/map-authored-geometry.cpp`,
`src/map/map-loader-collision.cpp`, `src/map/map-loader-collision.h`,
`src/mapeditor/map-editor.cpp`, `src/network/server-world.cpp`,
`src/network/server.h`, `src/physics/physics-types.h`,
`src/world/world-gltf-loader.cpp`, `src/world/world.cpp`, `src/world/world.h`,
`src/map/map-static-union.cpp`, `src/map/map-static-union.h`, and
`docs/changelog/2026-10-25/20261025_141709-gamemode-settings-handoff.md`.

## Human review

The logged usability requests and freeze duration remain player reports. The
`F` outliner behavior and distance, editor font configuration owner, camera
speed persistence, freeze causes, and desired cart-origin initialization need
implementation planning and, where appropriate, live editor review.
