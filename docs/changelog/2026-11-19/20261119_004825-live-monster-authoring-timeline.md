# Live monster authoring timeline

- Expanded `docs/specs/creators/monster-authoring.md` with an implementable
  in-game editor slice: multipart GLB import and rig mapping, per-tick
  animation editing, attack phase/sphere tracks, live sound binding, and
  isolated individual monster preview.
- Reused existing Avatar Animation JSON/runtime, fixed-tick Melee phase and
  hitbox contracts, and Map Editor document/service boundaries rather than
  specifying a separate animation or damage runtime.
- Evidence inspected: `src/avatar/avatar-editor-animation.cpp`,
  `src/entities/player-avatar-animations.cpp`, Griever/Wasp avatar rigs,
  `docs/specs/animations/animations.md`,
  `docs/specs/animations/blender-animation-authoring.md`,
  `docs/specs/weapons/weapons.md`, and the Map Editor specification.
- Validation: `git diff --check` and related-link checks. No implementation,
  build, runtime, or human acceptance was performed.
- Review instructions used: `docs/skills/spec-writing-v1.md` and
  `docs/skills/spec-behavior-review-v1.md`.
