# Hybrid arm camera aim regression

Time created: 2026-09-30T12:03:55-04:00
Time last updated: 2026-09-30T12:09:09-04:00
Status: ATTEMPTED FIX (5)

## Scope

This tracker covers the hybrid ragdoll arm behavior when
`physical.hybrid.arms_camera_follow` is greater than `1.0`. The desired result
is general weapon behavior, not a revolver-only or rocket-launcher-only offset.

The attached screenshots are visual evidence from playtesting. Their red
handwritten labels are not repository instructions.

## Expected behavior

- `arms_camera_follow: 1.0` keeps the current hybrid behavior.
- Higher values reduce movement-induced arm sway while keeping each arm's
  normal default-mode camera/animation pose.
- At `10.0`, the arms should be strongly blended toward the pose they would
  have in `aimbody.json` default mode, while still retaining some physical
  movement.
- Every weapon should point along the camera-forward line using its own real
  grip, muzzle, and model orientation.
- Looking up or down should rotate the arms like a real attached limb, not
  move the mesh around an incorrect origin or flip the weapon frame.

## Actual behavior

At `arms_camera_follow: 1.0`, weapons generally aim near the center and the
current behavior is acceptable. At `10.0`, the screenshots show weapon- and
pose-dependent errors:

- the weapon can point to the side instead of camera forward;
- the weapon can point toward the ground or backward;
- different weapons show different rotation errors;
- editing `animations.json` changes a mesh/animation local pose or origin but
  does not provide a general physical arm-to-camera aiming solution.

This is still not human-confirmed solved. The latest executable compiled, but
the latest axis correction has not yet been visually accepted for all weapons.

## Why this is happening

The important code is in `RagdollModeSystem::stabilizeHybridArms`:

1. `centeredTarget` is the arm's normal torso-relative animation/aim target.
   This is the target that preserves the centered behavior seen at `1.0`.
2. Before attempt 5, when `cameraFollow > 1.0`, the code created
   `cameraArmTarget` from the camera and the equipped weapon's
   `weaponLocalToArm` transform.
3. It then blends toward `cameraArmTarget`:

```cpp
target = glm::normalize(glm::slerp(
    centeredTarget, cameraTarget, cameraWeight));
```

At `10.0`, the old `cameraWeight` was `0.9`, so the arm was mostly following the
weapon-frame target, not the default-mode arm target. The weapon frame is a
different coordinate system and is built from weapon-specific grip, muzzle,
attachment, and animation transforms. Small frame differences therefore become
large visible side/roll/pitch errors when the camera target dominates.

The target is also applied to both `leftArm` and `rightArm`, even though the
weapon attachment is built from the right arm in
`src/combat/weapon-viewmodel.cpp`. That is another reason a weapon-derived
orientation is not a safe general target for both arms.

The current code additionally derives an axis correction from:

```cpp
player.weaponMuzzleLocal - player.weaponGripLocal
```

This is a useful weapon-barrel measurement, but it fixes the weapon frame; it
does not turn the weapon frame into the default-mode arm pose. It cannot by
itself guarantee correct arm presentation for every weapon and both arms.

## Specification and architecture

`config/aimbody.json` documents that:

- `default` preserves immediate aimbody behavior;
- `physical` runs the body as an active ragdoll;
- `hybrid` runs the physical body while also springing limbs toward the
  procedural animation pose.

`docs/architecture/json-configuration/json-configuration.md` requires one
configuration owner, a documented loader, validation, and an explicit reload
boundary for new JSON values.

The requested arm-only mode is therefore best represented as an explicit
arm policy in `config/aimbody.json` (for example an `arms_mode` value with
`"hybrid"` and `"default"` options), while the numeric blend remains in the
physical/hybrid tuning owned by `config/ragdoll.json`. A boolean can work for
the first switch, but a named mode avoids making a future third arm policy
ambiguous.

## Attempt history

### Attempt 1: make `arms_follow_force` a post-physics sway correction

Changed `src/ragdoll/ragdoll-mode.cpp` so arm correction happens after physics,
joints, and collision. Removed the old pre-physics multiplication of
`base_rate * follow_force * arms_follow_force`, which could saturate around
250,000 per second and make the setting appear ineffective.

Result: movement sway became controllable, but this did not create a default-
mode arm target.

### Attempt 2: add `physical.hybrid.arms_camera_follow`

Added the new numeric value through:

- `config/ragdoll.json`;
- `src/ragdoll/ragdoll-mode-config.cpp`;
- `src/ragdoll/physical-aim.h`.

At `1.0`, the old hybrid path remains active. At `10.0`, the first version
blended the arms toward a generic camera orientation and retained less angular
velocity.

