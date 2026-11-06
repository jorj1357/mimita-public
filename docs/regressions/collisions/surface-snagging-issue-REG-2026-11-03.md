# Actor snags on connected surfaces

Time created: 2026-11-03T19:09:00Z
Time last updated: 2026-11-03T19:09:00Z

Status: ATTEMPTED FIX (1) — bounce path now applies to walkable slopes;
runtime replay pending

Related specification: `docs/specs/movement/movement.md`
Related architecture: `docs/architecture/collision/collision.md`
Related changelogs: see `slope-edge-snag-REG.md` and
`block-seam-speed-collapse-REG.md`.

---

## Regression Occurrence 1

### Observed

Time: `2026-11-03T19:09:55Z`

Journal: `logs/2026-11-03/20261103_191001/events.jsonl`

### Expected Behavior

Held movement across connected walkable surfaces must preserve useful progress.
A real wall may stop the actor; a flush seam or walkable slope edge must not.

### Actual Behavior

The run contains:
- 2037 `collision.speed_collapse`
- 3577 `collision.bounce.applied`
- 1330 `collision.last_valid.rollback`
- 7 `collision.progress.stall`

### Confirmed Cause

Not confirmed. The first divergent contact has not been isolated. Candidate
stages remain `collapseCloseFeatureContacts()`, response-normal selection, and
step-up rollback in `src/physics/movement/actor-triangle-solver.cpp`.

### First Divergence

The useful recording is `scn-20261103_195642-23060-tdmsnag`, exported to
`devscripts/scenarios/user/scenario-20261103_195642-tdmsnag.jsonl` (6,480 ticks).

Grounded, moving speed collapses:
- tick `8929`: `46.67 → 15.78`, surface `263`, normal `[-0.45, 0, 0.89]`,
  penetration `0.12 m`
- tick `9534`: `18.87 → 3.82`, surface `311`, normal `[-0.45, 0, 0.89]`,
  penetration `0.19 m`

A separate airborne corner impact is at tick `6563`.

### Attempted Fix 1

Time: `2026-11-03T15:22:00Z`

`src/physics/movement/physics-collision-shared.h` no longer suppresses bounce
for every root contact whose normal is walkable. Only an already-supporting
flat floor (`normal.z > 0.995` and grounded/stable) is project-only. Walkable
slopes and walls take the configured bounce path and retain tangential
velocity according to `bounce.friction`.

Result: source and build only at this time. Replay the exported scenario and
check ticks 8929/9534 before claiming a fix.

### Next Step

Replay the exported scenario through the dev loop, capture the response normal
and before/after velocity at ticks 8929/9534, and require no speed collapse
greater than 35% while movement input is held. Human surface feel remains the
final acceptance.
