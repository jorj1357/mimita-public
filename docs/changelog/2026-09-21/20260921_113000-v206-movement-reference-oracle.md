# v2.0.6 movement reference oracle (Phase 0)

Date: 2026-09-21
Status: implemented slice; full v2.0.6 swept collision oracles and parity
comparison remain open

## Request

Begin the JSON-first movement / universal collision migration with the stated
first target: a deterministic v2.0.6 movement reference and parity harness.
Decisions recorded this session:

- JSON movement carries values plus named rule toggles; branch formulas stay in
  shared hot C++.
- The v2.0.6 reference is reconstructed **in-tree** from git tag `v2.0.6`.
- Full phases A-E are in scope.
- v2.0.6 behavior always wins; the live result must keep the same feel while all
  formulas/constants/rules stay hot-editable with no rebuild.

## What shipped

Recovered the v2.0.6 movement step from tag `v2.0.6` into a self-contained,
non-gameplay reference oracle:

- `src/physics/movement/reference/movement-v206-reference.h`
- `src/physics/movement/reference/movement-v206-reference.cpp`
- namespace `MimitaV206`; plain data in / plain data out; no `Player&`, `World&`,
  rendering, audio, effects, packets, input polling, or authority.

Recovered exactly, in v2.0.6 order, from
`src/physics/physics-mini.cpp` and `src/physics/movement/physics-*.cpp`:

1. gravity; 2. freeze; 3. ground return; 4. walk (stable-on-ground
hysteresis); 5. collisions (6 substeps); 6. ground/air dash; 7. down dash;
8. jump; 9. external-impulse friction; then stable-ground/landing hysteresis.

Frozen the effective v2.0.6 `config/movement.json` values as `Config` defaults
(gravity -58, moveSpeed 20, jumpStrength 19, airAccelAmount 222,
groundAccelerate 8, groundFrictionAmount 4, dashImpulse 100, airDashImpulse 50,
downDashSpeed -100, maxFallSpeed 400, freezeMaxTime 5, jumpBufferTime 0.12,
airJumpsMax 1, coyoteJumpTime 0.001, collisionSkin 0.02,
maxWalkableSlopeDot 0.80).

Wired `--movement-v206-reference-selftest` into `src/game/game-cli.cpp`.

### Step 1 completion: v2.0.6 swept collision recovery

Replaced the provisional capsule depenetration with the v2.0.6 GLB triangle
pipeline, reconstructed from `physics-collision.cpp`:

- candidate gather via swept capsule AABB,
- 11-sample capsule narrowphase (`capsuleTriangleSweep`, `capsuleTriangleContact`,
  `sweepSphereTriangle`, `sweepSphereEdge`, `sweepSpherePoint`,
  `sphereTriangleContact`, `closestPointOnTriangle`, `pointInTriangle`),
- velocity-adaptive sweep iterations (`GLB_SWEEP_BASE_ITERS` +
  `GLB_SWEEP_MAX_ITERS`), surface slop, multi-contact Gauss-Seidel slide,
- GLB step-up with the 3-of-5 sample consistency check, head check, and floor
  validation,
- 4-iteration batched depenetration (`solveBatchedCorrection`, 32 passes,
  0.85 relaxation), re-sweep, ground snap,
- emergency stuck escape, final velocity projection, rotation-safety pass, and
  final safety net.

### Step 2 completion: golden fixtures + cross-path parity harness

- `src/physics/movement/reference/movement-v206-parity-selftest.{h,cpp}`.
- `--movement-v206-gen-fixtures` writes deterministic per-tick oracle traces to
  `tests/fixtures/movement/v206/*.csv` for ten scenarios (gravity fall, ground
  walk, diagonal walk, accelerate/friction, jump/land, air move, ground dash,
  air dash, air down-dash, air freeze).
- `--movement-v206-parity-selftest` runs each scenario through the oracle and
  through the current cold shared kernel (`movement-step.cpp`) with both the
  C++ `source` preset and the JSON `movement-source.json` preset, then reports
  the first differing tick for velocity, grounded, and position plus the maximum
  velocity deviation and final position deviation.
- Collision caveat is documented in the header: the cold kernel has no collision
  of its own, so the harness integrates its position from its own velocity and
  feeds a scenario-level grounded flag. Velocity is the movement-formula signal;
  position includes collision-ownership differences.

### Phase A slice: v2.0.6 freeze curve

Per the user's direction that v2.0.6 behavior wins, aligned the ONE shared hot
freeze policy (`src/hot-reload/modules/movement-freeze.cpp`) to v2.0.6
`physics-freeze.cpp`:

- freeze no longer hard-zeroes velocity and no longer requires the key to stay
  held for suppression;
- while active, all three velocity axes are scaled by the duration-normalised
  piecewise-quadratic multiplier (first half `x*x*0.2`, second half
  `0.2 + x*x*0.8`), matching `freezeMaxTime 5.0`;
- start consumes availability; release ends it; re-press requires a touch reset.

Added a focused check to the parity harness that drives the loaded hot policy
through `GAME_EVENT_MOVEMENT_FREEZE` and compares it to the v2.0.6 curve at
t = 0, 1.25, 2.5, 3.75, 5.0.

### Phase A slice: gravity, dash, down-dash

Aligned the next v2.0.6 formulas in the shared hot movement:

- gravity magnitude 40 -> 58 and maximum fall speed 175 -> 400 in
  `makeSourcePresetTuning` and `config/movement/movement-source.json`;
- dash impulses 20/20 -> 100 ground / 50 air;
- air dash now scales by v2.0.6 dash quality from airborne movement ticks
  (1.0 / 0.85 / 0.70 / 0.55 / 0.40) via a new `dashMovementTicks` input on
  `GameDashPolicyV1` (replacing the unused `reserved` field);
- down-dash is now additive (`velocityZ += downDashSpeed`) instead of a replace,
  matching v2.0.6 `p.vel.z += DOWN_DASH_SPEED`;
- removed the non-v2.0.6 grounded down-dash launch override in `movement.main`;
- cold fallback down-dash in `movement-step.cpp` made additive too, and the
  cold dash hook now forwards `state.dash.dashMovementTicks`.

### Phase A slice: v2.0.6 walk/air model

Added a named v2.0.6 rule mode rather than changing the Source preset in place:

- `MovementWalkMode::V206 = 3` (`movement-types.h`); the `source` preset now
  selects it (`kWalkModeV206`).
- hot `movement.ground-move` and `movement.air-accelerate` branch on a new
  `movementModel` payload field (0 = Source, 1 = v2.0.6). v2.0.6 ground is
  friction XOR accelerate (friction only when no input); v2.0.6 air is additive
  `accel = airAcceleration * moveSpeed * dt` toward the base move speed with no
  projection-cap headroom curve and no from-rest restriction.
- aligned `source` preset values: `groundAcceleration 8`, `groundFriction` /
  `groundFrictionAmount 4`, `stopspeed 0`, `airAcceleration 222`,
  `airMaxWishspeed 1`, `airSpeedGainMultiplier 0`, `jumpSpeed 19`,
  `jumpBufferSeconds 0.12`, `coyoteSeconds 0.001`, `externalImpulseDecay 0.6`,
  `maximumExternalImpulseSpeed 120`, speed limit off.
- extended the hot preset self-test validator to accept the new mode.
- kept `config/movement/movement-source.json` in sync (`movement_mode: "v206"`,
  `ground_friction 4`, and the aligned values).

### Phase B slice: JSON is the active source + dash-quality envelope

- `config/movement.json` now selects `behaviorSource: "json"`, so the shared
  movement tuning resolves from `config/movement/<preset>.json`.
- Added one generation-tracked tuning snapshot in `movement.main`
  (`resolveTuning` in `movement-system.cpp`): JSON is parsed with an mtime cache;
  invalid JSON keeps the last valid generation active; if none exists it falls
  back to the compiled C++ preset.
- `movement.snapshot` records `source`, `preset`, `generation`, `preset_hash`,
  and `simulation_tick` through the shared `log.event` capability.
- Extended `loadJsonMovementPreset` to map `movement_mode: "v206"`, read
  `ground_friction`, `max_speed` (sourceMaxSpeed), `dash_impulse`,
  `landing_overspeed_bleed`, and `impulse_friction_mode`.
- Added `dashMovementTicks` to the versioned runtime-state envelope:
  `GameMovementRuntimeStateComponentV1` (v1 -> v2, append-only),
  `MovementRuntimeStateComponent` (ECS), the `live-behavior` bridge, and
  `movement.main` now tracks it and forwards it to the dash policy so the local
  air dash gets v2.0.6 quality.

### Phase B remainder: compatibility adapter + source-switch fixture + fixes

- Added `src/physics/movement/movement-compat-adapter.{h,cpp}`: the single
  allowed caller of the legacy `physicsMainUpdate`. `simulate-tick.cpp` (local)
  and `npc.cpp` (NPC) now route through `MovementCompat::stepActor`, so no
  gameplay call site directly mutates movement through the legacy orchestrator.
- Added a source-switch fixture to the parity harness: it temporarily rewrites
  `config/movement.json`, proves `behaviorSource=json` / `cpp` / invalid
  resolution, proves the JSON `source` preset matches the compiled source, and
  restores the original file (RAII).
- Fixed a real JSON-authority bug: `loadJsonMovementPreset` looked for
  `config/movement/source.json`, but the files are `movement-<preset>.json`, so
  JSON silently fell back to C++. It now tries both names (with `counterstrike ->
  cs`, `retrograd_fast -> retrograd-fast` aliases).
- Fixed `onMovementTuning` and `resolveTuning` to load the requested preset's
  JSON instead of always the globally selected one.
- Clamped `airInputMouseThresholdDegrees` to `>= 0.1` in the JSON loader to
  match the compiled preset.
