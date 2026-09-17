// 2026-09-16T18:18:46Z
/* purpose
* record the movement-source-authority + reconciliation-bootstrap session
* separate this session's edits from pre-existing uncommitted work
* report build/runtime evidence and what remains unverified
* this file DOES NOT claim human acceptance that did not happen
*/

# Task

- Task ID: restore-source-movement-phase1-2
- Summary: make the hot C++ Source preset the single movement tuning authority
  for client, server, and NPCs (JSON demoted to reference), and stop the
  floating correction loop by splitting reconciliation reasons and turning
  generation mismatch into a bootstrap instead of a hard snap.
- Status: implemented and verified. Cold EXE rebuilt; reconciliation, movement,
  algorithm, and full real-path air-parity selftests pass. Human/live gameplay
  acceptance still required.
- Date, time, timezone: 2026-09-16T18:18:46Z
- Branch: 8292026stash
- Base commit: b35a0ebd3a12b593bcae13e57df503d174a9a6ec
- Final commit: (none; no commit made this session)

# Pre-existing changes

- Exact status output (before this session's edits, the working tree already
  had uncommitted work from a prior session): many files modified including
  `src/entities/player.h`, `src/hot-reload/game-api.h`, `src/hot-reload/hot-package.h`,
  `src/live-code/live-behavior.*`, presentation modules, `src/combat/weapon-viewmodel.*`,
  `src/network/hot-combat-selftest.cpp`, and two untracked 2026-09-16 changelogs.
- Files not created or modified by this session: all of the above pre-existing
  presentation/animation/hot-package work. This session's edits are limited to
  the files listed under "Exact implementation changes".

# Requested behavior

- One C++ Source movement authority, hot-reloadable, default for all actors.
- `movement-source.json` / role / NPC difficulty movement JSON no longer decide
  runtime movement.
- Server and NPC movement use the same Source policy/tuning as the client.
- Reconciliation: separate lifecycle/position/generation-bootstrap causes;
  generation mismatch must not become a position correction; zero-distance must
  not produce a correction effect; only lifecycle/teleport/major divergence may
  hard-snap.

# Specification alignment

- Specs: `docs/specs/movement/movement.md` (shared movement, Source mode,
  correction levels), `docs/features/live-code-development/...`,
  AGENTS.md (single owner, hot-reload preference, smallest patch).
- Requirements met at the policy/tuning level. The literal per-actor
  `for (Actor...)` dispatch and a single orchestration function remain deferred
  (see "Still unverified / remaining").
- Conflicts or decisions: the user chose staged delivery, shared policy with a
  single tuning authority, Smooth = quiet converge (no hard snap), and JSON
  removed from runtime but kept as reference.

# Exact implementation changes

## `src/hot-reload/hot-movement-policy.h`
- Added `GameMovementTuningV1` payload and `GAME_EVENT_MOVEMENT_TUNING`
  (`gameHash("movement.tuning")`). The hot module is the one tuning authority.

## `src/hot-reload/modules/movement-system.cpp`
- Added `kSourceMovement`, `defaultMovementMode()`, and an
  `onMovementTuning` handler that publishes the active Source preset and logs
  `source=cpp mode=... authority=shared-hot-movement`.
- Registered `movement.tuning`. Existing local `movement.main` behavior
  unchanged.

## `src/config/movement-config.h` / `.cpp`
- Marked the JSON loader reference/archive; exposed
  `MovementConfig movementRuntimeDefaults()` returning the built-in C++ base.

## `src/physics/movement/movement-conversion.cpp`
- `makeCurrentRuntimeMovementConfig()` now builds its `MovementConfig` from the
  hot `movement.tuning` event (compiled-in identical fallback when no module),
  not from `MovementJsonConfig`. This is the single cold consumer change that
  routes server, NPC, and validation through the C++ Source preset.

## `src/network/server-players.cpp`
- Server player simulation drops `RoleMovementCache`; uses
  `makeCurrentRuntimeMovementConfig()`.

## `src/npc/npc.cpp`, `src/npc/npc-traversal.cpp`, `src/npc/npc-navigator.cpp`
- NPC movement/navigation/traversal drop role/difficulty movement JSON; use the
  C++ Source config. Difficulty AI fields (dash chance, noise, etc.) unchanged.

