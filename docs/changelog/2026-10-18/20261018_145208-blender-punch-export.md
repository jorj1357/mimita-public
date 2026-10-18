# Captain Falcon Blender punch export and sparse-keyframe default

- Timestamp: 2026-10-18 14:52 EDT
- Branch: `20261011zt`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Request and outcome

Updated the Blender exporter spec and implementation so the default export keeps
authored keyframes sparse and MiMITA interpolates smoothly between them. Dense
per-tick evaluated poses are now the opt-in mode. Exported the six-part Action
set in `C:\important\quiet\blender-projects\mimita sizing 6.blend` to
`config/animations/library/melee/captainfalcon/punch_a.json` and added the path
to the Captain Falcon avatar animation profile.

The source scene contains no `phase.windup`, `phase.active`, `phase.recovery`,
or `action.end` timeline markers. Therefore this output has no authoritative
Melee timeline and was exported as a generic pose clip. The current melee
runtime also still uses three phase clips from `config/weapons.json`; it does
not yet resolve one clip's marker timeline. The `a_ground_punch` config was
left intact so this export is not misrepresented as already playing in game.

## File changes

### `tools/mimita_animation_exporter.py`

- Before: default sampling exported a pose for every tick; the prompt called
  this `Interpolate` and offered `Direct poses`.
- After: lines 180-198 gather the union of integral transform-key frames;
  lines 353-355 default to `Authored keyframes`; lines 359-360 use the requested
  `melee/<avatar>/<clip>.json` folder for Melee export; lines 382-383 choose
  sparse or dense sampling. Lines 244-280 add a profile include without
  reformatting the existing profile JSON.
- Before: the output prompt used the flat `config/animations/library/<clip>.json`
  path for every clip.
- After: a Melee clip defaults to
  `config/animations/library/melee/captainfalcon/<clip>.json`.

### `docs/specs/animations/animations.md`

- Before: Blender exports always wrote one complete pose per 60 Hz tick by
  default.
- After: lines 447-467 define sparse `Authored keyframes` as the default,
  smooth runtime interpolation, and `Dense every tick` as opt-in.

### `docs/specs/animations/blender-animation-authoring.md`

- Before: the spec described dense per-tick output and the old Interpolate and
  Direct poses choices.
- After: lines 49-58, 113-118, 231-248, and 386-425 specify the new sparse
  default, dense opt-in, path organization, exact export result, and the
  unresolved Melee timeline/runtime integration.

### `config/animations/library/melee/captainfalcon/punch_a.json`

- New file; no prior file existed. It contains version 2, `durationTicks: 60`,
  two smooth keys at ticks 0 and 59, and translation plus WXYZ quaternion
  rotation for head, torso, both arms, and both legs at each key.

### `assets/avatars/captainfalcon/animations.json`

- Before: no `include` entry referenced the shared punch clip.
- After: lines 375-377 include
  `config/animations/library/melee/captainfalcon/punch_a.json` without
  reformatting the existing animation profile.

## Validation

- Opened and sampled the actual `.blend` in Blender 5.0.0. Scene is 60 FPS,
  frames 1–60; six per-part Actions are present and the marker list is empty.
- Ran the interactive exporter against that scene. It reported 60 ticks and
  wrote two authored keys at ticks 0 and 59.
- Parsed and checked the exported JSON and profile include; verified finite
  transforms, six complete part entries per key, tick bounds, and 60-tick
  duration.
- Python syntax compilation and `git diff --check` passed. No game build or
  in-game playback test was run.

## Preserved work and limitation

Existing edits to the Melee and weapon specs and earlier animation changelogs
were preserved. The exporter/profile/pose asset are usable as animation data,
but connecting `punch_a` to the grounded left-click action requires the pending
shared server/client clip timeline implementation and phase markers in the
Blender source.
