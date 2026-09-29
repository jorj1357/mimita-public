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

## Iteration 4 (same session): limb / ragdoll replication (Phase 6 first slice)

Adds a valid networking path for limb state. The owning client sends its six
limb world transforms at ~15 Hz; the server relays them to every other player;
remote clients interpolate and apply them to the replica skeleton, so both
rendering and client-side hitboxes follow the exact limbs. NPCs are viewers
only, so they do not need to send. This is a first slice: the source is the
owning client (not server-simulated), which is a noted trust boundary.

New files:

- `src/ragdoll/ragdoll-replication.h` — fixed six-limb pose, mode enum,
  `interpolateReplicatedPose`, and the two-sample `RagdollReplicationState`.
- `tests/ragdoll-replication-test.cpp` — limb mapping and buffer/interpolation.

Wire and transport:

- `src/network/packets.h` — `PACKET_RAGDOLL_STATE = 69`, `RagdollLimbWire`,
  `RagdollStatePacket` (200 bytes, static_assert < 1200).
- `src/network/server.cpp`, `server-packets.cpp` — `isKnownPacketType` also
  accepts 69 (the legacy numeric range ends at 61).
- `src/network/server-packet-handlers.cpp` + `server.h` — `handleRagdollState`
  normalizes the owner from the sender and relays to all other players, modeled
  on `handleGodballState`.
- `src/network/multiplayer-tick.cpp` — receive branch pushes into
  `ctx.remoteRagdoll`; send block emits the local pose at 15 Hz and one
  inactive pose when it stops.
- `src/network/multiplayer-context.h` — `remoteRagdoll` map + send timer.
- `src/network/multiplayer-interpolation.cpp` — after procedural animation,
  samples each owner's stream behind real time (100 ms) and applies it.
- `src/network/multiplayer-tick.cpp` / `multiplayer-packets.cpp` — clear the
  stream and cached bind body on despawn and disconnect.

Ownership:

- `src/ragdoll/ragdoll-mode.h/.cpp` — `replicatedPose()` / `clearReplicatedPose`
  / `cacheReplicatedPose` (owner side), `applyReplicatedPose` /
  `clearReplicatedBody` / `clearAllReplicatedBodies` (remote side, caches one
  bind `RagdollBody` per owner so mesh alignment is exact).
- `src/sim/simulate-tick.cpp` — clears the replicated pose each tick so a
  stopped body sends one inactive frame.

## Iteration 5 (same session): server validates the replicated pose

The user asked the server to validate the replicated pose and make limb damage
work for other players ("shooting the limbs = damage"), instead of the static
default-pose template.

- `src/ragdoll/ragdoll-replication.h`: each `RagdollLimbState` now also carries
  `hitCenter` / `hitHalf` (the world AABB the owner computes with the exact
  formula the client uses for hit detection), plus
  `ragdollReplicatedBodyPart(limbIndex)`.
- `src/ragdoll/ragdoll-mode.cpp`: `cacheReplicatedPose(const Player&, ...)`
  fills the hitbox from `player.physicalBody.parts[].collider` and the refreshed
  `worldTransform`. Ragdoll mode now refreshes world transforms before caching.
- `src/network/packets.h`: `RagdollLimbWire` carries `hx/hy/hz` +
  `hhx/hhy/hhz`. Packet is 344 bytes (still < 1200).
- `src/network/multiplayer-tick.cpp`: send fills the hitbox fields; receive
  parses them.
- `src/network/server.h` / `server-players.cpp`: `ServerPlayer` stores a bounded
  `ragdollHistory` (32 samples) and `hasRagdollPose`;
  `getPlayerRagdollPoseAtTick` returns the interpolated pose nearest the rewind
  tick.
- `src/network/server-packet-handlers.cpp`: `handleRagdollState` stores the pose
  in the history (and clears it when the body stops).
- `src/network/server-attack.cpp`: `buildTargetBodyPartBoxes` prefers the
  replicated limb hitboxes and falls back to the static template; used by both
  the authoritative trace (`fillTargetBodyParts`) and the client-claim
  validation (`claimedHitInBodyParts`), at the victim's rewind tick.