## `src/main-systems.cpp`, `src/engine/engine-tick-setup.cpp`, `src/network/server.cpp`
- Removed startup load and per-tick `MovementJsonConfig` polling so movement
  JSON is not silently loaded/used. `RoleMovementCache` polling left for
  gamemode role validation only.

## `src/hot-reload/hot-reconciliation.h`
- Added `GameReconcileReasonV1`, `GameReconcileModeV1`,
  `GameReconcileHardResetV1`. Payload size deliberately unchanged; bootstrap is
  encoded in the existing `hardReset` field (=2) so a running executable can
  hot-load the new policy safely.

## `src/hot-reload/modules/reconcile-policy.cpp`
- Generation mismatch (both generations non-zero and different) ->
  `correctionMode=None`, `hardReset=BOOTSTRAP`, no `shouldCorrect`.
- Distance policy: `error <= 0.25` or `< smallDistance` -> None;
  `>= smallDistance && < majorDistance` -> Smooth; `>= majorDistance` -> Snap.

## `src/network/multiplayer-reconcile.cpp`
- Added cold `LocalReconcileReason` (None/LifecycleSnap/PositionDivergence/
  GenerationBootstrap); computes the reason and logs it plus `bootstrap=`.
- `hotNeedsBootstrap` read from `hardReset==BOOTSTRAP`; post-gap resync now
  requires `>= majorDistance` (medium drift no longer hard-snaps every frame).
- Correction effect only when the reason is PositionDivergence, the snap was
  applied, and `error > 100` (`kCorrectionEffectMinimum`).

## `src/network/reconciliation-policy-selftest.cpp`
- Updated expectations: zero-distance no-op, smooth band widened, generation
  mismatch -> bootstrap (no hard reset), added mismatch-with-error case.

## `src/physics/movement/move-capsule.cpp` (parity root cause)
- `moveCapsuleStep` no longer imposes the rigid-body default `maxLinearSpeed`
  (60) on caller-supplied velocity. That clamp scaled horizontal velocity down
  whenever vertical speed pushed total speed above 60, so the client lost
  horizontal speed while falling fast and diverged from the server.
- `gravityScale < 0` now means "caller already integrated gravity" (no solver
  gravity), preserving the documented `0/absent = 1.0` contract for all other
  callers/tests.

## `src/hot-reload/modules/movement-system.cpp` (gravity ownership)
- `movement.main` sets `MovementStateV1.gravityScale = -1.0f` so the generic
  capsule solver does not add 9.81 on top of the hot gravity policy.

## `src/network/air-movement-parity-selftest.cpp`
- The harness now asserts full-sequence parity instead of always printing a
  "not achieved" warning.

# Diagnostics

- Owner/category: movement hot module (`[MOVEMENT TUNING]`), networking
  (`[LOCAL CORRECTION]`).
- Input: predicted/authoritative position+velocity, generations, error.
- Decision: reason + correction mode.
- Output: reason name, `bootstrap=`, applied flag, distance.
- Rate limiting: existing `[LOCAL CORRECTION]` 500 ms gate preserved.

# Validation

- Hot DLL build: `python build_game_dll.py` -> success, 54 sources (later
  concurrent session files changed the count; that build also succeeded).
- Cold EXE build: `python build_agent.py` -> SUCCESS (`mimita.exe` rebuilt).
- Selftests (real EXE, real hot DLL):
  - `--reconciliation-policy-selftest` -> PASS (all 8 checks).
  - `--movement-parity-selftest` -> PASS (7 checks).
  - `--movement-selftest` -> PASS.
  - `--movement-algorithm-selftest` -> PASS.
  - `--air-movement-parity-selftest` -> PASS, full 120-tick real-path parity
    `maxDev=0.000003` (was `2.2699` before the fix).
- Full cold EXE build: run successfully this session.
- Selftests: run successfully this session.

# Measured evidence

- Before: generation mismatch forced `correctionMode=4` hard reset every frame;
  medium drift could snap via post-gap resync (>1.5); air parity `maxDev=2.2699`
  (first divergence at tick 68) because the client capsule step double-applied
  gravity and clamped total speed to 60.
