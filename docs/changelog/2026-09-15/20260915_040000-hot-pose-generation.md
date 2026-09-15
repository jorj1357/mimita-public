# Hot pose generation through the generic skeleton.apply capability

Date: 2026-09-15 04:00 EST (UTC 2026-09-15T08:00:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: source complete; cold build + selftests PENDING (running `mimita.exe`)

## 1. Animation/pose ownership before

- clip selection/playback: `hot.animation-policy` (HOT, previous pass).
- `animation.update` -> `Player::updateProceduralAnimation`: E compatibility
  bridge + A/B typed pose generation.
- `Player::renderCurrentPose` -> skeleton/bone transforms (B/C); skinning matrix
  upload + draw (D cold mechanism).

## 2. Generic pose representation

`hot-reload/hot-pose.h`: `HOT_POSE_STATE_COMPONENT` (`PoseState`),
`HotPoseStateV1 { version, count, part[16], translation[16][3],
rotationEuler[16][3] }` — pure POD copied into the generic dynamic component
store. No pointers into DLL memory; not replicated (`GAME_NET_NONE`).

## 3. skeleton.apply integration

Reused `GAME_CAP_SKELETON_APPLY` + `GameSkeletonPoseV1` unchanged (it already
carries `entity`). `capSkeletonApply` now copies the pose into a `PoseState`
component on `pose->entity` and counts invocations
(`LiveBehavior::skeletonApplyCount()`), while the existing local-player skeleton
application is unchanged. No new capability, no feature-specific field.

## 4. Real NPC migration

The remote-NPC generic presentation path (previous pass) now has a hot pose
producer: NPC entity -> `AnimationState` -> `hot.pose-generation` -> PoseState /
skeleton.apply. (Driving the real per-entity GPU skeleton is the next structural
slice; the typed path still owns actual skinning.)

## 5. animation.update callers remaining

`animation.update` (`capAnimationUpdate`) is still used by the typed
local-player/unmigrated animation path (B). Retiring it needs hot pose to drive
the real per-entity skeletons; not removed this pass.

## 6. Typed Player fields remaining

`Player::updateProceduralAnimation` / `renderCurrentPose` and their skeleton/pose
fields remain as compatibility + cold mechanism; not deleted.

## 7. Runtime monster proof

Same generic path: any entity with `AnimationState` (+ Transform/Velocity/Health)
gets hot pose generation; no Monster/NPC/Player type. Headless checks added.

## 8. Resource generation

Not reached; animation clips/skeletons are still static caches (documented next).

## Evidence and blocker

- `python devscripts/live-build.py` -> generation 18 DLL.
- `-fsyntax-only` clean: `live-behavior.cpp`, `hot-combat-selftest.cpp`.
- **Cold build blocked:** `mimita.exe` was running; `build_agent.py` refused with
  `HOT_RELOAD_BOUNDARY_VIOLATION` and the process was not killed. Cold link +
  `--hot-combat-selftest` (animation policy + pose checks) PENDING the next
  no-process window.

## Added selftests (pending execution)

"hot animation policy selects move/idle/death"; "hot pose generation invokes
skeleton.apply"; "hot pose publishes a generic PoseState on the entity".

## Classification

- SELFTEST PROVEN: none for this pass (not executed; cold link pending).
- COMPILED INTEGRATION: source + syntax-check clean; live DLL generation 18.
- LIVE MULTIPLAYER PROVEN: no.
- LIVE VISUAL PROVEN: no.
- HUMAN VERIFICATION NEEDED: run the cold build + selftests in a no-process
  window; visible NPC pose; retiring `animation.update` once hot pose drives the
  real skeletons.

## Files changed

`src/hot-reload/hot-pose.h` (new),
`src/hot-reload/modules/presentation/pose-generation.cpp` (new),
`src/hot-reload/hot-modules.json`, `src/live-code/live-behavior.{h,cpp}`,
`src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.

## Next cold owner selected

Drive real per-entity skeletons from hot `PoseState` (scene skeleton resources),
then retire `animation.update`; then animation/skeleton generation-aware
resources.