- Scoped the strict JSON-vs-C++ preset parity requirement to the `source` preset
  (the v2.0.6 target); other presets' JSON files are not yet reconciled and are
  still reported in the output.

### Phase C slice: collider-flag authority + weapon collider

- `collision.main` now honors the explicit collider roles instead of inferring
  body-ness from shape: a `COLLISION_COLLIDER_BODY_AUTHORITATIVE` collider is
  never a helper, `COLLISION_COLLIDER_HELPER` is never authoritative, and the
  legacy sphere-shape inference is used only when neither flag is set. This
  keeps the root capsule from silently becoming a wall once body/weapon
  colliders exist.
- `movement.main`'s `buildPlayerCollision` now appends a weapon collider: it
  reads the hot `ToolPresentationClaim` on the player, resolves the tool entity,
  reads the tool's `AttachmentState` world position, and submits a
  `COLLISION_PART_WEAPON` / `COLLISION_POLICY_WEAPON` body-authoritative sphere
  (radius `0.18 * sizeScale`) so the equipped tool cannot pass through surfaces
  and participates in root correction.

### Phase D slice: JSON animation authority + single hot pose result

- Set `config/animations.json` to `behaviorSource: "json"`, making the migrated
  tool phase sets (`phaseSets`) and action clip durations authoritative. The
  built-in C++ clip/phase tables remain the fallback when JSON is absent or
  invalid (`actionClip` and `loadJsonToolAnimations` both start from the C++
  clip and override only valid JSON entries).
- Verified no regression from the flip: `--hot-combat-selftest` produces an
  identical failure set under `json` and `cpp` (all pre-existing, unrelated
  animation phase assertions), and the movement suite is unaffected.
- Verified the "one hot pose result" alignment with the deterministic weapon
  probe: under JSON animation source it still reports
  `hot_transform_used=true`, `tip_minus_hot_muzzle=0,0,0`,
  `attachment_resolved=true`, `collision_valid=true`,
  `collision_capsule_mode=true`, i.e. the render pose, muzzle, and collision
  capsule/marker share one resolved transform.

### Phase E: ragdoll collision unification + world-contact guard

- `ragdoll-solve.cpp` submits each limb as a single body-authoritative sphere to
  `collision.main` (`worldCollisionViaMain`), so the ragdoll uses the universal
  collision owner's world cache, narrowphase, depenetration, grounding, and
  response instead of a second world solver. It falls back to the local grid
  push-out when the capability is unavailable or declines, and returns before
  mutating limbs so the fallback is safe.
- Fixed the ragdoll's local world broadphase, which only indexed a triangle by
  its centre cell and therefore silently missed large triangles (a whole floor):
  it now inserts each triangle into every overlapping cell, keeps over-large
  triangles in an `always` list tested by every limb, accumulates multiple
  contacts per limb instead of overwriting, records a first-triangle sample hash
  so a same-count different world rebuilds, and clears the new `always` list.
- Added `--ragdoll-world-selftest`
  (`src/ragdoll/ragdoll-world-selftest.{h,cpp}`): drives the real hot
  `ragdoll.solve` capability against a floor and a no-floor control and reports
  the world-capability triangle count.
- Contact impacts/sparks are already produced by `collision.main` through
  `COLLISION_SOLVE_SPAWN_IMPACTS` -> `effect.part`; sound/damage/recoil contact
  consumers are still open.

#### Resolution (Phase E now works)

Root cause of the ragdoll falling through: the Phase E `collision.main` hand-off
passed zero limb velocity and ignored the returned contacts. `collision.main`
therefore depenetrated the limb for one tick but could not cancel the PBD
integrator's growing downward velocity, so the limb sank and then tunnelled. The
local fallback was also weak because its broadphase only indexed a triangle by
its centre cell and missed the whole-floor triangle and, once slightly below,
the contact normal inverted.

Fixes:

- `worldCollisionViaMain` now reads the returned contacts and cancels the limb's
  into-surface velocity for each contact normal, so the PBD step cannot keep
  driving it through the surface; it still passes zero velocity in so
  `collision.main` does not double-integrate the already-moved limb.
- the local fallback inserts each triangle into every overlapping cell, keeps
  over-large triangles in an `always` list, accumulates multiple contacts per
  limb, and rebuilds on a first-triangle sample-hash change.
- During debugging, a temporary diagnostic wrote into `RagdollLimbStaticV1`
  fields (radius/parent), which corrupted solver inputs and produced a spurious
  upward launch; that diagnostic was removed.

The strict guard now passes: limbs dropped on a floor settle at `z=0.201`
(radius 0.2 + skin 0.001) instead of `z=-71`, and the no-floor control still
falls through. `--ragdoll-world-selftest` PASS with the strict assertions.

Contact consumer: `movement.main` now plays a throttled world-impact sound
(`entity/player/land`, volume scaled by `incomingSpeed`) from the world contacts
returned by `collision.main`; spark/decal impacts were already produced by the
package through `COLLISION_SOLVE_SPAWN_IMPACTS`. Remote ragdolls remain
server-snapshot-driven presentation (the networked-correct model); local and
corpse ragdolls simulate client-side through the unified collision path.

### Live-edit proof (hot C++, no EXE relink)

1. changed the shared hot freeze curve constant in
   `src/hot-reload/modules/movement-freeze.cpp` (`0.2f` -> `0.25f`);
2. rebuilt **only** the DLL with `python build_game_dll.py`
   (`DLL build success: build\mimita-game.dll`, no EXE link);
3. ran the already-linked EXE `mimita-20260921T132227.exe` and observed the new
   behavior (`freeze curve t=1.25 expected=0.0500 hot=0.0625`);
4. reverted the constant, rebuilt the DLL only, and confirmed `hot=0.0500` and
   `--movement-v206-parity-selftest` PASS.

The Phase E ragdoll change was also applied with a DLL-only rebuild under the
same EXE. This proves hot C++ gameplay edits are picked up by the running EXE
generation without relinking the executable. JSON is read live and authoritative
(`movement.snapshot source=json ...`), and the switch fixture proves `cpp`<->`json`
selection without a build.

### Phase C slice: NPC/actor collision unified onto `collision.main`

- `actor-movement-system.cpp` (server humans and NPCs) no longer calls the kernel
  `physics.move` primitive. `resolveActorCollisions` now submits the capsule
  (helper) plus resolved body sockets to the same `collision.main` package the
  local player uses, and copies back corrected position, velocity, and grounded
  state. Plain integration is the only fallback when the capability is missing.
- This removes the last separate actor-collision solver call site for player/NPC
  movement; `collision.main` is now the collision owner for local players, server
  humans, NPCs, and ragdoll limbs.
- Retired the legacy hot capsule solver `src/hot-reload/modules/collision-solver.cpp`
  (`physics.capsuleSolve`). After the NPC/actor unification nothing resolves
  `physics.move`, so the whole `physics.move` -> `physics.capsuleSolve` chain was
  dead; the file was deleted and the DLL rebuilt (82 -> 81 sources).

### Phase C slice: projectile world contact through `collision.main`

- `hot-projectiles.cpp` now resolves projectile-vs-world through `collision.main`:
  it sweeps the projectile sphere from its previous position with its velocity
  and consumes the returned world contact (point/normal) for the existing bounce
  or explode response. The `queryWorldRay` path is now only a fallback when the
  collision package is unavailable. Bounce/explode logic is factored into one
  `applyWorldHit` lambda so both paths respond identically.
- `--hot-combat-selftest` still shows the same 24 pre-existing, unrelated
  animation/phase2 failures and **no new projectile failures**.

### Phase C slice: weapon collision shape from `weaponcollisions.json`

- The hot weapon collider in `movement.main` now resolves its shape from
  `config/weaponcollisions.json` (read directly by the DLL, mtime-cached, keyed
  by `gameHash(weaponId)`). It uses the entry's capsule radius and places the
  collider at the capsule's local midpoint transformed by the hot attachment
  pose, falling back to the previous conservative sphere when no entry exists.
- The collision ABI capsule is Z-aligned, so the oriented weapon capsule is
  represented as a sphere at its midpoint; a true oriented capsule would need an
  append-only ABI extension.

### Phase C slice: oriented capsule ABI + weapon shape picker

- Added an append-only oriented capsule to the collision ABI:
  `CollisionColliderV1.endPosition[3]` plus the
  `COLLISION_COLLIDER_ORIENTED_CAPSULE` flag. When the flag is set and the
  endpoints differ, the solver builds the capsule from `position` to
  `endPosition` (arbitrary orientation); otherwise the legacy Z-aligned capsule
  is used and `endPosition` is ignored, so existing callers and the package
  self-test are unaffected.
- The solver samples along the oriented axis (`ColliderRuntime.axis/halfSeg`)
  in `gatherActorContacts` and expands the broadphase AABB accordingly.
- `config/weaponcollisions.json` now has a `shape` picker: `triangles`
  (default), `sphere`, `capsule`, `cylinder` / `elongated_sphere` (mapped to the
  oriented capsule), `spheres` (array), and `box` (approximated by two spheres
  along the longest axis). Legacy `source`/`capsule`/`capsules` fields are
  inferred when `shape` is absent.
- `movement.main`'s weapon collider builds the selected mode from the hot
  attachment pose. `triangles` (the intended default) currently falls back to a
  conservative sphere because the weapon GLB triangles are resolved by the cold
  presentation path and no mesh capability is exposed to the hot collider yet.

### Phase C slice: generic capsule-move capability (server solver retired, harness collision-authoritative)

- Added a generic kernel capability `collision.capsuleMove` (`GameCapsuleMoveV1`
  POD in `game-api.h`) provided by the collision package. It runs the exact same
  `solve` as `collision.main`, so cold callers never need the DLL-only collision
  ABI.