- After: mismatch -> mode 0 + `hardReset=2` (no position change); post-gap snap
  requires `>= 100`; air parity `maxDev=0.000003`.
- Timestamps: cold EXE built 2026-09-16 ~14:47-14:54 local; selftests 14:55.
- Tick/frame/network measurements: 120-tick deterministic real-path comparison.

# Regression review

- Regression entry appended: no.
- Why: the floating correction loop is plausibly a real regression, but it was
  not reproduced live this session and the fix is unverified in the running
  build. No confirmed regression recorded without evidence.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not performed.
- Multiplayer review: not performed.
- Still unverified: live gameplay feel; NPC vs human parity under real input;
  hot-reload tuning edit/rollback while the same session is running.

# Related feature record

- Feature path: `docs/features/movement/movement.md` (not yet updated).

# Remaining after Phase 1 + 2

- One shared composition function (`simulateSourceMovement`) is still not a
  single named entry point; the client (`movement.main`) and the cold server
  kernel remain two orchestrations over the shared hot policies. Verified
  identical for ordinary airborne movement; other branches (freeze duration,
  external-impulse fold, contact reset, landing timers) are still client/server
  asymmetric.
- Actor parity test (human/NPC/generic fixture) and hot-reload tuning
  change/rollback test not added.
- Smooth correction has no visual offset (quiet converge only).
- Movement journal fields and JSON preset relabeling/relocation still pending.


---

# Step 0 addendum — generic hot actor-movement bridge (2026-09-16T19:43:14Z)

Cold bridge installed so all movement policy for players and NPCs can be fixed
live without another EXE restart. One-time cold rebuild; payload sizes unchanged
(reserved slots reused), so a running EXE can still hot-load the new policy.

## Root cause found during the bridge
- The shared movement kernel (`movement-step.cpp`) only ever updates velocity;
  it never does `position += velocity * dt`. `resolveCapsuleCollisionAgainstWorld`
  only depenetrates. So the server's `simulatePlayer` never advanced the player,
  which is why the server stayed at spawn and the client drifted until the
  100-unit catastrophic snap pulled it back. This addendum integrates position
  in the cold fallback and gives the hot system the primitive to own it.

## Cold changes
- `src/network/server-players.cpp`
  - `simulatePlayer(..., uint32_t serverTick)`; integrates `state.position +=
    state.baseVelocity * SERVER_DT` before world collision (fallback fix).
  - Yields to the hot result: skips the kernel when
    `MovementRuntimeStateComponent.lastSimTick == serverTick`.
- `src/network/server.cpp` (dedicated + listen): binds the headless collision
  world via `LiveBehavior::setDispatchHeadlessWorld`; call sites pass the tick.
- `src/live-code/live-behavior.cpp/.h`: `setDispatchHeadlessWorld`,
  `moveCapsuleStepHeadless` (integrate + resolve vs `HeadlessWorld`), selected by
  the new `GAME_PHYSICS_MOVE_HEADLESS` flag in `physics.move`; `capLog` now also
  writes to `StructuredLogger` (file-backed); `capFindEntities` filters for
  VELOCITY/MOVEMENT_INTENT/MOVEMENT_RUNTIME_STATE/BODY/AIM_INTENT.
- `src/ecs/components.h`: `MovementRuntimeStateComponent.lastSimTick/lastSimGeneration`.
- `src/hot-reload/game-api.h`: documented reserved-slot contract
  (`GAME_MOVEMENT_STAMP_*`, `GAME_PHYSICS_MOVE_HEADLESS`,
  `GAME_MOVEMENT_VALIDATION_FORCE_ACTIVE`); no struct size changes.
- `src/network/server-packets.cpp`: publishes accepted input to the actor's
  generic `MovementIntentComponent`; honors hot `forceActive` to clear the spawn
  wedge; resets `p.inputCommandBuffer` lifecycle runs through the hot policy.
- `src/network/multiplayer-reconcile.cpp`: honors hot `applyMode`; SMOOTH_ONCE is
  rate-limited (250 ms) so it can never become the floating loop.
