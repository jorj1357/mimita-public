# Interactive Blender animation export script

- Timestamp: 2026-10-18 14:37:17 EDT / 2026-10-18T18:37:17Z
- Branch: `20261011zt`
- HEAD before and after: `f2e91866` (no commit created)
- Result: `PASS_WITH_HUMAN_REVIEW`

## Request and result

Replaced the Blender add-on workflow with a standalone interactive Python
script. A normal Python launch asks for Blender's executable and a `.blend`
file, then starts Blender in background mode. The script asks for a rig, an
Action per canonical body part, clip ID/type, sampling mode, reference GLB,
output path, and an optional avatar profile to update. It prints the path after
export. Blender remains necessary to evaluate its scene and Action data.

The screenshot uses separate per-part Actions and Blender duplicate names. The
script now accepts unique canonical part objects with `.NNN` suffixes, offers
per-part Action choices, and supports leaving a part at the reference rest pose.
Exported values remain offsets against the normalized GLB rest transforms. The
spec calls absolute transforms in `plrOrigin` space a possible future format.

## Exact source changes

### `tools/mimita_animation_exporter.py` (lines 1-358)

Before, lines 1-4 of the previous implementation:

```python
"""MiMITA rigid-part animation exporter for Blender 5.x.

Install with Blender > Edit > Preferences > Add-ons > Install from Disk.
The exporter samples object Actions into the shared version-2 animation JSON.
"""
```

After, lines 1-10:

```python
"""Interactive standalone MiMITA animation exporter for Blender 5.x.

Run from ordinary Python for interactive setup:
  py tools/mimita_animation_exporter.py
Or run directly through Blender with a .blend file:
  blender --background "my_actor.blend" --python tools/mimita_animation_exporter.py

Blender supplies bpy and evaluates the open scene; this script prompts for the
rig, per-part Actions, clip settings, and output paths, then writes game JSON.
"""
```

The add-on operator/panel was removed. Lines 21-33 add the normal-Python
bootstrap that asks for Blender and the source `.blend`; lines 52-64 resolve
the canonical six parts with optional numeric Blender suffixes; lines 103-141
bind and restore each selected Action; lines 263-360 prompt, sample, validate,
write the clip, optionally update the avatar profile, and print the exported
path.

### `docs/specs/animations/blender-animation-authoring.md` (lines 1-393)

Before, lines 3-5:

```text
Status: **SPECIFICATION — first Blender add-on implementation exists; Blender
execution and runtime acceptance are pending.** The exporter writes the shared
version-2 clip shape. Server/client use of phase markers for gameplay timing
is still unimplemented.
```

After, lines 3-6:

```text
Status: **SPECIFICATION — standalone interactive exporter script exists;
Blender execution and runtime acceptance are pending.** The exporter writes the
shared version-2 clip shape. Server/client use of phase markers for gameplay
timing is still unimplemented.
```

After, lines 32-50 and 92-121 describe how to run the script, its prompts, its
per-limb Action selection, and the fact that it is not an installed add-on.
Before, the format description said exported limb values were in the actor's
`plrOrigin` frame. After, lines 134-160 say `plrOrigin` removes world placement
during sampling while the JSON stores offsets from normalized GLB rest pose;
absolute `plrOrigin` transforms are explicitly a possible future format.
Acceptance steps at lines 332-347 now describe running the script.

## Scope and evidence

- Preserved the existing 60 FPS dense sampling, Interpolate default, Direct
  poses validation, marker validation, rest-reference comparison, JSON shape,
  atomic clip writing, and optional avatar include update.
- `git diff --check`: passed. It reported only LF-to-CRLF conversion warnings
  for the animation spec documents.
- Blender is not available in this environment. The script was not executed;
  Blender prompt behavior, Action-slot binding, exported output, and game pose
  parity remain unverified. No test suite or game build was run.

## Instructions and pre-existing work

Reviewed `AGENTS.md`, `docs/ROUTER.md`, `docs/specs/animations/animations.md`,
`docs/specs/animations/blender-animation-authoring.md`,
`docs/specs/avatar/avatar-editor.md`,
`docs/skills/spec-behavior-review-v1.md`,
`docs/operations/asset-management/asset-management.md`,
`docs/operations/task-completion/task-completion.md`, and
`docs/architecture/time-and-formatting/time-and-formatting.md`. The router
references `docs/doc-review-09-03-2026.md`, which is absent.

Pre-existing dirty files preserved: `docs/specs/animations/animations.md`,
`docs/specs/gamemodes/supersmashbrosmelee.md`, `docs/specs/weapons/weapons.md`,
and earlier changelogs `20261018_181131-animation-timeline-source.md`,
`20261018_181704-blender-exporter-spec.md`,
`20261018_182333-blender-pose-sampling.md`, and
`20261018_182823-blender-exporter-code.md`. This turn modified the exporter and
Blender authoring spec and added this changelog.

## Human review still needed

Run the script from a terminal with Blender and a `.blend` file showing the
numbered six-part rig. Confirm prompts work in the Blender background process,
each part selects the intended Action, markers map to the intended ticks, the
GLB-rest offsets match the in-game normalized pivots, and the output path is
printed and loadable by an avatar profile. Server/client combat marker timing
remains separate implementation work.
