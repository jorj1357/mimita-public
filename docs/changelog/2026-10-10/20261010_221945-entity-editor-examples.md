# Entity Editor Examples and Current Behavior Clarification

Time UTC: 2026-10-10T22:19:45.312Z
Branch: current working tree; no commit created.

## Changes

- Expanded `docs/specs/entity-editor/entity-editor.md` with code-accurate
  definitions for `monster_zone`, `checkpoint`, `pickup`, `damage_volume`,
  `boss_trigger`, and `spawnpoint`.
- Added a type/consumer/property reference table and separate walkthroughs
  for a Zombie Tower encounter, checkpoint, pickup, damage volume, boss
  trigger, and player spawn points.
- Explicitly documented the current monster-zone behavior: player-radius
  activation and center-position NPC spawning. The requested random interior
  spawn behavior is identified as not implemented rather than implied by
  `size`.
- Corrected the gameplay-scope summary to distinguish live consumers from
  entities that are currently authoring/visualization only.

## Source evidence

- `src/gamemode/map-config.h/.cpp`: common entity fields, JSON load/save, and
  sphere/axis-aligned-box containment.
- `src/network/server-gamemode.cpp`: monster-zone activation, checkpoint and
  boss-trigger runtime, party-wipe respawn, and authored spawnpoint selection.
- `src/terminal/map-entity-commands.cpp`: accepted entity types and editable
  properties.
- `src/debug/debug-visuals-scene.cpp`: authored marker visibility/colors.

## Validation

- Documentation diff reviewed against the current command/property surface.
- `git diff --check` passed.
- No build or runtime validation was run because this session changed only
  Markdown documentation.
- The router-referenced `docs/doc-review-09-03-2026.md` is absent from this
  checkout; the available documentation-checker instructions were followed.

## Documentation TODOs observed

The repository-wide documentation scan found existing explanation/cleanup
TODOs, which were intentionally not edited as part of this focused change:

- `docs/architecture/player-npc-systems/player-npc-systems.md:1` — `todo explain`.
- `docs/architecture/player-npc-systems/npc-movement.md:1` — `todo - explain`.
- `docs/features/README.md:29` — `todo explain how this is like`.
- `docs/specs/collectibles/collectibles.md:1` — `todo explain`.
- `docs/specs/visuals/visuals.md:1` — `todo explain this more like`.
- `docs/specs/terminal/terminal.md:3` — `todo - explain`.
- `docs/specs/hotreload/hotreload.md:3` — `todo explain the direction...`.
- `docs/specs/gamemodes/zombie-tower.md:5302` — `todo - explain zombie tower`.

These remain separate documentation tasks and were not silently folded into
the entity-editor update.