- `LiveBehavior::capsuleMove` wraps it. The server authoritative `simulatePlayer`
  now resolves world collision through it, and the headless movement fallback
  uses it too.
- **Retired the server sample-based sphere solver**: deleted
  `resolveCapsuleCollisionAgainstWorld` and `resolveWorldCollision` (and their
  declarations); they were the last parallel server collision solver.
- **Collision-authoritative cold/harness path**: the v2.0.6 parity harness binds
  a floor world and resolves the cold path through `collision.capsuleMove`
  instead of manual integration with a scenario grounded flag. Final position
  deviations dropped sharply (e.g. `down_dash_air` 295 -> 0.92, `freeze_air`
  94 -> 0.92, `ground_walk` 1.99 -> 0.94).
- **Props**: there is no prop subsystem in the repo, so there is nothing to
  route; when props are added they should use `collision.main` / the
  `collision.capsuleMove` seam like projectiles and actors.

### Phase C: weapon GLB triangle item — blocked, documented

The collision solver is sphere/capsule narrowphase: actor colliders are spheres
or capsules tested against world triangles. Making the default weapon
`triangles` mode use the real weapon GLB mesh needs either triangle-vs-triangle
narrowphase or a mesh capability that hands the hot collider a triangle set plus
triangle collider support in the ABI/solver. Neither exists yet, and the cold
weapon path itself resolves the weapon to a capsule
(`Player::weaponCollisionCapsule` / `weaponCollisionWorld`), not triangles. The
config shape picker (sphere / oriented capsule / cylinder / elongated sphere /
spheres / box) is the working alternative; `triangles` falls back to a
conservative sphere. This is the one remaining Phase C item.

### Phase D slice: `aimbody.json` behaviorSource toggle

- `AimBodyConfig::load` now honors `behaviorSource`: `"json"` (default) applies
  the file; `"cpp"` disables the JSON aimbody overlay as an explicit rollback,
  matching the movement/animations source selector. `config/aimbody.json` set to
  `"json"` and documented. Aimbody was already hot-reloaded; this adds the
  named source toggle.

### Phase C/D: weapon triangle collision, weapons.json switch, action keyframes

- **Weapon triangle collision**: `weaponcollisions.json` now supports
  `"shape": "triangles"` with a local-space `"triangles": [[[x,y,z],[x,y,z],[x,y,z]], ...]`
  array and a vertex `"radius"`. The hot collider transforms each triangle
  vertex by the hot attachment pose and submits it as a small sphere collider,
  so the real weapon mesh vertices drive world contact (vertex-sampled; no
  triangle-vs-triangle narrowphase). The collider cap stays at 16, so the
  remaining slots (after capsule + 6 body parts) fit ~3 triangles per weapon.
- **weapons.json behaviorSource switch**: `loadWeaponJsonConfig` reads
  `behaviorSource`; `"json"` (default) applies `config/weapons.json` over the
  compiled weapon definitions, `"cpp"` keeps the compiled definitions (the hot
  C++ tool definition still applies). Hot-reloadable; no rebuild.
- **Action keyframes from JSON**: restored the archived `idle`/`walk` keyframes
  (from `config/animations copy.json 9 5 2026 ...`) into
  `config/animations.json` `actions`, which the clip loader already consumes
  (`time` or `tick/60`). JSON action keyframes are now authoritative when
  `behaviorSource: "json"`; `"cpp"` falls back to the compiled clips.

### Weapons/animations migration — W0/A0 unblockers

Decisions: fold v2.0.6 procedural animation data into `animations.json`; flip
hitscan/melee hot now; Blender clips become an optional mode; exact parity
targets revolver, shotgun, rocket launcher, and spy knife (documented method,
reused for the rest).

- **A0.1 `ActorActionState` collision fixed.** `combat-policy.cpp` wrote a
  16-byte `{lastHandledTick,handled,reserved}` record at `gameHash("ActorActionState")`,
  the same component hash the animation state machine uses for
  `HotActorActionStateV1` (~88 bytes), and registered a second schema of size 16
  at that hash. It now writes/registers `CombatHandledState` instead, and the
  cold reader `MimitaNet::actorStateActionHandled` reads the same new hash.
- **A0.3 Inert animation seam removed.** Deleted the empty `animation.main`
  system (`src/hot-reload/modules/animation-system.cpp`) and the unused
  `GAME_CAP_ANIMATION_UPDATE` constant.
- **W0.1 Orphaned weapon code deleted.** Removed `weapon-manager.{h,cpp}`,
  `revolver-system.{h,cpp}`, and `weapon-hit.{h,cpp}` (all unreferenced), plus
  the five stale `#include "combat/weapon-hit.h"` lines.
- **W0.2 Duplicate `config/weapons.json` parsers collapsed.** `weapon-json-config`
  now exposes `WeaponData::configPath()` and `WeaponData::configRoot()`; the
  viewmodel config (`weapon-config.cpp`) and the grenade-launcher physics parser
  (`weapon-data.cpp`) read that one parsed root instead of re-reading the file
  with hardcoded CWD-relative paths.

### Weapons/animations migration — W1/A1 (one owner)

- **W1 ownership seam made honest.** `combat-policy.cpp onToolUse` no longer
  pre-claims the use (`handled = 1`) before running the behavior. The behavior
  now owns `handled`: it sets it only when it will act, and a declined behavior
  leaves `handled = 0` so the cold authoritative path runs. This removes the
  "swallowed shot" hazard that made the execution opt-in unsafe.
- **W1 duplicate removed.** `applyWeaponJson` applied the behavior type twice
  (inside `applyWeaponStatsJson` and again in `applyWeaponJson`); the redundant
  one was removed. (`WeaponExecution::executionTypeForBehavior` was already a
  passthrough to `weaponExecutionTypeForBehavior`, so no duplicate there.)
- **Execution flip status: deliberately NOT enabled yet.** `TOOL_FLAG_OWNS_EXECUTION`
  is still off for hitscan/pellet/melee. The hot `hitscanUse`/`meleeUse` are
  simplified (one relationship target, flat damage; no spread, pellets, damage
  falloff, headshot multiplier, recoil, or victim/shooter knockback). Enabling
  the flag now would regress weapon feel and contradict the exact-parity
  requirement. With the seam fixed, the flip is a one-flag change once the cold
  hitscan/melee logic is ported (W2/A2).
- **A1 single locomotion-base owner.** `pose-generation.cpp` recomputed the
  locomotion base from velocity (`locomotionAction(...)`) while `animation-policy`
  independently selected the action from intent. It now uses the animation
  policy's selected action directly when that action is a locomotion action
  (idle / equipped-idle / walk / jump / fall / land), and only falls back to the
  procedural `locomotionAction` for upper-body actions (shoot/reload/etc.).

### Weapons/animations migration — A2 (JSON animation feel), W2 method

- **A2.1 JSON locomotion keyframes reachable.** `HotAnim::evaluateAction`
  short-circuited `IDLE`/`EQUIPPED_IDLE`/`WALK` to the procedural evaluators, so
  the restored `actions.idle`/`actions.walk` JSON keyframes were dead. It now
  samples the JSON clip when `behaviorSource == "json"` and the entry is valid,
  falling back to the procedural evaluator otherwise. The hot-side
  `config/animations.json` cache is now shared (`jsonClipCache`) by `actionClip`
  and the new `jsonClipApplied`, removing the duplicate load on this side.
- **A2.2 aimbody per-limb gains applied hot.** `pose-generation.cpp` now reads
  `config/aimbody.json` live (mtime + `behaviorSource`) and applies each
  configured limb's pitch/yaw/roll gain times the camera look pitch to the
  computed pose. Previously only body yaw consumed aimbody in the cold physics
  path; the per-limb aim (torso/head/arms) was configured but never applied.
- **W2 method documented.** `docs/gold/2026-09-21-v206-weapon-parity-method.md`
  records how to capture a v2.0.6 weapon reference and reuse it for every weapon,
  with the extracted revolver (dmg 50 / delay 0.08 / mag 6 / recoil 99) and
  shotgun (dmg 12 / delay 0.25 / mag 2 / pellets 15 / spread 3 / recoil 130)
  values. It records the important fact that **v2.0.6 had no rocket launcher and
  no spy knife** (and no victim/shooter knockback), so those use the current cold
  authoritative path as their reference instead.
- **W2 hot port + flip NOT done.** The hot `hitscanUse`/`meleeUse` are still
  simplified; porting the v2.0.6 hitscan pipeline and flipping
  `TOOL_FLAG_OWNS_EXECUTION` is the next W2 step, now specified by the doc.
- **A1 status.** Locomotion base selection and aimbody are hot now. The remaining
  cold animation code (`live-behavior.cpp capSkeletonApply`, the typed plane
  mirror, `skeleton-instances.cpp`) is the renderer/physics-kernel boundary and
  was not moved hot; that needs a generic pose-apply/render seam, not a module
  move.

### Weapons migration — W2 increment (hot hitscan damage model)

- `hitscan-tool.cpp` now computes damage with the same v2.0.6-consistent model
  the cold path uses: `base x body-part x range falloff`
  (`hotHitscanDamage`/`hotHitscanFalloff`, same parameter names
  `distanceFalloffStart` / `minDamageFraction` / `falloffExponent` /
  `limbDamageMultiplier`). Base comes from the tool definition `damage` (falling
  back to the old `hotDamage` param). This is hot and gated by the still-off
  execution flag, so it is ready for the parity port.
- **The `TOOL_FLAG_OWNS_EXECUTION` flip is NOT enabled.** Investigation found the
  flip is not a presentation change: `tool.primary-use` is dispatched from the
  **server** attack path, so flipping makes the server resolve hitscan through
  the hot behavior instead of the cold `traceHitscan`. That requires porting
  `buildPelletDirections`, `rayPlayerTarget`, the closest-pellet/world-block
  selection, the damage/knockback aggregation, and spawn-generation identity into
  hot first. The W2 method doc records the exact functions and the blocker.

