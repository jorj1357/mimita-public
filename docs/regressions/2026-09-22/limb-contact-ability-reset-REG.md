# Limb contact and touch-reset movement regression

Time created: 2026-09-22T23:04:52Z
Time last updated: 2026-09-22T23:04:52Z

Status: UNRESOLVED

Related specification:
`docs/specs/movement/movement.md`

Related changelog:
`docs/changelog/2026-09-22/20260922_230452-limb-contact-movement-audit.md`

Reference comparison:
`afad20a` versus the current working tree at `3150587` plus its pre-existing
working-tree edits.

---

## Regression occurrence 1

### Observed

The human report is that, compared with `afad20a`:

- a standing player cannot repeatedly Left Shift dash from ordinary ground contact;
- a small WASD movement may make one dash possible, after which the next dash is unavailable;
- Q down-dash is not repeatable from ground contact and does not naturally bounce from repeated contacts;
- E freeze followed by release permits only one dash before contact/reset behavior stops matching the old feel;
- visible limbs can enter world objects without pushing the actor/root out.

This record is based on the report and source comparison. No runtime replay,
events.jsonl capture, or human confirmation of a fix was performed in this audit.

### Expected behavior

The movement specification says that “Touching literally anything restores
movement abilities,” that grounded dash is allowed, that down-dash works on the
ground and slopes, and that dash/down-dash activation is edge-triggered while
contact restores availability. All body parts are intended to participate in
world collision and affect the root position.

### Actual behavior

The current implementation has multiple new hot owners and ordering boundaries
between ability activation, collision, contact classification, and reset. The
reported symptoms are consistent with a contact not reaching the reset owner,
or with the reset occurring after the input edge has already been consumed.

### Exact differences from `afad20a`

1. `src/physics/movement/movement-step.cpp` gained hot event dispatch for dash,
   down-dash, and freeze. The old direct formulas were replaced by
   `LiveBehavior::dispatchGameplayEvent64(...)` and the hot policies in:
   `src/hot-reload/modules/movement-dash.cpp` and
   `src/hot-reload/modules/movement-freeze.cpp`.

2. New `movement.main` code in
   `src/hot-reload/modules/movement-system.cpp` now performs movement, calls
   `collision.main`, and then resets abilities only from `st.collided`.
   The old path consumed typed movement contacts through
   `consumeMovementContacts(...)`, where `contact.resetsAbilities` explicitly
   controlled the reset.

3. New `collision.main` code in
   `src/hot-reload/packages/collision/collision-package-solver.cpp` classifies
   `worldContact`, `bodyContact`, and `collided`, applies body pushes, and uses
   contact/ground hysteresis. This did not exist in `afad20a`.

4. The hot local player collider builder adds a helper capsule, weapon shapes,
   and six body-part socket samples. The body parts are marked
   `COLLISION_COLLIDER_BODY_AUTHORITATIVE`, so they are intended to push the
   root, but their transforms come from socket/bounds capabilities rather than
   the old `Player::physicalBody.parts` sweep path.

5. Current `config/collision.json` selects `behaviorSource: "json"` and
   `groundResponse: "bounce"`; `config/movement.json` selects
   `behaviorSource: "json"` with the `source` preset. These selectors and the
   hot JSON polling layer are not present in the old direct path.

6. Current hot dash is explicitly availability-gated:

```cpp
if (io.dashPressed != 0u && io.dashEnabled != 0u && io.dashAvailable != 0u)
```

   and down-dash is likewise gated by `io.downDashAvailable`. Repeating requires
   the post-solve contact path to call `restoreTouchAbilities(...)`.

7. Current local ordering is: freeze policy -> ground/air movement -> dash and
   down-dash -> jump -> gravity -> collision solve -> ability reset. Therefore a
   ground contact must be reported by the new solver in the same tick after the
   ability edge has been consumed. If `st.collided` is false for a resting
   capsule or limb, no reset occurs until a later qualifying contact.

### Wrong/current code implicated

`src/hot-reload/modules/movement-system.cpp`:

```cpp
resolveCollisions(ctx, &st, dt, e, tick);
rs.grounded = st.grounded;
const bool contactNow = st.collided != 0;
if (contactNow) {
    MimitaHotMovement::restoreTouchAbilities(rs);
}
```

`src/hot-reload/modules/movement-system.cpp` also supplies the hot collision
query with a capsule plus optional weapon and socket-derived body colliders;
the new path does not use the old `Player::physicalBody.parts` sweep producer.

`src/hot-reload/packages/collision/collision-package-solver.cpp` has a special
touching-only branch that records contact and cancels into-surface velocity but
does not depenetrate:

```cpp
if (c.touching) {
    ...
    continue;
}
```

That is valid for a resting contact only if the collider is correctly placed
and the contact is retained. It does not by itself prove that every visible
limb is swept or that a limb penetration moves the root.

### Confirmed cause

The confirmed source-level cause is architectural: behavior moved from one
cold movement/contact pipeline to a hot movement pipeline plus a new generic
collision package, and the touch-reset and limb-root-response contracts now
depend on the new solver's `collided/worldContact` output. The old working
behavior is not preserved by simply compiling the new path.

The specific runtime cause of each symptom remains UNRESOLVED until a focused
60-Hz trace joins input edge, ability availability, collider list, contact
classification, output position, and reset decision for the same entity/tick.

### Related issues to track

- Resting ground contact may be classified as `worldContact` by the solver but
  not reach the exact reset consumer if the hot movement branch is not the path
  applying the result.
- Body-part socket/bounds positions may differ from the visible animation
  transforms, allowing a rendered limb to enter an object even when a proxy
  sphere is elsewhere.
- Multiple body-part sphere samples are not equivalent to the old continuous
  body-part sweep; fast limb motion can tunnel or miss thin geometry.
- Ground bounce, ground settling, depenetration, and final velocity response
  are all ordered inside the new solver. A later clamp or movement step can
  erase the response even when a contact was found.
- Freeze, dash, and down-dash each have persistent availability plus held-edge
  state. Resetting availability and preserving the edge state must happen in a
  defined order or a held key can either fail to repeat or repeat incorrectly.
- Large movement changes were grouped into hot reload, animation, collision,
  JSON ownership, and ordering changes without a small manual acceptance slice.

### Recommended next falsification test

Run one local player at fixed 60 Hz against a flat floor, wall, ceiling, and a
thin obstacle. For each tick log: `entity`, `tick`, `dashEdge`,
`downDashEdge`, `freezeHeld`, availability before/after, every collider
`partId` and world position, `worldContact`, `bodyContact`, `grounded`,
`collided`, output position/velocity, and whether `restoreTouchAbilities` ran.
Then manually verify, one change at a time: standing Shift dash twice with a
release between presses; standing Q down-dash twice with a release between
presses; freeze/release/dash; and arm/leg/head contact against a wall.

Do not mark this resolved from build success or source inspection. The future
workflow should use small chunks, manually confirm each baseline behavior, and
only then continue to the next ordering or hot-reload change.
