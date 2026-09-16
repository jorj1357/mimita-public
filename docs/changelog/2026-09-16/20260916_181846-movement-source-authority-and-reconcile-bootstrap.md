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
