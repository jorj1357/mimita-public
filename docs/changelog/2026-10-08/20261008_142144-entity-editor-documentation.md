# Entity Editor Documentation

Time UTC: 2026-10-08T18:21:44.758Z
Branch: `afad20a-rebuild`
Commit: none created; local changes remain uncommitted.

## Changes

- Expanded `docs/specs/entity-editor/entity-editor.md` from an informal TODO
  into the current Map Entity Editor v0 reference.
- Documented the active-map rule for
  `config/maps/zombietower4.json`, all commands, all six accepted entity
  types, supported `entity_set` properties, JSON examples, save/reload rules,
  last-valid behavior, and current gameplay limits.
- Added the requested `entity_help` terminal command in
  `src/terminal/map-entity-commands.cpp:76-85` so the same command surface is
  discoverable in-game.

## Live behavior documented

The current implementation resolves the active map ID from the server map or
loaded map path. For Zombie Tower 4 this resolves to `zombietower4.json`.
Command edits update the in-memory registry immediately; `entity_save` writes
the JSON file; external valid JSON edits are detected by the active config
poller; and malformed JSON retains the last valid state. The document
explicitly warns that commands do not write the file until `entity_save`.

## Validation

BUILD: `mimita-20261008T1423-entity-editor-docs.exe` built successfully with
return code 0. The build compiled no additional translation units because the
command source was already compiled in the previous named build; the final
link completed successfully.

IDENTITY: `--versioninfo` passed and emitted
`EVENTS_JSONL_PATH=logs/10-08-2026/20261008_142247/events.jsonl` for the exact
final executable.

JSON: `config/maps/zombietower4.json` parsed successfully as JSON.

RUNTIME: No live terminal or visual acceptance was claimed. Native access to
the Windows game window remains unavailable in this environment, so an actual
in-game `entity_help`, spawnpoint placement, JSON edit, and visible reload
still need human verification.

## Preserved work

All unrelated pre-existing worktree changes were preserved and are not
attributed to this session. No regression record was created because no
human-confirmed regression was observed.
