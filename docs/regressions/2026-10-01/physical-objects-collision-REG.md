# Physical Objects Collision and Settling

Time created: 2026-10-01T17:37:13Z
Time last updated: 2026-10-01T19:18:40Z

Status: ATTEMPTED FIX (6)

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
