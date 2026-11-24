# Physics-only settling attempt, jump fix, orientation telemetry

**Date:** 2026-11-24 UTC
**Agent:** vehicle-settle-physics
**Issue:** `issue-e5a086b0a441`

## Purpose

Remove the scripted settling behaviour and let a car settle through contact
physics, as requested. Also repair the jump, which had stopped leaving the
ground, and make orientation and support observable in the journal.

## What was removed

- The custom righting block in `updateArcadeControls` (align torque plus
  downforce toward the wheel contact normal).
- The pose-based idle sleep rule (`no input && slow -> sleeping`).
- The template idle-settle velocity kill (`idleSettleHorizontal/Vertical/Angular`
  hard multipliers). This was both the source of residual sway and the reason
  the jump never left the ground: it multiplied `velocity.z` by 0.05 on the same
  tick the jump impulse was applied.

## What was added

- `VehicleWheelSpec::collisionRadius`: optional collision sphere at the hub,
  appended to the chassis triangles. Supported, currently 0 in `octane_rl`.
- `VehicleSpec::collisionHalfExtents` plus `collisionLength/Width/Height`: a
  collision slab independent of the rendered body, so wheel colliders can be the
  lowest and widest points. Supported, currently unused by `octane_rl`.
- `MapVehicleSpawn.pitch` / `roll`: authored spawn tilt. Used by the map editor
  and by settling tests.
- Orientation and support tracking: `upDotWorldUp`, `wheelMask`, `wasSleeping`,
  sampled in `vehicle.telemetry` and reported by a new `vehicle.rest` event.
- Continuous support damping now applies to any pose with at least two wheel
  contacts (`stabilityAngularDamping`), not only a four-wheel stance.
- Jump-reset pad fix: the ground reset no longer clears the jump while the jump
  force is still running, so a held jump keeps its sustained force.
- Jump apex reporting fix: the landing tick no longer overwrites the apex with
  zero before it is reported.

## Measured results

Build: game generation **606**, DLL
`build/game/generation-606/game-g606.dll`,
sha256 `1df1802b94277bb9a922bc7ec7058cd91104bd1297ecca648ac80c99120928e7`.

**Settling does not work yet.** A car spawned at 80° of roll
(`settle-proof-4`, `20261124_192428_settle-proof-4`) was pushed from
`up_dot = 0.329` (70°) to `0.919` (23°) by differential suspension force and then
stayed there for the rest of the run, with no `vehicle.rest` event. With wheel
colliders enabled (`settle-proof-3`, `20261124_192336_settle-proof-3`) the car
vibrated instead: 42 airborne samples in 35 s, and a jump impulse was absorbed on
the contact tick so the car never left the ground.

Root cause, from reading the solver: entity-versus-world contacts are resolved by
translating the body, not by applying an impulse at the contact point. With no
contact torque, a wheel collider cannot roll the car back onto its wheels, and a
flat chassis face remains a stable resting pose. This is the solver gap that was
predicted before implementation; it is not a tuning problem. The collider support
is therefore implemented but disabled, and the finding is recorded in
`docs/specs/vehicles/vehicles.md` under the constraint-solver deferral.

**Jump works again.** `jump-proof` (`20261124_193047_jump-proof`), generation 606:
one `vehicle.jump`, 6 airborne telemetry samples, `vehicle.jump.apex`
`apex_height_m = 1.672`, `air_time_seconds = 3.0` (the window includes drift after
the apex). Earlier, with the idle-settle kill present, the same input produced
zero airborne samples.

## Not done / open

- Apex is 1.67 m against the Rocket League reference ≈ 2.6 m. The immediate
  impulse is correct; the sustained portion is still delivering less than the
  full 0.2 s of 14.58 m/s². Needs a per-tick trace of `jumpTimer`, `jumpHeld`,
  and vertical velocity.
- Settling on wheels requires contact torque at entity-versus-world contacts
  plus a simultaneous-contact solve. That is the next batch, together with the
  pinch convergence harness.
- `settle.stickyForceGravityScale`, `autoFlip*` and `autoRoll*` profile fields are
  now unused. They are left in place for the solver batch and should be removed
  or implemented then.
- The visual arena mesh is still the old 60 × 41 m `arena1.glb`.

## Ball contact defect: cancelled aggregate normal (fix written, not built)

The car drove 96.68 m through the arena centre with no `arena.ball.contact` and no
`arena.ball.extra_impulse`. `resolveEntityContacts` aggregates a pair's manifold
into one representative contact by summing contact normals, then
`resolveEntityPairContact` normalises that sum. For a flat box-on-box pair the
sum is coherent, but a sphere touches through normals pointing in many
directions that can cancel: the summed normal comes out near zero, the pair is
treated as separating, and the ball receives nothing.

