# General gamemode manager implementation

Time: 2026-09-09T12:13:56Z
Branch: `8292026stash`
HEAD before this session's final documentation write: `e10a476`

## Scope

Implemented the approved general gamemode-manager plan. The active server
owner is now `server-gamemode`, shared FFA/TDM/Bomb Tag paths retain NPC
participants, active PRE_MATCH emission was removed, `modepick` was removed,
and `modelist`, `modestart`, and `modestartnow` have distinct listing/start
semantics. Mode switches defer the new mode until a five-second RESULTS phase.

Automatic community map selection now reads only
`config/gamemode-good-maps.json`; explicit map selection remains outside that
restriction. Gamemode HUD mode titles and optional team score presentation no
longer contain hardcoded mode-name/TDM branches; the active presentation is
selected by `config/gui/gamemode-meta-gui.json`.

## Exact files and changes

- `src/network/server-gamemode.cpp` and
  `src/network/server-gamemode.h`: generalized the server owner/API names;
  added explicit weapon-set tracking, pending live-switch state, shared NPC
  participant assignment, direct countdown handling, and results-to-next-mode
  handoff. The fallback rule is now GUI/runtime selection first, gamemode JSON
  `weapon_set_id` last.
- `src/network/server.cpp:214-219,270`: automatic community pool now returns
  `GamemodeMapPool::instance().list()` and loads
  `config/gamemode-good-maps.json`.
- `src/engine/engine-tick-ui-overlays.cpp:584-596`: mode title is generated
  generically from the mode ID and score rendering is controlled by the JSON
  element's presence.
- `src/terminal/debug-commands.cpp` and `src/network/server-packet-chat.cpp`:
  removed `modepick`; retained listing-only `modelist` and start/switch
  commands `modestart` and `modestartnow`.
- `config/gui/help-menu.json:184`: documented the current command names.
- `docs/specs/gamemodes/gamemodes.md`: documented general ownership,
  2026-09-09 map-pool policy, lifecycle, command semantics, live switching,
  and weapon precedence.
- `docs/architecture/code-ownership/code-ownership.md`: documented that
  shared lifecycle ownership must use general gamemode/participant names.
- `docs/architecture/json-configuration/json-configuration.md:57`: migrated
  the game-mode configuration table to `gamemode-good-maps.json`.
- `docs/regressions/2026-09-09/duel-specific-gamemode-ownership-REG.md`:
  appended the requested architecture regression in the current regression
  format; it remains ATTEMPTED FIX pending live confirmation.

The repository already contained unrelated/pre-existing work in
`AGENTS.md`, the untracked draft changelog
`docs/changelog/2026-09-09/20260909_120408-agents-possible-draft.md`, and the
user/concurrent map-pool rename state. Those files were preserved and are not
claimed by this entry.

## Old and new ownership contract

Old active contract:

```cpp
ServerDuelState& serverDuelState();
void serverDuelTick(/* shared player/NPC lifecycle */);
```

New active contract:

```cpp
ServerGamemodeState& serverGamemodeState();
void serverGamemodeTick(/* shared player/NPC gamemode lifecycle */);
```

Old automatic map source:

```cpp
const MapCatalogResult catalog = scanMapCatalog();
```

New automatic map source:

```cpp
return GamemodeMapPool::instance().list();
```

## Documents and skills used

Read and followed `docs/ROUTER.md`, the routed gamemode, GUI, JSON
configuration, networking, regression, code-ownership, task-completion, and
build documents, plus `docs/skills/spec-behavior-review-v1.md` and
`docs/skills/documentation-checker-v1.md`.

## Validation

- `python -m json.tool config/gamemode-good-maps.json`: valid.
- `python -m json.tool config/gui/gamemode-meta-gui.json`: valid.
- Repository search found no active `modepick`, `server-duel`,
  `ServerDuel`, or `serverDuel` references. `DUEL_PHASE_PRE_MATCH` remains
  only as a wire-compatibility enum annotated as never emitted.
- `git diff --check`: passed; only normal line-ending conversion warnings
  were reported.
- `python build_agent.py`: `Status: SUCCESS`, compiled 3 objects, linked the
  canonical `C:\mimita-priv-v8\mimita.exe` at 2026-09-09 08:13 local time.
- Headless server smoke test reached map loading, collision setup, NPC spawn,
  UDP bind, and ICE initialization, then exited via the requested timeout.
  ICE gather reported a connectivity timeout, so this is not multiplayer
  acceptance evidence.

## Remaining human review

Two-client/live acceptance is still required for FFA NPC scoring and
leaderboards, GUI-selected weapon precedence across respawns and packets,
visible `3, 2, 1, GO!!!`, live `modestart`/`modestartnow` switching with the
five-second results view and 15-second intermission, hot reload of all
gamemode HUD elements, and automatic map rotation versus explicit map
exceptions. Invalid/unloadable entries in the configured pool should also be
observed in a live server log.