Result: the arms followed camera influence more strongly, but the target did
not preserve the actual weapon/arm frame.

### Attempt 3: convert the camera target through `weaponLocalToArm`

Changed `stabilizeHybridArms` to create a camera weapon rotation, then convert
it through the existing `player.weaponLocalToArm` rotation before applying it
to the arms.

Result: closer, but screenshots showed sideways aiming and pitch/up-down
reversal. The camera target was still not the same target used by default mode.

### Attempt 4: correct the weapon's measured barrel axis

Changed `src/ragdoll/ragdoll-mode.cpp` to measure the weapon's local
grip-to-muzzle axis and rotate that axis onto the helper's local `+Y` forward
axis before converting into the arm frame.

Build proof: `python build_agent.py` returned `BUILD SUCCESS`, compiled 1
translation unit, skipped 496, and linked `C:\mimita-v9\mimita.exe`.

Result: visual review of all weapons is still pending. This attempt corrects a
weapon-axis mismatch, but it may not solve the larger problem that the code is
blending arms toward a weapon target instead of the default-mode arm target.

## Attempt 5: blend toward the real default arm pose

The weapon-frame camera target was removed from `stabilizeHybridArms`. The
implementation now uses the exact per-arm target captured by
`captureAimTargets()` before physics:

1. `hybridPhysicalTarget`: the current physically simulated arm orientation;
2. `defaultArmTarget`: the exact per-arm target produced by the normal
   aimbody/animation path, including the arm's own local bind/mesh frame.

The numeric blend is now applied only between those arm targets:

```cpp
float defaultBlend = 1.0f - 1.0f / armsCameraFollow;
target = glm::normalize(glm::slerp(
    hybridPhysicalTarget, defaultArmTarget, defaultBlend));
```

At `1.0`, the blend is zero and behavior is unchanged. At `10.0`, the arms
are 90% closer to their real default-mode pose. The weapon continues using its
existing per-weapon attachment transform, rather than being used as the
universal arm orientation target.

Changed:

- `src/ragdoll/ragdoll-mode.cpp`: removed camera/weapon orientation creation
  from `stabilizeHybridArms`; high values now blend toward each arm's captured
  default target and increase correction speed.
- `src/ragdoll/ragdoll-mode.h`: simplified the correction function because it
  no longer needs the player weapon transform or camera vector.
- `src/ragdoll/physical-aim.h` and `config/ragdoll.json`: clarified that the
  value blends toward the default aimbody/animation pose, not a weapon frame.
- `config/aimbody.json`: added `arms_mode`, with `"hybrid"` preserving the
  physical arm behavior and `"default"` driving only the arms toward their
  normal default pose while the rest of the body stays hybrid.
- `src/entities/aimbody-config.h` and `src/entities/aimbody-config.cpp`: added
  hot-loaded parsing and ownership for `arms_mode`.

Build proof: `python build_agent.py` returned `BUILD SUCCESS`, compiled 7
translation units, skipped 490, and linked `C:\mimita-v9\mimita.exe`.

## Human review — leave open

Reviewer: ____________________

Review date/time: ____________________

Executable tested: ____________________

Weapons tested: revolver / shotgun / rocket launcher / other: _____________

Result at `arms_camera_follow: 1.0`: _________________________________

Result at `arms_camera_follow: 10.0`: ________________________________

Looking level/up/down: ______________________________________________

Moving left/right: _________________________________________________

Human result: PASS / FAIL / NEEDS ANOTHER ATTEMPT

Notes:

______________________________________________________________________

______________________________________________________________________

Add an explicit arm-only policy in `config/aimbody.json` only after confirming
where the default-mode per-arm target is owned. The policy should support:

```json
"arms_mode": "default"
```

for default-style arm aiming while the rest of the body remains hybrid, and
`"hybrid"` for the current physical arm behavior. The numeric blend should
remain available so the mode can transition gradually instead of snapping.

## Proof still required

- Compare `arms_camera_follow: 1.0` and `10.0` with revolver, shotgun, and
  rocket launcher.
- Look level, up, and down.
- Move left/right and verify only the intended physical sway remains.
- Confirm the weapon barrel and both arm meshes remain centered on camera
  forward.
- Confirm `animations.json` is not being used as a substitute for the physical
  arm-target owner.
- Human visual acceptance is required before changing the status to a solution.

## Related records

- `docs/changelog/2026-09-30/20260930_110900-hybrid-arm-sway-correction.md`
- `docs/regressions/2026-09-20/cold-build-required-REG.md` occurrences 26,
  27, 30, 31, and 32
