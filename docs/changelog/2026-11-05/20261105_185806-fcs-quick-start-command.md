# Fun Counter Strike quick-start command

Time created: 2026-11-05T18:58:06Z  
Time last updated: 2026-11-05T18:58:06Z  
Display timezone: America/New_York (2026-11-05 13:58:06 EST)

- Task: add the `fcs` terminal alias to start and join a local Fun Counter Strike room, or switch the connected room to Fun Counter Strike.
- Result: `PASS_WITH_HUMAN_REVIEW`. Source/configuration changes and build passed. Actual terminal invocation, joining, and connected-room switch were not exercised in a live client.
- Branch: `20261011zt`
- HEAD at session start: `afe5b69210eb43cc7be917a22283041a8750099b`
- Commit: none created.

## Changes

- `config/server-presets.json:24-33`: added preset `funcounterstrike_1`, with mode `funcounterstrike`, map `dust2cyberiav4`, local host joining enabled, and generic startup NPCs disabled. Previously there was no preset for this mode.
- `src/terminal/debug-commands.cpp:134-182`, `registerDebugCommands`: added `startfuncounterstrike`. Disconnected clients load the new preset and call the existing `launchServerPresetAndJoin` path. Connected clients send the existing by-ID command `start funcounterstrike`, which is handled by the server's generic authoritative mode-start path. A direct server-host console uses `serverCommunityStartMatch`.
- `config/command-aliases.json:6`: added `"fcs": "startfuncounterstrike"`.
- `docs/architecture/terminal-commands/terminal-commands.md:53-57`: documented the named quick-start contract and its configurable alias.

The source command follows the existing `startssbm` launch shape and the connected branch uses the existing `start <mode-id>` handler. No new packet or mode-specific server handler was needed.

## Before and after

- Before: no `funcounterstrike_1` entry existed. After:

  ```json
  "funcounterstrike_1": {
    "server_name": "Fun Counter Strike 1",
    "map": "dust2cyberiav4",
    "mode": "funcounterstrike",
    "startup_npcs": false,
    "npc_count": 0,
    "discord_notification": false,
    "map_rotation": false,
    "join_host": true
  }
  ```

- Before: no `fcs` alias existed. After: `"fcs": "startfuncounterstrike"`.
- Before: `registerDebugCommands` had no `startfuncounterstrike` registration. After: it dispatches to local preset launch/join, connected-room server mode start, or direct host mode start according to runtime state.
- Before: the terminal-command architecture did not describe named quick-start commands. After: it documents the preset and connected-server behavior and names `fcs` as its configurable alias.

## Documents and focused review

