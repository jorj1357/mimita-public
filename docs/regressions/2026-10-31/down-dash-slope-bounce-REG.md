# Down-dash onto a slope sometimes failed to bounce

Time created: 2026-10-31T11:50:24Z
Time last updated: 2026-10-31T11:50:24Z

Status: UNRESOLVED

Related specification:
`docs/specs/movement/movement.md`

Related architecture:
`docs/architecture/collision/collision.md`

Related changelog:
`docs/changelog/2026-10-31/20261031_140500-collision-stepup-and-scenario-evidence.md`

---

## Regression Occurrence 1

### Observed

Time: `2026-10-31T11:50:24Z` (human report)

The player reports that down-dashing onto slopes does not always produce the
expected bounce. The desired behavior is a reliable positive bounce from a
slope contact, using the existing configured bounce strength.

### Expected Behavior

Per `docs/specs/movement/movement.md`, down-dash against slopes intentionally
creates launches. A slope contact must not project the downward dash away and
leave the actor stuck or sliding without a positive normal response. The
existing configured bounce strength and speed cap should be used; the rule is
reliability, not a new full-energy reflection.

### Actual Behavior

Sometimes the down-dash contacts a slope without a reliable launch. The precise
contact order is not captured yet. A likely candidate is a floor/ground contact
responding before the slope contact, or a rounded feature normal replacing the
authored slope face normal.

### Why This Is Bad

It makes a core movement interaction feel inconsistent and can turn a slope
launch into a stop or slide, which is exactly the kind of collision failure the
movement specification says must not be removed without proving a bug.

### Specification

`docs/specs/movement/movement.md` requires slope down-dash launches to remain.
It also requires collisions to consume the final contact geometry and preserve
the authored face normal for slope response.

### Wrong Code

Not confirmed. Candidate stages:

- contact ordering and first-response selection in
  `src/physics/movement/actor-triangle-solver.cpp`;
- `manifoldResponseNormal()` choosing a rounded feature normal instead of the
  slope face normal;
- a ground contact projecting the downward component before the slope contact;
- bounce deduplication or the `bounceMaxSpeed` cap hiding the launch.

### Confirmed Cause

Not confirmed.

### Evidence

No complete recording of a failed slope down-dash is available in this report.
The human description is the current evidence.

### Next Step

Record one short, stationary slope down-dash in each failure case, with
`client_collision`, and add a `collision.speed_collapse` / contact trace. Add a
reliable slope scenario before changing response code.