- `src/hot-reload/modules/reconcile-policy.cpp`: sets `reserved` applyMode
  (NONE/SMOOTH_ONCE/SNAP).

## Validation
- Cold `python build_agent.py` -> BUILD SUCCESS.
- Hot DLL builds.
- `--server-spatial-authority-selftest` PASS; `--movement-parity-selftest` PASS;
  `--air-movement-parity-selftest` PASS; `--reconciliation-policy-selftest` PASS.

## Not yet done (Step 1)
- The hot actor-movement system that actually writes the stamp and owns player +
  NPC movement. Until it exists, the cold fallback (now integrating) runs.

---

# Bridge addendum 2 — generic actor policy seams (2026-09-16T16:14:00Z)

One more cold bridge so spawn, input send/receive, lifecycle, and server
movement are all hot-editable live. Built and selftested.

## Cold additions (dispatch facts; hot owns the decision)
- `game-api.h`: `GAME_COMPONENT_CONTROL_SOURCE` + `GameControlSourceComponentV1`;
  payloads/ids `ActorSpawnPolicyV1`/`actor.spawn-policy`,
  `InputSendPolicyV1`/`input.send-policy`,
  `InputReceivePolicyV1`/`input.receive-policy`,
  `ActorLifecyclePolicyV1`/`actor.lifecycle-policy`.
- `live-code/live-behavior.cpp`: read/write ControlSource; findEntities filter.
- `server.cpp`: dispatches the spawn policy for startup NPCs (dedicated + listen);
  runs `GAME_DOMAIN_POST_MOVEMENT` after NPC AI (dedicated + listen).
- `sim/simulate-tick.cpp`: runs `GAME_DOMAIN_POST_MOVEMENT` after NPC update.
- `server-packets.cpp`: dispatches the input-receive policy; tracks
  `inputPacketsSeen`/`lastInputPacketMs` per player.
- `multiplayer-tick.cpp`: dispatches the input-send policy with gate facts.
- `server-players.cpp`: dispatches the lifecycle policy before respawn.
- `server.h`: per-player input receive counters.

## Hot modules (all live)
- `spawn-policy.cpp`: suppresses startup NPCs (they spawned on the player's
  spawn point and killed the player). Toggle by editing the module live.
- `input-send-policy.cpp`: sends whenever connected + input present + due,
  ignoring the generation-bootstrap gate (a suspected input=0 cause).
- `input-receive-policy.cpp`: accepts received input.
- `lifecycle-policy.cpp`: respawns at the cold-chosen position.
- `actor-movement-system.cpp` (`movement.actors`, GAMEPLAY priority 50): moves
  authoritative remote-network actors from generic components, shared Source
  policy + `physics.move` (headless), stamps the tick so cold yields.
- `hot-actor-movement.h`: shared Source tuning table for actor movement.

## Validation
- `python build_game_dll.py` -> success (62 sources).
- `python build_agent.py` -> BUILD SUCCESS (136 files compiled).
- Selftests PASS: server-spatial-authority, movement-parity, air-movement-parity,
  reconciliation-policy, movement-selftest, movement-algorithm.
- Registered: system `movement.actors`; events `actor.spawn-policy`,
  `input.send-policy`, `input.receive-policy`, `actor.lifecycle-policy`.

## Log evidence that drove this (user session)
- `[SERVER NPC SPAWN] ... spawnpoint=0 position=(player spawn)` plus 9
  `SERVER_NPC_KILLS_PLAYER` and 4 `rocket_explosion` -> repeated death/respawn
  lifecycle snaps ("teleported back over and over").
- `input=0` in every `[SERVER STATUS]`; client emitted only 28/80-byte ICE sends
  and never logged `[CLIENT INPUT EPOCH]` (`sizeof(InputPacket)==244`) -> the
  client input-send block was not running.

## Still cold (mechanism only)
Packet layout/protocol, transport, entity/component storage, the hot-result
yield check, and the collision primitive. If the input=0 root cause is a
structural send-block bug, that one line is the only remaining cold repair.

---

# Bridge addendum 3 — server adopts client movement + contact reset (2026-09-16T17:09:00Z)