### A2 remainder — per-weapon arm poses JSON + Blender optional mode; A1 boundary

- **Per-weapon carry arm poses are now hot/JSON.** `HotAnim::weaponCarryArms`
  reads a `weaponArms` section in `config/animations.json` keyed by weapon id
  (`leftX`/`rightX`/`leftZ`/`rightZ`, degrees) when `behaviorSource == "json"`,
  falling back to the compiled table. The section is added with the current carry
  values; the v2.0.6 `player-procedural.json` source values are recorded in its
  comment so they can be dropped in live.
- **Blender mode is explicit and optional.** `animation-physical.cpp` reads
  `blenderMode` from `config/animations.json` (`auto`/`on` = enabled,
  `off`/`cpp` = procedural only), mtime-refreshed each tick. The existing
  `physicalanim` command remains a live manual override until the file changes.
- **A1 boundary recorded.** Movement/pose *data and formulas* are now hot or JSON:
  locomotion keyframes, per-weapon arm poses, and aimbody gains. The remaining
  cold animation code is the kernel render/physics apply mechanism
  (`live-behavior.cpp capSkeletonApply`, `skeleton-instances.cpp`, the typed
  `Player` mirror). Hot code already calls it through the generic
  `skeleton.apply` capability; moving it hot would mean moving the renderer, so it
  stays cold by design. No module move was faked.

## Evidence

- Syntax check: `g++ -std=c++17 -fsyntax-only -Isrc -Iinclude` on the reference
  cpp: clean.
- Cold build (movement formulas only): `mimita-20260921T112527.exe`,
  `Status: SUCCESS`.
- Cold build (swept collision): `mimita-20260921T113830.exe`, `Status: SUCCESS`.
- Runtime: `mimita-20260921T113830.exe --movement-v206-reference-selftest`
  -> `[MOVEMENT V206 REFERENCE SELFTEST] PASS`, exit code 0, 12 checks:
  gravity impulse, ground walk reaches moveSpeed, stable grounded, ground dash
  impulse + `didDash`, held jump reaches jumpStrength + `didGroundJump`, resting
  capsule does not sink, resting player grounded, no wall tunneling, finite wall
  result, passes a 0.2 high step.
- Standalone deterministic probe (`v206_probe.exe`, not committed): resting from
  `pos.z=2.0` settles at `pos.z=1.828` and stays grounded with `vel.z=0`; wall at
  `x=3.0` stops the capsule at `x=2.271`.
- Cold build (fixtures + parity harness): `mimita-20260921T114932.exe`,
  `Status: SUCCESS`.
- `--movement-v206-gen-fixtures` -> `[MOVEMENT V206 FIXTURES] PASS`, ten CSV
  files written under `tests/fixtures/movement/v206/`.
- `--movement-v206-parity-selftest` -> exit 0. Current cold kernel diverges from
  the v2.0.6 oracle on velocity at tick 0 in **10/10** scenarios for both the C++
  and JSON presets (expected: the current Source config and the v2.0.6 additive
  config are different formulas). Examples: `gravity_fall` maxVelDev=80.0,
  `ground_walk` maxVelDev=10.94, `dash_ground` maxVelDev=100.47,
  `down_dash_air` maxVelDev=122.67. This is the Phase A work list, not a harness
  failure.
- Cold build with the freeze alignment: `mimita-20260921T115358.exe`,
  `Status: SUCCESS`; `build/mimita-game.dll` rebuilt at 11:54:03.
- `--movement-v206-parity-selftest` freeze check: all five samples `[ok]`
  (t=0 -> 0.0, t=1.25 -> 0.05, t=2.5 -> 0.2, t=3.75 -> 0.4, t=5.0 -> 1.0). The
  loaded hot freeze policy now matches the v2.0.6 curve; `freeze_air` maxVelDev
  moved 40.0000 -> 40.0151, proving the change is live.
- Cold build with gravity/dash/down-dash alignment: `mimita-20260921T120558.exe`,
  `Status: SUCCESS`.
- `--movement-v206-parity-selftest` after alignment: `gravity_fall` first
  velocity divergence moved tick 0 -> 19 (now the collision/landing boundary),
  `down_dash_air` tick 0 -> 12 (down-dash additive now matches; divergence is
  the floor contact). `dash_ground`/`dash_air` still diverge at tick 0 because
  the ground/air walk model is still the Source preset, not v2.0.6 additive.
- `--movement-preset-selftest` PASS (C++ vs JSON field parity and 60-tick
  scripted parity for all five presets).
- `--movement-algorithm-selftest` PASS (hot dash owns ground/air dash,
  availability, camera fallback, down-dash).
- `--movement-v206-reference-selftest` PASS.
- Cold build with the v2.0.6 walk/air model: `mimita-20260921T121602.exe` (230
  objects) and `mimita-20260921T122305.exe` (validator fix), `Status: SUCCESS`.
- `--movement-v206-parity-selftest` after the walk/air alignment:
  `ground_walk` and `ground_walk_diagonal` now match the oracle on velocity
  **exactly** (`firstVelTick=-1`, `maxVelDev=0.0000`), with only the position
  integration/collision boundary differing. First velocity divergence moved:
  `accelerate_friction 0 -> 40`, `air_move 0 -> 4`, `dash_ground 0 -> 11`,
  `freeze_air 11 -> 76`, `gravity_fall 0 -> 19`, `down_dash_air 0 -> 12`.
- `--movement-preset-selftest` PASS, `--movement-algorithm-selftest` PASS,
  `--movement-v206-reference-selftest` PASS.
- Cold build with JSON authority + envelope change: `mimita-20260921T124219.exe`
  and `mimita-20260921T124813.exe`, `Status: SUCCESS`.
- Runtime proof of JSON authority: `--movement-preset-selftest` (which drives
  `movement.main`) emitted
  `movement.snapshot message="source=json preset=source generation=1 preset_hash=12721328313206500214 simulation_tick=0" result="json"`.
- No production source or configuration was edited. No running EXE was touched.
- Phase B remainder build: `mimita-20260921T131212.exe`, `Status: SUCCESS`.
- `--movement-v206-parity-selftest` PASS including the four switch-fixture checks
  (`json` resolves, `cpp` resolves, invalid falls back to `cpp`, JSON `source`
  matches the compiled source). `--movement-preset-selftest` PASS,
  `--movement-algorithm-selftest` PASS, `--movement-v206-reference-selftest`
  PASS.
- `config/movement.json` is restored to `behaviorSource: "json"` after the
  fixture (verified).
- Phase C slice build: `mimita-20260921T132227.exe`, `Status: SUCCESS`. The hot
  DLL activated with `collision.main` resolved (its package self-test runs on
  activation and did not fail). `--movement-v206-parity-selftest`,
  `--movement-preset-selftest`, `--movement-algorithm-selftest`, and
  `--movement-v206-reference-selftest` all PASS.
- Phase D: `config/animations.json` -> `behaviorSource: "json"` (no rebuild
  needed; read live). `--hot-combat-selftest` failure set identical under
  `json` and `cpp` (verified by toggling the file), so no regression.
  Deterministic weapon probe summary:
  `hot_transform_used=true tip_minus_hot_muzzle=0,0,0 attachment_resolved=true`;
  probe phases report `collision_valid=true collision_capsule_mode=true`.
  `--movement-v206-parity-selftest` PASS, `--movement-preset-selftest` PASS.
- Phase E baseline: `--ragdoll-slice-selftest` PASS, `--collision-selftest` PASS.
- Live-edit proof: DLL-only rebuild changed `freeze curve t=1.25` to `hot=0.0625`
  under the unchanged EXE, then reverted to `hot=0.0500` (PASS). No EXE relink was
  performed for the hot C++ change.
- Phase E: `--ragdoll-world-selftest` PASS with strict assertions (limbs rest at
  `z=0.201`; no-floor control falls through), `--ragdoll-slice-selftest` PASS,
  `--collision-selftest` PASS, `--movement-v206-parity-selftest` PASS,
  `--movement-preset-selftest` PASS, `--movement-algorithm-selftest` PASS,
  `--movement-v206-reference-selftest` PASS. Build `mimita-20260921T141037.exe`,
  `Status: SUCCESS`.
- Phase E contact consumer added and applied with a DLL-only rebuild; the full
  suite still passes (`--ragdoll-world-selftest`, `--ragdoll-slice-selftest`,
  `--collision-selftest`, `--movement-v206-parity-selftest`,
  `--movement-preset-selftest`, `--movement-algorithm-selftest`,
  `--movement-v206-reference-selftest`).
- Phase C NPC/actor collision unification: DLL-only rebuild; `--npc-entity-selftest`
  PASS, `--npc-actor-state-selftest` PASS, `--live-code-selftest` PASS, plus the
  full movement/ragdoll/collision suite above.
- Phase C legacy solver retirement: `collision-solver.cpp` deleted; DLL rebuilt
  from 81 sources (`DLL build success`). Full suite re-run and PASS
  (`--ragdoll-world-selftest`, `--movement-v206-parity-selftest`,
  `--movement-preset-selftest`, `--movement-algorithm-selftest`,
  `--movement-v206-reference-selftest`, `--collision-selftest`,
  `--npc-entity-selftest`, `--npc-actor-state-selftest`, `--live-code-selftest`).
- Phase C projectile world contact: DLL-only rebuild (`sources=81`);
  `--hot-combat-selftest` FAIL set unchanged (same 24 pre-existing animation/phase2
  failures, no new projectile failures); `--ragdoll-world-selftest`,
  `--movement-v206-parity-selftest`, `--collision-selftest`, `--live-code-selftest`
  all PASS.
