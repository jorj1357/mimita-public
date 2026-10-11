# Entity Shape Examples

Time UTC: 2026-10-10T23:14:21.706Z
Branch: current working tree; no commit created.

## Changes

Expanded `docs/specs/entity-editor/entity-editor.md` to document the current
entity-shape contract:

- Explicit supported values are `sphere` and `box`.
- `sphere` uses `radius`.
- `box` uses full axis-aligned `size x y z` dimensions centered at `position`.
- Empty `shape` preserves legacy inference from `size`.
- Added copy-paste `entity_set` examples for switching between sphere and box.
- Documented that `checkpoint`, `boss_trigger`, and `damage_volume` honor the
  explicit shape, while `monster_zone` currently uses radius-only activation.
- Updated the damage-volume documentation to reflect its implemented player/NPC
  environment-damage behavior.
- Added current `monsterRole` and `checkpointIndex` property descriptions.

## Source evidence

The documentation was checked against the current implementation in:

- `src/gamemode/map-config.h/.cpp` (`MapEntity::shape` and
  `mapEntityContainsPoint`).
- `src/terminal/map-entity-commands.cpp` (`entity_set ... shape`).
- `src/network/server-gamemode.cpp` (damage-volume and monster-zone consumers).
- `src/debug/debug-visuals-scene.cpp` (shape-specific marker rendering).

## Validation

- `git diff --check`: passed.
- No build or runtime run was needed because this session changed only the
  documentation; the referenced implementation and prior runtime evidence
  were inspected rather than modified.
- Existing unrelated worktree changes and untracked changelogs were preserved.
