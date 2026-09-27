# NPC animation hot-boundary slice

- EST timestamp: 2026-09-24 16:50:00
- Result: PASS_WITH_HUMAN_REVIEW
- Focused skill: `docs/skills/spec-behavior-review-v1.md`

## Comparison

`afad20a` uses idle, walk, and return-to-idle locomotion, dash/freeze pose
overlays, weapon arm poses, and spring easing. The current hot animation code
already contains that evaluator in `hot-animation-afad20a.h` and applies poses
through the generic `skeleton.apply` capability.

## Change

`src/hot-reload/modules/actor-movement-system.cpp` now seeds the persistent
generic `AnimationState` component for NPC actors when they first enter the hot
actor path. This makes NPCs discoverable by `hot.animation-policy` and
`hot.pose-generation`, so NPC animation policy, C++/JSON clip selection,
weapon arm poses, and pose edits remain inside the existing hot DLL boundary.
No NPC-specific cold animation system was added.

## Validation

- First candidate failed because the new file needed the existing
  `hot-action.h` action identifiers; the active generation was unchanged.
- Corrected candidate built successfully as
  `build/hotreload/mimita-live-g000035.dll`.
- The live-build path did not write or relink `mimita.exe`.
- Human visual acceptance remains required: spawn an NPC, edit
  `config/animations.json` or hot animation C++, observe idle/walk/dash/freeze/
  weapon pose changes in the existing running EXE, then verify failed-candidate
  rollback preserves the prior pose generation.
