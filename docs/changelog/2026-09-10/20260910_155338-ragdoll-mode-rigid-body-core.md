# Ragdoll mode: reusable rigid-body core + physics-driven rebuild

- Task ID: ragdoll-mode-rigid-body-core
- Summary: Replace the position-only ragdoll solver (no angular dynamics, child-
  only joints, collision-before-constraints, teleporting grabs) with a reusable
  rigid-body/constraint/contact core and a physics-driven `RagdollModeSystem`.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: `2026-09-10T15:53:38Z` (2026-09-10 11:53:38 EDT)
- Branch: `8292026stash`
- Base commit: working tree; no commit created
- Final commit: none (uncommitted)

## Pre-existing changes

- `git status --porcelain` also lists work not created by this session:
  `config/analytics.json`, `config/gamemodes/ffa.json`, `config/gamemodes/tdm.json`,
  `src/engine/engine-tick-camera.cpp`, `src/engine/engine-tick-ui-overlays.cpp`,
  `src/gamemode/gamemode.h`, `src/network/community-match-client.cpp/.h`,
  `src/network/server-gamemode.cpp/.h`, and
  `docs/changelog/2026-09-10/20260910_151628-ffa-countdown-fix.md` /
  `20260910_155252-unified-kill-event.md`. They are not claimed here.
- `config/ragdoll.json`, `src/ragdoll/ragdoll-mode.cpp`, and
  `src/ragdoll/ragdoll-mode.h` already carried uncommitted tuning from an earlier
  session (`lookRotation` head aim, camera smoothing, head capsule offset 1.5,
  head cone 90, camera smooth factor 1.0). Those edits are preserved; this
  session rewrote the solver internals around them.

## Requested behavior

- Limbs/head/torso must be loose rigid bodies that rotate about all axes, not
  stay aligned to world Z.
- Capsules must follow limb rotation in 3D and must not be stuck vertical.
- Body parts must not sink into the ground or surfaces; collisions must be as
  solid as normal movement collisions; no falling through the floor.
- Parts must be attached to the torso at attachment points with limits, and must
  collide with each other so limbs cannot pass through one another.
- Grabs must hold a physical point without teleporting the hand or pulling the
  body through geometry.
- Arms should extend outward along camera-forward; torso/head should aim with
  the camera through physical influence rather than a hard 1:1 lock.
- `plrOrigin` remains the authoritative root.

## Specification alignment

- `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md`: RAG-006 (physically
  simulated parts), RAG-008 (world collider per part), RAG-010 (self-collision),
  RAG-011 (no tunneling), §6 "No limb should merely teleport to its target
  transform", §35/#60 (reuse generalized physical-object primitives).
- `docs/specs/moving-physical-objects/moving-physical-objects.md` §7 (body
  properties incl. mass/angular velocity), §13 (force/torque/angular momentum),
  §33-36 (generalized constraints, ragdolls emerge from bodies+constraints).
- `docs/architecture/collision/collision.md`: fixed 60 Hz domain; reuse cached
  broadphase and existing exact capsule/triangle helpers.
- `docs/skills/spec-behavior-review-v1.md`: PASS_WITH_HUMAN_REVIEW. The prior
  implementation disagreed with RAG-006/008/010/011 and §6; this change moves the
  implementation toward those requirements. Full acceptance still needs play.

## Exact implementation changes

- New `src/physics/physical-body.h` / `physical-body.cpp`: shared deterministic
  `RigidBody` (mass/inverse mass, isotropic capsule inertia, linear/angular
  velocity, capsule shape), `setBodyMass`, `integrate`, `applyImpulseAtPoint`,
  `rotateBody`, `solvePointJointVelocity`, `solvePointJointPosition`,
  `solvePointToWorld`, `collideWithWorld`, `collideBodies`.
  - World collision is swept and substepped and reuses `capsuleTriangleSweep`,
    `capsuleTriangleContact`, and `appendChunkTrianglesForAABB` (cached
    broadphase). Contact impulses produce both linear and angular response.
  - No renderer, transport, input, or client/server identity in the core.
- `src/ragdoll/ragdoll-mode.h`: `RagdollModePart` now owns a `RigidBody` plus
  parent/child local anchors, `restDirectionLocal`, cone limit, and rest length.
  `RagdollGrabState` gained `handLocalAnchor`.