Fixes the "server position error grows forever / disagreement beam" by making the
server adopt the validated client movement as authoritative, and makes abilities
contact-reset-only (no time cooldown).

## Cold
- `game-api.h`: `InputReceivePolicyV1` extended with `playerEntity`, `serverTick`,
  `reportPosition/Velocity`, `reportGrounded`, out `adoptState`.
- `server.h`: `ServerPlayer.adoptClientMovement`.
- `server-packets.cpp`: fills the receive-policy report facts; records
  `adoptClientMovement` from the hot decision.
- `server-players.cpp`: at the top of `simulatePlayer`, when
  `adoptClientMovement` is set, project the accepted typed state to the generic
  Transform/Velocity/Health and skip kernel simulation (server tracks the client
  exactly, no drift).
- `live-behavior.cpp`: `MovementStateV1.collided` now means "any real world
  contact" (`grounded || !movementContacts.empty()`), enabling universal reset.

## Hot
- `input-receive-policy.cpp`: `adoptState = 1` (spec phase 1 client-trusting).
  Flip to 0 live to return to server simulation.
- `actor-movement-system.cpp`: `kSimulateServerActors = false` while adopting
  (no fighting); dash cooldown set to 0.
- `movement-system.cpp` (local client):
  - dash has **no time cooldown** (restored only by contact);
  - abilities reset on **any contact** (`grounded || collided`), not just ground;
  - jump eligibility uses grounded-or-recent-contact, so wall/ledge touch lets
    you jump; contact state is carried in the runtime-state flag bits.

## Validation
- `python build_game_dll.py` -> success (62 sources).
- `python build_agent.py` -> BUILD SUCCESS (166 files).
- Selftests PASS: server-spatial-authority, movement-parity, air-movement-parity,
  reconciliation-policy, movement-selftest.

## Evidence driving it (user session)
- Spawn policy fixed NPC-on-spawn (`spawned=0`, no NPC kills).
- Input send policy fixed `input=0` (`input=10762`).
- Remaining: `[SERVER MOVEMENT DECISION] correct reason=blocking-geometry`
  with `serverPos` ~60 units from `reportPos` -> server simulation drift.

---

# Addendum 4 — generation-mismatch policy, spawn protection, self-damage (2026-09-16T17:45:00Z)

## Confirmed cause of the current symptom
- The jsonl showed the client pinned to hot generation 10 while the server built
  77..83 with `result:"retry"`, plus
  `cold_restart_pending file=src/live-code/live-behavior.cpp`. The two peers ran
  different movement code, so the server never adopted the client's movement.
- New regression record:
  `docs/regressions/2026-09-16-server-client-generation-mismatch-spawn-lock.md`.

## Hot module changes (editable live)
- `net.generation-policy` (new `generation-policy.cpp`): a mismatch is now a hot
  decision; default `allowWorld = 1` so it can never silently wedge. Cold
  `mpGenerationWorldAllowed` dispatches it.
- `lifecycle-policy.cpp`: spawn protection is **ticks** (60 = 1 s at 60 Hz),
  armed on the actor entity as dynamic component `SpawnProtection { untilTick }`.
  Applies to every actor (players and NPCs).
- `rocket-behavior.cpp`: enforces spawn protection (zero damage/knockback while
  `tick < untilTick`); removed the leftover "explosion damage = 123" override.
- `tools/rocket-tool.cpp` + `tools/grenade-tool.cpp`: self-damage multiplier
  `0.5f -> 0.2f` (field `hot-projectile.h:36`, applied `hot-projectiles.cpp`).

## Cold bridge (this build)
- `game-api.h`: `ActorLifecyclePolicyV1.actorEntity` + `spawnProtectionTicks`;
  `GenerationPolicyV1` + event; `HotSpawnProtectionV1` in `hot-movement-policy.h`.
- `multiplayer-tick.cpp`: generation policy dispatch in `mpGenerationWorldAllowed`.
- `server-players.cpp`: fills `actorEntity` for the lifecycle policy.

## Validation
- Hot DLL build success (63 sources). Cold build success.
- Selftests PASS: server-spatial-authority, movement-parity, air-movement-parity,
  reconciliation-policy, movement-selftest.
- Registered: `actor.lifecycle-policy`, `net.generation-policy`.

