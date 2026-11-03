# Actor snags on connected surfaces

Time created: 2026-11-03T19:09:00Z
Time last updated: 2026-11-03T19:09:00Z

Status: UNRESOLVED

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

### Next Step

Replay the human recording or a fresh dev-loop run on the exact map. Record the
first speed collapse with actor part, triangle id, surface normal, response
normal, penetration, step-up result, and rollback state before changing code.
