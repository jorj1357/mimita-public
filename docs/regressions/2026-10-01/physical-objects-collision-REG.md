# Physical Objects Collision and Settling

Time created: 2026-10-01T17:37:13Z
Time last updated: 2026-10-01T21:03:19Z

Status: ATTEMPTED FIX (9)

Related specification:
`docs/specs/destructible-world/destructible-world.md`,
`docs/specs/moving-physical-objects/moving-physical-objects.md`,
`docs/architecture/collision/collision.md`

Related changelog:
`docs/changelog/2026-10-01/20261001_173713-physical-objects-collision-fix.md`

---

## Regression Occurrence 1

### Observed

Times: 2026-09-27 through 2026-10-01 (human playtesting, funworld3 and a test
map). This is the running record of getting moving physical objects and the
destructible world to feel right. It is attempt 5 or later. Confirmed
observations, grouped:

**A. Collision / phasing (worst)**
- Kicking a crate so one part intersects another can make the intersecting part
  "disappear into the floor".
- Crate on the floor against a perpendicular wall, with two crate corners
  touching floor + wall at once, disappears (likely into the floor).
- Crate phases into a cylinder it is resting on / next to.
- Standing on a tipping crate while it tips toward a cylinder lets the player
  phase into the world.
- Pushing a crate into a corner (cylinder + floor + roof) lets the player phase
  into the crate; it phased back out later, but phasing in must never happen.
- An edge/corner of the crate catches and jitters/slides along the edge of a
  wall that is partly inside the flat floor (funworld3).

**B. Settling / motion feel**
- Crates with no holes bounce better now and behave well at density 1.
- Density 2 settling is weird and slow; density 10 never settles flush, it sits
  "barely on the edge". Density 1 is the ideal.
- A crate falling seems to settle slowly; gravity feels too low.
- Velocity smoothing is odd: the crate goes very fast, then its velocity is
  killed exponentially (drag feels far too strong); it should keep going more
  like a real crate.
- Running into the crate at high speed did not move it; touching it at any speed
  should apply movement from the speed/force at that moment.

**C. Fragments**
- A big chunk floated in the air, not falling.
- A tiny "single pixel" fragment existed.
- Fragments do fall (one big piece split in two), then disappeared.
- A ~0.1x0.5x0.2 m piece floated unsupported; it should fall, and small/old
  pieces should be deleted rather than simulated forever.

**D. Destruction performance**
- Large FPS drop exactly when shooting a crate (holes); much worse shooting
  multiple times; sometimes a ~0.5 s freeze.
- FPS drop merely being near a crate with holes; worse when interacting with it.
- `perf_report` at ~10 fps; physics reported ~20-50 ms per frame, rendering
  ~20 ms.
- funworld3: ~450-500 fps average drops to ~200-250 fps with one crate with
  holes in the game.

**E. UI**
- Terminal has no word wrap.
- Shift+Tab should reverse-cycle the autocomplete list.

**F. Crashes**
- None observed during this test session (the earlier 11:36 crash is tracked
  separately in `destruction-uncaught-exception-REG.md`, still UNRESOLVED).

### Expected Behavior

Per `docs/specs/moving-physical-objects/moving-physical-objects.md` and
`docs/architecture/collision/collision.md`: collision uses the shared triangle
path at a fixed 60 Hz; moving and rotating bodies are swept and never phase into
each other or the world; a body comes to rest on a flat face it actually has,
regardless of density; touching a dynamic body transfers momentum from the
speed/force at the moment of contact; unsupported fragments fall and tiny/old
debris is removed; destruction is bounded and does not create frame spikes;
the terminal wraps long lines and supports reverse autocomplete.

### Actual Behavior

See Observed. Correctness failures (phasing / disappearing), incorrect settling
(density-dependent, not flush), too-strong drag, missing momentum on touch,
floating/tiny fragments, and large destruction/interaction frame drops.

### Why This Is Bad