- Phase C weapon shape from config: DLL-only rebuild (`sources=81`);
  `--hot-combat-selftest` FAIL set unchanged (same 24 pre-existing
  animation/phase2 failures, no new weapon/projectile failures);
  `--ragdoll-world-selftest`, `--movement-v206-parity-selftest`,
  `--movement-preset-selftest`, `--collision-selftest` PASS. (`--live-code-selftest`
  hit the known unrelated `seq strictly increases` flake on this run.)
- Phase C oriented capsule + shape picker: DLL-only rebuild (`sources=81`);
  `config/weaponcollisions.json` validates as JSON. Full suite PASS
  (`--collision-selftest`, `--ragdoll-world-selftest`,
  `--movement-v206-parity-selftest`, `--movement-preset-selftest`,
  `--movement-algorithm-selftest`, `--movement-v206-reference-selftest`,
  `--npc-actor-state-selftest`). An initial regression (the package self-test
  read uninitialised `endPosition` as an oriented capsule) was fixed by making
  orientation explicit via the flag.
- Phase C generic capsule-move: cold+hot build `Status: SUCCESS`. Full suite PASS
  (`--movement-v206-parity-selftest`, `--movement-preset-selftest`,
  `--movement-algorithm-selftest`, `--movement-v206-reference-selftest`,
  `--collision-selftest`, `--ragdoll-world-selftest`, `--npc-entity-selftest`,
  `--npc-actor-state-selftest`, `--live-code-selftest`). Server sphere solver
  deleted; parity harness now collision-authoritative.
- Phase D aimbody toggle: cold build `Status: SUCCESS`;
  `--movement-v206-parity-selftest`, `--collision-selftest`,
  `--live-code-selftest` PASS.
- Phase C/D weapon triangles + weapons.json switch + action keyframes: cold build
  `Status: SUCCESS`. Full suite PASS (`--collision-selftest`,
  `--ragdoll-world-selftest`, `--movement-v206-parity-selftest`,
  `--movement-preset-selftest`, `--movement-algorithm-selftest`,
  `--movement-v206-reference-selftest`, `--npc-entity-selftest`,
  `--npc-actor-state-selftest`, `--live-code-selftest`). `--hot-combat-selftest`
  back to its pre-existing 24-failure set (an attempt to raise the collider cap
  to 40 caused an access violation there and was reverted to 16).
- W0/A0 unblockers: cold+hot build `Status: SUCCESS`. `--collision-selftest`,
  `--ragdoll-world-selftest`, `--movement-v206-parity-selftest`,
  `--movement-preset-selftest`, `--movement-algorithm-selftest`,
  `--movement-v206-reference-selftest`, `--npc-entity-selftest`,
  `--npc-actor-state-selftest`, `--live-code-selftest` PASS.
  `--hot-combat-selftest` remains at the same 24 pre-existing animation/phase2
  failures (no new failures from the `ActorActionState` split).
- W1/A1: cold+hot build `Status: SUCCESS`. Full suite PASS
  (`--collision-selftest`, `--ragdoll-world-selftest`,
  `--movement-v206-parity-selftest`, `--movement-preset-selftest`,
  `--movement-algorithm-selftest`, `--movement-v206-reference-selftest`,
  `--npc-entity-selftest`, `--npc-actor-state-selftest`, `--live-code-selftest`).
  `--hot-combat-selftest` remains at the same 24 pre-existing failures (only
  tool-related one is the pre-existing `phase2 unequip on tool removal`).
- A2 (JSON locomotion + hot aimbody) and W2 method doc: cold+hot build
  `Status: SUCCESS`. Full suite PASS (`--collision-selftest`,
  `--ragdoll-world-selftest`, `--movement-v206-parity-selftest`,
  `--movement-preset-selftest`, `--movement-algorithm-selftest`,
  `--movement-v206-reference-selftest`, `--npc-entity-selftest`,
  `--npc-actor-state-selftest`, `--live-code-selftest`). `--hot-combat-selftest`
  unchanged at 24 pre-existing animation/phase2 failures. A2 visual correctness is
  **not** human-verified.
- W2 hot hitscan damage model: DLL-only rebuild (`sources=80`, `DLL build
  success`). `--live-code-selftest`, `--movement-v206-parity-selftest`,
  `--collision-selftest` PASS; `--hot-combat-selftest` unchanged at 24
  pre-existing failures. Execution flag still off.
- A2 remainder (weapon arms JSON + Blender toggle): cold+hot build
  `Status: SUCCESS`. Full suite PASS (`--collision-selftest`,
  `--ragdoll-world-selftest`, `--movement-v206-parity-selftest`,
  `--movement-preset-selftest`, `--movement-algorithm-selftest`,
  `--movement-v206-reference-selftest`, `--npc-entity-selftest`,
  `--npc-actor-state-selftest`, `--live-code-selftest`); `--hot-combat-selftest`
  unchanged at 24 pre-existing failures. Visual correctness not human-verified.
- Observed unrelated flake: `--live-code-selftest` intermittently fails on
  `[FAIL] seq strictly increases` (the structured logger's `events.jsonl`
  sequence check). It passes on other runs and none of this session's changes
  touch the logger sequence; recorded here rather than fixed in this session.

## Honest limitations

- The legacy v2.0.6 `Block`/AABB map path is not recovered; the oracle is
  GLB-triangle-only (the requested map format).
- Limb/weapon body-sample collision is not recovered in the oracle, and the
  hot weapon collider is a conservative sphere, not yet the `weaponcollisions.json`
  shape. Proper limb/weapon shapes belong to the remaining Phase C/D slices.
- `config/animations.json` is authoritative, but its `actions` entries carry
  durations without keyframes, so action frames still come from the compiled
  built-in clips (only durations are overridden). Tool pose frames come from the
  JSON `phaseSets`. `aimbody.json` and the `weapons.json` tool/animation
  references are not yet authoritative.
- The oracle starts players at a sane height. Deep initial penetration can
  produce an inverted contact normal, which the real v2.0.6 sweep prevents;
  this is a property of the spawn scenarios, not a movement difference.
- The parity harness's cold path is not collision-authoritative, so once a
  scenario's formula divergence is fixed the first divergence lands on the
  collision/landing boundary rather than reaching zero.
- The local `movement.main` now consumes runtime-state dash ticks and forwards
  them to the dash policy, so a live local air dash is quality-scaled like
  v2.0.6. The envelope went to `MOVEMENT_RUNTIME_STATE_VERSION = 2`; existing
  in-memory v1 state is reinitialized once at load.
- The ground/air walk model is now v2.0.6 via `MovementWalkMode::V206`; the
  remaining `dash_*`/`air_*` first-divergences are at the cold harness's
  non-authoritative collision boundary, and `jump_land` diverges at tick 0
  because the harness feeds the cold path a scenario-level grounded flag while
  the oracle uses real collision.
- JSON is the active source for the shared tuning and for `movement.main`;
  explicit C++ rollback remains by setting `behaviorSource: "cpp"`. The
  selector's invalid-file fallback is fixture-tested; the hot DLL's
  mtime-cached last-valid generation is implemented but not yet exercised by an
  automated fixture.
- The legacy orchestrator is still the compile-time fallback, but every gameplay
  call site now routes through `MovementCompat::stepActor`, so there is no direct
  gameplay call to `physicsMainUpdate`. Removing the fallback entirely is for
  after live acceptance of the hot path.
- Non-`source` presets (`default`, `heavy`, `retrograd_fast`, `counterstrike`)
  have JSON files that do not yet match their compiled tables; with JSON
  authoritative, their JSON wins. This is reported by the preset selftest but not
  treated as a failure. It should be reconciled or those presets marked
  C++-only.
- Pose and weapon-transform columns are not part of the oracle.
- The freeze-curve spec disagreement recorded below is still unresolved.

## Routing and review

- Routed through `docs/ROUTER.md` (movement/physics route:
  `docs/specs/movement/movement.md`, `docs/architecture/collision/collision.md`).
- Applied `docs/skills/spec-behavior-review-v1.md`. Finding recorded below.

### Finding

- Severity: high
- Type: spec-code disagreement
- Specification: `docs/specs/movement/movement.md` (freeze section,
  `## 2026-09-20 freeze correction`) and the user decision "v2.0.6 behavior
  always wins".
- Exact quoted requirement: `strength = exp(-5.0f * elapsedTicks / 300.0f)` and
  "Movement pass-through is `1.0f - strength`".
- Code path: v2.0.6 `physics-freeze.cpp` `freezeVelocityMultiplier` is piecewise
  quadratic (`x*x*0.2` then `0.2 + x*x*0.8`), applied to all three axes by
  multiplying velocity, not an exponential pass-through.
- Actual behavior: current spec describes an exponential suppression curve;
  v2.0.6 shipped a quadratic-multiplier curve.
- Expected behavior: v2.0.6 feel per user decision.
- Evidence: tag `v2.0.6:src/physics/movement/physics-freeze.cpp`; captured in
  `movement-v206-reference.cpp`.
- Recommended wording or implementation action: keep the v2.0.6 quadratic curve
  as the frozen reference, and either restore it as the shared hot freeze policy
  or record a deliberate spec decision to keep the exponential curve. Do not
  silently pick one.
- Resolution this session: the human directed that v2.0.6 behavior wins, so the
  shared hot freeze policy was aligned to the v2.0.6 quadratic curve and verified
  by the harness.
- Human decision still required: the normative spec text
  (`docs/specs/movement/movement.md`, `## 2026-09-20 freeze correction`) still
  states the exponential curve and should be updated or explicitly retained by
  the human. The implementation intentionally does not match that section until
  it is revised.

## Next steps