## Notes
- The build now emits timestamped exes; copied the newest to `mimita.exe`.
- After this one relaunch, all movement/spawn/input/lifecycle/reconcile/
  generation decisions are hot; no further cold restart is needed for those.

---

# Addendum 5 — jump buffer, disabled divergence correction, Accept-all (2026-09-16T18:06:00Z)

## Part 1 (hot)
- `movement-system.cpp` / `actor-movement-system.cpp`: jump buffer now comes from
  `hot-actor-movement.h` (`kJumpBufferMode`: 0=seconds default, 1=ticks;
  `kJumpBufferSeconds=0.2`, `kJumpBufferTicks=12`). Fixes "cannot jump":
  the buffer was 0 so the shared jump policy always early-returned. Held jump +
  auto-bhop now jumps whenever a jump resource is available; any contact resets
  the resource (wall climbing by touch).
- `reconcile-policy.cpp`: position-divergence corrections disabled
  (`MODE_NONE`/`APPLY_NONE`); server adopts client movement, so no rubberband.
  Restore block left in comments.
- `rocket-behavior.cpp`: movement-validation decision forced to Accept with a
  restore comment (`policy->decision = policy->computedDecision;`).

## Part 2 (bridge, this rebuild)
- `hot-reconciliation.h`: `GAME_RECONCILE_FLAG_ALLOW_POSTGAP` / `ALLOW_SNAP`.
- `multiplayer-reconcile.cpp`: honors the flags; post-gap and catastrophic snaps
  default off. The 100 m snap is now hot-controlled.
- `hot-modules.json`: tracks the physics/collision sources
  (`move-capsule.cpp`, `movement-step.cpp`, `physics-mini.cpp`,
  `movement-validation.cpp`) so a silent live edit is reported; added
  `hot-actor-movement.h` to hashed headers.
- `reconciliation-policy-selftest.cpp`: updated to the new policy (no divergence
  correction).

## Part 3 status (hot collision math)
- NOT completed in this pass. The real gameplay collision lives in the client
  `doCollisions` and the server `resolveCapsuleCollisionAgainstWorld`, both cold.
  Making the algorithm itself hot requires a hot capability provider (e.g.
  `physics.capsuleSolve`) that hot code implements against a generic world query;
  the stable EXE keeps only the dispatcher. This is the remaining large item.

## Regression record
- `docs/regressions/2026-09-16-movement-authority-adopt-fixed-rubberband.md`
  (fix direction confirmed, tuning remains).

## Validation
- Hot DLL current; cold build SUCCESS.
- Selftests PASS: reconciliation-policy (new expectations), movement-parity,
  air-movement-parity, server-spatial-authority, movement-selftest.

---

# Addendum 6 — spawn/death reliability + capsule fixes (2026-09-16T19:30:00Z)

## Phase 2 (spawn / lifecycle / death)
- `multiplayer-reconcile.cpp`: any new authoritative epoch with health > 0 now
  clears death state (`dead`, `proceduralFrozen`, `deathAnim`, `respawnTimer`,
  `killedBy`, `networkDeathPresented`) and sets a spawn flash. Fixes the sticky
  "you died to ..." death screen and the invisible/unmovable body after instant
  explode/auto-respawn, which previously depended on a Space-press serial or a
  health<=0 snapshot that could be skipped.
- `multiplayer-tick.cpp` `applyAuthoritativeSpawn`: refuses a (0,0,0) spawn
  position and keeps the last known good position (logs `[SPAWN GUARD]`).
- `config/gui/hud.json`: `deathOverlay` panel now `visible: false` and moved to
  a real y (was y=99460 and drawn unconditionally by the HUD).

## Phase 1 (capsule / float / stuck)
- `physics/movement/move-capsule.cpp`: fixed the half-vs-segment conflation.
  `MovementStateV1.halfHeight` is the tip-to-tip half extent; `RigidBody` wants
  the cylinder segment half. The old code inflated the fallback capsule by one
  radius per end (tip-to-tip 5.0 vs 3.6), a large constant float.
- `network/server.h`: server `PLAYER_RADIUS/HEIGHT` unified to the client
  capsule (0.7 / 3.6) so the two sides stop embedding each other at edges.
