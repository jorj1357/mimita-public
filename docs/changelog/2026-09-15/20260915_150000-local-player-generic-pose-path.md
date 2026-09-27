# THE_PLAYER migrated onto the generic entity + hot animation/pose path

Date: 2026-09-15 15:00 EST (UTC 2026-09-15T19:00:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW`

## 1. THE_PLAYER old ownership chain

THE_PLAYER -> `animation.main` (hot) -> `animation.update` bridge ->
`Player::updateProceduralAnimation` (cold animation policy + pose) ->
`Player::renderCurrentPose` (skeleton/skinning/draw). Editing
`pose-generation.cpp` did not affect the local player.

## 2. Canonical local-player entity/state

`Ecs::ensureLocalPlayerEntity()` is projected each frame
(`PresentationEntities::projectLocalPlayer`) with `Transform`, `Velocity`,
`Health`, `PresentationState` (`mesh.actor`), `AnimationState`. No
`LocalPlayerPresentationState`/`PlayerPoseState`/`FirstPersonAnimationState`.

## 3. Hot animation-policy integration

`hot.animation-policy` enumerates `AnimationState` and selects idle/move/dead for
the local entity exactly as for remote actors (Velocity/Health are projected).

## 4. Hot pose-generation integration

`hot.pose-generation` enumerates `AnimationState` and calls `skeleton.apply` for
the local entity too; `PresentationEntities::applyHotPoseToPlayer` maps the
resulting `SkeletonInstances[localEntity]` pose onto `physicalBody.parts[].pose`
before the typed body draw. Editing `pose-generation.cpp` therefore changes
THE_PLAYER at the architecture level.

## 5. skeleton.apply integration

Same EntityId-keyed `SkeletonInstances` mechanism; no Player pointer as pose
identity.

## 6. animation.update remaining callers

`animation.main` no longer calls `animation.update` (no-op). The capability
remains registered for replay/legacy compatibility (A/B). No other caller.

## 7. Cold Player responsibilities remaining

Body meshes, skeleton decode, skinning, first-person/weapon draw = mechanism.
`updateProceduralAnimation` still exists but is no longer the local animation
policy owner (it runs only if a movement `source` path invokes it).

## 8. Does editing pose-generation.cpp now affect THE_PLAYER?

At the architecture level yes: the local entity is enumerated by
`hot.pose-generation` and its pose is applied to the visible body. Visual
confirmation was not performed (no screen).

## Evidence

- `python build_agent.py` -> `Status: SUCCESS`.
- `--hot-combat-selftest` -> PASS incl. "local player has a canonical generic
  entity" and "local player entity is driven by the hot pose path".
- Full suite (9 selftests) -> PASS.

## Architecture-first rule recorded

Added to `hot-kernel-next-steps.md`: prioritize ownership transfer over polish;
a subsystem is migrated when its ordinary behavior owner is hot, state is
generic, cold code is mechanism-only, a running generation can replace the
algorithm, failure keeps last-good, and no concept-specific ABI was added.

## Classification

- SELFTEST PROVEN: local player entity projected; driven by the hot pose path.
- COMPILED INTEGRATION: `projectLocalPlayer`; `applyHotPoseToPlayer`;
  `animation.main` yield.
- LIVE HOT-EDIT PROVEN: no (no screen).
- HUMAN VERIFICATION NEEDED: edit `pose-generation.cpp` while running and confirm
  the local player's visible pose changes; verify camera/first-person still
  correct; verify no duplicate body.

## Files changed

`src/render/presentation-entities.{h,cpp}`, `src/render/skeleton-instances.{h,cpp}`,
`src/engine/engine-tick-render.cpp`,
`src/hot-reload/modules/animation-system.cpp`,
`src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.

## Next cold owner selected

Move on to the next cold behavior owner per the architecture-first order:
effects spawning/update policy, then audio behavior/playback policy, then weapon
presentation, then nameplates/health overlays, then UI interaction.