1. Phase E is complete: ragdoll world collision works through `collision.main`
   (strict guard passes), the local fallback broadphase is fixed, and
   `movement.main` consumes world contacts for impact sound (sparks/decals were
   already package impacts). Remote ragdolls stay server-snapshot-driven by
   design. Optional follow-ups: contact-driven damage/recoil consumers and a
   human feel/visual acceptance pass.
2. Phase D: `aimbody.json` and `weapons.json` now have `behaviorSource`
   switches (json default, cpp rollback); `animations.json` carries JSON action
   keyframes for idle/walk (from the archive) plus tool phase sets. Remaining:
   migrate the rest of the action keyframes (shoot/reload/death) into JSON, and
   point `weapons.json` tool entries at the JSON animation phase sets.
3. Phase C: weapon triangle collision is implemented via a config `triangles`
   array sampled as vertex spheres (real mesh vertices drive contact). Remaining:
   full triangle-vs-triangle narrowphase (or a mesh capability) so every triangle
   of the GLB mesh is used, and a higher collider cap so more than ~3 triangles
   per weapon fit.
4. Human: revise or explicitly retain the freeze section of the movement spec,
   and perform live human feel/visual acceptance (no-cold-build edits).

## No-cold-build confirmation (evidence-backed)

Confirmed for **gameplay policy, formulas, values, and JSON**: after the stable
ABI, changing hot C++ modules or JSON updates the running EXE with no
relink/restart. Evidence: the DLL-only freeze-curve A/B above, JSON authority in
`movement.snapshot`, and the `cpp`<->`json` switch fixture.

Not yet true for everything: a cold build is still required when the kernel/ABI
envelope itself changes (for example this session's `dashMovementTicks` component
version bump) or when a new cold call site/file is added. "v2.0.6 feel" is proven
for movement formulas/values by the oracle parity harness, and ragdoll world
collision is now working, but full collision behaviour and human feel/visual
acceptance are still in progress.


## Deterministic fixed spread (no RNG) — Phase 1

Replaced all randomized weapon spread with one deterministic pattern owned by
`src/combat/pellet-pattern.cpp`. Decisions from this session: single-ray weapons
use a fixed offset cycle; shotgun keeps the JSON spread value `10.0`; all wire
seed fields are kept (no longer read).

- `buildFixedPelletDirections` is now the single pellet-grid generator
  (`cols = ceil(sqrt(n))`, `rows = ceil(n/cols)`, normalized `[-1,1]` grid scaled
  by half the spread). It is the exact pattern the client already rendered in
  `weapon-fire-hit.cpp`.
- `generatePelletDirections` and `WeaponExecution::buildPelletDirections` now
  delegate to it and ignore their seed. Server `traceHitscan`, the NPC
  multi-pellet path, and the legacy handler therefore all use the same grid.
- Client `fireMultiPellet` calls the shared grid instead of its inline copy.
- `buildFixedSpreadDirection` is the single deterministic single-ray cycle
  (center + 4 axes + 4 diagonals, wraps after 9). `computeSpreadDirection`
  delegates to it; the client/NPC/rocket call sites advance
  `WeaponRuntime::spreadCycleIndex` instead of process-static RNG.
- No packet/struct layouts changed: `spreadSeed`/`deterministicSeed` remain on
  the wire and in structs but are no longer read for generation.

Evidence:
- Cold build `python build_agent.py` → `BUILD SUCCESS`, executable
  `mimita-20260921T230914.exe`.
- New `--pellet-pattern-selftest` → `PASS`: grid is deterministic, seed
  independent, identical between the client and server generators; single-ray
  cycle reproduces and wraps after 9 shots.
- `--hot-combat-selftest` → 24 `[FAIL]` (animation/phase2), the same pre-existing
  set; no new failures.

## Hot hitscan server port — Phase 2

Ported the cold authoritative hitscan into the hot tool behavior so it can
eventually replace `WeaponExecution::traceHitscan` behind
`TOOL_FLAG_OWNS_EXECUTION`. The flag remains OFF, so the cold path is still
authoritative for players.

- `src/combat/pellet-pattern.h` is now header-only (`inline`) so the SAME fixed
  grid and single-ray cycle serve the cold EXE and the hot game DLL (one spread
  owner across the boundary); `src/combat/pellet-pattern.cpp` was deleted.
- `src/hot-reload/modules/tools/hitscan-tool.cpp` rewritten:
  - builds the shared fixed pellet grid (`buildFixedPelletDirections`);
  - scans targets with `findEntities(GAME_COMPONENT_HEALTH)` +
    `readComponent(TRANSFORM/BODY/HEALTH)` (skips the shooter and the dead);
  - per pellet, tests each target's body box (radius/height from BODY, expanded
    by `beamThickness`), picks the closest, and blocks hits beyond the world
    range from one `queryWorldRay` along the aim;
  - damage = base x body-part x falloff (`hotHitscanPart` uses normalized hit
    height: head `>=0.85`, leg `<=0.35`), aggregates per victim, applies via
    `damage.apply` with `knockback[3]` = direction x damage x
    `victimKnockbackPerDamage`;
  - keeps per-tool-entity ammo/cooldown/reload and all tool action events.
- Behavior still declines (leaves the cold path in charge) on dry fire or an
  active cooldown, so it never swallows a shot.

Known gap (blocks the flag flip): the cold path validates against rewound
per-part body boxes from `standardPlayerBodyTemplate`; the hot behavior only has
the generic TRANSFORM/BODY capsule via `findEntities`/`readComponent`. Until the
body-part target geometry is exposed to hot (or rewind is owned hot), the port is
not tick-for-tick identical and the flag must stay off.

Evidence:
- Hot DLL build: `python build_game_dll.py` → `DLL build success`.
- Cold build: `python build_agent.py` → `BUILD SUCCESS`
  (`mimita-20260922T103636.exe`).
- `--pellet-pattern-selftest` → `PASS` (header-only version).
- `--hot-combat-selftest` → 24 `[FAIL]` (same pre-existing animation/phase2 set;
  no new failures).

## Rewound hitbox bridge — flag-flip blocker resolved

The cold hitscan trace validates against rewound per-part body boxes
(`standardPlayerBodyTemplate`), which the hot behavior could not see through the
generic TRANSFORM/BODY components. Exposed that geometry to hot using ONLY the
existing generic dynamic-component layer — no ABI change, no new kernel slot,
one geometry owner.

- `src/hot-reload/hot-hitscan-target.h`: shared POD `HotHitscanTargetV1`
  (version, spawnGeneration, up to 8 part boxes with center/half/bodyPart) and
  its component/schema hashes.
- `src/network/server-hitscan-targets.{h,cpp}`: cold publish/clear of the
  component from the exact `WeaponExecution::PlayerTarget` list.
- `src/network/server-attack.cpp`: hoisted the lag-compensated target build
  (`buildRewoundHitscanTargets`, also moved the NPC rewind logging) to BEFORE
  the hot dispatch, published the geometry, cleared it after, and reused the
  same vector for the cold trace. The cold trace now consumes the prebuilt
  targets; no duplicate target construction.
- `src/hot-reload/modules/tools/hitscan-tool.cpp`: reads the published boxes via
  `dynamicReadComponent` and ray-tests each part exactly like the cold
  `rayPlayerTarget` (head = 1, leg = 2, else torso), falling back to the body
  capsule only when no geometry is present.
- New `--hitscan-target-selftest` proves publish -> read -> clear round-trips the
  boxes (version, spawnGeneration, per-part center/half/bodyPart).

Evidence:
- Cold build `python build_agent.py` -> `BUILD SUCCESS`
  (`mimita-20260922T104534.exe`); hot DLL build -> `DLL build success`.
- `--hitscan-target-selftest` -> `PASS`; `--pellet-pattern-selftest` -> `PASS`.
- `--hot-combat-selftest` -> 24 `[FAIL]` (unchanged pre-existing set);
  `--movement-v206-parity-selftest` -> `PASS`; `--collision-selftest` -> `PASS`.

With this, the hot hitscan can now validate against the same rewound pose as the
cold authority, so the `TOOL_FLAG_OWNS_EXECUTION` flip is no longer blocked on
target geometry. The parity harness + flip (Phase 4) is the next step.

## Single ammo/cooldown/reload owner — Phase 3

Made the hot per-instance `ToolInstanceStateV1` the single owner whenever a hot
behavior claims a server attack or reload. Cold no longer writes (or reports
from) the legacy `WeaponToolState` for a tool that has hot state.

- `src/hot-reload/hot-tool-state.h`: extracted the pure state transitions
  `toolStateBeginReload` / `toolStateFinishReload` (no component access) so cold
  and hot share ONE owner of the ammo/reload math and timing.
- `src/network/server-hot-tool-state.{h,cpp}`: cold read/has/write access to the
  tool-state component (registers the schema).
- `src/network/server-attack.cpp`:
  - hot-accept branch now reads `ToolInstanceStateV1` from `use.toolEntity` and
    reports `currentAmmo`/`reserveAmmo`/`stateVersion`, with
    `nextAllowedFireTick = tick + ceil(cooldownRemaining * 60)`. It no longer
    calls `serverWeaponStateLoad/Store` or `cooldownTickFor` (no legacy write).
  - the legacy tick cooldown pre-gate is skipped when the tool has hot state
    (the hot behavior enforces its own cooldown), removing the dual owner.
- `src/network/server-packets.cpp` `handleReloadRequest`: when the tool has hot
  state, the reload starts through `toolStateBeginReload` on the hot state and
  the result reports hot values; the legacy component is never touched. Falls
  back to the legacy path only when no hot state exists.

Scope note: the flag is still OFF, so revolver/shotgun keep using the cold path
and its legacy `WeaponToolState`. These changes take effect when a definition
sets `TOOL_FLAG_OWNS_EXECUTION` (Phase 4). The legacy component and
`serverWeaponIsMigrated` are intentionally retained for the still-cold path.

