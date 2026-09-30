# Hybrid arm sway correction

Time: 2026-09-30T11:09:00-04:00 (America/New_York)
Branch: `afad20a-rebuild`

## Result

Implemented the requested hybrid-mode arm behavior. Arms now preserve the
animation/aim pose relative to the torso after joints and collision run, while
`physical.hybrid.arms_follow_force` controls how quickly movement-induced sway
is corrected. Higher values reduce sway; `0` leaves the arm fully physical.

## Exact changes

- `src/ragdoll/ragdoll-mode.cpp`
  - Removed the arm-only multiplier from the pre-physics hybrid rate in
    `RagdollModeSystem::applyHybridSprings`. The old value was multiplied into
    an already saturated rate before physics.
  - Added `RagdollModeSystem::stabilizeHybridArms` at lines 375-402. It builds
    an arm target relative to the current torso and applies an exponential
    correction after integration, joints, self-collision, and world collision.
  - Calls the new correction from `RagdollModeSystem::updateAim` at lines
    551-552.
- `src/ragdoll/ragdoll-mode.h`
  - Added the private `stabilizeHybridArms` declaration.
- `src/ragdoll/physical-aim.h`
  - Updated the `hybridArmsFollowForce` contract to describe its new
    post-physics sway-rate meaning.
- `config/ragdoll.json`
  - Updated the `arms_follow_force` comments to match the new contract.
  - The active values changed during the session from `follow_force=100.0`
    and `base_rate=50.0` to `follow_force=10.0` and `base_rate=10.0`; this was
    preserved as current worktree state and was not intentionally changed by
    this fix.

## Documents and focused review

Read and followed:

- `docs/ROUTER.md`
- `docs/specs/movement/movement.md`
- `docs/architecture/collision/collision.md`
- `docs/architecture/json-configuration/json-configuration.md`
- `docs/architecture/live-development/ragdoll-live-network.md`
- `docs/architecture/live-development/live-development.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/regressions/README.md`

Finding: the old implementation used an absolute world-space arm target and
applied `base_rate * follow_force * arms_follow_force` before physics. With the
old observed values this reached 250,000/s, so changing the arm value was
effectively saturated and could not control post-joint sway.

## Validation

- `git diff --check`: passed.
- First `python build_agent.py`: returned `SUCCESS` but `Nothing changed`; it
  skipped the changed ragdoll object and was not accepted as compile proof.
- Removed only the exact stale build artifacts
  `build/obj-debug/ragdoll_ragdoll-mode.o` and `.d`, then rebuilt.
- Second `python build_agent.py`: `BUILD SUCCESS`, `Compiled: 1`,
  `Skipped: 496`, return code `0`; linked `C:\mimita-v9\mimita.exe` at
  2026-09-30 11:07:52.
- The attempted `--ragdoll-slice-selftest` invocation launched the full client
  instead of a headless self-test, so no self-test pass is claimed. That test
  client was stopped; the two pre-existing `.dev\builds\0515\mimita.exe`
  processes were left running.

## Human review still required

Launch the newly built executable, use hybrid aimbody mode, and move left and
right. Confirm that the arms still move naturally but remain closer to the
centered torso-relative angle, and tune `arms_follow_force` (currently `50.0`).
Build success is not visual acceptance.

## Follow-up: camera-directed arm scale

Time: 2026-09-30T11:29:50-04:00 (America/New_York)

Added `physical.hybrid.arms_camera_follow` in `config/ragdoll.json` and wired
it through `RagdollModeConfig` / `PhysicalAimConfig`.

- `1.0` leaves the previous hybrid arm correction path unchanged.
- `10.0` blends the arm target 90% toward the camera-look orientation and
  applies the extra arm correction at 9x the base rate, while retaining only
  1/10 of arm angular velocity. The torso, legs, and other body parts do not
  use this value.
- The camera target is built from `camForward` with the arm's existing
  torso-relative pose, so it does not replace the arm's model/weapon offset.

Follow-up build: `python build_agent.py` returned `BUILD SUCCESS`, compiled 3
translation units, skipped 494, and linked `C:\mimita-v9\mimita.exe`.
The additional compiled files were pre-existing unrelated work and were not
modified by this task. Visual left/right movement acceptance remains pending.

