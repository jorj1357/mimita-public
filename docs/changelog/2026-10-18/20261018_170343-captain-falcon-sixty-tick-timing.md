# Captain Falcon animation clip timing

- Result: PASS_WITH_HUMAN_REVIEW
- Time: 2026-10-18T17:03:43Z (2026-10-18 13:03:43 EDT, America/New_York)
- Branch: `20261011zt`
- HEAD: `3a89b2de`

## Request and final state

Retimed all 47 local Captain Falcon clips so keyframes are evenly distributed
from tick 0 through tick 59. Each clip declares `durationTicks: 60`. Two-pose
clips now use ticks `[0, 59]`; three-pose clips use `[0, 30, 59]`. Existing
pose/channel values and interpolation were preserved.

## Files and exact changes

- `assets/avatars/captainfalcon/animations.json`, `clips.jump.keyframes`:
  changed the last keyframe tick from `30` to `59` (current line 51).
- `assets/avatars/captainfalcon/animations.json`, all other two-keyframe clips:
  changed their final keyframe tick to `59` (current lines 59, 67, 75, 83, 91,
  99, 107, 114, 121, 129, 136, 143, 151, 158, 165, 173, 180, 187, 195, 202,
  209, 217, 224, 231, 239, 246, 253, 261, 268, 275, 283, 290, 297, 305, 312,
  319, 327, 334, 341, 349, 356, 363, 371). This includes all clips whose
  second keyframe previously used tick 7, 9, 11, or 19.
- `idle`, `walk`, and `run` already used `[0, 30, 59]`; their timing was left
  unchanged.
- Other working-tree edits were present before this timing pass, including
  pose/channel changes in the target JSON and unrelated repository files. They
  were preserved and are not attributed to this change.

Example exact timing change in `clips.jump.keyframes`:

```json
- { "tick": 30, "interp": "smooth", "parts": { "leftArm": { "rotationEulerDegrees": [0,0,-110] }, "rightArm": { "rotationEulerDegrees": [0,0,-110] }, "leftLeg": { "rotationEulerDegrees": [0,0,45] }, "rightLeg": { "rotationEulerDegrees": [0,0,-45] }, "torso": { "rotationEulerDegrees": [0,0,-10] } } }
+ { "tick": 59, "interp": "smooth", "parts": { "leftArm": { "rotationEulerDegrees": [0,0,-110] }, "rightArm": { "rotationEulerDegrees": [0,0,-110] }, "leftLeg": { "rotationEulerDegrees": [0,0,45] }, "rightLeg": { "rotationEulerDegrees": [0,0,-45] }, "torso": { "rotationEulerDegrees": [0,0,-10] } } }
```

For a previously two-pose clip whose last tick was `9`, the exact tick-only
change was:

```text
- { "tick": 9, ... }
+ { "tick": 59, ... }
```

## Documents and focused review

- Read `docs/ROUTER.md` and `docs/specs/animations/animations.md`.
- Read `docs/specs/avatar/avatar-editor.md`,
  `docs/operations/asset-management/asset-management.md`,
  `docs/skills/asset-checker-v1.md`,
  `docs/skills/spec-behavior-review-v1.md`, and
  `docs/operations/task-completion/task-completion.md`.
- Asset checker result: PASS. Target asset is the avatar-local `animations.json`.
- Specification review: PASS for the requested timing update. The animation
  specification defines `durationTicks` and tick-addressed keyframes; this
  request explicitly sets every clip to a 60-tick span ending on tick 59.

## Validation and remaining review

- Parsed the edited file as JSON and checked all 47 clips have `durationTicks`
  of 60, first keyframe tick 0, and final keyframe tick 59: PASS.
- Checked representative layouts: idle/walk/run `[0, 30, 59]`; jump/fall/land/
  crouch `[0, 59]`: PASS.
- No build or runtime scenario was run; this was an asset-only timing edit.
  Review the resulting animation pacing in game: HUMAN REVIEW REQUIRED.
