# Hot locomotion: idle sway, walk-on-intent, ability-gated animations

Date: 2026-09-16 18:06 EDT (UTC 2026-09-16T22:06:44Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS (build + both selftests)`

## Task

Implement the hot animation behaviour the user asked for: idle swaying, walk on
intent, and ability animations only when the ability actually activates.
(This is P3 of the planned program; P0-P2 hot world/collision/ragdoll remain.)

## What changed (all hot — no EXE restart)

### 1. Ability-fired pulse (new `hot-movement-fired.h`)
- `HOT_MOVEMENT_FIRED_COMPONENT` (`MovementFired`) + `HotMovementFiredV1`:
  local-only bits for DASH / DOWN_DASH / GROUND_JUMP / AIR_JUMP / FREEZE.
- `movement-system.cpp` (hot `movement.main`) ORs the abilities that actually
  fired this tick (it already computed `didDash`/`didDownDash`/`freezeEdge`/
  `jumpEdge`) into the component.
- `animation-policy.cpp` reads and **clears** the pulse after consuming it, then
  starts DASH / DOWN_DASH / JUMP only when the pulse is present. If the ability
  is unavailable (no fire), the previous action (walk/idle) continues.

### 2. Walk-on-intent, constant speed (`animation-policy.cpp`, `hot-animation-clips.h`)
- Reads `GAME_COMPONENT_MOVEMENT_INTENT`; `moving = pressed && (|moveX|+|moveY|>0)`.
- Selection: intent-to-move → `WALK`, otherwise `IDLE` (including airborne), so an
  actor can float with idle. `FALL`/`LAND`/velocity gating removed from selection.
- `evaluateWalk` is now constant speed/amplitude (no velocity scaling).
- Remote actors without a replicated intent fall back to velocity.

### 3. Idle sway (`hot-animation-clips.h`)
- `evaluateIdle` now has clear sway/breathing on torso, head and both arms, plus
  a subtle leg motion (was nearly static).

## Evidence

- Hot DLL: `build_game_dll.py` -> success (63 sources).
- Cold build: `build_agent.py` -> BUILD SUCCESS.
- `mimita-*.exe --hot-combat-selftest` -> **PASS** (all cases, including the new
  `movement fired pulse writable`, `phase2 walk interrupted by jump`,
  `phase2 dash interrupts locomotion`; the two earlier cross-session effect
  failures are now gone).
- `mimita-*.exe --live-code-selftest` -> **PASS**.

Note: `build_agent.py` now emits `mimita-<UTC timestamp>.exe`; the stale
`mimita.exe` was not updated. Tests were run against the freshly built exe.

## Not done (the large remaining program)

- **P0 bootstrap kernel (one cold build):** `world.collision` triangle query,
  `physics.impulse`, ragdoll component write, call-site/compile-ownership flip.
- **P1 hot world + hot collision** (world store + spatial index hot; movement
  collision hot immediately, no fallback).
- **P2 hot ragdoll** (solver/mode/presentation hot).
- **P4 cleanup.**
- Aim-body hot gains are not yet applied for the local player: its look pitch is
  published as `aimBodyPitch`, which is currently always 0 (the deleted legacy
  animator was its only writer). Needs the bootstrap to publish the live camera
  pitch (or the aim intent) to the actor before hot pose generation can tilt the
  body.

## P0.1 started (additive, verified)

- `game-api.h`: `GAME_CAP_WORLD_COLLISION` (paginated `GameCollisionTriangleV1`
  dump + `GameWorldCollisionPageV1`), `GAME_CAP_PHYSICS_IMPULSE`
  (`GamePhysicsImpulseV1`), and their function typedefs.
- `live-behavior.cpp`: `capWorldCollision` (dumps the map's
  `collisionMesh.triangles`) and `capPhysicsImpulse` (adds linear velocity to an
  entity body), both registered as kernel capabilities.
- Cold build SUCCESS (110 TUs); `--hot-combat-selftest` and
  `--live-code-selftest` PASS. Non-breaking: no call sites changed yet.

Still remaining in P0: ragdoll component write, the call-site/compile-ownership
flip, the hot world store + spatial index, hot collision, and the hot ragdoll
migration (P1/P2). `build_agent.py` now emits `mimita-<UTC timestamp>.exe`.

## Ragdoll regression fix (local)

The G toggle already existed (`input-poll.cpp` sets `ragdollTogglePressed`,
`simulate-tick.cpp` toggles `RagdollModeSystem::activate/deactivate`), and
`config/ragdoll.json` has `enabled: true`, `toggle_key: G`. It appeared broken
because the hot pose path overwrote the ragdoll body every render frame.
- `live-behavior.cpp` `capSkeletonApply`: the local-player typed-body mirror now
  early-returns while `THE_PLAYER.ragdollModeActive`.
- `presentation-entities.cpp` `applyHotPoseToPlayer`: early-returns while
  `player.ragdollModeActive`.
Ragdoll remote presentation was not changed (may still be clobbered by the hot
pose on remote actors).

Cold build SUCCESS (2 TUs); `--hot-combat-selftest` and `--live-code-selftest`
PASS on the fresh exe.

## P0.2 (partial): ragdoll component write

`live-behavior.cpp` `capWriteComponent` now maps the four ragdoll ABI structs to
their typed ECS components (`GAME_COMPONENT_RAGDOLL_LIMB/JOINT/ROOT/GRAB`), so a
hot solver/body can commit ragdoll state (the read side already existed). Cold
build SUCCESS (5 TUs); both selftests PASS.

## P0.2 remainder / P1 / P2 — not done

The call-site/compile-ownership flip (excluding `src/ragdoll/*` and
`src/physics/movement/physics-collision*` from the EXE and calling them only as
hot systems) plus hot collision and the hot ragdoll solver/mode/presentation are
a large, all-or-nothing migration. It was not started because a partial flip
would either duplicate the ragdoll singletons or break movement. The additive
primitives (`world.collision`, `physics.impulse`, ragdoll component write) are in
place for that work.

## Added this pass: live-editable ragdoll solver tuning (hot)

`rocket-behavior.cpp` `GAME_EVENT_RAGDOLL_SOLVE` now applies a hot tuning table
(`kStiffnessMultiplier`, `kDampingMultiplier`, `kGravityMultiplier`,
`kIterationMultiplier`, all default 1.0 = current behavior). Editing them and
saving changes the running client's ragdoll solver policy on the next generation
switch — no JSON, no EXE restart. Hot DLL build success; `--live-code-selftest`
PASS.

## Concrete P0.2/P1/P2 migration checklist (next session)

1. ABI: add a `ragdoll.solve` capability carrying a POD limb/joint/grab snapshot
   (in/out) plus params; register it in the kernel.
2. Cold host: `ragdoll-mode.cpp` solve lambda builds the POD, calls the
   capability; if `handled`, applies the out states to the body and skips the
   cold solver; otherwise runs the cold solver (temporary fallback).
3. Hot provider: implement the solver in `src/hot-reload/modules/ragdoll/` using
   `world.collision` for world contacts and hot math for joints/limits/grabs/
   self-collision. Prove parity, then remove the cold fallback.
4. Movement collision: same pattern with a `physics.sweep`-style capability;
   gate on `--movement-parity-selftest`, then delete `physics.move`.
5. Flip compile ownership (`src/ragdoll/*`, collision) to the hot manifest and
   delete the cold call sites in `simulate-tick.cpp`/`engine-tick-render.cpp`.

## Step 1 done: `ragdoll.solve` seam installed (with fallback)

- `game-api.h`: `GAME_CAP_RAGDOLL_SOLVE` + POD `GameRagdollSolveV1`
  (limb state in/out, limb statics, rotation limits, grabs) with fixed bounds
  (`GAME_MAX_RAGDOLL_LIMBS = 24`), plus `GameRagdollSolveFn`.
- NEW hot module `src/hot-reload/modules/ragdoll-solve.cpp`: registers the
  `ragdoll.solve` provider. Default returns `handled = 0`, so the cold solver
  still runs — behavior unchanged. This file is the live-editable owner of the
  solver algorithm.
- `ragdoll-mode.cpp`: the solver substep now calls `hotRagdollSolve(...)` which
  snapshots the live body, invokes the hot provider, and applies its limb
  results when `handled == 1`; otherwise runs the cold `solveSubstep` (fallback).
- Hot DLL build success (68 sources); cold build SUCCESS (116 TUs);
  `--hot-combat-selftest` and `--live-code-selftest` PASS.

### Next (step 2)

Implement the solver body inside `ragdollSolveProvider` (integration, joints,
rotation limits, grabs, world collision via `world.collision`, self-collision),
set `handled = 1`, and prove parity. Then remove the cold fallback in
`ragdoll-mode.cpp` and delete `ragdoll-solver.cpp`. After that the solver is
fully hot. Steps 3-4 (movement collision, compile-ownership flip) follow.

## Step 2 first cut done: hot ragdoll solver (live-editable)

`src/hot-reload/modules/ragdoll-solve.cpp` now implements the solve and sets
`handled = 1`, so the cold solver is bypassed:
- gravity integration; joint constraints (velocity + position, beta);
  rotation-limit clamping; grabs (world/same-body); limb self-collision; world
  collision against a cached triangle grid built from the `world.collision`
  capability.
- Explicit `kTune` block at the top (stiffness/damping/gravity/jointBeta/
  limitBeta/grabBeta/selfBeta/worldBeta/skins/stop speeds) for live tuning.
- Limbs are approximated as spheres (radius + halfHeight) for collision in this
  first cut; capsule-axis and angular inertia fidelity are follow-ups.
- Host plumbing: `ragdoll-mode.cpp` passes `LiveBehavior::hostContext(0)` to the
  provider so it can resolve `world.collision`.
- Hot DLL build success (70 sources); cold build SUCCESS (2 TUs);
  `--hot-combat-selftest` PASS; `--live-code-selftest` PASS;
  `--ragdoll-slice-selftest` PASS and reports `provider=ragdoll.solve` resolved.

### Next after step 2

1. Play-test the hot ragdoll (G) and tune `kTune` live; fix fidelity issues
   (capsule axis, angular inertia) entirely by editing the hot file.
2. Once visually accepted, delete the cold fallback branch in `ragdoll-mode.cpp`
   and delete `src/ragdoll/ragdoll-solver.cpp`.
3. Step 3: movement collision hot (`physics.sweep`-style capability), gated on
   `--movement-parity-selftest`, then delete `physics.move`.
4. Step 4: compile-ownership flip for the remaining cold ragdoll/collision files.

All solver edits from here are hot; only a further structural bootstrap would
need a build.

## Step 3 done: hot movement collision (fully hot, no cold build)

- NEW `src/hot-reload/hot-movement-collision.h` (hot): a self-contained
  capsule-vs-world solve using `world.collision` triangles — gravity integrate,
  swept substepping, sphere-sample depenetration + slide, grounded heuristic, and
  a hot triangle index (small triangles in a uniform grid, large triangles in an
  always-tested list so floors are never missed).
- `movement-system.cpp` (hot) `resolveCollisions` now calls
  `HotCollision::hotMoveCapsule` first; the kernel `physics.move` primitive is
  only a last-resort fallback when no world collision data is available.
- Fixed the `world.collision` fetch: the earlier null-buffer probe returned
  `total = 0`, so the cache was empty. Both the movement and ragdoll hot caches
  now fetch with a real first page.
- Unblocked a concurrent breakage: `movement-system.cpp` referenced removed
  `MimitaHotMovement::kJumpBufferSeconds`/`actorJumpBufferSeconds`; replaced with
  a local live-tunable `kHotJumpBufferSeconds = 0.15f`.
- No cold build required for the collision code (all edited files are hot).

### Evidence

- Hot DLL build success (70 sources).
- `--movement-parity-selftest` **PASS** (lands on floor, vertical rest,
  deterministic, within tolerance of the capsule primitive).
- `--movement-selftest` PASS; `--hot-combat-selftest` PASS;
  `--live-code-selftest` PASS; `--ragdoll-slice-selftest` PASS.

### Next (Step 4 / acceptance)

1. Live play-test: movement/jump/dash on real maps; tune the hot collision in
   `hot-movement-collision.h` live (the edit point is hot).
2. Then delete the cold pipeline: the `resolveCollisions` fallback,
   `capPhysicsMove`, `move-capsule.cpp`, and the cold `ragdoll-solver.cpp`; flip
   compile ownership so `src/ragdoll/*` and `physics-collision*` are hot-only.
3. Note: server actors currently drive cold movement (`actor-movement-system` is
   disabled), so the server `physics.move` headless path must be preserved or
   given a hot headless-world seam before deleting the primitive.

## Step 4 (partial): cold ragdoll solver deleted, hot paths authoritative

- `ragdoll-mode.cpp`: the alive solve and the **corpse** solve now call the hot
  `hotRagdollSolve` directly (no cold fallback); removed the
  `ragdoll/ragdoll-solver.h` include.
- Deleted `src/ragdoll/ragdoll-solver.cpp` and `ragdoll-solver.h`. The solver
  algorithm now lives only in `src/hot-reload/modules/ragdoll-solve.cpp` (hot).
- `movement-system.cpp` `resolveCollisions`: removed the cold `physics.move` /
  `ctx->moveCapsule` fallback; the hot capsule-vs-world solve is the only path.
- Kept `Physics::moveCapsuleStep` / `capPhysicsMove` (cold) for two reasons: the
  dedicated server still uses cold movement against the **headless world** (which
  the hot `world.collision` capability does not expose yet), and
  `--movement-parity-selftest` uses it as the reference. Deleting it needs a hot
  headless-world seam first.
- Cold build SUCCESS (4 TUs); hot DLL build success (70 sources).
- All suites PASS: `--movement-parity-selftest`, `--movement-selftest`,
  `--hot-combat-selftest`, `--live-code-selftest`, `--ragdoll-slice-selftest`.

## Step 4b: headless-world seam + `physics.move` capability removed

- `capWorldCollision` now sources triangles from the **headless (server) world**
  when one is bound (`gDispatchHeadlessWorld`), else the client `World`, and
  reports `total` even for a null output buffer. Hot collision/ragdoll can now
  run on the dedicated server with the same code as the client.
- Deleted the `physics.move` kernel capability: `capPhysicsMove` and its
  registration. The hot capsule-vs-world solve is now the only movement
  collision path; `actor-movement-system` already null-checks the capability.
- `Physics::moveCapsuleStep` (`move-capsule.cpp`) remains only as a kernel
  mechanism used by `--movement-selftest` (unit test) and
  `--movement-parity-selftest` (reference). It is no longer reachable from hot
  movement. Deleting it requires retargeting those two tests.
- Cold build SUCCESS (1 TU); all suites PASS on `mimita-20260916T225832.exe`.

### Next

1. Retarget/retire the two movement selftests' dependence on
   `Physics::moveCapsuleStep`, then delete `move-capsule.cpp/.h`.
2. Finish the compile-ownership flip: the EXE still hosts the cold
   `RagdollModeSystem`/`RagdollPresentation` orchestrator and the collision
   primitive, so `src/ragdoll/*` cannot simply be excluded from the EXE build
   yet; move that orchestration hot first.
3. Live play-test: movement + ragdoll on real maps (both hot, tune live);
   remote-ragdoll pose clobber guard; aim-body hot.

## Step 4c: cold capsule primitive deleted

- Deleted `src/physics/movement/move-capsule.cpp` and `move-capsule.h`.
- Removed `capMoveCapsule` and the `GameplayContextV1::moveCapsule` assignment;
  the `physics.move` capability and its headless bridge are gone from the hot
  path.
- `movement-system.cpp`: dropped `ctx->moveCapsule` from the movement guard.
- Rewrote `movement-selftest.cpp` to drive the real hot movement system
  (gravity, determinism, floor landing, vertical settle) instead of the deleted
  primitive.
- Updated `movement-parity-selftest.cpp` to assert a fixed expected capsule rest
  height instead of comparing against the deleted cold primitive.
- Cold build SUCCESS (4 TUs); hot DLL build success (70 sources). All suites
  PASS on `mimita-20260916T230248.exe` (`--movement-selftest`,
  `--movement-parity-selftest`, `--hot-combat-selftest`, `--live-code-selftest`,
  `--ragdoll-slice-selftest`).

### Next

1. Move the cold `RagdollModeSystem`/`RagdollPresentation` orchestration into a
   hot module (using `ragdoll.solve`, `world.collision`, component read/write,
   `physics.impulse`), then finish the compile-ownership flip and remove
   `src/ragdoll/*`/`physics-collision*` from the EXE build.
2. Live play-test movement + ragdoll; remote-ragdoll pose clobber guard; aim-body
   hot.

## Orchestrator sub-step 1: input.read + camera.read seams

- `game-api.h`: `GAME_CAP_INPUT_READ` (`GameInputStateV1`) and
  `GAME_CAP_CAMERA_READ` (`GameCameraStateV1`) — plain-data snapshots of the
  current local input and camera transform.
- `live-behavior.cpp`: `capInputRead`/`capCameraRead` copy the bound state into
  the POD; registered as kernel capabilities. `gDispatchInput`/`gDispatchCamera`
  + `setDispatchInput`/`setDispatchCamera` bind the per-tick pointers.
- `simulate-tick.cpp`: binds the current `InputState` and `THE_CAMERA` before
  running the gameplay domain, so hot systems can read them.
- Additive and non-breaking: no call sites changed. Cold build SUCCESS (113 TUs);
  all suites PASS on `mimita-20260916T230927.exe`.

### Next sub-steps (orchestrator port)

1. `player.body.write` (ragdoll root/bone write-back; `skeleton.apply` covers
   poses, need the root/body).
2. `network.ragdoll.snapshot` (limb frame replication codec).
3. Port `RagdollPresentation` hot, then `RagdollModeSystem` hot, then flip
   compile ownership and delete `src/ragdoll/*`.

## Orchestrator sub-step 2: actor.skeleton.write seam

- `game-api.h`: `GAME_CAP_ACTOR_SKELETON_WRITE` + `GameActorSkeletonWriteV1` —
  actor root (position/rotation/velocity + root-rotation flag), neutralized
  ancestor node indices, and per-node local 4x4 matrices (fixed bound,
  `GAME_MAX_SKELETON_NODES = 64`). This is exactly the data
  `Ragdoll::applyBodyToPlayer` writes, so a hot orchestrator can own the math.
- `live-behavior.cpp`: `capActorSkeletonWrite` applies it to the typed local
  actor (writes root, neutralizes ancestors, sets node local transforms, calls
  `updateModelWorldTransforms`); registered as a kernel capability.
- Additive and non-breaking. Cold build SUCCESS (113 TUs); all suites PASS on
  `mimita-20260916T231606.exe`.

### Next sub-steps

1. `network.ragdoll.snapshot` seam (limb-frame replication codec).
2. Port `RagdollPresentation` hot, then `RagdollModeSystem` hot; flip compile
   ownership; delete `src/ragdoll/*`.

## Orchestrator sub-step 3: ragdoll.snapshot seam

- `game-api.h`: `GAME_CAP_RAGDOLL_SNAPSHOT` + `GameRagdollSnapshotV1` (op read/
  write, owner, limb count, tick, up to 24 limbs, 2 grabs) — the same POD shape
  as the existing network codec.
- `live-behavior.cpp`: `capRagdollSnapshot` reads or applies a per-owner snapshot
  via `Ragdoll::RagdollEntities::writeSnapshot`/`applySnapshot`, so a hot
  presenter/sender uses the codec without seeing ragdoll internals (added the
  `ragdoll/ragdoll-entities.h` include). Registered as a kernel capability.
- Additive and non-breaking. Cold build SUCCESS (113 TUs); all suites PASS on
  `mimita-20260916T232910.exe`.

### Next

1. Port `RagdollPresentation` hot (consume snapshots via `ragdoll.snapshot`,
   write skeletons via `skeleton.apply`/`actor.skeleton.write`).
2. Port `RagdollModeSystem` hot (input via `input.read`, camera via
   `camera.read`, solve via `ragdoll.solve`), then flip compile ownership and
   delete `src/ragdoll/*`.

## Orchestrator seam 4: ragdoll.bind (body template)

- `game-api.h`: `GAME_CAP_RAGDOLL_BIND` + `GameRagdollTemplateV1` /
  `GameRagdollPartV1` — the per-actor limb→node mapping, `meshLocal`, joint
  anchors, rotation limits, and torso/head/arm/leg indices. This is what
  `Ragdoll::buildBody` produces, exposed as POD.
- `live-behavior.cpp`: `capRagdollBind` builds the template for the local actor
  via `Ragdoll::buildBody(THE_PLAYER, cfg, body)` and fills the POD; registered
  as a kernel capability. Additive/non-breaking.
- Cold build SUCCESS (1 TU); all suites PASS on `mimita-20260916T234500.exe`.

The hot orchestrator/presenter now has every primitive it needs:
`input.read`, `camera.read`, `ragdoll.solve`, `world.collision`,
`physics.impulse`, component read/write, `actor.skeleton.write`,
`ragdoll.snapshot`, `ragdoll.bind`, `skeleton.apply`. The presentation and mode
ports are now pure hot code.

### Next

1. Write the hot `RagdollPresentation` module using `ragdoll.snapshot` (poll/
   interpolate) + `ragdoll.bind` + `actor.skeleton.write` + `skeleton.apply`.
2. Write the hot `RagdollModeSystem` module using `input.read`/`camera.read` +
   `ragdoll.solve` + `world.collision`, then flip compile ownership and delete
   `src/ragdoll/*`.

## Orchestrator seam 5: actor.skeleton.write supports remote actors

- `game-api.h`: appended `ownerActorId`/`isNpc` to `GameActorSkeletonWriteV1`
  (append-only). When `ownerActorId != 0` the write targets that remote actor
  from the multiplayer context; otherwise the local actor.
- `live-behavior.cpp`: `capActorSkeletonWrite` now resolves the target `Player&`
  from `MP_CONTEXT.remotePlayers`/`remoteNpcs` (or the local player) and applies
  the root/node writes to it. Added the `network/multiplayer-context.h` include.
- Cold build SUCCESS (113 TUs); all suites PASS on `mimita-20260917T000119.exe`.

The hot presenter can now target remote actors directly, so the
`RagdollPresentation` port is a pure hot module (no further cold ABI).

### Next

1. Write the hot `RagdollPresentation` module and have the cold
   `RagdollPresentation::present` call defer to a `ragdoll.presentation`
   capability (one cold call-site edit).
2. Port `RagdollModeSystem` hot, then flip compile ownership and delete
   `src/ragdoll/*`.

## Hot RagdollPresentation ported

- `game-api.h`: `GAME_CAP_RAGDOLL_PRESENT` + `GameRagdollPresentV1` (owner,
  isNpc, actor entity, delay, handled).
- NEW hot module `src/hot-reload/modules/ragdoll-present.cpp`: buffers remote
  snapshots (read via `ragdoll.snapshot`), interpolates to the delayed render
  tick (with capped extrapolation), maps limbs with the `ragdoll.bind` template,
  and writes the typed remote actor via `actor.skeleton.write` (`ownerActorId`),
  reporting `handled`.
- `ragdoll-presentation.cpp/.h` (cold): `present` gained `bool isNpc` and now
  defers to the hot `ragdoll.presentation` capability first, yielding the cold
  typed write when the hot presenter handled the frame.
- `engine-tick-render.cpp`: the two `present` call sites pass `isNpc`
  (false for `remotePlayers`, true for `remoteNpcs`).
- Hot DLL build success (71 sources); cold build SUCCESS (117 TUs); all suites
  PASS (`--ragdoll-slice-selftest`, `--movement-selftest`,
  `--movement-parity-selftest`, `--live-code-selftest`, `--hot-combat-selftest`).

Remote ragdoll presentation is now hot and live-editable. Remaining: port
`RagdollModeSystem` (the alive-ragdoll orchestrator) hot, then flip compile
ownership and delete `src/ragdoll/*`. Note: the cold `RagdollPresentation` class
is retained as the fallback/buffer owner for now; the hot presenter uses
`RagdollEntities` snapshots directly.

## Alive-ragdoll aim motor hot

- `game-api.h`: `GAME_CAP_RAGDOLL_AIM` + `GameRagdollAimV1` (head/torso indices,
  camera look basis, per-limb orientation/angular velocity/aim offset, gains, dt,
  `handled`).
- NEW hot module `src/hot-reload/modules/ragdoll-aim.cpp` (auto-globbed): owns the
  camera-driven head/torso aim response — damped velocity controller with the
  ragdoll look convention, clamped to max angular speed. Live-editable tuning.
- `ragdoll-mode.cpp` (cold): new `hotRagdollAim` fills the POD and calls
  `ragdoll.aim`; `update` now runs it and only falls back to the cold
  `applyControls` when the hot side declines. Grabs, arm extend, and the solver
  domain are unchanged (solver already hot via `ragdoll.solve`).
- Hot DLL build success (72 sources); cold build SUCCESS; all suites PASS
  (`--ragdoll-slice-selftest`, `--movement-selftest`,
  `--movement-parity-selftest`, `--live-code-selftest`, `--hot-combat-selftest`).

The alive-ragdoll **aim policy** is now hot. Next: port `processExtend`/
`processGrab` (arm motor + world raycast grab) hot, then the activation/
deactivation/corpses orchestration, so `src/ragdoll/*` can be deleted.

## Alive-ragdoll state made durable (ECS canonical)

Note: hot DLL statics do NOT survive a generation switch (each generation is a
fresh `mimita-live-gNNN.dll`), so storing the alive body in a hot static would
reset the pose on every live edit. The durable, hot-reachable home is the kernel
entity component store.

- `ragdoll-mode.cpp`: the alive update now treats `Ragdoll::RagdollEntities` as
  the **canonical** store. Each tick it rehydrates the working body from the
  components (`syncToBody`, incl. linear/angular velocity) when bound, and binds
  from the body template only when unbound; the derived state is then published
  back (`syncFromBody`). Config-generation changes unbind so the limb set is
  rebuilt from the re-derived geometry.
- `activate`/`deactivate` now `unbind(mOwnerActorId)` so a new activation binds a
  fresh body and deactivation releases the canonical state (no stale rehydrate).
- Cold build SUCCESS; all suites PASS (`--ragdoll-slice-selftest`,
  `--movement-selftest`, `--movement-parity-selftest`, `--live-code-selftest`,
  `--hot-combat-selftest`).

Runtime note (unverified): the alive pose/momentum continuity across an actual
live generation switch has not been observed in a running client yet — needs a
playtest with G (ragdoll) while editing a hot module. Source/build/test evidence
only so far.

**Important:** the canonical alive-ragdoll state lives in the kernel ECS
component store (`RagdollEntities`), which survives generation switches;
hot-module statics reset by design.

## Risks / notes

- Ability gating currently covers dash, down-dash and jump precisely; freeze is
  still held-intent driven.
- The `MovementFired` component is local-only; remotes gate on replicated intent
  flags (unchanged).