Pre-existing unrelated work was preserved: `config/accounts/default.json`,
`config/analytics.json`, `docs/specs/20260930plan.md`, and the pre-existing
`docs/changelog/2026-09-30/20260930_121500-destructible-crate-audit.md`.

## Follow-up: weapon-frame camera correction

Time: 2026-09-30T11:50:35-04:00 (America/New_York)

The screenshots showed that the camera-follow target was being created in the
arm frame, while the attached weapon uses local `+Y` as its barrel-forward
axis. That caused the arms to point sideways at high values and to flip when
the camera pitched up or down.

Updated `stabilizeHybridArms` to aim the actual weapon forward direction from
`camForward`, then convert that desired weapon rotation back through the
existing `player.weaponLocalToArm` transform. The arm correction still blends
with the physical torso-relative pose, so `arms_camera_follow: 1.0` remains
the baseline and higher values only increase camera influence.

Follow-up build: `python build_agent.py` returned `BUILD SUCCESS`, compiled 13
translation units, skipped 484, and linked `C:\mimita-v9\mimita.exe`.
Visual left/right and up/down acceptance remains pending.

## Follow-up: use the real weapon barrel axis

Time: 2026-09-30T12:00:00-04:00 (America/New_York)

The latest screenshot showed the direction was now correct in shape but
rotated approximately 90 degrees downward. The camera helper assumes local
`+Y` is forward, while the equipped weapon's actual grip-to-muzzle axis is
different.

The camera target now measures `player.weaponMuzzleLocal -
player.weaponGripLocal` and computes the axis correction that maps that real
barrel direction onto the helper's local `+Y` direction. This automatically
applies the needed 90-degree correction with the correct sign before the
existing weapon-to-arm conversion.

Follow-up build: `python build_agent.py` returned `BUILD SUCCESS`, compiled 1
translation unit, skipped 496, and linked `C:\mimita-v9\mimita.exe`.
Visual acceptance remains pending.

## Follow-up: regression tracker and attempt-5 diagnosis

Time: 2026-09-30T12:03:55-04:00 (America/New_York)

Added the append-only tracker
`docs/regressions/2026-09-30/hybrid-arm-camera-aim-REG.md`.

The tracker records four attempted fixes, the exact code paths and builds, the
weapon-specific evidence, and the planned fifth attempt. The current finding
is that high `arms_camera_follow` values blend 90% toward a weapon-frame target
instead of the exact per-arm default-mode target. The next implementation
should blend physical arm orientation toward that default arm target and keep
weapon attachment transforms weapon-specific.

No gameplay code was changed in this documentation-only follow-up.

## Follow-up: attempt 5 default-arm target blend

Time: 2026-09-30T12:07:26-04:00 (America/New_York)

Implemented the fifth attempted fix from the regression tracker. Removed the
weapon-derived camera orientation from `stabilizeHybridArms`. Values above
`1.0` now blend each arm toward its own `captureAimTargets()` pose, which is
the pose used by the normal animation/aim pipeline, while retaining the
physical body and reducing movement sway. The existing weapon viewmodel keeps
ownership of weapon-specific grip, muzzle, and attachment transforms.

Build: `python build_agent.py` returned `BUILD SUCCESS`, compiled 1 translation
unit, skipped 496, and linked `C:\mimita-v9\mimita.exe`.

Human visual review is intentionally left open in
`docs/regressions/2026-09-30/hybrid-arm-camera-aim-REG.md`.

## Follow-up: arm-only default/hybrid switch

Time: 2026-09-30T12:09:09-04:00 (America/New_York)

Added hot-loaded `config/aimbody.json` setting `arms_mode`:

- `"hybrid"` preserves the physical hybrid arm behavior;
- `"default"` makes only the arms use their default aimbody/animation target,
  while the torso, legs, and rest of the body remain hybrid/physical.

Build: `python build_agent.py` returned `BUILD SUCCESS`, compiled 7 translation
units, skipped 490, and linked `C:\mimita-v9\mimita.exe`.

Human visual review remains open in the regression tracker.
