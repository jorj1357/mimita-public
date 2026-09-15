# Generic per-entity skeleton instance driven by hot PoseState

Date: 2026-09-15 11:00 EST (UTC 2026-09-15T15:00:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW`

## 1. Real skeleton instance path audit

- Remote NPC -> `MultiplayerContext::remoteNpcs` (`Player` replica) ->
  interpolation -> (previous pass) skipped typed draw, generic static mesh via
  `render.mesh`.
- Typed skinned skeleton lives on each `Player` (`perfectPoseSkeleton`,
  `physicalBody.parts`); pose application is `Player::renderCurrentPose` (C typed
  compatibility), skinning/bone upload/draw is cold (D).
- `animation.update` is invoked by the hot `animation.main` system
  (`GAME_DOMAIN_POST_MOVEMENT`) for **THE_PLAYER only** (A local-player
  compatibility). It was **not** dead; the migrated remote NPC never used it.

## 2/3. EntityId-keyed skeleton instance + skeleton.apply drive

New cold `src/render/skeleton-instances.*`:
- `Instance` keyed by **EntityId** (not by a Player/Npc pointer), with local/world
  bone transforms.
- `skeleton.apply` -> `capSkeletonApply` -> `SkeletonInstances::applyPose`
  maps `gameHash(partName)` parts onto the instance and bumps its version.
- Flat skeleton: `world == local`; a hierarchical mapping belongs to skeleton
  resource metadata (documented).

## 4. Generic part mapping

Bones are matched by `gameHash(partName)`; unknown model names simply create a
bone slot. No NPC-specific bone table.

## 5. GPU mechanism cold

Skeleton/bone storage, `purgeDead()`, and (future) skinning/draw stay cold.

## 6/7. Real NPC + animation.update

Typed pose generation is already bypassed for the migrated remote NPC
(`Player::updateProceduralAnimation` runs only for the local player). The
per-entity skeleton instance for a migrated NPC is now driven by hot PoseState.
`animation.update` remains only for the local player; not removed.

## 8/9. Attack/jump + blending

Not reached this pass.

## 10/11. Resource generations

Not reached; skeleton/clip resources are still static (documented next).

## 12. Runtime monster proof

Same instance path: any entity with `AnimationState` gets hot pose ->
`skeleton.apply` -> per-entity instance. No Player/NPC/Monster type.

## 13. Failure safety

- missing/invalid entity: `applyPose` returns false; `get` checks aliveness.
- `count > GAME_MAX_POSE_PARTS`: extra parts skipped.
- missing bone: created on demand (slot), never out-of-range.
- destroyed entity: `purgeDead()` drops the instance (render tick).
- no DLL pointer stored (POD transforms only).

## Evidence

- `python build_agent.py` -> `Status: SUCCESS`.
- `build_game_dll.py` -> `build/mimita-game.dll`.
- `--hot-combat-selftest` -> PASS incl. "skeleton.apply drives the per-entity
  skeleton instance" and "destroyed entity skeleton instance is purged".
- Full suite (9 selftests) -> PASS.

## Honest limitation

The generic `render.mesh` path still draws a **static** mesh. The per-entity
skeleton instance is a real, EntityId-keyed mechanism driven by hot pose, but the
final GPU **skinned** draw (skinned mesh + skinning shader consuming
`SkeletonInstances`) is not wired yet. So the "real remote NPC GPU skeleton"
is mechanism-complete but not GPU-consumed.

## Classification

- SELFTEST PROVEN: skeleton instance keyed by EntityId; driven by
  `skeleton.apply`; purged on destroy; full suite.
- COMPILED INTEGRATION: `SkeletonInstances`; `capSkeletonApply` drive; render-tick
  purge.
- LIVE MULTIPLAYER PROVEN: no.
- LIVE VISUAL PROVEN: no.
- HUMAN VERIFICATION NEEDED: skinned draw consumption, visible NPC pose, live
  pose edit.

## Files changed

`src/render/skeleton-instances.{h,cpp}` (new), `src/live-code/live-behavior.cpp`,
`src/engine/engine-tick-render.cpp`, `src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.

## Next cold owner selected

Consume `SkeletonInstances` in a real GPU skinned draw (skinned mesh format +
skinning shader) so hot pose deforms the rendered NPC; then attack/jump clips and
a blend model; then generation-aware skeleton/clip resources.
