# RMB weapon aim reversal

Time created: 2026-11-05T17:34:10Z
Time last updated: 2026-11-05T19:32:14Z

Status: ATTEMPTED FIX (2) — final-build runtime direction proof passed;
human visual confirmation pending

Related specification:
`docs/specs/weapons/weapons.md` — Aim system

Related changelog:
`docs/changelog/2026-11-05/20261105_125610-rmb-aim-fcs-loadout-debug.md`

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
camera while RMB is held. Shotgun and rocket launcher aim correctly. The live
Fun Counter Strike journal contains the expanded aim-body fields, but none
record RMB held, so it does not independently capture this report.

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

The weapon-frame correction is present in source and the identified live
build. The current journal emitted 139 aim samples with weapon barrel,
camera-forward dot, and collision-capsule fields; zero samples had
`right_mouse_held=true`. The available Windows UI click did not create a
sustained hold. The aimed runtime result and visible barrel direction remain
unconfirmed.

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

Runtime evidence: build ID
`2026-11-05T17:52:25.918603Z:de51dc23ec632b6f78efba1575fc50feed5be90a0e427e525d03262bede40d9d`;
client journal
`logs/2026-11-05/20261105_175404/client-01/events.jsonl`. The sample confirms
the required transform values are available, but the game was not observed
with RMB held. Human review remains pending.

### Correction to Attempt 1 Cause

Time:
`2026-11-05T19:32:14Z`

The unconditional arm correction was one target error, but live held-RMB
evidence showed it was not the remaining first divergence. The arm motor
rotates the canonical physics body. `player.weaponLocalToArm`, as produced by
`WeaponViewModel`, maps weapon model space to the rendered arm-node frame.
`RagdollModePart::meshLocal` maps the physics body frame to that rendered
node. The solver had omitted this non-identity bind transform when measuring
the weapon axis. For the sampled right arm, this caused the solver's local
barrel axis to differ from the actual model and collision axis. No Revolver or
Deathball per-weapon offset was responsible.

### Attempted Fix 2

Change:

Compose `part.meshLocal * player.weaponLocalToArm` before converting the
grip-to-muzzle and weapon-up axes into the arm target frame. Leave the arm
position spring, recoil response, and per-weapon configuration unchanged.
Extend the existing bounded aim sample with the right-arm bind transform and
the resulting physics-arm-local weapon axis. Add the outgoing shot start,
direction, end, and camera-forward dot to the existing local attack-request
event.

Add `agent.hold <primary|secondary> <duration_ticks>` as a fixed-tick
gameplay-only input command (1–3600 ticks), retaining `agent.holdprimary` as
the primary alias. The hold scheduler writes correlated accepted/applied/
released journal events. Its secondary state feeds the arm input frame,
camera aim FOV, and weapon input through the shared gameplay mouse resolver;
GUI input is unchanged.

### Proof

Build: dev-loop published source generation 4, compiled 1 changed C++ source,
and reported `BUILD SUCCESS`. Build ID:
`2026-11-05T19:22:50.002621Z:1c821a2565f74d67f68ea032ae2aa3ef4b401985fff94654c89c1e1db97ae3bb`.

Live run: Fun Counter Strike launch mode 45, run `20261105_192259`, with the
same build ID on client and server. The server journal recorded weapon set 30
at actor spawn; the client journal recorded each normal equip command and the
actual equipped weapon in its held-RMB samples.

Client journal:
`logs/2026-11-05/20261105_192259/client-01/events.jsonl`.

For each weapon and angle, four held-RMB `AIMBODY_LIVE_SAMPLE` records were
captured. The last settled sample in each case had the following barrel and
collision-capsule dot products against camera forward:

| Weapon | Level | Up | Down |
|---|---:|---:|---:|
| Revolver | 0.99962 | 0.99956 | 0.99967 |
| Deathball | 0.99970 | 0.99963 | 0.99975 |
| Shotgun | 1.00000 | 1.00000 | 1.00000 |
| Rocket Launcher | 1.00000 | 1.00000 | 1.00000 |

The collision capsule direction also agreed with camera forward in all 12
cases; its lowest dot was 0.99956. One normal Revolver shot was accepted by the
server. The client's `weapon.attack.local_request` recorded the outgoing
direction and a fired-direction/camera-forward dot of `0.99999`.

Human visual review: Pending. The live window was present, but visual barrel
alignment has not been confirmed by a human. Do not mark this regression
solved until that review is recorded.
