# Rocket League vehicle mode — continuation handoff

- Date: 2026-11-28
- From: `rocket-rl-settle`
- Repository: `C:\mimita-v9` (shared checkout, current branch)
- Current game generation: **820** (`build/game/current.json`, status ok, revision `d4ad672e65c9ad0129f009a42af449fefaf36fc407cf607eab6868082289628c`)
- Current EXE: `2026-11-27T17:45:19.700118Z:758a748d853e7b8b5bed98c614f8b243c0c9e5b7e8b9d9455fd04d6d9b096fdc`

## Objective

Make the Rocket League vehicle arena behave like Rocket League/RocketSim:
smooth client-owned cars, no flat-ground bounce, smooth wall/ceiling/slope
contact, no out-of-world, visible boost pads with per-pad recharge timers,
sphere VFX instead of text, boost trail, axis labels, and no camera/ball-cam
regressions.

## Read these first

- `docs/specs/gamemodes/vehicle-arena.md` (settling, camera, evidence ledger)
- `docs/specs/dev-loop/agent-lanes-build-authority.md`
- `docs/specs/dev-loop/change-to-evidence-latency.md`
- `AGENTS.md` gates 0, 0b, 0c, 6
- `docs/changelog/2026-11-28/20261128_0237-client-car-smooth-ballcam-vfx.md`

## Current state (what is in gen 820)

### Working / built

- Client-owned arena car: prediction on `enterLocalDriver`; `PACKET_VEHICLE_POSE`
  is relayed by the server from the input packet (`VEHICLE_INPUT_HAS_POSE`).
  Local car never accepts a server snapshot pose (`vehicle.pose.server_error`
  only; no `hard_resync`).
- 120 Hz render interpolation (`renderAlpha` fixed from 1/60 to 1/120); camera
  follows `physicalEntityRenderPosition`.
- Remote cars use `vehicle.pose.relayed` + `poseRelay` (physical state ignored).
- Local ball prediction with 15% reconciliation; ghost ball VFX/sound after
  250 ms without packets.
- Ball cam finds `arena-ball` (spawn `reserved` bit 2; client persistenceId),
  falls back to car nose instead of freezing.
- Proximity fade forced off while seated; mode JSON `proximity_fade: off`.
- `va_car_reset [blue|orange]`, server reset, camera follow snap.
- HUD km/h uses 3D speed; seated net overlay says client-authoritative.
- Vehicle WAVs: `assets/sound/vehicle/{boost,jump,airjump,dodge,land,wall_bump,ball_hit,goal,ghost,demolish}.wav`.
- Gen 820: sticky force sign fixed (`-groundUp`, into the surface) and auto-roll
  gated to inverted + zero wheel contacts + chassis contact + throttle.

### Known broken / open

1. **Pad VFX render as big text.** The pad orbs and boost trail call
   `EffectPartSystem::spawnCustom`, which sets `billboardText = true`. The
   effect renderer draws the label (`arena_boost_pad`) as text, not a sphere.
   This is why the user still sees `arena_boost_pad` text in gen 820.
   Fix: set `billboardText = false`, `modelPath =
   "assets/objects/things/physics-objects/5x5x5-sphere.glb"`,
   `modelNormalize = true`, `scale = radius * 2`, `color` = yellow/orange/grey/
   white. The model renderer applies `uTint` (`effect-part-render.cpp`).
2. **User cannot see their own car.** The live client `20261128_092602` was
   connected to the older server `20261128_082433`, had no `vehicle.entered`,
   no `vehicle.camera.applied`, and a 45 s stale snapshot. Restart the run and
   confirm `vehicle.entered` + `vehicle.prediction.enabled`. If the car is
   still invisible after a clean join, inspect `renderEnabled` (no assignments
   found), `drawGeneratedEntityMesh`/`drawTexturedPhysicalBox`, and the camera
   branch.
3. **Pad timers.** A `PACKET_BOOST_PAD_STATE` (10 Hz) already sends
   `(index, remainingCentiseconds)` per depleted pad and the arena HUD projects
   `%.2f` above each grey pad. The visible "one timer updating over and over"
   is the effect-label text. After removing `billboardText`, verify one timer
   per depleted index and add local monotonic countdown between packets.
4. **Bounce still reported by the user.** The sticky sign fix is code-certain
   but not yet measured. The first flat-drive check was contaminated by the old
   fuzz server `20261128_084634`: the car fell from the ceiling and bounced at
   22.9 m/s. That fuzz server and the contaminated run were stopped.
5. **Wall/ceiling/slope smoothness is untested.** Wall band now starts at 4.55
   (flush with the ramp), invisible backstops exist, wheel normals are
   low-passed. Continuous wall/ceiling driving has not been measured.
6. **Collision owner split.** The actor uses capsule→`ActorWorldContact`→
   `solveActorWorld`; the car is a `PhysicalEntity` box + wheel rays but also
   has its own `lastContactPenetration` push and force layer. The refactor goal
   is one contact *response* (not one shape): shape queries (capsule/box/rays)
   → one `WorldContact[]` → one resolver with per-contact restitution
   (wheels 0, chassis 0.3), friction, adhesion `-normal`, penetration bias.
   Do **not** make the car a capsule; RL cars are boxes.
