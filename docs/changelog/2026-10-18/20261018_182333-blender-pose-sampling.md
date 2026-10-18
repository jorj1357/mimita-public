# Blender pose sampling decisions

- Local time: 2026-10-18 14:23:33 EDT (America/New_York).
- UTC: 2026-10-18T18:23:33Z.
- Branch: `20261011zt`.
- HEAD before this documentation change: `f2e91866`.
- Result: `PASS_WITH_HUMAN_REVIEW`.

## User decisions recorded

- The exporter is a Blender add-on UI.
- Use the recommended explicit root/part mapping workflow.
- Use `assets/entity/player/default/mimita-char-no-animations-v4.glb` as the
  animation-free reference pose for this avatar.
- Use named Blender timeline markers for action phase boundaries.
- Export every Blender-authored animation as a dense keyframe pose at every
  60 Hz tick.
- Default to Blender-evaluated interpolation between artist-authored keys,
  then sample/write every tick. Offer a direct-pose mode that requires an
  explicit complete authored pose on every tick and rejects gaps.
- Use the canonical Z-up game frame and map Blender frame 1 to game tick 0;
  thus frames 1–60 are game ticks 0–59 and the exclusive endpoint is tick 60.

## Specification changes

Updated `docs/specs/animations/blender-animation-authoring.md` and
`docs/specs/animations/animations.md` to carry these requirements into both the
authoring tool contract and the animation export/runtime contract.

1. `docs/specs/animations/blender-animation-authoring.md`, lines 82–94,
   101–139, 153–205, 263–315, 336–350.
   - Before (control table): “| Sampling | Show source FPS, Action frame range, target 60 Hz, resulting duration, and tick count. |”
   - After (line 90): “| Sampling mode | Choose `Interpolate` (default) or `Direct poses`; always write one complete pose per output tick. |”
   - Before (rest-pose decision): “The exact rest-pose source is **OPEN**: it
     could be the GLB's normalized bind pose or a designated Blender reference
     frame/action.”
   - After (line 122): “**DECIDED for this avatar:** the rest-pose reference is
     `assets/entity/player/default/mimita-char-no-animations-v4.glb`, which
     contains the character without animation.”
   - Added add-on controls, explicit reference GLB, Z-up/+X forward/+Y lateral,
     named phase markers, one-pose-per-tick output, 60 FPS requirement, default
     interpolation mode, direct-pose gap rejection, and detailed validation.

2. `docs/specs/animations/animations.md`, lines 420–439, 447–473.
   - Before (Blender conversion): “Let `B` be the explicit orthogonal basis
     conversion from Blender coordinates to the canonical game coordinates
     (including any axis permutation or handedness policy).”
   - After (line 420): “Use the canonical avatar coordinate contract: Blender
     and MiMITA are Z-up; game forward is +X and lateral is +Y.”
   - Added dense Blender export requirement, Interpolate/Direct poses behavior,
     and frame/tick convention. Hand-authored JSON can still be sparse.

3. `docs/changelog/2026-10-18/20261018_182333-blender-pose-sampling.md`.
   - New file; no prior text. Records user decisions, exact spec changes,
     validation, and implementation boundary.

Pre-existing dirty changes from earlier turns in the same task include the
Melee, weapons, and animation spec alignment and two earlier changelog files.
Those files were not modified in this turn. Other unrelated source/config
changes were preserved.

## Review and validation

- Read `AGENTS.md`, `docs/ROUTER.md`, `docs/specs/animations/animations.md`,
  `docs/specs/animations/blender-animation-authoring.md`,
  `docs/specs/avatar/avatar-editor.md`,
  `docs/skills/spec-behavior-review-v1.md`,
  `docs/skills/documentation-checker-v1.md`, and the routed time/changelog
  procedures.
- `docs/doc-review-09-03-2026.md`, listed by the router, is absent in this
  checkout. Recorded as a warning; no replacement was created.
- Focused documentation/spec review: `PASS_WITH_HUMAN_REVIEW`. User decisions
  are written distinctly from open implementation details. The dense Blender
  export is a target contract; no exporter or game code was implemented.
- `git diff --check` passed. No tests/build/runtime validation were run because
  this is documentation-only work.
- Still open before implementation: exact GLB normalization/pivot matrix math,
  clip registry discovery on server/client, supported Blender version,
  Blender API evaluation equivalence, and non-60-FPS resampling.
