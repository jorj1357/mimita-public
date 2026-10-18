# Blender animation exporter implementation

- Timestamp: 2026-10-18 14:28:23 EDT / 2026-10-18T18:28:23Z
- Branch: `20261011zt`
- HEAD before and after: `f2e91866` (no commit created)
- Result: `PASS_WITH_HUMAN_REVIEW`

## Request and result

Implemented the first Blender add-on for exporting rigid-part MiMITA animation
clips as version-2 JSON. The add-on offers rig root, Action, clip ID/type,
sampling mode, GLB rest-reference path, output path, and optional avatar
`animations.json` profile fields. It samples all six canonical parts at 60 FPS,
derives local transform deltas from the supplied GLB reference, emits a dense
pose for each tick, checks required Melee markers, validates serialized JSON,
and writes via a temporary file plus replacement. The optional profile field
adds the clip path to `include`.

## Exact source changes

### `tools/mimita_animation_exporter.py` (new file, lines 1-334)

Before: no exporter file existed.

After, lines 7-14:

```python
bl_info = {
    "name": "MiMITA Animation Exporter",
    "author": "MiMITA",
    "version": (1, 0, 0),
    "blender": (5, 0, 0),
    "location": "View3D > Sidebar > MiMITA",
    "category": "Import-Export",
}
```

After, lines 189-204 sample rest-relative transforms and write all six parts
for each frame; the key operation is:

```python
delta = rest_local[part].inverted_safe() @ relative
loc, quat, scale = delta.decompose()
```

After, lines 228-299 provide the export operator and visible settings, including
`Interpolate` as the default and optional avatar profile linkage.

### `docs/specs/animations/blender-animation-authoring.md`

Before, lines 3-5:

```text
Status: **PROPOSAL — exporter and shared runtime timeline integration are not
implemented.** This document describes the intended author-to-game pipeline;
it does not claim Blender currently exports runtime-ready MiMITA data.
```

After, lines 3-6:

```text
Status: **SPECIFICATION — first Blender add-on implementation exists; Blender
execution and runtime acceptance are pending.** The exporter writes the shared
version-2 clip shape. Server/client use of phase markers for gameplay timing
is still unimplemented.
```

Before, lines 150-156 left the matrix delta convention open. After, lines
150-158 document `inverse(restRootRelative) * animatedRootRelative`, its
`rest * pose` runtime assumption, and the limitation that the add-on does not
repeat geometry-based avatar normalization. After, lines 368-373 list the
remaining runtime discovery, marker-timing, visual parity, and extension work.

## Scope and evidence

- Existing loader inspection: `src/entities/player-avatar-animations.cpp`
  loads profile `include` entries. This supports the optional profile link.
- Existing animation config parser inspection: `src/entities/player-animation-config.cpp`
  reads version-2 keyframes and duration. The current parser/runtime does not
  consume clip markers for combat phase timing. Server/client phase authority
  therefore remains outstanding and is not claimed as implemented.
- `git diff --check`: passed; it reported only existing LF-to-CRLF conversion
  warnings for the modified animation spec files.
- No Blender executable was available in this environment. The add-on was not
  run in Blender; no Blender UI, exported clip, runtime pose parity, or gameplay
  acceptance is claimed. No test suite or game build was run.

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
and the three earlier changelogs `20261018_181131-animation-timeline-source.md`,
`20261018_181704-blender-exporter-spec.md`, and
`20261018_182333-blender-pose-sampling.md`. This turn additionally edited
`docs/specs/animations/blender-animation-authoring.md` and added the exporter
and this changelog.

## Human review still needed

Install and run the add-on in Blender 5.0 with the supplied reference GLB and a
six-part test Action. Confirm Action-slot binding, frame sampling, imported GLB
cleanup, output/profile linkage, and visual parity against MiMITA's normalized
rest pivots. Follow with shared server/client clip discovery and combat-phase
marker integration before treating Blender-authored timing as authoritative
for hit windows.
