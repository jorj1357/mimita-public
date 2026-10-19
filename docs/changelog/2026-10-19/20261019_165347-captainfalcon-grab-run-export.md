# Captain Falcon grab and run exports

- Local time: 2026-10-19 16:53:47 EDT (UTC-04:00)
- UTC: 2026-10-19T20:53:47Z
- Branch/starting commit: `20261011zt` / `35c9b12af4ac2258b24e0116d156d40eedf99cbb`
- Request: export the Captain Falcon grab from the supplied Blender scene with the repository exporter, use it in the Melee grab action, and keep related authored animation clips on the shared clip path.

## Changes made

- `config/animations/library/melee/captainfalcon/grab.json:1-374` is a new exporter-produced version-2 clip from `C:\important\quiet\blender-projects\mimita sizing 6.blend`, root `plrOrigin.038`, and the six currently assigned part Actions. It has 60 ticks, four authored poses, all six canonical parts per pose, and phase markers at playback/source ticks 0, 20, 40, and 60. There was no prior file. The Blender source was not saved or modified.
- `config/weapons.json:1922-1930` now selects the shared `grab` clip. Before, the action had separate `cf_grab_windup`, `cf_grab_active`, and `cf_grab_recovery` clips with 3/3/8 ticks. After, it has `"animation_clip_id": "grab"`; clip markers supply its 20/20/20 phase timing. Existing hitbox, pull, damage, knockback, and audio values were preserved.
- `assets/avatars/captainfalcon/animations.json:7,366-368` maps the run state to `captainfalcon_run` and includes `upkick_a.json`, `grab.json`, and the new locomotion `run.json` in the shared profile. Before, run mapped to `captainfalcon_walk`, and the include list ended at `downkick_a.json`. The other existing punch/kick exports were left intact.
- `config/animations/library/locomotion/captainfalcon/run.json:1-456` is a new version-2 generic clip exported from root `plrOrigin.031` and its six current part Actions. It has 59 ticks, five authored poses, loops, and no unrelated scene-global Melee phase markers. There was no prior file.
- `tools/mimita_animation_exporter.py:166-170` skips the reserved Melee phase markers for Generic clips. Before, all scene markers were copied into every clip type. After, Generic clips omit those four scene-global markers while retaining other marker names; Melee clips still validate and export them.

## Preserved workspace state

The checkout already contained unrelated edits before this work. They were preserved: `config/analytics.json`, the six existing Captain Falcon attack JSON files, `config/audio/music-settings.json`, `docs/specs/animations/animations.md`, `docs/specs/animations/blender-animation-authoring.md`, `docs/specs/gamemodes/supersmashbrosmelee.md`, the existing Melee/combat and player-animation C++ edits, and earlier edits to `config/weapons.json` and `tools/mimita_animation_exporter.py`. `config/accounts/default.json` was also modified at entry and no longer appears modified in the final status; this task did not edit it. Only the grab action block in the weapon config and the Generic marker filter in the exporter were changed here.

## Documents and focused reviews

- Read `docs/ROUTER.md`, `docs/specs/gamemodes/supersmashbrosmelee.md`, `docs/specs/animations/animations.md`, `docs/specs/animations/blender-animation-authoring.md`, `docs/specs/avatar/avatar-editor.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, `docs/operations/asset-management/asset-management.md`, `docs/operations/build-and-exe/build-and-exe.md`, `docs/operations/task-completion/task-completion.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/specs/debug-logging/debug-logging.md`, `docs/architecture/terminal-commands/terminal-commands.md`, and `docs/regressions/2026-10-18/melee-hit-feedback-and-hitbox-accuracy-REG.md`.
- `docs/skills/spec-behavior-review-v1.md`: PASS_WITH_HUMAN_REVIEW. The one-clip marker timeline agrees with the current animation and Melee requirements; visual playback remains unreviewed.
- `docs/skills/asset-checker-v1.md`: PASS. Both files are under the shared animation library, and the Captain Falcon profile includes them.
- `docs/skills/terminal-command-checker-v1.md`: runtime action start not confirmed; see runtime evidence below.
- Existing `docs/changelog/2026-10-19/20261019_204700-animation-retiming-implementation.md` was reviewed for prior build and runtime context.

## Validation and runtime evidence

- Blender 5.0 ran `tools/mimita_animation_exporter.py` on the supplied scene. Grab export reported 60 ticks/four poses; run export reported 59 ticks/five poses. The exporter reported the calibrated Blender-to-game basis for each.
- Focused validation passed: exporter Python syntax; both JSON files parse; every exported keyframe contains head, torso, both arms, and both legs; grab markers and source ticks are ordered and end at duration; run has no Melee phase markers and loops; the profile includes all six existing attack clips plus grab and run; the grab action has one `animation_clip_id` and no duplicate phase timing.
- The canonical dev loop published build `2618` at `.dev/builds/2618/mimita.exe`; status reported it current. Running that executable with `--versioninfo` reported `EVENTS_JSONL_PATH=logs/10-19-2026/20261019_165149/events.jsonl`.
- The active dev-loop run used build 2618 and journal `logs/2026-10-19/20261019_205156/events.jsonl`. It recorded `weapon.melee.moveset_loaded` with 14 actions and `anim.set.loaded` for Captain Falcon with `valid=true`, 55 clips, and both `grab.json` and `run.json` in its include paths.
- The real terminal command `agent.melee grab` was delivered and resolved in that run. The journal recorded `avatar.command.dispatch`, but no corresponding `melee.client.action` or server `melee.action.start` for `grab`; therefore the grab action start is not proven by this command. This record does not claim a successful gameplay invocation or visual acceptance. No process was stopped to retry; the active run was left running.

## Status

The exported assets and configuration wiring are in place and build-validated. Runtime profile loading is confirmed. A confirmed grab action start and human visual review remain open. No regression is declared from the unconfirmed command path.
