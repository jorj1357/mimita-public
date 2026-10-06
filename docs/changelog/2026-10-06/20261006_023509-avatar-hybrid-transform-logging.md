# Avatar hybrid transform logging

Time (EST): `2026-10-06 02:35:09 -04:00`
Branch: `afad20a-rebuild`
Status: `PASS_WITH_HUMAN_REVIEW`

## Request

Add live structured logging for the girl-body rotation investigation so the
running executable writes inspectable transform evidence to `events.jsonl`.

## Changes

- `src/ragdoll/ragdoll-mode.cpp:54-140`: added bounded Avatar-category
  snapshots for the `plrOrigin` root and every aim-body part. Each record
  includes avatar name, node indices, node world matrix/quaternion, physics
  position/orientation, `meshLocal` translation/matrix/quaternion, determinant,
  and the skeleton matrix.
- `src/ragdoll/ragdoll-mode.cpp:312-313,860-862`: emits one `bind` snapshot
  when the hybrid aim body is created or rebound and one `post_sync` snapshot
  after the physical pose is written back to the skeleton. It does not log per
  frame.
- `src/ragdoll/ragdoll-mode.h:227`: added the one-shot pending flag.
- `config/debuglogger.json:42-44`: enabled Avatar `important` logging so the
  records reach the canonical, immediately flushed `events.jsonl` stream.

Event names:

- `AIMBODY_AVATAR_BIND_ROOT`
- `AIMBODY_AVATAR_BIND_PART`

## Reasoning

The logger is the existing `StructuredLogger` owner. No gameplay-local file
writer or raw debug file was added. The bind and post-sync stages let the next
run distinguish a loaded GLB/body-part frame problem from a hybrid physics
writeback problem.

## Validation

- First cold build: `BUILD SUCCESS`, 2 translation units compiled, 525 skipped.
- Second unique-output build: `BUILD SUCCESS`, 0 compiled, 527 skipped;
  produced `mimita-20261006T023600-avatar-log.exe`.
- `git diff --check` passed for the changed files; an unrelated pre-existing
  whitespace warning remains in `config/behavior-profiles.json`.
- Fresh executable PID `28452` started successfully and created
  `logs/10-06-2026/20261006_023235/events.jsonl`.
- The executable loaded the girl GLB and six avatar body-part overrides. It
  remained at the menu during this run, so no `AIMBODY_AVATAR_BIND_*` record
  was emitted yet. The journal path and immediate logger write were confirmed.

## Pre-existing edits

The worktree already contained unrelated changes across configuration, audio,
networking, NPC, collision, rendering, and other files. They were preserved.
The existing MiMITA processes under `.dev/builds/1531` and `.dev/builds/1520`
were not stopped. The fresh root executable remains running for the next
playable-state test.

## Human review still required

Enter gameplay with the girl avatar and hybrid aimbody active, then search the
same run's `events.jsonl` for `AIMBODY_AVATAR_BIND_ROOT` and
`AIMBODY_AVATAR_BIND_PART`. Compare the `bind` and `post_sync` matrices for
the torso and arms against a default-body run. The log proves the transform
path was exercised; it does not by itself prove the visual lean is fixed.

## Routed documents and focused review

- `docs/ROUTER.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/logging-checker-v1.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`

Logging review result: `PASS_WITH_HUMAN_REVIEW`. The diagnostic is owned by
the avatar/hybrid bind path, bounded to bind transitions, structured, and
written through the shared JSONL logger.

## Follow-up: named avatar run

- `src/ragdoll/ragdoll-mode.cpp:67-80`: the diagnostic identity now falls
  back from `Player::avatarName()` to `Player::avatarInstance->name`, then to
  an explicit `<avatar-name-unset>` marker instead of silently emitting an
  empty string.
- Fresh executable: `mimita-20261006T024000-avatar-log-named.exe`.
- Build: `BUILD SUCCESS`, 1 translation unit compiled, 526 skipped.
- Running PID: `1036`.
- Verified journal:
  `logs/10-06-2026/20261006_024026/events.jsonl`.
- Runtime evidence: 28 avatar bind records, 14 `bind` and 14 `post_sync`,
  all identified as `abusivegirl3`.
- The six normal parts have `mesh_local_basis_determinant = 1`; the
  `rightLeg` has `-1` in both stages, confirming the avatar's intentional
  mirrored right-leg transform is present in the live bind.

## Follow-up: bounded live hybrid samples

- `src/ragdoll/ragdoll-mode.cpp`: added `AIMBODY_LIVE_SAMPLE` through the
  existing Avatar structured logger, sampled once per 30 simulation ticks after
  final skeleton writeback. Each sample includes camera position/forward,
  camera pitch/yaw, player root yaw, hybrid/arms mode, procedural look-pitch
  state, limb roll/pitch/yaw gains, hybrid target orientation, physics
  orientation/angular velocity, final mesh-node and body orientation, and
  target-to-physics, final-to-physics, and target-to-final angle deltas.
