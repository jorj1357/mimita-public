# Animation exporter prompts and Blender smoke run

- Timestamp: 2026-10-18 14:44:04 EDT / 2026-10-18T18:44:04Z
- Branch: `20261011zt`
- HEAD before and after: `f2e91866` (no commit created)
- Result: `PASS_WITH_HUMAN_REVIEW`

## Request and result

Set the normal-Python launcher to use
`C:\Program Files\Blender Foundation\Blender 5.0\blender.exe` by default and
ask for the Blender executable if that default file is missing.
Added purpose and concrete examples to the export questions so that each choice
is understandable. The clip ID prompt explains that it is the ID consumed by
MiMITA and need not match per-part Blender Action names. The spec now includes
an example prompt transcript and the default executable path.

## Exact changes

### `tools/mimita_animation_exporter.py` (lines 1-404)

Before: the launcher searched `PATH` first and otherwise asked for the Blender
executable without a repository-configured default. The prompt helper printed
the choices without explanations/examples.

After, lines 21-39 define and use the requested default:

```python
DEFAULT_BLENDER_EXE = r"C:\Program Files\Blender Foundation\Blender 5.0\blender.exe"
```

After, lines 254-278 show examples and explanations for every common prompt;
lines 292-338 use them for rig, part Action, clip ID, type, sampling, GLB path,
output path, and optional avatar profile.

The first Blender run failed with `Reference GLB did not import a plrOrigin
root`. In the user's scene the imported GLB root was renamed to
`plrOrigin.028` because `.blend` already contained `plrOrigin` through
`plrOrigin.027`. Before, the root lookup required the exact object name
`plrOrigin`. After, line 160 accepts `plrOrigin` with an optional three-digit
Blender suffix.

### `docs/specs/animations/blender-animation-authoring.md` (lines 1-420)

Before, lines 43-48 did not specify a Blender executable default or explain the
interactive prompt examples. After, lines 43-48 name the default executable,
fallback behavior, and requirement that each prompt provide an explanation and
concrete example. After, lines 123-141 explain the difference between a Blender
Action name such as `head.026Action` and the game-facing Clip ID, and show a
sample prompt sequence using `punch_a`.

## Validation

- Blender executable found at the requested path; version: Blender 5.0.0.
- Source project opened read-only for the export run:
  `C:\important\quiet\blender-projects\mimita sizing 5.blend`.
- The normal Python launcher (`Python 3.12`) successfully invoked Blender,
  accepted interactive input, and exported to
  `%LOCALAPPDATA%\Temp\mimita-launcher-test-final.json`.
- Output inspection passed: version 2, `durationTicks: 60`, exactly 60
  keyframes with ticks 0–59, all six canonical part transforms on every tick,
  and finite translation/quaternion components. File size: 127,354 bytes.
- The initial reference-root naming failure was reproduced, diagnosed from the
  Blender import object list, fixed, and the full launcher export then passed.
- `git diff --check` passed, with only line-ending conversion warnings in the
  animation spec documents.
- Profile auto-include and Melee marker paths were not tested. The output was
  not loaded in a running game; visual/runtime pose acceptance remains open.

## Instructions and pre-existing work

Reviewed `AGENTS.md`, `docs/ROUTER.md`,
`docs/specs/animations/blender-animation-authoring.md`,
`docs/specs/animations/animations.md`,
`docs/specs/avatar/avatar-editor.md`,
`docs/skills/spec-behavior-review-v1.md`,
`docs/operations/asset-management/asset-management.md`,
`docs/operations/task-completion/task-completion.md`, and
`docs/architecture/time-and-formatting/time-and-formatting.md`. The router
references `docs/doc-review-09-03-2026.md`, which is absent. The focused
specification and behavior review resulted in `PASS_WITH_HUMAN_REVIEW`: the
script now matches the decided prompt and export flow, while game visual and
combat timing acceptance remain unresolved.

Pre-existing dirty files preserved: `docs/specs/animations/animations.md`,
`docs/specs/gamemodes/supersmashbrosmelee.md`, `docs/specs/weapons/weapons.md`,
and the earlier animation changelogs. This turn modified the exporter and
Blender authoring spec and added this changelog.
