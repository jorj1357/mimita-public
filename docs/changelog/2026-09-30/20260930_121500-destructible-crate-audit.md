// 2026-09-30T12:15:00-04:00
/* purpose
* Record the investigation of the 20260930 destructible/moving-object plan.
* No gameplay source or configuration was changed in this session.
*/

# Task

- Summary: Compare `docs/specs/20260930plan.md` with the current moving-crate,
  projectile-rifle, and destructible-geometry implementation.
- Status: INVESTIGATION_COMPLETE / RUNTIME_VALIDATION_REQUIRED /
  IMPLEMENTATION_NOT_REQUESTED
- Branch: current working branch; pre-existing edits were present in
  `config/accounts/default.json`, `config/analytics.json`, and the untracked
  plan file.

# Findings

- A generic `PhysicalEntitySystem` owns dynamic crates in
  `src/physics/physical-entity.h/.cpp`. Crates use fixed 60 Hz gravity,
  collision response, angular velocity, inertia, player push, and sleeping.
- `src/impact/impact-system.cpp` computes kinetic energy as
  `0.5 * projectileMass * speed^2`, applies an impact-angle factor, derives a
  bounded spherical cut radius, stores the cut in entity-local space, and writes
  the generated low-poly surface back into the entity collision mesh.
- `src/combat/projectile-simulation.cpp` uses swept sphere-versus-triangle
  collision and has an `EntityImpact` result, so the current kernel has an
  anti-tunneling path for fast projectile-rifle shots.
- `src/network/server-projectiles.cpp` queries physical entities and submits a
  projectile-rifle entity impact on the authoritative server. The current
  server path is gated by `explodeOnWorldImpact` and only the projectile rifle
  submits a destructible cut; other weapons are intentionally deferred.
- `src/combat/client-collision-world-view.h/.cpp` has no physical-entity query,
  and `src/network/multiplayer-projectiles.cpp` predicts world/player impacts
  but not entity impacts. Therefore an immediate client-predicted crate hole is
  not implemented.
- No physical-entity cut/state replication packet or client reconciliation
  owner was found. The moving-object specification explicitly records entity
  networking and client prediction as later work. A server-side cut therefore
  cannot be treated as complete multiplayer-visible behavior yet.
- The current crate surface is deliberately planar/low-poly. One face's cuts
  are merged around the largest cut on that axis; separate holes on the same
  face are not yet represented independently. Arbitrary imported-mesh triangle
  cutting is not implemented.
- The configured projectile-rifle values are `projectile_speed: 500.0` m/s,
  `projectile_mass: 0.02` kg, and `projectile_density: 7800.0` kg/m^3.
  Density is currently metadata in the impact event; kinetic energy uses mass.
  The 5 m crate defaults to gameplay density `1.0` kg/m^3, giving it a 125 kg
  mass, rather than using the wood material density for rigid-body mass.
- The crate's slow tip is explained by `applyRestingRightingTorque` plus
  supported-contact damping in `src/physics/physical-entity.cpp`: angular
  damping runs every dynamic tick, and supported low-speed contact multiplies
  angular velocity by `0.55` after applying the righting torque. This is an
  explicit stability policy, not an unexplained physics failure.

# Validation and limitations

- Existing focused self-tests are present for moving-crate behavior and the
  destructible projectile sweep, but this session did not run a build or live
  game acceptance. The prior changelogs record those self-tests as passing while
  runtime visual/multiplayer acceptance remains open.
- No code, JSON, packet, or runtime state was changed by this investigation.

# Documents and skills reviewed

- `docs/ROUTER.md`
- `docs/specs/20260930plan.md`
- `docs/specs/destructible-world/destructible-world.md`
- `docs/specs/moving-physical-objects/moving-physical-objects.md`
- `docs/specs/weapons/weapons.md`
- `docs/architecture/collision/collision.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/efficiency-checker-v1.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- Relevant 2026-09-28 and 2026-09-29 crate/destructibility changelogs.

# Human review still needed

- Live test projectile-rifle hits against a spawned crate and inspect server
  and client logs separately.
- Confirm whether the desired first milestone is server-authoritative holes
  with replicated cuts or immediate local prediction plus reconciliation.
- Decide whether the low-poly planar hole approximation is acceptable before
  expanding to arbitrary imported meshes.