7. **Body bounce.** `bodyRestitution` is still 0.30. If flat/wall body impacts
   are too bouncy after the sticky fix, lower it to 0.10-0.15; the wheel path
   should be non-restituting.

## Evidence (pinned)

- Gen 812, run `20261128_073416`: prediction on, `vehicle.pose.accepted`
  `source=input`, 0 hard resyncs, ball cam found, reset works.
- Two-client run `20261128_080510` gen 814: client-02 `vehicle.pose.relayed`
  17+ samples, owner player 1, 17.56 m/s.
- Stiffness 0.5 run `20261128_080939` gen 815: tau 0.1539, 8.22 m/s → 1.36 m
  extra vs 1.27 expected.
- Fuzz run `20261128_081709` gen 816: 17 kicks, 17 wheels, 0 out_of_world.
- Fuzz run `20261128_084634` gen 817: 16/16 wheels, 0 out_of_world (this server
  was left running and contaminated a later flat test; stopped).
- Live user run `20261128_082433`: 118 auto-roll events during throttle (motivated
  the roof-only gate), landings up to 5 m/s, containment at y≈41.9/42.1.
- Gen 820 build `game-dll-20261128T170629149896Z-5ccf06`:
  changed `src/vehicles/vehicle-system.cpp`; compiled 2, skipped 645;
  compile 18.2 s, link 4.0 s, total 27.9 s; CPU mean 46%, min free RAM 3012 MB.

## Build / resource notes

- Gen 820 was fast because only one TU changed: the object cache skipped 645
  TUs (`cache.compiled=2`, `skipped=645`). The earlier 6-8 minute builds were
  header fanout (41+ TUs) or full snapshots.
- `devscripts/dev_loop/game_build.py` hardcodes `--jobs 10`; it is **not**
  resource-aware today. The build gate checks CPU/RAM before starting, and
  `build.resource_sampling` records mean/peak/min, but the job count does not
  adapt. With 2 TUs jobs are irrelevant; for full/header builds a cap like
  `min(10, cores-1, available_ram/400MB)` would reduce thrash. Evidence:
  gen 800 min free RAM 57.8 MB / CPU 100%; gen 820 3012 MB / 46%.
- Use the root dev-loop only. Queue `build-note.py` before compiler saves,
  `agent-coord.py build request --auto`, then `dev-loop.py --request
  agent-build --build-request <id>`. Read the postmortem after every build.

## Next steps (ordered)

1. Restart the user's run cleanly; confirm `vehicle.entered`,
   `vehicle.prediction.enabled`, and a visible car. Stop old fuzz servers.
2. Replace pad/boost effects with the sphere GLB (no `billboardText`), radius
   1 m small / 2 m big, per-pad timer only for depleted pads, local countdown.
3. Clean flat-drive/drop test on gen 820+:
   - throttle only 3 s at the kickoff (not into the ball/wall); sample
     `vehicle.chassis` origin_z, expect stable 0.31 with no oscillation.
   - 2 m drop: settle <1 s, rebound <5 cm.
4. Wall/ceiling drive test: boost up a wall and across the ceiling; check wheel
   contact continuity, no auto-roll while wheels touch, no out_of_world.
5. If bounce persists, implement the shared contact resolver described in
   "Collision owner split"; remove `lastContactPenetration` and per-tick
   containment from normal play.
6. Add a clean run to the spec evidence ledger and update the changelog.

## Ownership / files

- `src/vehicles/vehicle-system.cpp` — sticky/auto-roll/landing, containment,
  prediction, boost pads force, input.
- `src/vehicles/vehicle-render.cpp` — wheels, pad orb effects, axes flag.
- `src/vehicles/vehicle-hud.cpp` — HUD, pad timers, axes legend.
- `src/engine/engine-tick-render.cpp` — axes lines, pad draw call.
- `src/engine/engine-tick-camera.cpp` — vehicle camera, ball cam.
- `src/network/vehicle-networking.cpp` — pose relay, pad state packet, reset.
- `src/network/multiplayer-physical-entities.cpp` — pose corrections, ball.
- `src/physics/physical-entity.{h,cpp}` — render interpolation, poseRelay.
- `config/vehicles/octane_rl.json` — damping 1900, collisionCenter -0.0045.
- `config/vehicle-profiles/rl_soccar*.json` — auto-roll/sticky values.
- `devscripts/gen-arena2.py`, `config/maps/arena2.json` — wall band 4.55,
  backstops, 3 m floor.
- `config/gamemodes/vehicle_freeplay.json` — proximity fade off.
- `docs/changelog/2026-11-28/20261128_0237-client-car-smooth-ballcam-vfx.md`.

## Do not

- Do not stop another agent's or the human's run; the live user run is
  `20261128_082433` / `20261128_092602`.
- Do not claim bounce/wall smoothness fixed without a clean run measurement.
- Do not make the car a capsule; do not add per-pose `if x > 0.5` collision
  hacks.
- Do not modify the map editor or unrelated agents' files.
