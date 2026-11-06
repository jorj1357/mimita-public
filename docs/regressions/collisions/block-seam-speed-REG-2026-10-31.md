# Block seam caused a forward speed collapse

Time created: 2026-10-31T11:50:24Z
Time last updated: 2026-10-31T18:06:36Z

Status: IMPLEMENTED IN SOURCE; RUNTIME SEAM PASS; HUMAN FEEL PENDING

Related specification:
`docs/specs/movement/movement.md`

Related architecture:
`docs/architecture/collision/collision.md`

Related changelog:
`docs/changelog/2026-10-31/20261031_140500-collision-stepup-and-scenario-evidence.md`

---

## Regression Occurrence 1

### Observed

Time: `2026-10-31T11:50:24Z` (human report; recording created earlier)

The player reports getting caught while walking forward on the seams between
flat blocks. The supplied recording
`devscripts/scenarios/user/scenario-20261031_143812-collisss.jsonl` shows a
large forward-speed collapse at a floor-level seam: velocity near `(14, 0.9)`
falls to `(1.8, 0.9)` repeatedly, then recovers, while the actor is grounded.

### Expected Behavior

Held forward input across connected flat block faces must preserve useful
horizontal progress. A real wall may stop the actor, but a flush seam or a low
walkable lip must not create repeated speed collapse or backward correction.

### Actual Behavior

At frames 377-398 the recording shows repeated wall contacts at floor height
with penetration values around 0.29-0.56 m while the player is grounded. The
same contact has duplicate surface ids. The actor oscillates backward and
forward until the wall contact disappears and it passes through at full speed.

### Why This Is Bad

The player loses almost all forward speed on ordinary connected ground and can
be pushed backward without a real wall. It directly violates the movement
specification's requirement that connected walkable geometry remain traversable
and that collision not become an invisible seam wall.

### Specification

`docs/specs/movement/movement.md` requires the current local collision feel to
be preserved, one collision meaning, and a shared fixed-60-Hz actor path. It also
requires step-up, slope interaction, and wall response to remain distinguishable.

### Wrong Code

Not yet confirmed. The current candidate stages are:

- `collapseCloseFeatureContacts()` in
  `src/physics/movement/actor-triangle-solver.cpp`, which can remove vertical
  contacts near walkable support;
- `manifoldResponseNormal()` and the near-feet response override, which use
  different `surfaceNormal.z` versus `fabs(surfaceNormal.z)` rules;
- step-up being applied after the movement solve while later correction or
  rollback may restore the pre-step pose.

A fixed-tick trace is required before changing one of these stages.

### Confirmed Cause

Not confirmed.

### Evidence

- `devscripts/scenarios/user/scenario-20261031_143812-collisss.jsonl`, frames
  377-398, `client_collision` contacts.
- Velocity at frame 379 is about `(13.0, 0.6)`; frame 380 drops to
  `(2.36, 0.71)` while the player is grounded.
- The blocking contact normal is approximately `[-1, 0, 0]` at floor height.
- The recording has no footer, so it is evidence, not a completed acceptance
  artifact.

### Next Step

Reproduce with a complete recording on a dev-loop-owned server and capture the
selected response normal, triangle id, surface normal, actor part, step-up
result, and any rollback at the first speed collapse.

## Resolution — 2026-10-31T18:06:36Z

Implemented a low flush-seam rule in `collapseCloseFeatureContacts()`:
when a near-feet vertical map contact blocks the intended move but its world
triangle top is no more than one step above the walkable support, the contact is
removed. Real tall walls keep their full height and remain blocking.

Deterministic checks pass:
- `flush block seam does not produce a wall contact`
- `flush block seam preserves intended forward motion`

Runtime evidence:
- Run `logs/2026-10-31/20261031_175544/events.jsonl`, build 4003.
- `agent.move 1 0 420` crossed the recorded Dust 2 seam area from x `-821` to
  x `-681` with no `collision.speed_collapse` and no
  `collision.progress.stall` event.
- The pre-fix recording still shows frame 377 horizontal speed `20.00 -> 0.40`;
  `scenario-analyze.py` now reports it as `SPEED COLLAPSE`.

Not yet claimed: human feel on the exact seam, or a complete server-backed replay
with a footer.
