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

## Validation

- `tests/physical-aim-torque-test.cpp`: compile and run with
  `g++ -std=c++17 -O2 -Iinclude -Isrc -DGLM_ENABLE_EXPERIMENTAL tests/physical-aim-torque-test.cpp -o <tmp>`.
  Result: `5 passed, 0 failed`.
- Cold build `python build_agent.py`: compiled
  `aimbody-config.cpp`, `ragdoll-mode-config.cpp`, `ragdoll-mode.cpp`,
  `simulate-tick.cpp`; `Status: SUCCESS`, return code 0. Executable:
  `C:\mimita-v9\mimita.exe` (2026-09-29 09:24:29 local).

## Pre-existing edits

The working tree already had many modified and untracked files before this
session (movement, collision, NPC, networking, destructible geometry, crates,
dev-loop). None were reverted or claimed. This session's edits are limited to
the files listed above; the config/aimbody.json mode was intentionally left at
`default`.

## Human review still needed

In-game behavior is not yet observed. Set `"mode": "physical"` in
`config/aimbody.json` (hot-reloads) and verify: look is a wish direction; head
and torso lag and converge; a fast left/right look sends opposite momentum
through the limbs; limbs keep their own velocity; no teleport or oscillation;
limb hitboxes match the visible limbs. Multiplayer limb damage is expected to
disagree with the server's static body template until ragdoll pose replication
exists.
