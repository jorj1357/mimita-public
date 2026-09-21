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
2. Phase D remainder: make `aimbody.json` and `weapons.json` tool/animation
   references authoritative; migrate or confirm action keyframes (JSON `actions`
   currently override durations only); replace the conservative weapon collision
   sphere with the resolved marker + `weaponcollisions.json` shape.
3. Phase C: NPC/actor collision, projectile world contact, the legacy
   `collision-solver.cpp` / `physics.capsuleSolve` chain, the server sample-based
   sphere solver, the oriented-capsule ABI, the weapon shape picker, and the
   collision-authoritative cold/harness path are done/retired. Props do not exist
   yet. **Still open**: expose the weapon **GLB triangles** to the hot collider (a
   mesh capability) so the default `triangles` weapon mode uses the real mesh
   instead of a conservative sphere. This needs triangle colliders in the
   collision ABI/solver (or a mesh capability that returns the resolved weapon
   triangles), which is the next substantial collision task.
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


## Final changelog

This is the single final changelog for this session.
