# aimbody physical mode (active ragdoll during normal play)

Time (UTC): `2026-09-29T13:25:03Z`
Time (local): `2026-09-29 09:25:03 EDT`
Branch: `afad20a-rebuild`
Base commit: `7f74d5f3`

## Summary

Adds an experimental `"physical"` mode to `config/aimbody.json`. When selected,
normal play runs the player as an active ragdoll: the torso is tethered to the
authoritative movement root, head and torso receive a torque from the wished
look direction, and arms/legs inherit motion through the existing joint
constraints. The movement controller stays authoritative; `player.pos`/`vel`
are never written by the body. The physics transforms are written to the
skeleton and therefore to the rendered body and the client damage hitboxes.

Default `aimbody.json` mode remains `"default"`, so normal behavior is
unchanged until the mode is selected.

## Config ownership (decision)

- `aimbody.json` only selects the mode (`default` / `smooth` / `physical`).
- All physical aim tuning lives in `ragdoll.json` under `physical.*`, per
  `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md` section 46.
- A `damping_mode` selector chooses between the historical `look` velocity
  blend and the new `physical` PD torque model.

## Files and exact changes

- `src/ragdoll/physical-aim.h` (new): pure, header-only controller.
  `PhysicalAimConfig`, `PhysicalAimDamping`, `aimLookRotation`,
  `rotationErrorVector`, `aimDesiredAngularVelocity`, `computeAimTorque`.
  No orientation is ever assigned directly.
- `src/ragdoll/ragdoll-mode-config.h`: include `ragdoll/physical-aim.h`; add
  `PhysicalAimConfig physicalAim;` to `RagdollModeConfigData`.
- `src/ragdoll/ragdoll-mode-config.cpp`: parse and clamp `physical.*`
  (`torque_gain`, `angular_damping`, `max_angular_speed`, `head_weight`,
  `torso_weight`, `limb_inheritance`, `torso_tether_stiffness`,
  `damping_mode`). Also added `<algorithm>`.
- `src/entities/aimbody-config.h`: add `physicalMode()`.
- `src/entities/aimbody-config.cpp`: accept `"physical"` as a valid mode
  (invalid/malformed reload still preserves the last valid config).
- `src/ragdoll/ragdoll-mode.h`: add `activateAim`/`deactivateAim`/`updateAim`/
  `aimActive`, private `syncAimToPlayer`/`applyAimMotor`/`tetherAimRoot`, and
  `mAim`/`mAimActive`.
- `src/ragdoll/ragdoll-mode.cpp`: removed the local `lookRotation` (uses
  `aimLookRotation`); `applyControls.aimAtCamera` now honors `damping_mode`;
  implemented the aim-body methods. `tetherAimRoot` pulls the dynamic torso to
  `player.pos - rootRot * rootOffsetLocal`; `syncAimToPlayer` uses the
  authoritative root and writes the skeleton with no render smoothing so the
  hitbox equals the visible transform.
- `src/sim/simulate-tick.cpp`: after the normal `physicsMainUpdate`, run
  `updateAim` when `AimBodyConfig::physicalMode()`; deactivate it when the mode
  is off or ragdoll mode takes over.
- `config/ragdoll.json`: add the `physical` block.
- `config/aimbody.json`: document the `physical` mode (active mode stays
  `default`).
- `tests/physical-aim-torque-test.cpp` (new): standalone controller test.

## Reasoning

`RagdollModeSystem` already owns `initParts`, the joint solver, self-collision,
and world collision, so the always-on body reuses them rather than adding a
second solver. The movement root is a one-way kinematic tether so gameplay
movement is untouched. The look direction is a wished orientation converted to
torque, never a snapped transform.

## Documentation and skills

- Read: `docs/ROUTER.md`, `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md`,
  `docs/operations/build-and-exe/build-and-exe.md`,
  `docs/operations/task-completion/task-completion.md`,
  `docs/regressions/README.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`.
- Spec disagreement recorded: `docs/architecture/live-development/ragdoll-live-network.md`
  describes `Ragdoll::Solver`, `ragdoll-components`, a `ragdoll.solver` domain,
  and `RagdollStatePacket`. None exist in `src` on this branch; the solver is
  inline in `ragdoll-mode.cpp`. That document is not authoritative here and no
  ragdoll networking exists. Multiplayer parity is a separate prerequisite.
- `docs/skills/spec-behavior-review-v1.md` was not run in this checkpoint; it
  should accompany the human-behavior review.

## Iteration 2 (same session): range limits + hybrid mode

After the first in-game check the physical mode was kept, and two changes were
requested:

