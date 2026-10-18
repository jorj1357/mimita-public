# Captain Falcon animation Z-axis test

- UTC: `2026-10-18T16:41:51Z`
- Branch: `20261011zt`
- HEAD before this change: `3a89b2de`
- Result: `PASS` for the data edit and JSON validation; no live visual acceptance performed.

## Scope and starting state

- Converted the Captain Falcon animation rotation values from X to Z for an
  axis test. For each `rotationEulerDegrees` vector with nonzero X, the new
  vector is `[0, y, z + x]`. This keeps Y as authored and combines X with any
  existing Z value. Translation vectors, keyframe timing, and interpolation
  were not changed.
- Before this task, `assets/avatars/captainfalcon/animations.json` already had
  a user edit moving the walk clip's `leftLeg` values onto Z. That change was
  preserved. Other pre-existing dirty files were left untouched:
  `config/analytics.json`, `config/audio/music-settings.json`,
  `config/command-aliases.json`, `config/server-presets.json`,
  `devscripts/dev-loop.py`, and `docs/specs/dev-loop/dev-loop.md`.

## Exact file changes

- `assets/avatars/captainfalcon/animations.json`, lines 22-372: moved 133
  nonzero X rotation values into Z. Ten already had nonzero Z values; those
  were added to the moved X value. All 200 rotation vectors now have X equal
  to zero. For example, line 41 changed from:

  ```json
  { "tick": 0,  "interp": "linear", "parts": { "leftLeg": { "rotationEulerDegrees": [50,0,0] }, "rightLeg": { "rotationEulerDegrees": [-50,0,0] }, "leftArm": { "rotationEulerDegrees": [-45,0,0] }, "rightArm": { "rotationEulerDegrees": [45,0,0] }, "torso": { "rotationEulerDegrees": [8,0,0] } } },
  ```

  to:

  ```json
  { "tick": 0,  "interp": "linear", "parts": { "leftLeg": { "rotationEulerDegrees": [0,0,50] }, "rightLeg": { "rotationEulerDegrees": [0,0,-50] }, "leftArm": { "rotationEulerDegrees": [0,0,-45] }, "rightArm": { "rotationEulerDegrees": [0,0,45] }, "torso": { "rotationEulerDegrees": [0,0,8] } } },
  ```

## Documents and validation

- Reviewed `docs/ROUTER.md`, `docs/specs/animations/animations.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`, and
  `docs/operations/task-completion/task-completion.md`.
- Focused review skill: `docs/skills/spec-behavior-review-v1.md`; result:
  `NOT_APPLICABLE` to runtime behavior because this request changed test data,
  not implementation or the animation contract.
- The JSON parser accepted the edited file. A scan found 200 rotation vectors
  and zero with nonzero X. `git diff --check` passed.
- No build, dev-loop run, or in-game visual check was performed. This confirms
  the file is valid and its values changed; it does not prove how the avatar
  looks in the running game.
