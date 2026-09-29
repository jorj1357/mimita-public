# Slope-edge and downward-landing response fix

Time: `2026-09-29T19:01:50Z`

## Change

Updated `src/physics/movement/actor-triangle-solver.cpp` so a nearby
walkable, near-feet contact becomes the response authority for the actor
manifold. A shallow rounded edge contact on the same support is promoted to the
support normal instead of behaving like a small invisible wall.

Downward landings on static support now project inward velocity instead of
using the body/weapon bounce response, preserving tangent movement along a
flat floor or slope and preventing lateral down-dash ricochet.

Collision JSONL records now include `support_contact`, `support_edge_promoted`,
and `landing_bounce_suppressed` for this decision.

## Evidence

- `python build_agent.py`: `Status: SUCCESS`, return code 0.
- Build 0435 `mimita.exe --collision-selftest`: `COLLISION SELFTEST PASS`.
- Human authored-map acceptance remains pending; this is not a claim that all
  slope/corner cases are solved.
