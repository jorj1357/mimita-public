# Collision step-up, scenario evidence, and stall classification

- Task: Reduce Dust 2 ramp/seam snagging, make collision diagnostics usable for
  AI scenario testing, and preserve the existing mace/NPC issue intake.
- Status: PARTIAL. The low-lip step-up path and diagnostics are implemented and
  unit-tested. Runtime Dust 2 acceptance, mace root-sinking containment, and NPC
  sinking/wall-clipping fixes remain open.
- Date, time, timezone: 2026-10-31 (UTC).
- Branch: `20261011zt`.
- Base and final commit: unchanged; working changes are uncommitted.

## Findings

- The supplied schema-2 recording
  `devscripts/scenarios/user/scenario-20261031_034240-dustcoll.jsonl` has only
  `client_input` and `client_actors`, so it cannot be server-authoritatively
  replayed. It was captured in an external room.
- Client-only schema-1 conversion played, but diverged from the recording at
  frame 309 by 2.07 m. Local physics did not reproduce the remote session's
  positions, so this route did not validate the reported snag.
- Offline re-analysis corrected an earlier misclassification. The apparent
  2678-2687 freeze starts with recorded `freeze_pressed`, continues with
  `freeze_held`, and was airborne. The recording contains no sustained grounded
  input-held/no-progress window. It does contain airborne horizontal-pinned
  windows at frames 1328-1347 and 2131-2138.
- The active actor-triangle solver had no step-up phase; the legacy GLB
  sweep-slide still owned the only step-up code.

## Changes

- Added a bounded grounded low-lip step-up probe in
  `src/physics/movement/actor-triangle-solver.cpp`. It uses body-only lowest
  geometry, limits lift to `MAX_STEP_HEIGHT`, advances the same tick's forward
  move, and rejects clearance contacts with non-walkable normals.
- Added `collision.step_up` evidence and actor-triangle result fields
  `stepUpApplied`, `seamContactsRemoved`, and `featureContactsCollapsed`.
- Added bounded grounded `collision.progress.stall` and `.end` events with
  contact evidence to `CollisionState` and the solver.
- Added `client_collision` recording tracks with grounded state, contact counts,
  duplicate/overflow counts, and up to eight canonical contacts.
- Added `devscripts/scenario-analyze.py`. It separates grounded stalls from
  airborne pinned windows and excludes intentional freeze input.
- Added a low-lip regression case to
  `mimita.exe --actor-triangle-solve-selftest`.
- Documented the collision evidence contract and future named launch lanes in
  `docs/specs/dev-loop/dev-loop.md`, and updated the Dust 2 intake in
  `docs/specs/open-gameplay-issues.md`.
- Added `devscripts/scenarios/user/dust2_ramp_collision_v1.json` for a future
  same-route server-backed recording.
- Fixed pre-existing build blockers only to enable validation:
  `src/network/server-world-destruction.cpp` include path and a
  `src/impact/destructible-selftest.cpp` `submitRifle` argument. These were not
  requested gameplay changes.

## Validation

- Build: canonical dev-loop build succeeded at `build=3920`.
- Self-test: `mimita.exe --actor-triangle-solve-selftest` passes all four new
  low-lip step-up assertions. Two unrelated pre-existing cases still fail:
  `actor is not glued to an old wall contact` and
  `weapon contact does not launch the player`. The overall self-test remains red.
- Offline analyzer: the supplied recording now reports zero grounded stall
  windows, two airborne pinned windows, and three repeated-position freeze
  frames. This corrects the earlier collision-stall interpretation.
- Replay: client-only schema-1 playback reached frame 309 and failed with
  `position_divergence` (2.07 m). No collision fix was validated by replay.
- Runtime/human acceptance: not performed for Dust 2 ramp behavior, mace root
  sinking, or NPC sinking/wall clipping. No visual or multiplayer claim is made.

## Remaining work

- Capture a fresh Dust 2 recording on a dev-loop-owned server so
  `client_collision` and `server_actors` are present, then inspect the first
  blocking contact at the reported ramp.
- Decide whether the airborne pinned windows are wall pinning or expected
  airborne contact; do not treat them as grounded stall evidence.
- Implement and validate root-support containment for the mace primary swing;
  this pass did not change mace animation or root support.
- Investigate NPC sinking/wall clipping separately with a real NPC session.

## Specification pass — 2026-10-31T11:50:24Z

Status: SPECIFICATION ONLY. No collision code changed in this pass.

### What changed

- Corrected `docs/architecture/collision/collision.md`: NPCs already use the
  actor-triangle solver for static-world collision; the stale “capsule now,
  triangle later” statement was replaced. Actor-pair capsules are now explicitly
  separate from static-world body/weapon hitboxes.
- Added the NPC/monster final-pose containment contract to
  `docs/specs/npc-behavior/npc-behavior.md`: final animated body and held-weapon
  meshes collide; root is the only translation authority; 1–2 cm standoff;
  last-clear restore; bounded safe-point/respawn fallback; no downward ray
  through solids.
- Documented render mesh versus collision mesh, collision decimation/LOD, and
  triangle-test budgets. Pure body-triangle vs nearby-world-triangle collision is
  the target, but unlimited model complexity is not claimed.
- Documented that the Chainsaw Boss `2.0 m / 6.6 m` capsule is an actor-pair
  approximation, not its static-world hitbox.
- Marked `drawCapsuleWire()` and `drawCapsuleApprox()` as separate legacy
  visualizations. The target is one accurate capsule renderer with both
  hemispherical caps.
- Added the target `entity_visibility_actor_hitboxes` category for final
  body/weapon hitboxes, separate from actor-pair capsules.
- Added collision and final-pose tracks plus expected-good/forbidden invariants
  to `docs/specs/dev-loop/scenario-input-recording.md`.
- Added the collision evidence event contract to
  `docs/specs/debug-logging/debug-logging.md`, including
  `collision.speed_collapse` and `npc.pose.*` events.
- Added automated expected-good scenario requirements to
  `docs/workflows/runtime-scenario-validation.md`.
- Updated entity editor, terminal commands, animations, movement, and NPC
  movement documentation to reference the same ownership rules.
- Added regression records:
  - `docs/regressions/2026-10-31/block-seam-speed-collapse-REG.md`
  - `docs/regressions/2026-10-31/npc-final-pose-penetration-REG.md`
  - `docs/regressions/2026-10-31/down-dash-slope-bounce-REG.md`
  - appended occurrence 2 to
    `docs/regressions/2026-10-25/flat-floor-backed-face-dash-REG.md`

### Validation

- Documentation-only pass. No build, runtime, or human acceptance is claimed.
- The supplied collision recording lacks a footer and cannot be treated as a
  completed acceptance artifact.