- Read `AGENTS.md`, `docs/ROUTER.md`, `docs/specs/gamemodes/funcounterstrike.md`, `docs/specs/gamemodes/gamemodes.md`, `docs/architecture/terminal-commands/terminal-commands.md`, `docs/skills/terminal-command-checker-v1.md`, `docs/skills/spec-behavior-review-v1.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/specs/dev-loop/dev-loop.md`, `docs/specs/dev-loop/quality-gates.md`, `docs/operations/build-and-exe/build-and-exe.md`, `docs/operations/task-completion/task-completion.md`, `docs/architecture/live-development/live-development.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, and `docs/skills/documentation-checker-v1.md`.
- Terminal-command review: name, usage, description, preset failure handling, alias, and host/server routing are defined. The command is registered with the normal terminal registry, which exposes it through command help.
- Specification review: the new command selects the documented `funcounterstrike` mode and its configured map. No gameplay rules changed.
- Documentation review: quick-start behavior is documented in the terminal-command architecture. The broader TODO scan found existing unrelated notes, listed below; none were edited.

## Validation and evidence

- Configuration assertions for the preset's mode, map, NPC setting, and `fcs` target: `PASS`.
- `git diff --check` for the four changed source/config/documentation files: `PASS` (Git printed a line-ending normalization notice for the Markdown file).
- Canonical dev-loop build request `2026-11-05T18:55:31.007Z-e21094ea`: `SUCCESS`; published current debug artifact `2026-11-05T18:55:57.744849Z`, SHA-256 `320ceeef14909590192459f608f17560498ea34fda04044a475f726f21a84123`. Build reported one source compiled and 602 skipped.
- Exact artifact identity probe: `C:\mimita-v9\.dev\builds\2026-11-05T18-55-57.744849Z\mimita.exe --versioninfo`; run ID `20261105_135728`, `EVENTS_JSONL_PATH=logs/11-05-2026/20261105_135728/events.jsonl`.
- Runtime command, local server launch/join, active mode, and connected-room switch: `NOT_COVERED`. The dev-loop server already running was an older build with no client attached. It was left running; the new command was not sent to it.
- Human visual/multiplayer review: not performed.

## Existing unrelated edits preserved

The shared tree already had edits in `AGENTS.md`, `config/accounts/default.json`, `config/analytics.json`, `config/audio/music-settings.json`, `devscripts/blender-ai` (submodule), `devscripts/dev-launch-modes.json`, `devscripts/dev-loop.py`, `devscripts/quality/agent-gate.py`, `docs/architecture/accessibility/accessibility.md`, `docs/architecture/vision-mentality/vision.md`, `docs/regressions/2026-11-05/rmb-weapon-aim-reversal-REG.md`, `docs/regressions/master/gameplay-REG.md`, `docs/specs/animations/animations.md`, `docs/specs/dev-loop/dev-loop.md`, `docs/specs/entity-editor/entity-editor.md`, `docs/specs/gamemodes/counterstrike.md`, `docs/specs/gamemodes/gamemodes.md`, `docs/specs/networking/networking.md`, `docs/specs/open-gameplay-issues.md`, `docs/specs/visuals/blood-bulletholes-viscera.md`, `docs/specs/weapons/weapons.md`, `src/network/server-players.cpp`, and pre-existing untracked changelog/regression files. None were changed by this task.

## Existing documentation TODO notes

Suggested owner follow-up; these unrelated notes were not changed:

- `docs/specs/emotes/emotes.md:1`, `docs/specs/gui/gui-general.md:1`, `docs/specs/anticheat/anticheat.md:1`, `docs/specs/website/website-forums.md:1`, `docs/specs/collectibles/collectibles.md:1`, `docs/specs/config-load/config.load.md:1`, `docs/specs/non-game-specs/3d-models/3d-models.md:1`, `docs/specs/terminal/terminal.md:3`, `docs/specs/gamemodes/aooni.md:1`, `docs/specs/gamemodes/caravan.md:8`, `docs/specs/gamemodes/flood-escape.md:1`, `docs/specs/gamemodes/freeforall.md:1`, `docs/specs/gamemodes/highway-swim.md:1`, `docs/specs/gamemodes/madness-combat.md:1`, `docs/specs/gamemodes/tung-tung-sahur-bossfight.md:1`, `docs/specs/gamemodes/fortnite/fortnite.md:1`, `docs/specs/gamemodes/infinite-dungeon-slayer.md:1`, `docs/specs/gamemodes/minecraft.md:1`, `docs/specs/gamemodes/workatasandwichshop.md:1`, `docs/specs/music/music-fadein.md:1`, `docs/specs/code-wrapper/code-wrapper.md:1`, `docs/specs/colliisons/collisions.md:1`, `docs/specs/moving-physical-objects/moving-physical-objects.md:18`, `docs/specs/procedural-infinite-world/procedural-infinite-world.md:1`, `docs/specs/gravity-gun/gravity-gun.md:1`, `docs/specs/john-fister/john-fister.md:1`, and `docs/specs/competitive/competitive.md:31`: replace each placeholder with that document's scope, intended behavior, owning subsystem, and acceptance evidence, or label it as a dated idea.
- `docs/specs/gui/guiv2.md:23,25`: state that terminal and GUI actions should share command/action owners; GUI buttons should invoke those same actions.
- `docs/specs/actor-presets/actor-presets.md:264`: explain the referenced actor-preset behavior and link its runtime/configuration owner.
- `docs/specs/dev-loop/dev-loop.md:17`: define the missing operational guidance and link its current dev-loop behavior.
- `docs/specs/movement/movement.md:594`: define which contacts restore jump resources and how ground and air jumps differ.
- `docs/specs/weapons/weapons.md:74`, `docs/specs/gamemodes/counterstrike.md:959,2247,2397,2542`, `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md:36,97`, and `docs/specs/npc-behavior/npc-behavior.md:1`: resolve the specific weapon reload, smoke timing, hot-editing, diagnostic aggregation, aiming, and NPC collision contracts in their owning specifications.
- `docs/specs/monetization/monetization.md:1,125`, `docs/specs/mods/mods.md:5`, `docs/specs/lore/lore.md:111`, `docs/specs/visuals/visuals.md:1`, `docs/specs/visuals/blood-bulletholes-viscera.md:2`, `docs/specs/weapons/tool-weapon-ideas.md:1`, `docs/specs/music/music-midi.md:27`, `docs/specs/gamemodes/mode-packs.md:168`, `docs/specs/gamemodes/inifinite-counterstrike-duel.md:3`, `docs/specs/gamemodes/gamemodes.md:471`, `docs/specs/gamemodes/defend-telamon.md:1`, and `docs/specs/gamemodes/zombie-tower/zombie-tower.md:1723,5861`: convert the informal note into a dated concept or explicit behavior contract with owner and acceptance criteria.
- `docs/specs/avatar/avatar-editor.md:5`: replace the cleanup note with specific sections to consolidate and a clear desired document structure.
- `docs/specs/networking/networking.md:107,109,115,1233`: route product vision out of networking; link the terminal-command architecture for command documentation; name the central owner for the general concept; define the logging evidence needed to diagnose the 60 Hz issue.
- `docs/architecture/player-npc-systems/npc-movement.md:22`: name the missing movement behavior and link its route/acceptance evidence.
- `docs/specs/debug-logging/AI-fix-live.md:51`: define the general task applicability and clean structure intended for the live-fix guidance.
- `docs/specs/hotreload/hotreload.md:1,3`: define hot-reload goals and safe edit/compile/activation boundaries, linking the live-development owner.

Archive and historical changelog matches were not treated as active requirements.