## Iteration 6 (same session): server accepts the client-authoritative ragdoll root

The user reported that the server position error climbed the further they moved
while ragdolled, because the server kept simulating movement from input. The
client is authoritative over its ragdoll body, so the server now treats the
client's reported root as authoritative while a pose is active.

- `src/ragdoll/ragdoll-replication.h`: `RagdollReplicationPose` gained
  `rootPosition` / `rootYaw`; `interpolateReplicatedPose` interpolates them.
- `src/network/packets.h`: `RagdollStatePacket` carries `rootX/rootY/rootZ/
  rootYaw` (packet now 360 bytes).
- `src/ragdoll/ragdoll-mode.cpp`: `cacheReplicatedPose` fills the root from
  `player.pos` / `player.yaw`.
- `src/network/multiplayer-tick.cpp` and `server-packet-handlers.cpp`: copy the
  root through send/receive/store.
- `src/network/server-players.cpp`: `simulatePlayer` now has a ragdoll branch
  (after the fly branch, before movement simulation). While
  `p.hasRagdollPose` and the latest pose is active, it sets `p.pos`/`p.yaw` from
  the client root, derives `p.vel` from the delta, skips movement simulation,
  and returns. The per-tick step is clamped to 4 m (240 m/s) so a malformed or
  hostile packet cannot teleport; the clamp logs once per second.

## Iteration 7 (same session): raise the client hard-snap distance and keep the ragdoll exit clean

The user asked to raise the "server position error snap back" limit from 100 to
999 and to keep the client authoritative on leaving ragdoll.

- `src/network/movement-validation.h`: `MovementValidationConfig::
  majorCorrectionDistance` default raised 100.0 → 999.0. This is the threshold
  the client's `classifyMovementCorrection` uses to call an error `Major`, which
  (with the other conditions) triggers the catastrophic-divergence hard snap in
  `multiplayer-reconcile.cpp`.
- `src/config/networking-config.h` + `config/networkingconfig.json`
  (`local_player_reconciliation.hard_snap_distance`) and
  `config/networking/presets/default.json`: 100.0 → 999.0.
- `src/network/multiplayer-reconcile.cpp`: the reconcile now builds its
  `MovementValidationConfig` from `localReconciliation.hardSnapDistance`, so the
  JSON key actually controls the snap (it was parsed but previously unused).
- `src/network/server-players.cpp`: the ragdoll-authoritative branch now also
  refreshes `lastAcceptedClientPosition/Velocity`,
  `hasAcceptedClientTransform`, and `movementValidation.lastAcceptedClientTick`,
  so leaving ragdoll cannot trip the post-gap drift correction.

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
  - Iteration-4 build recompiled `multiplayer-interpolation.cpp`,
    `multiplayer-packets.cpp`, `multiplayer-tick.cpp`,
    `server-packet-handlers.cpp`, `server-packets.cpp`, `server.cpp`,
    `ragdoll-mode.cpp`, `simulate-tick.cpp`; `Status: SUCCESS`, return 0.
  - `tests/ragdoll-replication-test.cpp`: `4 passed, 0 failed`.
  - Iteration-5 build recompiled `multiplayer-tick.cpp`, `server-attack.cpp`,
    `server-packet-handlers.cpp`, `server-packets.cpp`, `server-players.cpp`,
    `server.cpp`, `ragdoll-mode.cpp`, `simulate-tick.cpp`; `Status: SUCCESS`,
    return 0.
  - Iteration-6 build recompiled `multiplayer-tick.cpp`,
    `server-packet-handlers.cpp`, `server-players.cpp`, `ragdoll-mode.cpp`;
    `Status: SUCCESS`, return 0. (One build reported FAILED due to a race with
    the background dev-loop rebuilding the same objects; the clean rebuild
    succeeded.)
  - Iteration-7 build recompiled `multiplayer-reconcile.cpp` and
    `server-players.cpp`; `Status: SUCCESS`, return 0.
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