- `src/ragdoll/ragdoll-mode.h`: added per-aim-body sampling state so logging is
  bounded and resets on activation/rebind/deactivation.
- Build: `BUILD SUCCESS`, 1 translation unit compiled, 526 skipped.
- Fresh executable: `mimita-20261006T024900-aim-live.exe`, PID `11004`.
- Runtime journal:
  `logs/10-06-2026/20261006_024907/events.jsonl`.
- Runtime verification: 3 `AIMBODY_LIVE_SAMPLE` records appeared. The live
  sample identified `abusivegirl3`, `mode=hybrid`, `arms_mode=hybrid`, camera
  pitch `10`, root/camera yaw both `-90`, and per-part rotation deltas. For the
  observed sample, final-to-physics deltas were `0` while hybrid target-to-
  physics deltas remained nonzero, showing the final skeleton was faithfully
  following the current physical body at that point.

## Follow-up: player-yaw-frame measurements

- Added per-part physics, hybrid-target, and final rotation vectors relative to
  the player's yaw frame, plus explicit roll-in-player-frame degree fields.
  This isolates a character-space left/right lean from a mesh-local or world
  quaternion difference.
- The running `mimita-20261006T024900-aim-live.exe` produced 174 live samples
  before this additional field was added. Across those samples, final-to-
  physics was `0` degrees for every part, while torso target-to-physics ranged
  from `0.69` to `139.08` degrees. This makes final mesh writeback an unlikely
  source of the lean; the leading suspect is the target/body orientation frame
  or the hybrid body's allowed roll, not the right-leg mirror.
- Build after the measurement change: `BUILD SUCCESS`, 1 translation unit
  compiled, 526 skipped. The linker reported a resource-merge warning while
  the existing root executable was locked, but the build system completed and
  the fresh executable was launched as PID `28756`.

## Estimated fix: restore authored root frame before hybrid capture

- `src/ragdoll/ragdoll-mode.cpp`: before `captureAimTargets`, restore every
  non-body ancestor from `restLocalTransforms` and rebuild skeleton world
  transforms. The existing final sync still clears those ancestors while the
  physics body owns the rendered pose.
- Rationale: `syncAimToPlayer` clears `plrOrigin` to identity. That is harmless
  for the default model, whose root is identity, but the girl model stores its
  90-degree coordinate-frame correction on `plrOrigin`. Without restoring the
  authored root before the next target capture, the hybrid target is captured
  in a frame missing that correction and can appear rotated left.
- Build: `BUILD SUCCESS`, 1 translation unit compiled, 526 skipped.
- Fresh executable: `mimita-20261006T025600-girl-frame-fix.exe`, PID `31260`.
- Human gameplay verification is still required. The new executable was
  launched successfully; its decisive validation is whether the girl body is
  visually upright and whether the new player-yaw-frame fields in
  `AIMBODY_LIVE_SAMPLE` stop showing the fixed frame offset.

## Proper fix: avatar-specific AimBody bind anchors

- `src/ragdoll/ragdoll-mode.h/.cpp`: normal-play AimBody construction now
  derives each parent and child attachment anchor from the avatar's actual
  bind pose and capsule geometry. The configured generic ragdoll anchors remain
  unchanged for full ragdolls, corpses, and replicated bodies.
- The live sample now also records `parent_local_anchor`, `child_local_anchor`,
  and `rest_length` for each limb so the girl/right-leg attachment can be
  checked directly in `events.jsonl`.
- Build: `BUILD SUCCESS`, 13 translation units compiled, 514 skipped.
- Fresh executable: `mimita-20261006T030300-bind-anchors.exe`, PID `32376`.
- Runtime startup: process alive and created
  `logs/10-06-2026/20261006_030111/events.jsonl`; six girl bind-part records
  were observed. Gameplay/visual acceptance remains pending.

## Full-ragdoll extension and gold behavior record

- Extended bind-derived attachment anchors to local ragdoll activation,
  lifecycle rebinds, replicated ragdoll bodies, and corpse bodies, so a girl
  avatar does not fall back to generic default-avatar leg anchors when entering
  ragdoll mode.
- Added the reusable gold reference:
  `docs/gold/2026-10-06-avatar-bind-anchors-and-jsonl-evidence.md`.
  It records the evidence-first workflow: instrument the owner, run the actual
  executable, inspect the active `events.jsonl`, use runtime values to rule out
  causes, then make and verify the narrow fix.
- Human acceptance remains required for both hybrid mode and full ragdoll mode;
  no visual fix claim is made from compilation alone.