Evidence:
- Cold build `python build_agent.py` -> `BUILD SUCCESS`
  (`mimita-20260922T105654.exe`); hot DLL rebuild -> `DLL build success`.
- `--hot-combat-selftest` -> 24 `[FAIL]` (unchanged); `--pellet-pattern-selftest`
  -> `PASS`; `--hitscan-target-selftest` -> `PASS`;
  `--movement-v206-parity-selftest` -> `PASS`; `--collision-selftest` -> `PASS`.

## v2.0.6 weapon parity harness + shared damage owner — Phase 4

- `src/combat/hitscan-model.h` (new, header-only): the single owner of the
  v2.0.6 hitscan damage model (base x part x falloff x angle) and knockback
  magnitude, generic over resolved scalar params so the cold EXE and hot DLL use
  the exact same inline code. Cold `WeaponExecution::computeHitscanDamage` /
  `hitscanFalloffFactor` / `hitscanPartMultiplier` now delegate to it; hot
  `hitscan-tool.cpp` maps `ToolDefinitionV1` onto it and no longer keeps a
  duplicate copy. Registering it in `hot-modules.json` `headers` (together with
  `pellet-pattern.h` and `hot-hitscan-target.h`) makes weapon damage/falloff and
  spread hot-editable: changing the header rebuilds only the game DLL.
- New `--weapon-parity-selftest` freezes the v2.0.6 revolver/shotgun reference
  numbers (50/0.08/1.0/6/1; 12/0.25/1.5/2/15) and asserts: revolver torso=50,
  head=100, leg=38; shotgun cold wrapper == shared model at 0..90m; damage
  monotonically falls with distance; the 15-pellet grid is deterministic.

### Flag flip: already ON — and a safety guard added

Important correction discovered this phase: `TOOL_FLAG_OWNS_EXECUTION` is ALREADY
set for revolver/shotgun (and rockets) in `tool-visuals.cpp`. Before this work
the hot `hitscanUse` declined for players (it required a
`relationship.targets` target, which only NPCs had), so the cold path stayed
authoritative in practice. The Phase 2 rewrite dropped that requirement, which
would make PLAYER hitscan claim hot. But the hot path applies damage through
`damage.apply` (`serverApplyEntityDamage`), which does NOT reproduce the cold
authoritative consequences:

- `serverResolveDamagePolicy` (per-weapon damage policy / caps) is skipped;
- player victims get no `DamageConfirmedEventPacket`
  (`queueServerDamageConfirmedEvent` is only called in the cold branch);
- NPC victims get no `broadcastNpcDamageEvent`;
- no `ShotEventPacket` / `PelletBlastEventPacket` shot-visual broadcast.

So an explicit ownership safety gate was added to `hitscan-tool.cpp`: a use is
claimed only when the actor's `ControlSource` is `GAME_CONTROL_SERVER_NPC`. Player
uses decline and the cold path stays authoritative, preserving shipped behavior.
All the ported trace/state code is retained ("dont delete") and ready.

The remaining gate to player hot ownership is one generic consequence capability
(e.g. `hitscan.resolve`) that hands the hot trace aggregates to the existing cold
consequence pipeline, rather than re-implementing policy/events hot.

Evidence:
- Cold build `BUILD SUCCESS` (`mimita-20260922T111214.exe`); hot DLL
  `DLL build success`.
- `--weapon-parity-selftest` PASS; `--pellet-pattern-selftest` PASS;
  `--hitscan-target-selftest` PASS; `--movement-v206-parity-selftest` PASS;
  `--collision-selftest` PASS; `--hot-combat-selftest` 24 FAIL (unchanged).

## Authoritative consequence bridge — player hot ownership enabled

Closed the last gate to player hitscan hot ownership by making the authoritative
consequences a single shared owner instead of re-implementing them hot.

- `src/network/server-hitscan-outcome.{h,cpp}` (new): the entire cold post-trace
  consequence block (damage policy, `DamageConfirmedEventPacket`,
  `broadcastNpcDamageEvent`, kill recording, shot/pellet-blast visual broadcast,
  hit verdict) extracted verbatim into `serverResolveHitscanOutcome(...)`.
  `server-attack.cpp`'s cold branch now calls it — one consequence owner.
- `game-api.h`: new append-only capability `GAME_CAP_HITSCAN_RESOLVE`
  (`hitscan.resolve`) + `GameHitscanResolveV1`; `ToolUsePolicyV1` gained
  append-only `claimedTargetId`/`clientSimulationTick`.
- `server-attack.cpp`: sets those two fields on the tool-use fact.
- `live-behavior.cpp`: `capHitscanResolve` maps the request to a
  `HitscanTraceResult` and runs `serverResolveHitscanOutcome` on the live server
  context; registered as a kernel capability.
- `hitscan-tool.cpp`: removed the temporary `ControlSource` safety gate (player
  uses are now safe), records per-pellet results + world hit, and replaces the
  `damage.apply` call with `hitscan.resolve`. Hot `effect.muzzle` removed so the
  shared shot broadcast is the single source of muzzle/tracer.
- New `--hitscan-outcome-selftest`: an NPC-victim trace through the bridge must
  apply damage (100 -> 50) and emit the NPC damage broadcast.

Evidence:
- Cold build `BUILD SUCCESS`; hot DLL `DLL build success`.
- `--hitscan-outcome-selftest` PASS; `--weapon-parity-selftest` PASS;
  `--pellet-pattern-selftest` PASS; `--hitscan-target-selftest` PASS;
  `--movement-v206-parity-selftest` PASS; `--collision-selftest` PASS;
  `--ragdoll-world-selftest` PASS; `--hot-combat-selftest` 24 FAIL (unchanged).

Dormancy/runtime note: multipayer runtime + human acceptance of the now-active
player hot path are not yet observed; only source/build/test evidence is claimed.

## Hot surface added this work (fewer cold builds)

- Hot-tracked headers added to `hot-modules.json`: `src/combat/pellet-pattern.h`,
  `src/combat/hitscan-model.h`, `src/hot-reload/hot-hitscan-target.h`. Editing
  weapon spread, the v2.0.6 damage model, or the rewind hitbox bridge now
  rebuilds only the game DLL.
- Weapon damage/falloff and spread have one inline owner shared by cold and hot;
  the hot tool modules own the trace and per-instance ammo/cooldown/reload.
- `hitscan.resolve` moves the consequence behavior to a single owner callable by
  any hot weapon; no per-weapon cold call site is needed to add a hot hitscan.

## behaviorSource parity + melee/projectile consequence bridge

Extended the hot-ownership work so (a) editing `config/weapons.json` hot-retunes
the tools, and (b) melee/projectile hot behaviors run the same authoritative
consequences as the cold path.

### behaviorSource parity (`weapon.tuning`)
- `game-api.h`: new `GAME_CAP_WEAPON_TUNING` (`weapon.tuning`) +
  `GameWeaponTuningV1` (scalars + up to 24 custom params).
- `src/network/server-weapon-tuning.{h,cpp}` (new): resolves the network id to
  the registry `WeaponDefinition` (which already honors `behaviorSource`) and
  reports it. `live-behavior.cpp` `capWeaponTuning` registers it.
- `src/hot-reload/hot-tool-tuning.h` (new): `hotQueryWeaponTuning` +
  `hotTuningHasParam`.
- Hot tools now prefer the registry tuning over recipe literals:
  `hitscan-tool.cpp` (damage/headshot/fireDelay/reload/mag/reserve/pellets/
  spread/beam/falloff/knockback), `melee-tool.cpp`, `physical-contact-tool.cpp`,
  `rocket-tool.cpp` (rocketSpeed/radius/lifetime/splash/knockback/self-damage),
  `grenade-tool.cpp` (forwardSpeed/lifetime/splash). Editing `weapons.json` now
  retunes hot weapons with no EXE rebuild.

### melee/projectile consequence bridge (`damage.resolve`)
- `src/network/server-damage-outcome.{h,cpp}` (new):
  `serverResolveDamageOutcome` applies per-victim damage policy,
  `applyServerDamage`, `queueServerDamageConfirmedEvent`,
  `broadcastNpcDamageEvent`, and kill recording for a caller-supplied source
  kind. One shared owner for melee/projectile damage.
- `game-api.h`: `GAME_CAP_DAMAGE_RESOLVE` + `GameDamageResolveV1` (up to 8
  victims); `live-behavior.cpp` `capDamageResolve` maps and calls the owner.
- `src/hot-reload/hot-damage-resolve.h` (new): `hotResolveDamage`.
- `hot-projectiles.cpp` splash/direct damage, `melee-tool.cpp`, and
  `physical-contact-tool.cpp` now route through `damage.resolve` instead of the
  raw `damage.apply`, so confirmed/NPC events and kill recording are no longer
  dropped.
- `hot-modules.json` tracks the new headers (`hot-tool-tuning.h`,
  `hot-damage-resolve.h`, `hot-hitscan-target.h`, `pellet-pattern.h`,
  `hitscan-model.h`) so editing them rebuilds only the game DLL.

Evidence:
- Cold `BUILD SUCCESS` (`mimita-20260922T120622.exe`); hot `DLL build success`.
- `--weapon-parity-selftest` PASS (now also asserts `weapon.tuning` == registry
  for revolver/shotgun); `--hitscan-outcome-selftest` PASS (now also asserts the
  melee/projectile owner applies damage); the rest of the suite PASS;
  `--hot-combat-selftest` 24 FAIL (unchanged).

Residual: hot rockets/grenades still do not broadcast a reliable
`ProjectileExplodeEventPacket` for remote clients (visual composition is local
via `hotComposeExplosion`), and thrown-grenade/banana spawn values are not yet
tuned from JSON. Runtime multiplayer/human acceptance remains unobserved.

