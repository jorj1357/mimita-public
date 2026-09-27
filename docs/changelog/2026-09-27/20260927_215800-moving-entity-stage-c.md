// 2026-09-27T21:58:00Z
/* purpose
* record Stage C of the unified collision/contact migration: one generic moving
* physical entity, entity contacts folded into the canonical actor manifold, and
* moving-support carry plus velocity inheritance
* separate build, deterministic-test, and runtime evidence from human review
* does NOT claim runtime gameplay acceptance or any network/protocol change
* does NOT implement vehicles, destruction, ragdolls, or NPC/server entities
*/

# Task

- Summary: Add `PhysicalEntity` / `PhysicalEntitySystem`, let moving entities
  participate in the one actor-triangle manifold with surface velocity, and
  prove support carry, walking, jump inheritance, and departure without velocity
  loss in a deterministic moving-crate fixture.
- Status: CODE_COMPLETE / DEV_BUILD_VERIFIED / DETERMINISTIC_TEST_PASS /
  HUMAN_REVIEW_REQUIRED
- Date, time, timezone: 2026-09-27T21:58:00Z, ISO 8601 UTC
- Branch: `afad20a-rebuild`
- Commits: Stage A and the concurrent Phase 7 were committed by the user as
  `696f79ee`; this Stage C delta is in the working tree.

# Pre-existing / concurrent changes (not mine)

- The user committed the concurrent session's Phase 6/7 (opt-in
  `actorTriangleSolver` toggle, `runActorTriangleCollisionStep`) together with
  this session's Stage A as `696f79ee`.
- Stage C builds on that committed solver; it does not rework or revert the
  Phase 7 opt-in gate.
- `config/accounts/default.json` and `config/analytics.json` remain others'
  edits; untouched.

# Source changes

## Added

- `src/physics/physical-entity.h` / `physical-entity.cpp` — `PhysicalEntity`,
  `PhysicalEntitySystem`, `buildBoxCollisionTriangles`,
  `collectActorEntityContacts`, `advanceKinematics`, `drawPhysicalEntities`, and
  `physicalEntitySelfTest`.
- `src/terminal/crate-commands.{h,cpp}` — `crate_spawn` and `crate_clear`,
  registered from `main-systems.cpp`.
- `--moving-crate-selftest` in `game-cli.cpp`.

## Runtime command and rendering

- `crate_spawn [distance] [vx vy vz]` spawns a 0.5 m-half box `Kinematic` entity
  `distance` metres (default 5) along the local player's camera look direction;
  `crate_clear` removes all entities.
- `PhysicalEntitySystem::advanceKinematics(dt)` is called each frame from
  `engine-tick-combat.cpp` so constant-velocity crates move.
- `drawPhysicalEntities(camera)` is called from `engine-tick-render.cpp` and
  draws each entity as a filled box plus wire outline through the always-on
  production triangle flush (`DebugVis::drawFilledBox` / `drawWireBox`), so a
  spawned crate is visible without enabling debug visuals.
- Collision still only occurs when `actorTriangleSolver` is enabled; the command
  logs a reminder when the toggle is off.

## Modified

- `src/physics/movement/physics-collision.h` — `RecoveryContact` carries
  `entityId` and `surfaceVelocity` (appended; existing initializers unaffected).
- `src/physics/movement/actor-triangle-solver.h` — `ActorWorldContact` carries
  `entityId` / `surfaceVelocity`; `solveActorTriangleCollision` takes an optional
  `const std::vector<PhysicalEntity>* entities = nullptr`.
- `src/physics/movement/actor-triangle-solver.cpp` — folds entity contacts into
  the manifold; `mergeContactsByNormal` never merges different entity ids;
  `runActorTriangleCollisionStep` applies moving-support carry and departure
  inheritance.
- `src/entities/player.h` — `CollisionState` gained `supportEntityId` /
  `supportVelocity`.

# Exact implementation changes

- Entity collision reuses `collectActorMeshContacts` through a temporary world
  view of the entity's world-space triangles, so there is no second triangle
  routine and contacts keep the canonical fields.
- Static-world behavior is unchanged because `solveActorTriangleCollision`'s
  entity argument defaults to null and the legacy default path is untouched.
- Carry: while grounded on a walkable entity contact, `pos += entity.velocity*dt`.
  Departure: when support ends, `vel += supportVelocity`. The entity's velocity is
  never written into `vel` while supported, so it cannot be silently dropped.

# Diagnostics

- No new logging this stage; the deterministic test reports each assertion.

# Validation

## Build

- `python build_agent.py` — `Status: SUCCESS` (exit 0), `mimita.exe` linked.
- `python build.py build-only` — `Nothing changed`.

## Deterministic self-tests (all PASS)

- `--moving-crate-selftest` — 12 checks: stays grounded on the moving crate;
  world motion includes crate velocity; contact identifies the support entity;
  support velocity exposed; walking stays supported; walking adds to support;
  settles on the crate; jump lifts the actor; jumping preserves inherited
  velocity; leaving clears support; leaving does not zero inherited velocity;
  inherited velocity remains.
- `--canonical-contact-selftest`, `--collision-selftest`,
  `--collision-subgrid-selftest`, `--actor-triangle-spike`,
  `--actor-collision-mesh-selftest`, `--actor-triangle-solve-selftest` — all PASS,
  unchanged.

# Human review still needed

- Runtime play: entities are not yet created or moved by any game mode; the proof
  is deterministic only. To observe in-game, a mode must add a
  `PhysicalEntitySystem` crate and enable `actorTriangleSolver`.
- Human play of the opt-in solver path (Phase 7) remains pending from the
  concurrent session's report.
- No network, damage, destruction, vehicle, ragdoll, or NPC/server entity change
  was made or verified.

# Unchanged and protected

- Default `doCollisions` / legacy pipeline behavior is unchanged.
- `config/hitfx.json` and `src/effects/hit-effects*` untouched.

# Next stage

- Wire entity creation/movement into a mode (map or sandbox) so the carry can be
  observed in-game; then extend the contract to NPC/server actors and rigid
  bodies (ragdolls), followed by destructibles, explosions, and vehicles.
