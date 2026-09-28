# Phase 2 deletion debt and Phase 3 physical-object audit

- Date: 2026-09-28
- Scope: mark completed Phase 2 behavior and inspect the Phase 3 crate foundation
- Status: first Phase 3 runtime slice implemented; later persistence/network work remains

## Phase 2 debt markers

- Added dated `TODO-DELETE` comments for the per-iteration actor mesh copy.
- Added dated `TODO-DELETE` comments for the temporary `World` and triangle
  copy used by moving-entity contacts.
- Phase 1 and Phase 2 are treated as behavior-complete for the current project
  milestone; these are later cleanup/performance-debt items, not blockers for
  beginning Phase 3.

## Phase 3 audit

- `PhysicalEntitySystem` is the correct starting owner for persistent world
  objects, but its record is currently runtime-only and lacks persistence,
  render asset, mass/friction, health/destruction, ownership, and network data.
- `crate_spawn` currently creates a half-meter kinematic box and
  `drawPhysicalEntities` renders a colored debug box, not a textured model.
- Collision/support/carry are proven in deterministic tests, but player push,
  save/load, damage, destruction, server authority, and client interpolation
  are not implemented.
- `PersistentPhysicsSystem` currently owns grenade/projectile objects and is a
  separate owner; it should not be merged into crates without a contract audit.

## First Phase 3 slice

- Expanded `PhysicalEntity` into the shared runtime object record with stable
  persistence/ownership fields, transform and angular velocity, shape and
  dimensions, render asset paths, mass/friction, strength/health, destructible
  state, and sleeping state.
- Changed `crate_spawn` to create a 5 m wide box (`halfExtents = 2.5 m`) and
  assign `assets/textureshq/clouds11.png` to the generated textured-box
  renderer.
- Moved kinematic physical-object stepping behind a fixed 60 Hz accumulator
  and added swept world-triangle contacts with correction and velocity sliding.
- Kept save/load, crate-to-crate contacts, shooting/damage, server snapshots,
  and client interpolation out of this slice so they can be added behind the
  same owner rather than creating a second crate system.
- Follow-up fix: crates are now Dynamic, receive shared gravity, stop inward
  floor velocity, sleep when settled, and receive a mass-scaled horizontal
  player push after the actor manifold is solved.
- Corrected generated-box collision/render winding and explicitly restores
  back-face culling around the textured crate draw.
- Physical-object world contacts now opt into the existing rounded feature
  manifold: triangle faces remain solid while points behave as spheres and
  edges as capsules at the configured movement feature radius.

## Evidence

- Build: `mimita.exe` linked successfully; 6 changed translation units compiled.
- Deterministic checks: `--moving-crate-selftest`,
  `--actor-triangle-solve-selftest`, and `--collision-selftest` passed.
- Runtime visual acceptance of the textured crate and live pushing was not
  performed in this session.

## Rigid-body follow-up

- Added arbitrary quaternion orientation, center of mass, box inertia and
  inverse inertia to the same `PhysicalEntity` record.
- Player pushes now apply an impulse at the nearest crate contact point, so
  off-center pushes produce angular velocity and subsequent orientation change
  instead of an arbitrary scripted rotation.
- Replaced immediate settling with stable-contact plus linear/angular velocity
  thresholds over 45 fixed ticks; `crate_sleep` and `crate_wake` provide debug
  overrides.
- Added `crate_info`, `crate_mass [kg]`, and `crate_density [kg/m3]`. Mass and
  density are kept mathematically consistent through the crate volume.
- Added deterministic coverage for falling, settling, off-center angular
  impulse, and orientation change. The build and all focused collision tests
  pass.

## Crate stability follow-up

- World contacts now apply a contact-point normal impulse plus friction impulse,
  then enforce the same no-inward-velocity invariant used by the player solver.
- Increased crate air/angular damping, added an angular-speed guard, and added
  static-contact damping so resting crates stop rolling instead of spinning in
  place.
- Added up to three fixed-tick recovery/contact passes per object step so a
  rotated crate is corrected out of the world instead of remaining embedded.
- Local contact response remains immediate on the client in the current local
  simulation. The branch has no physical-object snapshot packet owner yet, so
  server prediction/reconciliation was not fabricated in this pass.

## Density and resting follow-up

- Made density the crate material authority: spawn initializes mass from
  `density * volume`, and `crate_mass` converts back to density instead of
  creating a second inconsistent mass rule. Density is finite and bounded while
  still allowing `999999` to behave as effectively immovable at crate scale.
- Player pushes now use player-side momentum rather than multiplying the push
  by crate mass, so high-density crates move much less while light crates move
  readily. A held movement input can wake and push a crate even when the actor
  velocity was projected to zero by the contact manifold.
- Dynamic-entity contacts now carry mass/restitution into the actor response,
  allowing a dense crate to return more of the player's normal momentum while
  a lighter crate absorbs more of the interaction.
- Added a center-of-mass righting torque and an upright requirement for sleep;
  edge-balanced boxes stay active and rotate toward a supported face. Added a
  small multi-contact escape bias for wall/corner traps.

## Pair contacts and smooth presentation follow-up

- Added fixed-tick dynamic physical-entity pair contacts. Crates now reject
  overlap, exchange normal momentum using inverse mass, and use each crate's
  restitution for crate-to-crate bounce.
- Moved the player push impulse onto the actual actor/entity contact before the
  actor velocity projection. The old post-solve AABB fallback remains only as
  a safety path, preventing a legitimate touch from producing zero impulse.
- Added `crate_bounce [0..2]`; `0.35` is ordinary restitution, `1` returns the
  incoming normal speed, and `2` intentionally adds energy as requested.
- Rendered physical crates interpolate between the previous and current fixed
  poses using the existing fixed-step accumulator, so authoritative collision
  correction is not displayed as a one-tick visual snap.
- Added deterministic coverage for two dynamic crates exchanging momentum and
  separating instead of remaining overlapped.
- Tuned generated crates to right themselves faster: lower angular damping,
  stronger resting righting torque, less aggressive support damping, and an
  18-tick stable-rest requirement instead of 45 ticks.
