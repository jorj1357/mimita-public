# Juggernaut bat authoring contract

- Result: PASS_WITH_HUMAN_REVIEW (specification only)
- Timestamp: 2026-11-19T00:47:00Z (2026-11-18 19:47 EST)
- Branch/commit: current checkout; no commit created
- Pre-existing working changes: preserved. Initial inspection found `config/accounts/default.json`, `config/analytics.json`, `devscripts/blender-ai`, `docs/ROUTER.md`, `docs/specs/gamemodes/gamemodes.md`, `docs/changelog/2026-11-18/20261118_111500-zombie-attack-mode-spec.md`, `docs/changelog/2026-11-19/`, `docs/specs/creators/`, and `docs/specs/gamemodes/zombie-attack.md` modified/untracked. A later status also showed `src/gui/gui-main.cpp` and `docs/regressions/networking/gui-room-startup-white-screen-REG-2026-11-18.md`; this session did not edit them.

## Request and decisions

The user asked for a large dense Juggernaut bat, reliable melee contacts, at
least 10 damage for a barely registered hit, proximity-scaled damage during
active ticks, visible red contact spheres, and in-engine model/animation/pose
authoring. The user selected including editor authoring and then confirmed the
first authoring slice should use primitive parts plus keyframes.

## Findings

- The current Map Editor edits map data and GLB node positions; it does not
  create reusable weapon assets or author skeletal/weapon animation clips.
- `force_punch` is a `QuickHit` definition with a one-tick active phase and
  `minDamage` 5. Its server contact geometry is a sphere centered at a
  hand-derived point, not an authored bat capsule.
- Authored Melee actions already have phase clips and attached-part hitboxes,
  but no in-engine weapon model/keyframe authoring surface was found.
- Shared physical contact and server damage owners exist; no runtime behavior
  was changed in this session.

## Files and reasoning

- Added `docs/specs/creator-tools/weapon-authoring.md` defining ownership,
  persistence boundaries, primitive/keyframe editor workflow, shared contact
  validation, bat-specific minimum/falloff, mode integration, and acceptance
  evidence. Weapon assets are owned by item identity and must not be serialized
  as map geometry.
- This resolves the authored contract without claiming that a map object or
  visible debug sphere is already a functional weapon. Implementation remains
  required.

## Documents and focused review

- Read `docs/ROUTER.md`,
  `docs/specs/dev-loop/agent-lanes-build-authority.md`,
  `docs/specs/weapons/weapons.md`,
  `docs/specs/networking/networking.md`,
  `docs/specs/networking/contact-authority.md`,
  `docs/specs/entity-editor/entity-editor.md`,
  `docs/specs/animations/animations.md`,
  `docs/specs/npc-behavior/npc-behavior.md`,
  `docs/specs/avatar/avatar-editor.md`,
  `docs/specs/actor-presets/actor-presets.md`,
  `docs/architecture/collision/collision.md`,
  `docs/specs/debug-logging/debug-logging.md`,
  `docs/workflows/runtime-scenario-validation.md`,
  `docs/regressions/README.md`, and
  `docs/architecture/time-and-formatting/time-and-formatting.md`.
- Applied `docs/skills/spec-behavior-review-v1.md`: PASS_WITH_HUMAN_REVIEW.
  Findings are recorded above; implementation and live acceptance remain open.

## Validation and review still needed

- No tests, build, dev-loop action, or runtime validation was run; this session
  changed documentation only.
- The bat is not yet present in the Juggernaut loadout, and neither the bat
  model nor its animation is implemented.
- Implement the editor/asset/runtime path, then follow the repository's live
  dev-loop scenario workflow. Review close/edge damage, active-phase hit
  volumes, per-swing duplicate policy, Force Punch compatibility, and the
  visual bat model in-game before calling the feature complete.
