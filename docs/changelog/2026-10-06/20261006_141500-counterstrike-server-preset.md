# Counter-Strike server preset and CT1 alias

Time: 2026-10-06 14:15:00 EDT
Branch: current working tree

## Summary

Implemented the first generalized JSON-defined local server preset and wired
the `CT1` terminal alias to it. Running `CT1` from the game's terminal now
loads the `counterstrike_1` preset, starts an in-process localhost server, and
queues the existing direct-connect path so the client enters gameplay.

## Exact changes

- `config/server-presets.json:1-15`: added the `presets` schema and the first
  `counterstrike_1` preset with `dust2cyberiav4`, `counterstrike`, startup NPCs
  disabled, Discord notification disabled, map rotation disabled, and host
  auto-join enabled.
- `config/command-aliases.json:3-8`: added
  `"CT1": "server_start_preset counterstrike_1"`.
- `src/network/server-presets.h:1-24`: added the generic `ServerPreset` data
  contract and loader declaration.
- `src/network/server-presets.cpp:1-94`: added validated JSON loading for
  named server presets with safe defaults and bounded numeric fields.
- `src/terminal/network-commands.cpp:358-419`: added the owner-level
  `server_start_preset <preset>` command. It starts the existing local listen
  server with the preset settings, queues the existing localhost connection,
  and switches to gameplay.

## Documents and focused review

Read and applied:

- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/architecture/terminal-commands/terminal-commands.md`
- `docs/architecture/json-configuration/json-configuration.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/terminal-command-checker-v1.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`

## Validation evidence

- `config/command-aliases.json` parsed successfully and resolved `CT1` to
  `server_start_preset counterstrike_1`.
- `config/server-presets.json` parsed successfully and reported map
  `dust2cyberiav4`, mode `counterstrike`, NPCs `false`, rotation `false`, and
  Discord notification `false`.
- Canonical build: `python build_agent.py` completed with `Status: SUCCESS`,
  return code `0`, and compiled the two changed C++ translation units before
  linking `mimita.exe`.
- `mimita.exe --versioninfo` completed successfully. The run wrote its
  authoritative journal to
  `logs/10-06-2026/20261006_141302/events.jsonl`.
- `git diff --check` showed no whitespace errors in the session-owned files;
  existing unrelated modified documentation retains pre-existing whitespace
  warnings.

## Human/runtime review still needed

The live game path was not claimed as complete in this session. Open the newly
built executable, open the in-game terminal from the main menu, run `CT1`, and
confirm visually that the client joins `dust2cyberiav4`, the Counter-Strike
round starts, no startup NPCs appear, and no unexpected Discord/map-rotation
behavior occurs. Inspect the live `events.jsonl` for the server-start and
direct-connect records during that run.

## Pre-existing edits preserved

All unrelated modified and untracked files shown by `git status` were left
untouched, including account/config changes, renderer/ragdoll changes,
Counter-Strike documentation edits, and the existing entity-editor directory.
