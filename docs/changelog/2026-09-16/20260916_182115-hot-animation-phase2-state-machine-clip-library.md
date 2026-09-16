# Hot animation ownership — phase 2: full state machine and procedural C++ clip library

Date: 2026-09-16 14:21 EDT (UTC 2026-09-16T18:21:15Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_PENDING_COLD_BUILD`

## Task

Phase 2 of the fully hot-reloadable C++ animation migration: replace the
phase-1 demo policy with the complete deterministic state machine, a
procedural C++ keyframe/pose library for every gameplay action, and a
production pose generator (sampling, interpolation, loop/one-shot, transition
blending, per-part masks, weapon arm overrides, dash/freeze overlays, speed
scaling, one-shot interruption rules). Pure hot code plus one cold unit fix.

## What changed

### 1. Procedural clip library (`src/hot-reload/hot-animation-clips.h`, new)
- `HotAnim::AnimationClipSource { ProceduralCpp, ImportedAsset }` extension
  point; only `ProceduralCpp` is active.
- Plain-data `PartPose` / `Pose` / `Keyframe` / `ActionClip` (no STL/pointers).
- Six body parts (torso, head, leftArm, rightArm, leftLeg, rightLeg); masks for
  full/upper/arms/lower; safe hash<->index mapping.
- `sampleClip` interpolates translation+rotation, wraps loops, clamps and holds
  one-shot final frames.
- `evaluateIdle` / `evaluateWalk` (speed-scaled, procedural) and keyframe tables
  for jump, fall, land, dash, down-dash, freeze, equip, shoot, reload, slash,
  lunge, hurt, death, respawn.
- `applyMask`, `blendPose`, `locomotionAction` (single source of truth for the
  locomotion base), `applyWeaponArms` with per-tool carry poses.
- Authored in degrees; converted to the hot boundary unit (radians) only at
  `skeleton.apply`.

### 2. Full state machine (`animation-policy.cpp`)
- Documented precedence implemented highest-first: dead > freeze > dash /
  down-dash > shooting / slash / lunge > reload > equip > hurt > jump / fall /
  land > walk > idle / equipped-idle.
- Reads generic facts only: Velocity, Health, MovementRuntimeState, and
  `ActorActionState` (grounded, dash/down-dash, freeze, shoot/reload/equip/melee
  flags, weapon key, lifecycle generation).
- One-shot actions persist to completion unless a strictly higher-precedence
  action interrupts (rank table). Action changes restart playback and open a
  blend window; a forced respawn (lifecycle-generation change after death)
  bypasses the death hold.
- New local-only replicated-independent `HotAnimationMemoryV1`
  (`AnimationMemory`, `GAME_NET_NONE`) for edge detection (just landed, hurt,
  respawn) and a free-running locomotion clock. Owns the `AnimationState.v2`
  schema and the v1 -> v2 migration.

### 3. Production pose generation (`pose-generation.cpp`)
- Full-body action pose, or upper-body action overlaid on a locomotion base via
  masks (shoot/reload/slash/lunge/equip).
- Weapon carry arm overrides for idle/walk/jump/fall/land.
- Blends from the previously applied `PoseState` during the blend window.
- Writes only `GameSkeletonPoseV1` through `skeleton.apply`; degrees -> radians
  at the boundary. `posedebug` extreme pose retained.

### 4. Cold rotation-unit normalization
- The generic `skeleton.apply` path (`SkeletonInstances`) treats
  `GamePosePartV1.rotationEuler` as radians; the cold typed mirror treated it as
  degrees. Normalized on radians: `poseOffsetMatrix` (live-behavior.cpp) no
  longer calls `glm::radians`, and `applyHotPoseToPlayer` now converts to
  degrees for the legacy typed field.
- `PoseState` schema kept at v1 so live activation of a new generation cannot
  be rejected for a missing migration.

### 5. Selftest assertions added (cold; pending run)
- Phase-2 transitions: idle -> walk, walk interrupted by jump, dash interruption
  and return, freeze begin/hold/end, equip, shooting, reload, slash/lunge,
  death, respawn after lifecycle-generation change.
- Deterministic pose generation: two actors with identical facts produce
  byte-identical `PoseState`.
- Phase-1 assertions retained (v2 contract, migration registered, action-state
  bridge, legacy bridge not invoked).

## Evidence

- Hot DLL build: `python build_game_dll.py` -> `DLL build success:
  build\mimita-game.dll` (54 sources).
- Standalone clip-engine test (compiled with the project toolchain, run out of
  tree): 14/14 PASS — clip metadata, interpolation, one-shot clamping,
  deterministic procedural walk, masking, blending, weapon carry override.
- Cold compile check: `g++ -fsyntax-only` (project flags + PCH) on
  `live-behavior.cpp`, `presentation-entities.cpp`, `hot-combat-selftest.cpp`
  -> exit 0.
- Cold EXE build (`python build_agent.py`): NOT RUN. Two `mimita.exe` processes
  are still running (PIDs 20660, 23824); relinking a running EXE is forbidden.
- Runtime, visual, and human acceptance: NOT performed.

## Pre-existing changes (not mine)

`src/hot-reload/hot-presentation.h`, `hot-reconciliation.h`,
`movement-system.cpp`, `movement-config.*`, `presentation/attachment.cpp`,
`presentation/effect-composition.cpp`, `reconcile-policy.cpp`,
`presentation-render.cpp`, `weapon-viewmodel.*`, and other worktree edits were
already present before this session and were not modified by this work.
`game-api.h` and `hot-combat-selftest.cpp` contain both pre-existing and this
session's edits.

## Blockers / follow-up

1. Cold build required before any runtime proof; blocked on the two running
   `mimita.exe` instances. Until then a live reload into the new DLL pairs it
   with the old cold mirror (degrees) and can show a wrong local pose.
2. Run `--hot-combat-selftest` and `--live-code-selftest` after the cold build.
3. Then live visual proof: edit a walk keyframe in `hot-animation-clips.h`,
   activate, observe, roll back.

## Classification

- COMPILED: hot DLL; changed cold TUs (syntax-only).
- UNIT-PROVEN (out of tree): procedural clip sampling/masking/blending/
  determinism.
- NOT RUN: cold link, in-engine selftests, runtime, visual, human acceptance.