- `entities/player.cpp`: drop the rendered model root by 0.138 so the mesh feet
  meet the capsule bottom (model AABB feet at -1.662 vs capsule -1.8).

## Phase 3 (packet send / reliability)
- Added the networking/death sources to `hot-modules.json` `cold`
  (`server.cpp`, `client.cpp`, `packets.h`, `reliable-gameplay-events.cpp`,
  `snapshot-chunks.cpp`, `ice/ice-agent.cpp`, `death-system.h`) so a silent live
  edit is now reported instead of lost.
- The full hot `net.send` / `net.reliable` capability surface and hot collision
  math remain (large, untested this pass). Documented as the next bridge.

## Validation
- Cold build SUCCESS; hot DLL unchanged this pass.
- Selftests PASS: server-spatial-authority, movement-parity,
  air-movement-parity, reconciliation-policy, movement-selftest,
  entity-slice.

---

# Addendum 7 — Phase 3: hot send/reliability policy (2026-09-16T19:46:00Z)

## Hot seams added
- `game-api.h`: `GAME_EVENT_NET_SEND_POLICY` / `NetSendPolicyV1` and
  `GAME_EVENT_NET_RELIABLE_POLICY` / `NetReliablePolicyV1`.
- `reliable-gameplay-events.cpp`:
  - `tickReliableGameplayEvents` dispatches `net.reliable-policy` at the
    expiry/exhaustion branch (honors `keepConnection`, replacing the hardcoded
    chat-only special case) and at the retry branch (honors `retry`).
- `server-packets.cpp`: `buildAndSendSnapshot` dispatches `net.send-policy` per
  viewer (honors `send`).
- New hot modules:
  - `modules/net-send-policy.cpp` — default `send = 1`.
  - `modules/net-reliable-policy.cpp` — default `retry = 1`,
    `keepConnection = 1` (reliable failures never drop the connection; editable).
- Wire format (`packets.h`), chunking, and the transport sockets remain
  kernel-owned; only the send/retry/keep decisions are hot.

## Tracking
- Added the networking/death sources to `hot-modules.json` `cold` in the previous
  addendum so silent live edits are reported.

## Validation
- Hot DLL build success (67 sources, including the two new modules).
- Cold build SUCCESS.
- Selftests PASS: reconciliation-policy, movement-parity, air-movement-parity,
  server-spatial-authority.
- Registered: `net.send-policy`, `net.reliable-policy`.

## Still remaining
- Hot collision math (`physics.capsuleSolve`) — the actual sweep/slide algorithm
  in a hot module over the existing `world.collision` triangle capability.
- `net.reliable-policy.reliable` is carried but not yet honored at queue time
  (best-effort downgrade path still cold).

---

# Addendum 8 — hot collision solver seam + reliable downgrade (2026-09-16T20:34:00Z)

## Hot collision math (seam installed)
- `game-api.h`: `GAME_CAP_PHYSICS_CAPSULE_SOLVE` (`physics.capsuleSolve`) +
  `GameCapsuleSolveV1` (state in/out, `handled`).
- `live-behavior.cpp`: `capMoveCapsule` / `capPhysicsMove` call the hot provider
  when registered and it returns `handled = 1`; otherwise the kernel solve runs.
  This is the seam that lets the collision algorithm be edited live.
- `move-capsule.cpp` dispatches `movement.collision-policy`
  (`CollisionPolicyV1`) and honors `outRadius` / `outHalfHeight` /
  `outGroundedVelocityEpsilon`, so collision constants are hot.
- New hot modules:
  - `modules/collision-policy.cpp` — default keeps cold values (editable live).
  - `modules/collision-solver.cpp` — registers the provider; declines by default
    (`kHotCapsuleSolveEnabled = false`). Flip it on and implement the sweep over
    the `world.collision` triangle capability to own the algorithm live.
- Registered: event `movement.collision-policy`; capability
  `physics.capsuleSolve`.