- `src/ragdoll/ragdoll-mode.cpp`: rebuilt `initParts`, tick order, joints,
  cone limits, grabs, self-collision, and root sync.
  - Torso is the root at `player.pos` (removed the +0.6 offset that caused
    repeated G-toggle drift).
  - Anchors are rotated by parent orientation; child anchor is derived from the
    bind pose so all six parts attach correctly.
  - Joints apply equal/opposite linear and angular correction to both bodies
    (reaction), so limbs swing and tip instead of orbiting a fixed point.
  - Gravity acts at each center of mass; angular velocity integrates torque, so
    limbs rotate about all axes and the torso can tip over.
  - New order: integrate -> joints (velocity+position) -> cone limits -> grabs
    -> world collision -> self-collision -> joint velocity cleanup -> sync.
  - Grabs are compliant point-to-world constraints solved with `solvePointToWorld`
    (no teleport, no direct torso translation).
  - Self-collision runs capsule-vs-capsule for all non-directly-jointed pairs.
  - Head aims at the camera through a physical angular torque, not a hard set.
  - Exit preserves velocity, adds the configured hop, and copies the torso root
    back to the player.
- `src/ragdoll/ragdoll-mode-config.h` / `.cpp`, `config/ragdoll.json`: added
  hot-reloadable `mass`, `solver_iterations`, `max_fall_speed`, `restitution`,
  `friction`, `self_collision`, `body_linear_damping`, `body_angular_damping`,
  `grab.compliance`, `grab.grace_distance`, `head.rotation_strength/speed`, and
  `exit.preserve_velocity/hop_velocity`.

## Diagnostics

- Existing `StructuredCategory::Ragdoll` `RAGDOLL_ENTER` / `RAGDOLL_TICK` events
  retained, now reporting part count and kinetic energy from `RigidBody` state.
- World/self collision contacts are not individually logged; the structured tick
  remains the low-volume owner. No new loose debug files.

## Validation

- Skill `docs/skills/spec-behavior-review-v1.md`: PASS_WITH_HUMAN_REVIEW.
- Build: `python build_agent.py` -> `Status: SUCCESS`, return code 0,
  duration 8.50s; `mimita.exe` relinked at 2026-09-10 11:53:18. Compiled and
  linked in this pass: `src/physics/physical-body.cpp`,
  `src/ragdoll/ragdoll-mode.cpp`, `src/ragdoll/ragdoll-mode-config.cpp`,
  `src/ragdoll/ragdoll-commands.cpp`, `src/sim/simulate-tick.cpp`,
  `src/engine/engine-tick-render.cpp`, `src/engine/engine-tick-camera.cpp`.
  No compiler errors or warnings in `build/changelog.txt`.
- Runtime: not performed this session. No visual, collision, grab, or
  multiplayer acceptance is claimed.

## Regression review

- No append-only `docs/regressions/regressions-v1.md` entry added. The reported
  symptoms are incomplete first implementation of an uncommitted feature, not a
  confirmed previously-working behavior that a change broke. If human play shows
  a previously working ragdoll behavior regressed, add an entry with the exact
  wrong/corrected code.

## Human acceptance

- Visual review required: enter ragdoll (G); confirm parts hang, swing, and
  rotate about all axes; capsules follow limb orientation (no world-Z lock);
  torso can tip over.
- Visual review required: no part sinks into the ground or a wall; running and
  jumping into a block then pressing G does not pass through it.
- Gameplay review required: A/D grab holds a wall point without teleporting or
  pulling the body through the wall; LMB/RMB push arms along camera-forward.
- Gameplay review required: exiting ragdoll preserves momentum, stays upright,
  and does not drift upward across repeated toggles.
- Skill `docs/skills/efficiency-checker-v1.md` review still recommended: verify
  collision candidate counts and frame time with six substepped parts.

## Spec TODOs observed (not edited)

- `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md:1` — "todo explain ragdoll
  like retrograd". Suggest a short definition section contrasting ragdoll mode
  with the normal controller and naming the countnee/source-style relation.
- `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md:7` — "todo relate it to
  moving-physical-objects.md". Suggest replacing the file-path reference with an
  explicit "ragdoll is composed from these primitives" list matching §60.