The core gameplay promise ("objects behave like physical matter, and shooting
carves real holes cheaply") is broken when objects phase into geometry, never
settle, or tank the frame rate.

### Specification

`docs/specs/destructible-world/destructible-world.md` (sections 17, 19, 20, 21,
44-46), `docs/specs/moving-physical-objects/moving-physical-objects.md`,
`docs/architecture/collision/collision.md`.

### Confirmed Causes (from code inspection)

- Entity-vs-entity contacts intentionally passed `contactSkin = 0.0` in
  `resolveEntityContacts`, which **disables the rounded feature shell**
  (edges/vertices are not thickened). This is the prime suspect for phasing at
  corners/edges and against curved geometry.
- `collectActorEntityContacts` also passes `0.0` (legacy entity contact
  contract), so player-vs-crate uses exact triangle tests without the rounded
  shell.
- The entity world sweep in `advanceKinematics` does only 3 correction passes
  with no explicit deep-depenetration pass; two simultaneous contacts (floor +
  wall) can leave residual penetration and the body sinks.
- Settling uses `isBoxRestingUpright` (local box axes only) and
  `applyRestingRightingTorque`; any authored mesh is judged by box axes, and
  angular response scales with `1/density`, so density changes settling.
- `refreshMassProperties` scales inertia by density; sleep thresholds are
  velocity-based, so high density never reaches the "settled" state.
- Object gravity is hardcoded `PHYS.gravity = -58` times `gravityScale`; there
  is no object/crate gravity config.
- Drag: `linearDamping` (default 0.15) plus a supported-body
  `velocity.x *= 0.65` (`physical-entity.cpp`) kills velocity exponentially.
- `applyPlayerPush` uses an AABB overlap and a fixed push; it does not scale
  with the player's speed/force at contact.
- `applyFracture` rebuilds the primary piece's mesh/mass but does not clear
  `sleeping`/`sleepTicks`/`supportGraceTicks`, so a previously-asleep primary
  piece can stay frozen ("big chunk floating").
- There is no minimum-fragment-size / lifetime / max-count policy; tiny
  fragments persist.
- Performance: the entity world sweep uses the uncached
  `appendChunkTrianglesForAABB` gather; `cachedEntitySurface` rebuilds world
  triangles + AABB tree on every transform change; `resolveEntityContacts` is
  O(n^2) with fragments; the generated-mesh renderer uses one shared VBO that
  re-uploads on every entity switch.

### Attempt history (why we are where we are)

- **Attempt 1 — SDF/grid surface.** Sampled box-minus-spheres on a grid. No
  outside-air border, so a localized hole eroded the whole shell and shrank the
  crate. Abandoned.
- **Attempt 2 — Manifold boolean, canonical base + ordered cut history.** Real
  CSG subtraction, one cut stored in local space. Geometry was correct but
  repeated shots at the same spot did not deepen.
- **Attempt 3 — queue + budget + no-op detection.** `ImpactSystem::submit`
  enqueues; `flushPendingCuts` runs once per fixed tick under a cut/time budget;
  fixed the "after waiting, shooting makes no new holes" stall. Destruction no
  longer wedges, but penetration still failed.
- **Attempt 4 — swept-capsule bore + bore-walk.** Repeated shots deepen and
  eventually tunnel; the lingering bug was a phantom impact on the entry plane.
- **Attempt 4b — `closestPointOnTriangle` vertex-C fix (2026-10-01).** The
  phantom impact was a wrong closest-point region test returning a plane point
  outside the triangle. Fixed; penetration now works.
- **Attempt 5 — crash-safe + shared triangle entity collision (2026-10-01).**
  Replaced the AABB-only entity-vs-entity response with the shared triangle
  narrowphase, added projectile momentum/torque, config-driven budgets, GLB
  spawn list, crash diagnostics. Crates now bounce and exchange momentum, but
  the collision/settling/perf/fragment issues above remain. This record tracks
  the remaining work.

### Implemented fixes (Attempt 5, 2026-10-01)

- **WE (UI, done).** `Shift+Tab` now reverse-cycles the autocomplete list
  (`terminal-input.cpp`); terminal output is word-wrapped to the live draw width
  (`terminal.cpp` `wrapTerminalLine`, `terminal-render.cpp` sets `mWrapColumns`).
- **WA (partial).** Entity-vs-entity contacts now use the rounded feature shell
  (`resolveEntityContacts` passes `contactSkin = -1`), thickening edges/vertices
  so a moving crate cannot slip through a corner or curved surface. Player push
  now includes `externalImpulse` so a dash into a crate transfers the contact
  speed. Actor-vs-entity stays exact (skin 0) because the rounded shell changed
  player-carry support; that migration is still open.
- **WB (done, needs tuning).** Settling now uses the object's real face
  orientations (`DestructibleGeometry::restAxes`, `computeRestAxes`) instead of
  only local box axes, and righting is applied as angular velocity so it is
  density-independent. Object gravity and damping are config-driven
  (`config/destructible-world.json` `physics`: `objectGravity`,
  `objectLinearDamping`, `objectAngularDamping`, `supportedFrictionRetain`),
  hot-reloadable. Defaults in code preserve legacy feel for the self-tests; the
  JSON holds the tuned in-game values.
- **WC (done, needs tuning).** `applyFracture` wakes and clears the primary
  piece's sleep/support state (the floating-chunk cause) and flags children as
  fragments. A fixed-tick lifecycle pass removes fragments smaller than
  `minFragmentVolume`, older than `fragmentLifetimeSeconds`, or beyond
  `maxTotalFragments` (all in the JSON `fragments` section).
- **WD (partial).** The entity world sweep no longer allocates per entity per
  collision pass (thread-local scratch + `collectActorMeshContactsInto`), and
  generated meshes now use a per-entity GPU buffer cache keyed by
  `(id, geometryRevision, count)` instead of one shared VBO re-uploaded on every
  entity switch. The cached world-triangle gather, local-space entity triangles
  + tree, and the entity broadphase for `resolveEntityContacts` are still open.

### Still open / next

- WA: actor-vs-entity rounded shell without breaking carry; deep-depenetration
  pass for simultaneous floor+wall corner contacts; edge jitter with a wall
  partly inside the floor.
- WD: cached entity-vs-world triangle gather; local-space entity triangles +
  tree (refit, do not rebuild on move); entity broadphase for the O(n^2)
  pair pass; the 0.5 s shoot freeze still needs a profiling run.
- WC: confirm fragments collide/replicate correctly on two clients.
- Targeted perf run (`perf_top` / `perf_file_logging`) to confirm no frame over
  4 ms render with ~10 holey crates.

### Proof

Human playtest: pending. Automated: existing
`--moving-crate-selftest` / `--destructible-selftest` /
`--destruction-replication-selftest` / `--destruction-stress-selftest` are
extended as fixes land. Evidence recorded in the linked changelog.

### Solution

Not yet. Status remains `ATTEMPTED FIX (6)` until the correctness,
settling, fragment, and performance issues are human-confirmed fixed.

---

## Regression Occurrence 2 (Attempt 6, 2026-10-01 14:37 EDT)

### Observed (new human playtest)

- Hole cutting still improved from the previous pass (the chain of fixes
  below). Crates move better; kicking a holey crate is less costly to render
  than before.
- Still large FPS drops when shooting holes, and especially when a crate with
  holes is **moving or settling** (10 fps, physics 20-80 ms). Standing ~10 m
  from a settled holey crate at 70 fps average with physics spikes 5-10 ms.
- `perf_report` showed only ~70 fps while the game felt like 1 fps; the
  per-subsystem numbers did not add up.
- Fragments work but a fragment "disappears and falls through the floor".
- The crate goes into a wall (sphere + wall + floor) and can get stuck; crate
  on a cylinder bounces/jitters and will not settle.
- Running into a crate with the projectile-rifle weapon flung it away; weapon
  hitboxes are not on the same collision path as the player body.

### Confirmed causes (evidence)

1. **Perf numbers were not real.** On-screen FPS = `FramePacer` previous-frame
   wall time averaged over 120 frames (`frame-pacer.cpp:37,167`), and the graph
   clamps every sample at 20 ms; `PerfTimes.physics` double-counted
   (`"Simulation"`+`"Physics"`), `PerfTimes.rendering` was never written, and
   `advanceKinematics` was not summed into `PerfTimes` at all.
2. **Nothing performance-related reached `events.jsonl`**: `performance.level`
   was `off` and the events carried only text, no numeric fields.
3. **Holey-crate physics cost**: `advanceKinematics` runs up to 5 substeps x 3
   passes per frame, each gathering world triangles uncached and scanning every
   candidate for every body triangle with `worldTree=nullptr`.
4. **`cachedEntitySurface` rebuild storm**: exact `transform ==` key, so every
   moving/jittering body rebuilds all world triangles + the full AabbTree per
   query (actor solver per iteration, pair pass, projectile queries).
5. **`resolveEntityContacts`** is O(n^2) with 2x narrowphase per overlapping
   pair every substep (fragment piles).
6. **Fragments fall through the floor**: no deep-depenetration; detection is
   only triangle crossing/containment or the 0.1 m rounded shell, and a body
   with near-zero velocity is skipped before any contact collection.
7. **Weapon collisions** use the legacy sphere/capsule group injected into the
   actor manifold; the weapon triangle mesh is excluded for configured weapons.

### Attempt 6 changes (this pass)

- **Perf truth + event logging (P0).**
  - `Perf::ScopedTimer("Rendering")` around the render stage and
    `Perf::ScopedTimer("PhysicsEntities")` around `advanceKinematics`, so the
    overlay reports real render/entity-physics ms.
  - New `PerfTimes` fields `entityPhysics`, `destruction`, `simulation`;
    `"Simulation"` no longer double-counts `"Physics"`.
  - `PERFORMANCE_FRAME` now carries numeric `fields` (`fps`, `frame_ms`,
    `physics_ms`, `entity_physics_ms`, `simulation_ms`, `rendering_ms`,
    `networking_ms`, `combat_ms`, `npcs`, `effects`, `projectiles`, `allocs`)
    and is emitted at `Important`; `config/debuglogger.json` `performance` is
    now `important`, so these land in `events.jsonl`.
- **World-triangle pruning (P1, partial).** The entity-vs-world sweep now
  indexes each collision pass's gathered candidates into an `AabbTree` and
  passes it to `collectActorMeshContactsInto`, so each body triangle only tests
  the world triangles it can touch instead of scanning the whole candidate list.
  This is the documented accelerated path with identical semantics; all four
  self-tests still pass.

### What got better / worse this pass

- Better: the perf report now shows the real physics/render cost, and
  `events.jsonl` records fps and per-subsystem ms (searchable).
- Better: holey-crate body-vs-world narrowphase is pruned by a tree.
- Not yet changed: the full local-space entity surface refactor, the entity
  broadphase, deep-depenetration, and the config-triangle weapon hitboxes. The
  observed phasing/sticking, fragment-through-floor, and settling-on-cylinder
  issues remain open.

### Still open / next (priority order)

1. **Local-space entity surfaces** (local triangles + local tree, transform the
   query) to stop `cachedEntitySurface` rebuilding world triangles + tree on
   every move; still gated on the user's "full refactor" decision.
2. **Entity broadphase** for `resolveEntityContacts` (replace the O(n^2) pair
   loop) and skip sleeping/static pairs.
3. **Deep-depenetration pass** with a non-zero recovery margin to stop
   tunneling/phasing/sticking and the fragment-through-floor.
4. **Weapon hitboxes as config triangles** (`weaponcollisions.json` boxes/
   capsules/spheres tessellated; append the weapon `ActorCollisionMesh`; gate
   off the sphere injection; align the config and model transforms).
5. **Settle-on-cylinder** and edge jitter (same solver margin/depenetration
   work).
6. Confirm no frame over 4 ms with ~10 holey crates using the new numeric
   `events.jsonl` records.

### Proof (Attempt 6)

- Build: `python build.py build-only` -> success.
- `--destructible-selftest`, `--moving-crate-selftest`,
  `--destruction-replication-selftest`, `--destruction-stress-selftest` PASS.
- Perf logging: numeric `PERFORMANCE_FRAME` records now emitted to
  `events.jsonl` when the performance category is `important`.

---

## Regression Occurrence 3 (Attempt 7, 2026-10-01 15:28 EDT)

### Observed (from the user's own logs, no playtest needed)

The user pointed at `logs/10-01-2026/20261001_152118/events.jsonl`. Analysis:

- `events.jsonl` for a ~1.5 minute run was **74 MB**; the previous runs were
  **2.2 GB** and **4.1 GB**. In `20261001_152118`, **99,715 of ~112k** records
  were `COLLISION` events.
- The spill came from `src/physics/movement/actor-triangle-solver.cpp`
  (`collision.contact.before_response`, `collision.contact.after_response`,
  `collision.solve.summary`, all with `correlation_id:
  "slope-edge-investigation"`), emitted **per contact per solve iteration**, and
  `config/debuglogger.json` had `collision.level: "trace"`.
- The numeric `PERFORMANCE_FRAME` records now work: e.g. a bad frame was
  `frame_ms=42.4`, `simulation_ms=13.5`, `rendering_ms=1.2`,
  `networking_ms=0.56`, `entity_physics_ms≈0`. The named subsystems did **not**
  sum to the frame, confirming the earlier finding that the breakdown is
  incomplete (unaccounted = `MIMITA_PERF_SCOPE` stages, swap/sleep, and the
  collision logging + JSON construction itself).

### Confirmed cause (new, high impact)

Per-contact JSON construction + file write inside the fixed tick. The
`nlohmann::json contactFields` object was built **unconditionally** for every
contact (the logger's early-out happened only after construction), and with the
collision category at `trace` every record was also serialized to disk. This is
a large per-tick CPU + disk cost and is a plausible dominant contributor to the
felt FPS drops and spikes.

### Attempt 7 changes

- `src/physics/movement/actor-triangle-solver.cpp`: the per-contact
  `before_response`/`after_response` JSON and the `solve.summary` JSON are now
  built **only when the collision category is enabled**
  (`shouldLog(Collision, Trace/Verbose)`); the fixed tick no longer pays for
  disabled diagnostics.
- `config/debuglogger.json`: `collision.level` `trace` -> `off` (the
  slope-edge investigation is complete; re-enable to debug collisions).
- `src/physics/physical-entity.cpp` + `physical-entity.h` + `game-cli.cpp`:
  new headless **`--physical-perf-selftest`** — builds a floor world, spawns 10
  crates each riddled with 48 holes (43,340 collision triangles), settles them,
  and measures a fixed tick. This is a perf guard runnable without a window.
  Measured: **~1.2-1.4 ms per entity fixed tick** for 10 holey crates
  (stable across 3 runs), PASS under the 4 ms budget.

### What got better / worse

- Better: the fixed tick no longer constructs/writes collision debug JSON;
  `events.jsonl` volume should drop by ~90% in normal play.
- Better: a repeatable headless measurement of the entity/destruction physics
  cost exists now.
- New evidence: entity physics for 10 holey crates is ~1.3 ms/tick, so the
  user's large frame spikes were likely the logging + actor/boolean/unaccounted
  work, not the entity sweep alone. This lowers the urgency of the full
  local-space refactor relative to fixing the logging.

### Still open / next

- Confirm in a real run that `events.jsonl` volume and the FPS drops are gone
  (the performance category should now be the only heavy logger; `network` is
  still `verbose` and can be lowered if needed).
- Full local-space entity surface refactor, entity broadphase, deep
  depenetration, and config-triangle weapon hitboxes remain (Attempt 6 list).

### Proof (Attempt 7)

- `--physical-perf-selftest` PASS, ~1.3 ms/tick, 3/3 runs.
- `--destructible-selftest`, `--moving-crate-selftest`,
  `--destruction-replication-selftest`, `--destruction-stress-selftest` PASS.

---

## Regression Occurrence 4 (Attempt 8, 2026-10-01 16:02 EDT)

### Observed (user's plan doc + log `20261001_153220`)

- Crates still get stuck in geometry; big FPS drops when a holey crate moves or
  settles; shooting holes still drops FPS.
- `perf_report` says physics 10-13 ms but fps is 6 and frame 150-200 ms — the
  report is not accurate.
- Settling looks special-coded and stops nearly all velocity; density >1 never
  settles flush; a crate with no holes "does not settle, keeps moving around".
- Crate loses too much speed when it bounces.
- Requested: freeze an object that has not moved > ~0.1 m in N ticks, distances
  configurable in `config/destructible-world.json`; remove the special settling;
  expose restitution/friction.

### Confirmed causes

1. **The profiler was logging every frame.** `PERFORMANCE_FRAME` is gated on
   `deepProfiling` (`perf.cpp`), and `perf_report` turns `deepProfiling` on. The
   153220 log had **280,871 PERFORMANCE_FRAME** rows (plus 794 spikes) in
   **259 MB**. The profiler cost more than it measured and inflated the reported
   cpu/frame ms — this is why the numbers disagreed with the felt FPS.
2. **Special settling**: `advanceKinematics` multiplied velocity by
   `supportedFrictionRetain`, applied an artificial righting torque, and scaled
   angular velocity by 0.55 while supported; sleep also required
   `isRestingOnFace` (upright within 0.985). Density >1 therefore never settled
   flush and the motion stopped unnaturally.
3. **Moving-hole-crate collision cost** was not previously measurable: the new
   perf probe now shows **settled = 0.026 ms/tick but moving = ~21.6 ms/tick**
   for 10 crates x 48 holes (43,340 triangles). The single-pass body-vs-world
   narrowphase transforms and tests every body triangle per tick.

### Attempt 8 changes

- **Profiler cadence (P0).** `PERFORMANCE_FRAME` is now aggregated over ~1 s and
  emitted **once per second** (avg + max for frame/physics/entity/simulation/
  rendering/networking/destruction, plus max npcs/effects/draw calls), never per
  frame and no longer triggered by `deepProfiling`. The spike report is
  rate-limited to once per second. `network` category lowered to `important`.
  The overlay now shows `MAX` frame + per-subsystem worsts and an
  `UNACCOUNTED` line (frame minus named timers) so an incomplete breakdown is
  visible, reset when `perf_report` opens.
- **Settling redesign (P1).** Removed the artificial righting/friction/angular
  kill and the `isRestingOnFace` sleep gate. Added `updateSettling`: if the body
  stays within `sleepMoveThresholdMeters` of an anchor for `sleepRequiredTicks`
  fixed ticks it freezes (zero velocity, `sleeping=true`) until disturbed; it
  never freezes while penetrating (>0.05 m). Waking (impulse/contact/push) resets
  the anchor. Config in `config/destructible-world.json` `physics`:
  `sleepMoveThresholdMeters` 0.1, `sleepRequiredTicks` 20. This made settled
  crates effectively free (0.026 ms/tick) in the probe.
- **Config expansion (P1).** Added `objectRestitution` (0.1) and `objectFriction`
  (0.6) to the JSON; new dynamic entities take restitution/friction from it.
- **Destruction timing + impact log (P3/P5).** `Perf::ScopedTimer("Destruction")`
  wraps `flushPendingCuts`, and `ImpactSystem::submit` emits a throttled
  `PROJECTILE_IMPACT` event (entity, mass, speed, energy, impulse, velocity and
  angular before/after) to `events.jsonl`; `physics` category is now
  `important`.
- **Perf probe (P2 target).** `--physical-perf-selftest` now measures both a
  settled and a moving phase and reports `TARGET(4ms) moving: MET/MISS`.

### What got better / worse

- Better: the profiler no longer costs frames; the report now shows max and
  unaccounted; settled objects are free; settling is natural + configurable;
  the moving-crate collision cost is now quantified (21.6 ms/tick for 10). 
- Worse/unchanged: moving holey crates are still ~21.6 ms/tick; stuck-in-geometry
  and fragment-through-floor remain (deep depenetration not done); the local-space
  entity surface refactor and entity broadphase are still not done.

### Still open / next (priority)

1. **Local-space entity surfaces + entity broadphase** to cut the moving
   ~21.6 ms/tick (the measured target). Body triangles must be pruned by a local
   tree against the world candidates, not transformed/scanned wholesale.
2. **Deep-depenetration pass** to stop stuck-in-geometry and fragment-through-
   floor.
3. Config-triangle weapon hitboxes; two-client verification.

### Proof (Attempt 8)

- `--physical-perf-selftest` PASS (settled 0.026 ms/tick; moving 21.6 ms/tick
  reported as TARGET MISS).
- `--destructible-selftest`, `--moving-crate-selftest`,
  `--destruction-replication-selftest`, `--destruction-stress-selftest` PASS.

---

## Regression Occurrence 5 (Attempt 9, 2026-10-01 17:03 EDT)

### Observed (user playtest + log `20261001_163224`)

- Logging fix confirmed: that run's `events.jsonl` is **1.2 MB** (vs 259 MB /
  2.2 GB before); `COLLISION=0`, `PERFORMANCE=2061`, `NETWORK=1`, `PHYSICS=17`.
- The 1-second aggregated `PERFORMANCE_FRAME` windows showed `max_entity_physics`
  ≈ `max_destruction` up to **300-380 ms** in a single tick, while actor
  `physics` ≤ 12.5 ms and `rendering` ≤ 2.8 ms. **The FPS dips when shooting are
  the boolean destruction rebuild**, not render or the moving sweep.
- Still: shooting crates drops FPS; touching a holey crate drops to ~5 fps;
  crate on a cylinder rotates/bounces and never settles; 75%-cut crate collides
  as if it still has a corner; player phases into a crate in a weird corner;
  `perf_report` text is clipped and `UNACCOUNTED` goes negative; jitter settling
  fixed (good); no-special-settle fixed (good).

### Confirmed cause

`boolean-mesh.cpp fillResult` called `difference.Decompose()` on **every**
rebuild (splits shells, `Volume()` each), `circularSegmentsForRadius` emitted up
to ~20+ segment cutters (~N^2 triangles per hole), there was no `Simplify`, the
triangle cap was 120000, and `maxCutsPerEntityPerTick` was 8 — so a single cut
could add hundreds of triangles and a burst produced a 300-380 ms rebuild. The
2 ms budget is checked after an entity, so one rebuild blows it.

### Attempt 9 changes (the biggest FPS win so far)

- **No `Decompose()` per rebuild.** `fillResult` now reports shell/component = 1;
  `DestructibleGeometrySystem::rebuild` computes a cheap connected-component
  count (`countSurfaceComponents`, vertex-weld + union-find over the output
  triangles) for the fracture trigger. The real shell meshes are produced only
  when a fracture actually happens (`booleanDecomposePieces`).
- **Aggressive simplification.** `circularSegmentsForRadius` now targets a large
  edge length and clamps to `[4, 8]` (a low-poly, icosahedron-like rim). Added
  `booleanSubtractIncremental(..., maxTriangles, simplifyTolerance)` which calls
  `Manifold::Simplify(tolerance)` after the subtraction (config
  `meshSimplifyTolerance`, 0.02).
- **Triangle cap 4096, 1 cut per flush.** `config/destructible-world.json`
  `maxTrianglesPerEntity` 120000 -> **4096**, `maxCutsPerEntityPerTick` 8 -> **1**.
- **Over-budget is no longer destructive.** The wrapper rejects an over-cap cut
  by restoring its pre-cut running solid and NOT committing; `discardPending`
  keeps the session on `ResultTooLarge` so the next cut stays incremental instead
  of replaying the whole history (which was a frame spike).
- **Object response config.** Added `objectMinBounceSpeed` (1.0; below it objects
  project and settle instead of bouncing) and `objectMaxSpeed` (60; speed cap).
  Applied in world and pair contact response and the integration step.
- **perf_report polish.** `UNACCOUNTED` clamped at 0 (named timers overlap).
- Sensitivity: unbalanced-support `comOffsetFraction` 0.12 -> 0.08 (the coarse
  cutter removes slightly less mass; the trigger is still gated on component==1).

### Measured effect (`--physical-perf-selftest`, 10 crates x 48 holes)

- Boolean build: **1400 ms -> ~230-290 ms** for 480 cuts (~0.6 ms/cut).
- Triangles: 43,340 -> **6,560** (656/crate for 48 holes).
- Moving holey crates: **21.6 -> 2.6-3.1 ms/tick** — TARGET(4ms) **MET**.
- Settled crates: 0.024 ms/tick.

### Still open / next

1. Player phasing into a crate in odd corners (deep depenetration / rounded
   actor-vs-entity).
2. Heavily-cut crate COM/support: confirm the removed corner is gone from
   `localTriangles` and that the freeze rule does not freeze an unstable pose.
3. Cylinder contact response after the new min-bounce/speed settings — retest.
4. Local-space entity surfaces + entity broadphase (moving is now under budget,
   but fewer triangles per body still helps).
5. Config-triangle weapon hitboxes; overlay word wrap; two-client verification.

### Proof (Attempt 9)

- `--destructible-selftest`, `--moving-crate-selftest`,
  `--destruction-replication-selftest`, `--destruction-stress-selftest` PASS.
- `--physical-perf-selftest` PASS; moving 2.9 ms/tick (TARGET MET).