## Reliable downgrade honored at queue time
- `reliable-gameplay-events.cpp`: `queueReliableGameplayEventToPlayer` now
  dispatches `net.reliable-policy` and, when the hot handler sets
  `reliable = 0`, sends the packet best-effort once (no queue/retry) instead of
  entering the reliable queue. This completes the carried-but-unused field.
  Default hot handler keeps `reliable = 1`.

## Validation
- Hot DLL build success (70 sources).
- Cold build SUCCESS.
- Selftests PASS: server-spatial-authority, movement-parity,
  air-movement-parity, reconciliation-policy, movement-selftest.

## Remaining
- The hot solver body itself (`collision-solver.cpp`) is a documented TODO: the
  seam is live, but the algorithm must be implemented before enabling
  `kHotCapsuleSolveEnabled`. The kernel solve remains authoritative until then.

---

# Addendum 9 — hot collision solver algorithm implemented (2026-09-16T20:42:00Z)

## Implemented
- `modules/collision-solver.cpp`: the `physics.capsuleSolve` provider now contains
  a working capsule solve:
  - integrates the caller velocity (gravity only when `gravityScale > 0`);
  - fetches world triangles through the kernel `world.collision` capability
    (added `collisionFn` / `collisionHost` to `GameCapsuleSolveV1`; populated in
    `live-behavior.cpp` `tryHotCapsuleSolve`);
  - closest-point-on-triangle depenetration + velocity projection over 3 passes
    and 3 capsule samples (segment endpoints + center), sets grounded on
    `normal.z > 0.35` and `collided`.
- Activation is a single live-editable constant:
  `kHotCapsuleSolveEnabled` in `modules/collision-solver.cpp`.
  Default `false` (kernel solve stays authoritative) so the working movement
  feel is unchanged; flip to `true` live to run the hot algorithm.

## Cold
- `game-api.h`: `GameCapsuleSolveV1` gained `collisionFn` / `collisionHost`.
- `live-behavior.cpp`: passes `capWorldCollision` to the provider.

## Validation
- Hot DLL build success (70 sources).
- Cold build SUCCESS.
- Selftests PASS: server-spatial-authority, movement-parity,
  air-movement-parity, reconciliation-policy, movement-selftest.
- Registered: `physics.capsuleSolve` capability.

## Notes / limits
- The hot solver is depenetration-based (matches the server resolver), not the
  client sweep/slide/step-up pipeline. Enabling it replaces the client solve, so
  step/slope feel may differ; that is why it defaults off and is live-toggleable.
- Triangle fetch is capped at 4096 per call (`world.collision` page limit).

---

# Addendum 10 — hot collision solver enabled + full pipeline (2026-09-16T20:55:00Z)

## Enabled + implemented in the hot module
- `modules/collision-solver.cpp`: `kHotCapsuleSolveEnabled = true`. The hot
  solver now owns the full capsule pipeline (all constants hot / live-editable):
  - integration + gravity (only when `gravityScale > 0`);
  - **swept substepping** (`kMaxSubsteps`, `kMinSubstepMove`) so fast actors do
    not tunnel;
  - **slide**: per-contact depenetration + velocity projection over
    `kResolvePasses` at 3 capsule samples;
  - **slope classification**: `kWalkableSlopeDot` decides walkable floor;
  - **step-up**: when horizontal progress is blocked (`kBlockedFraction`) on the
    ground, retry the move lifted by `kStepHeight` and settle back down;
  - **ground snap**: zero residual vertical velocity while grounded
    (`kGroundSnapEpsilon`);
  - geometry fetched through the kernel `world.collision` capability.
- This runs for every `physics.move`/`moveCapsule` caller (client prediction and
  any hot actor system), replacing the kernel solve.

## Validation
- Hot DLL build success (70 sources); no cold rebuild required (only a hot
  module changed).
- Selftests PASS with the hot solver active: movement-parity (hot movement lands
  on floor + vertical rest), air-movement-parity, movement-selftest,
  server-spatial-authority, reconciliation-policy.

## Notes
- Tuning is entirely in `modules/collision-solver.cpp` constants; edit and
  hot-activate live. Flip `kHotCapsuleSolveEnabled = false` to return to the
  kernel solve (also live).
- `movement.collision-policy` (capsule radius/half-height/grounded epsilon)
  still applies to the kernel fallback path.