- **Per-limb range limits.** `src/ragdoll/physical-aim.h` gained
  `torsoMaxPitchDeg`, `torsoMaxRollDeg`, `headMaxSwingDeg`, `armMaxSwingDeg`,
  `legMaxSwingDeg`, and hybrid spring gains. `clampAimRanges` in
  `ragdoll-mode.cpp` clamps the torso's pitch/roll relative to the movement yaw
  frame and clamps each child limb's swing from its bind orientation. Config:
  `ragdoll.json physical.limits`.
- **Hybrid mode.** `aimbody.json mode == "hybrid"`. `captureAimTargets` snapshots
  the procedural animation pose before physics overwrites the skeleton, and
  `applyHybridSprings` pulls each part's orientation (and limb positions) toward
  it, on top of the look torque. Limbs therefore follow animations/weapons via
  forces while still colliding and carrying momentum. Config:
  `ragdoll.json physical.hybrid`.
- `RagdollModePart` gained `aimTargetPosition` / `aimTargetOrientation`.
- `AimBodyConfig` gained `hybridMode()` / `bodyPhysicsMode()`; `simulate-tick`
  runs the aim body for physical or hybrid.
- Active `config/aimbody.json` mode was set to `hybrid` for human testing.

## Iteration 3 (same session): stable follow force + config-edit fix

Two problems reported after testing hybrid:

- **Explicit-spring blow-up.** `hybrid.position_gain`/`rotation_gain` were fed
  into explicit Euler (`v += (err*gain - v*damp)*dt`). At `gain: 500` the term
  `gain*dt ≈ 8.3` is far past the stability limit, so the body exploded and the
  GLB body collision shoved `player.pos` off the map; reset could not recover
  because the aim body stayed unstable. Replaced with a stable exponential
  blend in `applyHybridSprings`: `alpha = 1 - exp(-rate*dt)`, orientation via
  `slerp`, limb position via `position += delta*alpha`. Stable at any magnitude.
- **Single follow-force knob.** `physical.hybrid` now exposes `follow_force`
  (1.0 baseline, 10.0 = ten times harder to depart from the animations.json /
  weapon / aimbody pose), `base_rate` (tracking rate at 1.0), and
  `position_follow`. This is the "how hard limbs track the pose" control; it
  also lets the arms follow the weapon/aim pose closely enough to aim up.
- **Config edits no longer rebuild the aim body.** `updateAim` no longer calls
  `reinitPreservingState` on `ragdoll.json` generation changes; tuning is read
  live, so editing gains cannot reset the pose or move the player. Geometry
  changes require toggling the mode off/on.

## Validation

- `tests/physical-aim-torque-test.cpp`: compile and run with
  `g++ -std=c++17 -O2 -Iinclude -Isrc -DGLM_ENABLE_EXPERIMENTAL tests/physical-aim-torque-test.cpp -o <tmp>`.
  Result: `5 passed, 0 failed`.
- Cold builds `python build_agent.py`:
  - First build compiled `aimbody-config.cpp`, `ragdoll-mode-config.cpp`,
    `ragdoll-mode.cpp`, `simulate-tick.cpp`; `Status: SUCCESS`, return 0.
  - Iteration-2 build compiled `ragdoll-mode-config.cpp` and `ragdoll-mode.cpp`
    (the latter initially failed on a missing forward declaration of
    `quatToRotationVector`; a forward declaration was added and the rebuild
    returned `Status: SUCCESS`, return 0).
  - Iteration-3 build recompiled `ragdoll-mode-config.cpp` and
    `ragdoll-mode.cpp` after deleting their objects; `Status: SUCCESS`,
    return 0.
  - Executable: `C:\mimita-v9\mimita.exe`.

## Pre-existing edits

The working tree already had many modified and untracked files before this
session (movement, collision, NPC, networking, destructible geometry, crates,
dev-loop). None were reverted or claimed. This session's edits are limited to
the files listed above; the config/aimbody.json mode was intentionally left at
`default`.

## Human review still needed

Physical mode was confirmed by the user to "feel good" but limbs ranged too far
from the body. Iteration 2 addresses this and adds the hybrid mode; the hybrid
feel, limb alignment, and the specific limit values are not yet human-verified.

Test `config/aimbody.json mode == "hybrid"` (currently active, hot-reloads) and
verify: `physical.hybrid.follow_force` makes limbs track the animation/weapon
pose as hard as wanted (10.0 should look like default mode even at speed, and
should aim the arms up); low values give sway. Confirm editing `ragdoll.json`
(such as `follow_force` 500→50) no longer moves or drops the player. Tune
`physical.limits` and `physical.hybrid`. Multiplayer limb damage is expected to
disagree with the server's static body template until ragdoll pose replication
exists.
