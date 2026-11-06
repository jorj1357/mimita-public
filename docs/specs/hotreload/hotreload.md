// 2026-09-08 10:13 EST
/* purpose
* document current JSON reload and the current C++ hot-reload boundary
* identify the active owners, polling cadence, validation behavior, and commands
* distinguish proven reloadable presentation from remaining hardcoded UI paths
* DOES NOT define gameplay rules, network protocol behavior, or gamemode scoring
* DOES NOT claim visual acceptance merely because a file parsed successfully
* DOES NOT claim all C++ is live-reloadable today
*/

# MiMITA Hot Reload: Current State and Direction

## C++ file-save reload: current state and future mode 3

The current dev-loop watches compiler inputs, debounces changes, and
incrementally compiles and links changed source generations. It publishes a
new executable artifact but does not automatically replace code in an already
running game. A changed header can require recompiling many translation units;
"incremental" means only affected outputs are rebuilt, not necessarily one
file.

The current C++ hot-reload DLL is limited to the sources listed in
`build_game_dll.py`. `HotReloadSystem` watches that focused source set, invokes
the DLL build synchronously, validates the exported `GameAPI`, and loads the
candidate from a unique path. Arbitrary C++ files and their dependencies are
not covered.

Future mode 3 should make a saved file trigger a debounced background check in
the owning agent lane. It should compile affected translation units, link the
replaceable module, report errors to that lane, and activate a validated
generation only at a safe runtime boundary. Changes during a build should be
coalesced into one follow-up for the latest saved inputs. Unsaved keystrokes
should not start compilers. A compile or link failure must leave the last valid
runtime generation active.

The long-term target is every C++ edit, reached by migrating code behind stable
replaceable interfaces and defining persistent-state ownership, dependency
tracking, compatibility checks, and rollback. Until a file is covered by that
boundary, automatic compilation only proves it compiles; running it still
requires a complete executable build. This goal does not remove review or the
final stable build of the combined agent changes. Mode 3 is a direction, not a
current command or a claim that arbitrary C++ can be reloaded safely.

The agent build workflow and commands are documented in
`docs/specs/dev-loop/dev-loop.md`.

## JSON hot reload

Status recorded: 2026-09-08T14:13:15Z / 2026-09-08T10:13:15-04:00 (Eastern Daylight Time).

Status recorded: 2026-09-08T14:13:15Z / 2026-09-08T10:13:15-04:00 (Eastern Daylight Time).

This document describes the implementation currently present in the repository.
The GUI behavior specification remains authoritative: a GUI element's
presentation must be JSON-defined and hot reloadable. This document records
which paths currently satisfy that rule and which still need migration.

## Master gate and polling

The master switch is `config/hotreload.json`. The owner is
`src/engine/engine-tick-setup.cpp`.

1. `engine-tick-setup.cpp:53-75` reads the master gate once at startup.
2. A missing or malformed file defaults hot reload to enabled.
3. `engine-tick-setup.cpp:100-102` polls configuration every ten frames when
   the gate is enabled.
4. `engine-tick-setup.cpp:104-147` calls the individual reload owners,
   including `GamemodeRegistry::pollReload()` and
   `GuiLayoutManager::pollReload()`.

At approximately 60 FPS, the ten-frame throttle means a changed GUI file is
normally noticed within about 0.17 seconds, subject to frame timing and file
save behavior.

## Ordinary GUI layouts

`src/gui/gui-layout.cpp:82-287` owns parsing. It supports the v2 array format
and v3 object format. A successful parse replaces the element map and records
the file timestamp.

When parsing fails, the loader returns false and leaves the prior element map
active. Therefore malformed JSON does not intentionally erase a working GUI.

`src/gui/gui-layout.cpp:445-450` compares the current file timestamp with the
last accepted timestamp. `src/gui/gui-layout.cpp:485-510` reloads changed
layouts and reports failures while retaining the previous valid layout.

The terminal commands are registered in
`src/gui/gui-editor-commands.cpp`:

- `gui_load <filepath>` at lines 53-65 explicitly reloads one layout.
- `gui_debug_layout` at lines 150-177 lists known layouts and reports that
  polling is active.

## Gamemode metadata GUI

`config/gui/gamemode-meta-gui.json` is the single presentation file for
gamemode-specific HUD sections. Its top-level `gamemodes` object is keyed by
the gamemode ID from `config/gamemodes/*.json`.

`src/gui/gui-layout.cpp` reuses the normal `GuiElement` parser for a selected
section. `GuiLayoutManager::getGamemodeLayout()` normalizes network names to
the current gamemode IDs:

- `team_deathmatch` -> `tdm`
- `free_for_all` -> `ffa`
- `bomb_tag` -> `bombtag`
- `duel` -> `duel`
- `sandbox` -> `sandbox`

Existing loaded gamemode sections are checked by the same
`GuiLayoutManager::pollReload()` path. The active HUD reads the new section on
the next UI render after a successful reload.

The migrated consumers are:

- `src/engine/engine-tick-ui-overlays.cpp:557-621` for community match title,
  TDM score, countdown, intermission, and time-left presentation;
- `src/gui/hud/match-leaderboard.cpp:37-195` for FFA/TDM leaderboard and
  score-gain presentation;
- `src/duel/duel-ui.cpp:192-277` for network duel presentation;
- `src/game/duel.cpp:241-279` for local duel presentation.
- `src/game/gamemode-manager.cpp:266-301` for Bomb Tag HUD presentation.

## Current limitations and unresolved paths

The following visible presentation was still hardcoded or only partially
JSON-backed at the time this document was recorded:

- `src/engine/engine-tick-ui-overlays.cpp:541-547` draws room status with
  hardcoded position, scale, and colors.
- `src/engine/engine-tick-ui-replay-hud.cpp:95-109` assembles reconnect text
  in C++ rather than using a JSON template for the complete presentation.
- `src/engine/engine-tick-ui-replay-hud.cpp:111-121` draws the replay-record
  indicator with hardcoded coordinates, text, scale, and colors.
- `src/game/duel.cpp:301-303` contains a duel result score path with
  hardcoded presentation values.

These remain unresolved until a runtime test proves that editing their owning
JSON definition changes the visible result without a rebuild or restart.

## Safe editing workflow

1. Keep `config/hotreload.json` enabled.
2. Edit a JSON presentation value and save it.
3. Wait for the next polling interval.
4. Confirm the relevant reload diagnostic and visible result.
5. If JSON is malformed, fix the file; the last valid layout should remain
   active.

Source parsing and reload success are not the same as visual acceptance. A
live game or replay capture is still required to prove that the intended HUD
element changed and that the correct owner is being rendered.
