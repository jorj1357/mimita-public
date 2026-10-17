# Melee launch, scenario result, and network evidence

- UTC: `2026-10-17T21:01:53Z`
- Scope: investigate `startmelee`, the extra NPCs, and missing runtime evidence;
  define the intended launch behavior and make the smallest command/log updates.
- Skills: `docs/skills/spec-behavior-review-v1.md` and
  `docs/skills/logging-checker-v1.md`.

## Findings from the existing runtime journal

- `logs/2026-10-17/20261017_204912/events.jsonl` shows that the initial client
  joined room `JLLZ33P`; `network.map-required` assigned player 1 and required
  map `cramped`. The first completed snapshots showed one remote NPC.
- That room's server launch arguments selected `--mode sandbox`. Dev-loop's
  `mode=1(current)` is a dev-loop launch profile/mode selector, not the active
  in-game mode.
- The first `startmelee` attempt produced `terminal.command.unknown`, but the
  scenario runner marked the command step executed when it saw delivery. A
  later attempt through the `modestart 14` alias did configure the server as
  Melee.
- `melee_start_v1.json` also issued `npc_spawn 1` despite the selected launch
  profile already starting one NPC. Repeated runs could accumulate additional
  NPCs. The scenario now avoids that second spawn and requires one remote NPC.
- The log shows server mode configuration and client snapshots, but not one
  compact event tying registered room, selected mode/map, startup NPC IDs,
  command acceptance, and connection transition together.

## Changes

- `startmelee` now has a disconnected branch using the existing server-preset
  launch/join path, with `melee_1` configured for Melee, map `cramped`, two
  player slots, and exactly one startup NPC. Connected users still request the
  server to switch the existing room to Melee.
- `melee` now routes to the named `startmelee` command rather than numeric mode
  14. Removed the `startmelee` numeric alias that bypassed the command handler.
- Runtime scenarios now wait for a matching `terminal.command.completed` event
  and fail on `terminal.command.unknown`; delivery alone no longer counts as
  execution.
- Added structured client connection-transition and client/server host-command
  result events, plus a dedicated-server room-registration event with mode,
  map, room code, capacity, and startup NPC IDs.
- Clarified the two `startmelee` paths, exactly-one-NPC first-match rule, and
  separate command/server/join evidence in the Melee and gamemode specs.

## Validation and remaining evidence

- `devscripts/dev-loop.py` parses successfully with Python's AST parser.
- `config/server-presets.json` and `devscripts/scenarios/melee_start_v1.json`
  parse as JSON. `config/command-aliases.json` is JSON-with-comments; its
  existing loader explicitly allows comments, so the standard-library strict
  JSON parser is not applicable to that file.
- No build or live rerun was performed. The current game client remains live;
  the canonical dev-loop build/launch action could replace it. The source
  changes therefore have not yet been compiled or confirmed in a fresh
  `events.jsonl` run.
- The connected-room scenario is improved but does not yet prove the
  disconnected start-and-join path. The Melee spec now requires both scenarios.
- The existing NPC diagnostics do not yet provide a complete, compact live
  actor roster at every join/spawn/despawn boundary. The room-registration
  event includes only startup NPC IDs. This remains a diagnostic gap.
- The server's actual Melee NPC reconciliation policy is not implemented by
  this change; the exact one-human/one-NPC requirement is specified, and the
  new offline preset starts one NPC. Existing generic servers with extra NPCs
  may still need cleanup when switching to Melee.
- No human visual/gameplay acceptance was performed.