Fix written in `src/physics/physical-entity.cpp`: when the summed normal is weak
relative to the contact count (`|sum| < 0.25 * count`), fall back to the
centre-to-centre direction, which is the correct normal for a sphere pair, and
use the deepest contact point. Two bounded diagnostics were added alongside it:
`arena.ball.narrowphase.miss` (bounds overlap, no contacts) and
`arena.ball.state` (ball eligibility: motion, sleeping, flags, triangle count).

**Not built.** Generation 623 contains the first diagnostic but not the fix; the
next build request failed in `src/main-systems.cpp` — `weaponsPtr`, `cameraPtr`,
`playerPtr`, `npcSystemPtr` are not members of `mimita::game` — which is another
agent's in-progress edit. Signalled to `codex-networking-runtime-proof` and
`melee-parity`. Generation 623 remains current.


## Risk

Deleting the idle-settle kill removes the previous "car stops fidgeting" hack.
Until the contact-torque solver lands, a parked car may keep a small suspension
oscillation. This is visible in the journal as a non-zero `speed_mps` at rest and
is preferable to a hidden velocity kill that also disables jumps.

## Follow-up: accumulated-impulse contact solver (linked, generation 618)

Implemented in `src/physics/physical-entity.cpp`:

- `solveEntityWorldContacts` clusters world contacts that describe the same
  physical contact (similar normal within 20 degrees, point within 12 cm, capped
  at 8 clusters per body), builds a tangent basis and effective masses, then runs
  8 sequential-impulse iterations over accumulated normal and two-axis friction
  impulses, clamped by `mu * accumulatedNormal`. Restitution is a velocity target
  and only applies above the configured minimum bounce speed.
- `resolveWorldContactVelocity` is deleted. Its wall-contact telemetry moved into
  the new solver, which now runs once per body per fixed tick instead of once per
  gathered contact. This removes the per-contact over-application that threw the
  car over in generation 608.

Wheel colliders and the narrow collision slab are now **enabled** in
`octane_rl.json` (`collisionRadius 0.085`, `collisionWidth 0.56`,
`collisionHeight 0.28`), with `bodyFriction 0.85` and
`stabilityAngularDamping 8.0`.

### Measured on generation 618

Run `20261124_204047_settle-final`:

- An upright car stays stable and returns to a stable stance after a jump:
  `up_dot = 1.000`, tilt `0.1 deg`, `speed 0.12 m/s`, 4 wheels in 126 of 129
  telemetry samples; `vehicle.jump.apex` `1.410` m.
- A car authored at 80 degrees of roll starts at `up_dot 0.039` and is rolled by
  contact physics to about `up_dot 0.86` (30 degrees), but it does not finish:
  it keeps rolling on its wheel spheres at about 1.6 m/s and produces no
  `vehicle.rest` event.

### Remaining gap: rolling resistance

A side-lying car is supported only by its wheel spheres, and its suspension rays
point sideways so `grounded` is 0. There is therefore no suspension damping and
no slip at the contact point, so Coulomb friction does nothing and the car rolls
like a ball. Settling it needs a physical **rolling resistance** term (oppose the
tangential angular velocity, scaled by the normal impulse and the tire's
`rollingResistance`), not a pose rule. That is the next change.

Also noted: `PhysicalEntity::lastWallContactNormal` still has a comment naming
the deleted `resolveWorldContactVelocity`; it was left alone because editing
`physical-entity.h` triggers a large dependent rebuild.



## Follow-up experiment: impulse-only world contacts (reverted, generation 608)

Attempted after the first pass and **reverted**. Entity-versus-world contacts
were made impulse-only by deleting the blanket linear correction, and wheel
colliders were re-enabled with a narrower/lower collision slab.

Build generation 608, run `20261124_193915_torque-proof`:

- the tilted car flipped to `up_dot = -0.767` (140 degrees) and never rested;
- the second car stayed airborne for all 124 samples;
- no `vehicle.rest` event was produced.

Cause: `resolveWorldContactVelocity` was already applying a contact-point impulse
with angular response, but `s_objectContacts` is a gathered list, so each entry
applied a full impulse. Removing the blanket correction exposed that
over-application and the body was thrown over instead of settled.

Reverted to the known-good world-contact rule and disabled the wheel colliders
again. Build generation **609**, run `20261124_195501_final-check` confirms the
restored state: upright car, jump apex `1.6715` m, 7 airborne samples, ending
upright (`up_dot = 0.980`).

The correct next step is a constrained contact solve (accumulated impulses per
contact, warm starting, several iterations) rather than more per-contact tuning.

