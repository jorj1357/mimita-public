# Juggernaut Duel Mode

- Timestamp: 2026-10-16T02:55:34Z (2026-10-15 22:55:34 America/New_York, EDT)
- Branch: `20261011zt`
- HEAD: `3a345200` (`girl avatrr big`)
- Result: `PASS_WITH_HUMAN_REVIEW`

## Outcome

Added a distinct **Juggernaut Duel** choice to the community mode picker. It uses
its own gamemode JSON and the existing 1v1 Duel lifecycle, with the shared
Juggernaut movement and `juggernaut_mode` weapon set assigned to both duelists.
The mode's `max_players: 2` is enforced at join time and used for the advertised
server capacity. The existing team-based Juggernaut mode remains separate.

## Exact changes

- `config/onlinemodes.json:75-83`: added `juggernaut_duel`, mapped to its
  gameplay definition, score limit 20, and a two-player cap. Before, the
  community picker had no Juggernaut Duel entry.
- `config/gamemodes/juggernaut_duel.json:1-18`: added the match definition:
  Juggernaut actor preset, 20-kill target, no kill healing, immediate respawn,
  Duel map pool, and `victory.type = duel`. Before, no such definition existed.
- `config/actor-presets/juggernaut_arcade.json:5`: added
  `weapon_set: juggernaut_mode`; its existing `movement_preset: juggernaut`
  supplies the shared Juggernaut movement. This keeps both players on the same
  preset/loadout owner.
- `src/network/community-server-config.h:27` and
  `src/network/community-server-config.cpp:71`: added optional `max_players`
  loading; zero preserves the normal limit.
- `src/network/server-packets.cpp:1408-1416`: changed the join ceiling from
  only the protocol maximum to the lower of that maximum and the selected
  community mode's configured cap.
- `src/network/server.cpp:552-559`: capped the server's advertised capacity.
- `src/gui/gui-main.cpp:209-212`: capped the community-server launch setting
  for the selected mode.
- `src/network/server-gamemode.cpp:4312`: routed the classic 1v1 score path by
  the configured `victory.type` instead of requiring the mode ID to equal
  `duel`, allowing this mode to reuse the existing Duel lifecycle.
- `docs/specs/gamemodes/juggernaut-duel.md`: documented player experience,
  ownership, and separate acceptance requirements.

## Documents and focused review

Read `docs/ROUTER.md`, `docs/specs/gamemodes/gamemodes.md`,
`docs/specs/gamemodes/juggernaut.md`, `docs/specs/networking/networking.md`,
`docs/specs/gui/guiv2.md`, `docs/architecture/json-configuration/json-configuration.md`,
`docs/architecture/terminal-commands/terminal-commands.md`,
`docs/architecture/live-development/live-development.md`,
`docs/workflows/runtime-scenario-validation.md`,
`docs/operations/build-and-exe/build-and-exe.md`,
`docs/operations/task-completion/task-completion.md`,
`docs/architecture/time-and-formatting/time-and-formatting.md`,
`docs/skills/spec-behavior-review-v1.md`, and
`docs/skills/documentation-checker-v1.md`.

- Specification and behavior review: `PASS_WITH_HUMAN_REVIEW`. The selected
  actor preset has a Juggernaut movement preset; assigning its existing
  `juggernaut_mode` weapon-set key gives the same loadout to both legacy Duel
  participants. The shared score owner now recognizes the explicit configured
  `duel` victory type.
- Documentation checker: `PASS_WITH_HUMAN_REVIEW`. The new document states its
  purpose, exclusions, owners, and acceptance. The router references
  `docs/doc-review-09-03-2026.md` and `docs/regressions/regressions-v1.md`, but
  those exact files are absent in this checkout (`regressions-v1.md` is under
  `docs/regressions/archive-ignore/`). This task did not rewrite routing docs.
- Todo scan found these literal examples, which are instructions/examples rather
  than feature TODOs: `docs/skills/documentation-checker-v1.md:22` says
  “todo: explain this better”; `docs/ROUTER.md:116` says “like \"todo: explain
  this better\"”. Suggested replacement for the skill example is “Search all
  current Markdown under `docs/` for unresolved explanatory TODO comments and
  report each exact path, line, quote, and proposed replacement.”

## Validation and evidence

- `python build_agent.py`: `BUILD SUCCESS`, compiled 1 unit, linked
  `mimita.exe`; build log: `build/changelog.txt`.
- Dev-loop status reported newly published build 2405 at
  `.dev/builds/2405/mimita.exe`, source generation current, and a separate
  active client session on build 2403 using the Zombie Tower profile. The active
  client was preserved; no mode switch, restart, or second dev loop was issued.
- Ran `.dev/builds/2405/mimita.exe --versioninfo`:
  `EXECUTABLE_PATH=C:\mimita-v9\.dev\builds\2405\mimita.exe`,
  `EVENTS_JSONL_PATH=logs/10-15-2026/20261015_225438/events.jsonl`,
  `RUN_ID=20261015_225438`. This proves executable identity only; the journal
  contains no Juggernaut Duel gameplay scenario.
- Python JSON parsing passed for `config/onlinemodes.json`,
  `config/gamemodes/juggernaut_duel.json`, and
  `config/actor-presets/juggernaut_arcade.json`.
- `git diff --check` passed (with standard LF/CRLF notices).
- No tests were added or run.

## Pre-existing work preserved

At session start, these unrelated files were already modified and were left
untouched: `config/accounts/default.json`, `config/analytics.json`, and
`config/audio/music-settings.json`.

## Remaining acceptance

The real two-client scenario has not been run because the existing dev-loop
session was actively running a different user-selected mode. Source/config,
build, and executable identity are established. The live journal has not yet
proven that the picker loads the option, the third client is rejected, both
players receive the movement/loadout preset, or the duel ends at 20 kills.
Human visual and multiplayer acceptance remain pending.

