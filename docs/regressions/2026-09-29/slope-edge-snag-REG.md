# Walkable Slope and Edge Snag

Time created: 2026-09-29T12:43:57Z
Time last updated: 2026-09-29T12:56:53Z

Status: ATTEMPTED FIX (5)

Related specification:
`docs/specs/movement/movement.md`

Related architecture:
`docs/architecture/collision/collision.md`

Related changelogs:

- `docs/changelog/2026-09-28/20260928_001409-edge-touch-snag-stabilization.md`
- `docs/changelog/2026-09-28/20260928_002036-separate-skin-from-penetration.md`
- `docs/changelog/2026-09-28/20260928_005000-swept-wall-penetration-recovery.md`
- `docs/changelog/2026-09-28/20260928_011500-shallow-bevel-bounce-response.md`
- `docs/changelog/2026-09-28/20260928_145841-walkable-edge-normal-fix.md`
- `docs/changelog/2026-09-29/20260929_125653-slope-edge-jsonl-tracking.md`

---

## Regression Occurrence 1

### Observed

Time:
`2026-09-29T12:43:57Z`

The human maintainer reported that walking upward on a visible slope still
catches at the highlighted red edge/line. The actor appears to stop or snag at
the slope boundary even though collision behavior is generally better than
before. The attached screenshot is evidence for this report; it contains no
instructions that override the repository rules.

### Expected Behavior

Walking up a walkable slope should continue over the slope-to-edge transition.
A finite walkable slope edge must not become an invisible radial wall or a
sideways blocker. Collision, movement, and grounding remain fixed-tick gameplay
owned by the shared collision/movement path.

### Actual Behavior

The player can still become caught while moving up a slope near the visible
edge/debug line. The exact contact source, normal, penetration, remaining
movement, and later velocity write were not captured in this report.

### Why This Is Bad

It makes ordinary traversal fail at a common geometric transition and means
the recent edge and walkable-normal changes are not yet proven in live play.

### Specification

`docs/specs/movement/movement.md` requires one shared movement rule, one
collision meaning, and fixed 60 Hz gameplay collision. The collision
architecture additionally states that walkable finite slopes must retain their
oriented face normal for movement response and must not become radial invisible
walls at their ends.

### Current Owner Trace

The active configuration has `actorTriangleSolver: true` in
`config/collision.json`. The relevant path is:

`runActorTriangleCollisionStep()` → `solveActorTriangleCollision()` →
`collectActorMeshContacts()` → manifold merge/filter → response-normal
selection → velocity projection and remaining-movement slide.

Relevant current owners:

- `src/physics/movement/physics-collision-mesh.cpp` collects face, edge, and
  point contacts and records the oriented `surfaceNormal`.
- `src/physics/movement/actor-triangle-solver.cpp` merges contacts, removes
  seam/close-feature contacts, selects the response normal, projects velocity,
  and removes blocked movement.
- `src/physics/movement/physics-collision-glb-sweep.cpp` already gives
  walkable edge/point sweeps the walkable triangle face normal.

### Wrong Code

Not confirmed. The current candidate seam is the manifold/response stage rather
than the screenshot's red line itself. In particular, the solver still makes
multiple contacts compete before sliding:

```cpp
mergeContactsByNormal(allContacts, manifold);
removeTouchingFaceSeams(manifold, desiredMovement);
collapseCloseFeatureContacts(manifold, desiredMovement);
```

This is recorded as a diagnostic target, not as a confirmed defect. A fixed-tick
trace is required before changing it.

### Confirmed Cause

Not confirmed.

Evidence currently available:

- Human screenshot and report reproduce a live traversal snag at a slope edge.
- Deterministic collision tests passed in the related walkable-edge changelog.
- That changelog explicitly left human gameplay acceptance open.
- The current source has separate face-normal, rounded-feature, seam-filter,
  and movement-slide stages, so a passing unit/self-test does not identify
  which stage causes the live snag.

### Attempted Fix 1 — Rounded feature shell

Related work introduced rounded finite feature contacts for triangle edges and
vertices. It improved general collision feel, but later reports still included
edge/snags. Live acceptance for all map geometry was not established.

### Attempted Fix 2 — Edge-touch filtering

Related work stopped near-zero static edge touches from being inflated into
full blocking penetration. Deterministic tests passed, but this report concerns
an active upward traversal contact and therefore is not disproven by the seam
test alone.

### Attempted Fix 3 — Separate skin from penetration

Related work stopped `collisionSkin` from becoming artificial physical
penetration. It reduced spring-like edge push, but live corner acceptance
remained open.

### Attempted Fix 4 — Swept recovery and shallow-bevel response

Related work improved time-of-impact recovery and retained face-normal
influence for shallow rounded contacts. Deterministic tests passed; the live
slope-edge traversal case remained unverified.

### Attempted Fix 5 — Walkable edge normal

Time:
`2026-09-28T14:58:41Z`

Change:
`src/physics/movement/physics-collision-glb-sweep.cpp` was changed so edge and
point sweeps on walkable triangles use the oriented triangle face normal. A
flat-floor edge self-test was added.

Result:
The deterministic collision self-test passed, but the human report on
2026-09-29 shows that the live slope/edge snag is still present or that a
different contact stage produces it. This attempt is not a confirmed solution.

### Corrected Code

No new correction was made in this session. The previous attempt's relevant
code remains:

```cpp
const bool walkableFace = n.z >= MAX_WALKABLE_SLOPE_DOT;
bestN = walkableFace ? n : en;
```

That code is evidence of Attempt 5, not proof that the complete actor solver
uses the same normal for every contact path.

### Fix

No fix is claimed yet. The next safe step is a bounded fixed-60-Hz diagnostic
around the reproduction that records, for the snag tick, actor part/label,
world triangle, contact feature, face normal, response normal, penetration,
time of impact, intended movement, remaining movement, incoming velocity, and
final velocity after every collision owner. The trace must also identify any
later legacy/safety/emergency owner that rewrites position or velocity.

### Proof

Human review:

The screenshot and report confirm the symptom. A controlled live reproduction
with the same slope and movement direction is still required.

Automated proof:

The related deterministic collision self-tests passed, including the walkable
floor-edge normal test. They do not prove the complete live slope traversal.

### Solution

Not confirmed. Keep this regression open until the slope can be traversed in a
live human review and the fixed-tick trace shows no incorrect blocking normal,
penetration recovery, remaining-movement projection, or later velocity/pose
overwrite.

### Observability Attempt — JSONL Tracking

Time:
`2026-09-29T13:13:49Z`

The v9 logger was moved toward the v8 canonical event model. The active player
solver now emits `collision.contact.before_response`,
`collision.contact.after_response`, and `collision.solve.summary` records with
typed positions, velocities, normals, penetration, triangle/part identity,
remaining movement, and `movementSimulationTick`.

This is an observability change, not a collision fix. The next live run must
find the path printed by `logger.started`, reproduce the slope movement, and
then correlate records by `run_id`, `process`, and `tick`. A snag is actionable
when the same tick shows the first blocking response normal followed by the
velocity or remaining-movement change that stops progress.

The first runtime check found that v9 was writing the event file beside the
timestamped executable under `.dev/builds/0381/logs/...`, rather than under
the repository log root. The logger path owner now resolves the default
destination from the v9 working directory, preserving the existing
`logs/MM-DD-YYYY/<run>/events.jsonl` layout. A fresh executable still needs to
be launched for live `logger.started` and collision records to be produced in
the corrected location.