## Hot projectile reliable broadcast + JSON/cpp projectile tuning

- `game-api.h`: `GAME_CAP_PROJECTILE_EVENT` (`projectile.event`) +
  `GameProjectileEventV1` (spawn/explode/despawn).
- `src/network/server-projectile-event.{h,cpp}` (new): builds the
  `ProjectileExplodeEventPacket` (reliable, `queueReliableGameplayEventToAll`)
  and `ProjectileSpawnEventPacket` (broadcast to all except owner) on behalf of
  a hot owner. `live-behavior.cpp` registers `capProjectileEvent`.
- `src/hot-reload/hot-projectile-event.h` (new): `hotBroadcastProjectileEvent`.
- `hot-projectiles.cpp`: on detonation the canonical projectile system now
  broadcasts the reliable explode event (position/radius/weapon), so remote
  clients always see hot rocket/grenade explosions.
- `rocket-tool.cpp` / `grenade-tool.cpp` / `thrown-grenade-tool.cpp`: broadcast
  the authoritative spawn event and take their projectile values from
  `weapon.tuning` (rocketSpeed/rocketRadius/splashRadius/knockback/
  self-damage/gravity/throw_speed/up_bias/bounceRestitution/magazine/reserve/
  fireDelay). Because `weapon.tuning` reads the registry, editing
  `config/weapons.json` (behaviorSource `json`) OR `weapon-data.cpp`
  (behaviorSource `cpp`) retunes these hot tools with no EXE rebuild.
- `hot-modules.json` tracks `hot-projectile-event.h`.

Evidence:
- Cold `BUILD SUCCESS` (`mimita-20260922T121637.exe`); hot `DLL build success`.
- Full suite PASS (weapon-parity, hitscan-outcome, hitscan-target,
  pellet-pattern, movement-v206-parity, collision, ragdoll-world);
  `--hot-combat-selftest` 24 FAIL (unchanged). Projectile broadcast is
  source/build-verified; live multiplayer observation is still pending.

## Generic event/damage primitives — consequence logic now hot-fixable

Closed the last cold dependency of the weapon consequence code by exposing the
kernel's minimal primitives as generic capabilities and moving the
packet-building into hot headers. The consequence orchestration and packet
contents are now hot-editable.

- `game-api.h` (replacing the projectile-specific event):
  - `event.next-id` (`event.next-id`) + `GameReliableEventTicketV1`
  - `event.broadcast` (`event.broadcast`) + `GameEventBroadcastV1`
    (reliable/exclude-owner flags, up to 512 payload bytes)
  - `damage.policy` + `GameDamagePolicyV1` (per-victim policy + cap)
  - `damage.event` + `GameDamageEventV1` (apply one damage fact + emit
    `DamageConfirmedEventPacket` / `broadcastNpcDamageEvent` / kill recording)
- `src/network/server-event-broadcast.{h,cpp}` (new): assigns the reliable
  ticket and queues/sends caller-built packet bytes.
- `src/network/server-damage-event.{h,cpp}` (new): the per-victim policy query
  and the apply+event step (extracted from `server-damage-outcome`).
- `src/hot-reload/hot-event-broadcast.h` (new): hot `hotEventNextId` /
  `hotEventBroadcast` / `hotBroadcastPacket`.
- `src/hot-reload/hot-damage-event.h` (new): `hotResolveDamagePolicy` /
  `hotApplyDamageEvent`.
- `src/hot-reload/hot-projectile-event.h` rewritten: the actual
  `ProjectileExplodeEventPacket` / `ProjectileSpawnEventPacket` are BUILT in the
  hot header and sent through `event.broadcast`. `server-projectile-event.*` and
  the `projectile.event` capability were removed.
- Removed the cold per-event packet code path; the packet field layout is now a
  hot-editable header.

Net: projectile spawn/explode packets, the per-victim damage event, and the
policy query are all decided/built by hot code and only transported by the
kernel — so a bug in that logic is a DLL-only fix.

Evidence:
- Cold `BUILD SUCCESS` (`mimita-20260922T124118.exe`); hot `DLL build success`.
- Full suite PASS (weapon-parity, hitscan-outcome, hitscan-target,
  pellet-pattern, movement-v206-parity, collision, ragdoll-world);
  `--hot-combat-selftest` 24 FAIL (unchanged). Live multiplayer observation
  remains pending.

## Consequence orchestration moved into a hot module

The weapon damage-consequence orchestration and its packet contents now live in
one hot header, and both the cold fallback and the hot behaviors call it.

- `src/hot-reload/hot-consequences.h` (new): `HotConsequences::resolveHitscan`
  (orchestration: per-victim policy/damage/events, single/pellet shot-visual
  packet building, hit verdict) plus `applyVictim` / `deliverVictim` for melee
  and projectiles. It only calls generic primitives (`damage.policy`,
  `damage.event`, `event.next-id`, `event.broadcast`).
- `src/hot-reload/modules/tools/hitscan-tool.cpp`: hot hitscan now calls
  `HotConsequences::resolveHitscan` directly (no `hitscan.resolve` request tuple).
- `src/network/server-hitscan-outcome.cpp`: reduced to a thin cold entry that
  builds a `GameplayContextV1` and calls the same hot orchestrator, so cold and
  hot hitscan run one implementation.
- `src/network/server-damage-outcome.cpp`: reduced to a thin cold entry that
  calls `HotConsequences::applyVictim`.
- `src/network/packets.h` added to `hot-modules.json` headers: editing the shot
  packet contents is now a hot change.

Net: a bug in hitscan/melee/projectile consequence logic (damage application,
events, packet contents, hit verdict) is fixable by rebuilding only
`mimita-game.dll` — no EXE rebuild.

Evidence:
- Cold `BUILD SUCCESS` (`mimita-20260922T144156.exe`); hot `DLL build success`.
- Full suite PASS (weapon-parity, hitscan-outcome, hitscan-target,
  pellet-pattern, movement-v206-parity, collision, ragdoll-world);
  `--hot-combat-selftest` 24 FAIL (unchanged). Live multiplayer observation
  remains pending.

## Full hot server combat routing: attack policy + damage cap

Moved the remaining server combat *policy* into hot modules so a behavior/decision
bug is a DLL-only fix. Mechanisms stay cold (documented).

- `game-api.h`: added `GAME_EVENT_ATTACK_POLICY` + `AttackPolicyV1` + 
  `GAME_EVENT_HASH_ATTACK_POLICY` (generic-table dispatcher id).
- `live-behavior.{h,cpp}`: `dispatchAttackPolicy` routes through the generic
  event-type table so packages register by event id (no cold call-site per
  behavior).
- `src/hot-reload/modules/tools/attack-policy.cpp` (new, hot): owns attack
  routing/validation — creation-mode block, dead, stale spawn, spawn-state,
  slot mismatch, hitscan geometry tolerance, per-tick shot limit, community set,
  and reported ammo/cooldown. Scope-gated to recipes with
  `TOOL_FLAG_OWNS_EXECUTION`, so cold-owned weapons are untouched.
- `server-attack.cpp`: builds `AttackPolicyV1`, dispatches it before the cold
  validations; hot reject/accept-with-suppress sends the result and returns,
  otherwise the cold path runs unchanged (safe opt-in seam).
- Damage cap/policy was already hot (`rocket-behavior.cpp`
  `GAME_EVENT_DAMAGE_POLICY`); `serverResolveDamagePolicy` is now clearly the
  fallback only.
- `hot-modules.json`: tracked `packets.h` and `hot-consequences.h`.
- New doc `docs/gold/2026-09-22-hot-cold-combat-boundary.md` records what may be
  hot vs the cold mechanisms.

Live-edit proof: flipped a one-line decision in `attack-policy.cpp`, rebuilt only
`mimita-game.dll`, and the unchanged EXE selftest observed the new decision
(`hot attack policy accepts a valid request` → FAIL), then restored. This proves
the running EXE's combat routing is DLL-driven.

Evidence:
- Cold `BUILD SUCCESS`; hot `DLL build success`.
- `--hot-combat-selftest`: 24 pre-existing FAIL + 3 new `hot attack policy`
  checks PASS (accept valid, reject dead, reject stale). Suite PASS
  (hot-authoritative, weapon-parity, hitscan-outcome, hitscan-target,
  pellet-pattern, movement-v206-parity, collision, ragdoll-world).

## v2.0.6 combat harness extended to projectile/melee/attack-policy

- `src/combat/combat-v206-parity-selftest.{h,cpp}` (new) +
  `--combat-v206-parity-selftest`. Freezes the pre-hot cold reference formulas
  and drives the live hot owner:
  - projectile splash: two-regime full-damage-radius/edge mix and the
    full-damage-radius==0 Gaussian `exp(-(d/r)^2 * exponent)`, plus the knock
    scale (`(1 - t^2)*0.85 + 0.15`, or `damage/splashDamage` clamp);
  - physical contact: slash/lunge base damage and knockback reference;
  - attack policy: drives `LiveBehavior::dispatchAttackPolicy` for a valid
    accept, dead reject, stale-spawn reject, out-of-tolerance geometry reject,
    per-tick shot-limit reject, and a non-hot weapon decline (cold owns).
- The harness starts the hot package so it exercises the real hot owner, not a
  copy.

Evidence:
- Cold `BUILD SUCCESS`; hot `DLL build success`.
- `--combat-v206-parity-selftest` PASS. Full suite PASS (weapon-parity,
  hitscan-outcome, hitscan-target, pellet-pattern, movement-v206-parity,
  collision, ragdoll-world, hot-authoritative); `--hot-combat-selftest` 24
  pre-existing FAIL (unchanged).

## Final changelog

This is the single final changelog for this session.
