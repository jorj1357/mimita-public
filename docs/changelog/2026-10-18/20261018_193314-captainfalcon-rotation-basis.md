# Captain Falcon Blender-to-game rotation basis

- Timestamp: 2026-10-18T19:33:14Z (2026-10-18 15:33:14 EDT)
- Branch: `20261011zt`
- HEAD before work: `3bf384e4`
- Status: `PASS_WITH_HUMAN_REVIEW`

## Request and result

Fix the Captain Falcon punch's in-game orientation mismatch by making Blender
Action sampling, transform basis conversion, JSON output, and the authoring spec
match the supported default-player rig. The exporter now reads the selected
Blender 5 Action slot's transform curves instead of relying on stale evaluated
world matrices, validates the root's stored calibrated −90° X quaternion basis,
and converts each local delta once. The generated punch remains a 60-tick clip
with ticks 0 and 59 and the existing phase markers at 0, 20, 40, and 60.

This fixes the identified exporter-side coordinate conversion. It does not
establish visual in-game acceptance: the current dev-loop server is down and its
animation command was queued but not delivered. The live client was preserved.

## Changed files and exact edits

### `tools/mimita_animation_exporter.py` — `action_curves`, `action_transform`, `sample_clip`, export report (lines 204–295, 475–477)

Before:
```python
depsgraph = bpy.context.evaluated_depsgraph_get()
depsgraph.update()
eval_root = root.evaluated_get(depsgraph)
root_inv = eval_root.matrix_world.inverted_safe()
evaluated = objects[part].evaluated_get(depsgraph)
relative = root_inv @ evaluated.matrix_world
delta = rest_local[part].inverted_safe() @ relative
```

After: `action_curves()` selects the assigned Action slot; `action_transform()`
evaluates `location`, quaternion/Euler rotation, and scale FCurves and constructs
the local matrix. `sample_clip()` rejects unsupported hierarchy, constraints,
and root motion; reads `root.rotation_quaternion` even though the supported
scene's active Euler rotation is identity; validates the calibrated −90° X
basis; and applies `basis @ delta @ basis_inv` once before decomposition.
The report names evaluated Action channels and basis-converted rest-relative
WXYZ deltas.

### `config/animations/library/melee/captainfalcon/punch_a.json` — tick 0 and 59 part transforms

Before (head tick 0):
```json
"translation": [0.58908653, 3e-08, -0.19297302],
"rotationQuaternionWxyz": [0.99463761, -4e-08, 0.10342155, 3e-08]
```

After:
```json
"translation": [0.59, -0.19, 0.0],
"rotationQuaternionWxyz": [0.99, 0.0, 0.0, -0.1]
```

All six parts at both exported keys were regenerated from the actual
`mimita sizing 6.blend` Actions, basis-converted, and rounded to two decimals.
The file parses as JSON, contains all six parts, has no exponent notation, and
retains duration 60 and markers `phase.windup=0`, `phase.active=20`,
`phase.recovery=40`, `action.end=60`.

### `docs/specs/animations/blender-animation-authoring.md` — exact transform contract (lines 180–243)

Before:
```text
The script does not reproduce geometry-based avatar normalization; align
mismatched rig pivots before export and compare poses in Blender and MiMITA.
```

After: the exact `rest * delta` composition, supported default-player basis,
root quaternion-versus-active-Euler detail, one-time conjugation, Blender 5
assigned-slot FCurve sampling, and unsupported rig conditions are specified.
The document still requires a live punch and human visual comparison for
acceptance.

### `docs/specs/animations/animations.md` — conversion pipeline (lines 435–476)

Before:
```text
Evaluate the Blender object/bone transform at that sample ... Validate B
against the normalized avatar frame before accepting an exporter.
```

After: the shared spec requires Blender 5 assigned-slot FCurve evaluation and
documents the calibrated default-player root quaternion basis, excludes root
translation/scale, and rejects unsupported rigs until their mapping is verified.

### `docs/changelog/2026-10-18/20261018_193314-captainfalcon-rotation-basis.md`

New file; there was no prior text. This record contains the session evidence,
precise changes, review result, and remaining acceptance requirement.

## Validation and evidence

- Blender 5.0.0 opened `C:\important\quiet\blender-projects\mimita sizing 6.blend`,
  imported `mimita-char-no-animations-v4.glb`, evaluated all six selected
  Actions, and exported the 60-tick clip with ticks `[0, 59]`. The exporter
  reported basis quaternion WXYZ `[0.71, -0.71, -0.00, 0.00]`.
- `python -m py_compile tools/mimita_animation_exporter.py`: passed.
- JSON parse and focused content check: passed; 6 parts/key, 84 scalar transform
  values, at most 2 decimal places, no scientific notation, duration and marker
  values preserved.
- Canonical `python devscripts/dev-loop.py --request build --wait-for-build`:
  success, published build 2603. The client remains on build 2601 in run
  `20261018_191211`; no process was stopped or relaunched.
- Runtime journal: `logs/2026-10-18/20261018_191211/events.jsonl`. It identifies
  avatar `captainfalcon`, but has no delivered punch command for this new pose.
  Dev-loop status reports `server_alive=false`; command `anim.play punch_a` was
  accepted/queued but `delivered=false`, `completed=false`.
- Human visual acceptance remains pending. Do not claim the punch now looks
  correct in-game until it is played in a live Melee session and compared.
- `git diff --check` reports a pre-existing trailing-whitespace line at
  `docs/specs/gamemodes/supersmashbrosmelee.md:865`; that unrelated file was
  preserved.

## Instructions and focused reviews

Read `AGENTS.md`, `docs/ROUTER.md`,
`docs/specs/animations/animations.md`,
`docs/specs/animations/blender-animation-authoring.md`,
`docs/specs/avatar/avatar-editor.md`,
`docs/workflows/runtime-scenario-validation.md`,
`docs/specs/debug-logging/AI-fix-live.md`,
`docs/architecture/terminal-commands/terminal-commands.md`,
`docs/operations/build-and-exe/build-and-exe.md`,
`docs/operations/task-completion/task-completion.md`,
`docs/architecture/time-and-formatting/time-and-formatting.md`, and
`docs/skills/spec-behavior-review-v1.md`. The review found the exporter was not
sampling Blender 5 Action-slot channels and did not convert the authoring basis.
Focused review result: `PASS_WITH_HUMAN_REVIEW`.

## Pre-existing work preserved

`docs/specs/gamemodes/supersmashbrosmelee.md` was already modified before this
request and remains untouched. The earlier
`docs/changelog/2026-10-18/20261018_192100-blender-rotation-precision.md` is
also pre-existing and was not rewritten.
