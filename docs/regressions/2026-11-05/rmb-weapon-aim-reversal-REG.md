# RMB weapon aim reversal

Time created: 2026-11-05T17:34:10Z
Time last updated: 2026-11-05T17:34:10Z

Status: ATTEMPTED FIX (1) — source fix and instrumented build/runtime recheck pending

Related specification:
`docs/specs/weapons/weapons.md` — Aim system

Related changelog:
`docs/changelog/2026-11-05/20261105_173410-rmb-weapon-aim-reversal.md`

## Regression Occurrence 1

### Observed

Time:
`2026-11-05` (reported by user; exact report time unavailable)

### Expected Behavior

While RMB is held, the weapon barrel and its collision direction should follow
camera forward. Revolver, deathball, shotgun, and rocket launcher should agree
at level, upward, and downward camera angles.

### Actual Behavior

The user reports revolver and deathball aim in the opposite direction from the
camera while RMB is held. Shotgun and rocket launcher aim correctly. The
currently running journal contains aim-body samples, but none record RMB held,
so it does not independently capture this report.

### Why This Is Bad

The visible weapon and collision direction disagree with the camera aim, making
RMB aiming unusable and weapon-dependent.

### Specification

`docs/specs/weapons/weapons.md`, Aim system:

> When RMB arm pointing is enabled, the right arm's physical target must account
> for the equipped weapon's measured grip-to-muzzle axis and weapon-to-arm
> transform so the barrel points along camera forward.

### Wrong Code

File: `src/ragdoll/ragdoll-mode.cpp`,
`RagdollModeSystem::applyRightArmPointMotor`

```cpp
if (hasWeaponAimAxis) {
    const glm::quat desiredWorld = aimLookRotation(targetForward, targetUp);
    const glm::quat localWeaponInArm =
        aimLookRotation(armBarrelLocal, armUpLocal);
    target = glm::normalize(desiredWorld * glm::inverse(localWeaponInArm));
} else {
    target = aimLookRotation(targetForward, targetUp) * part.aimOffset;
}
target = glm::normalize(target * glm::quat(glm::radians(
    aimConfig.rightArmPointingRotationDegrees())));
```

### Confirmed Cause

The measured-axis branch solves an arm orientation that maps the weapon's
barrel frame to camera forward, then unconditionally applies the fixed arm
correction. That extra rotation invalidates the solved barrel alignment. The
code-level cause is confirmed by the transform composition; live RMB samples
are still needed to confirm that this is the first runtime divergence for the
reported weapons.

Evidence:

- `applyRightArmPointMotor` uses the grip-to-muzzle axis to solve the target.
- `config/aimbody.json` supplies a nonzero shared correction of
  `[90.0, -90.0, 0.0]`.
- The current dev-loop journal has no `right_mouse_held: true` aim samples.

### Attempted Fix 1

Time:
`2026-11-05T17:34:10Z`

Change:

Apply `rightArmPointingRotationDegrees()` only in the fallback branch where no
valid measured weapon barrel axis exists. Keep recoil as the additive response
after the solved target. Extend the existing bounded aim sample with the
equipped weapon, grip/muzzle, weapon transforms, world barrel direction,
camera-forward dot product, and collision capsule endpoints.

Result:

Source change made. Build and live journal recheck are pending. The existing
live game window did not provide a sustained RMB hold through the available
UI controls, so this attempt is not yet runtime- or visually confirmed.

### Corrected Code

File: `src/ragdoll/ragdoll-mode.cpp`,
`RagdollModeSystem::applyRightArmPointMotor`

```cpp
if (!hasWeaponAimAxis) {
    target = glm::normalize(target * glm::quat(glm::radians(
        aimConfig.rightArmPointingRotationDegrees())));
}
```

### Proof

Human review: Pending. Test revolver, deathball, shotgun, and rocket launcher
with RMB held at level/up/down angles after the rebuilt executable is running.

Automated/runtime proof: Pending. The journal now records the transform fields
needed to compare camera direction, weapon axis, collision capsule, and arm
state. Record the exact dev-loop build identity and client `EVENTS_JSONL_PATH`
when the recheck is performed.
