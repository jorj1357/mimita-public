// 2026-11-20
/* purpose
* check a performance/culling change for measured ownership, safe scope, and proof
* this skill DOES NOT remove authoritative simulation or replication
* this skill DOES NOT accept an average FPS as frame evidence
*/

# Performance culling v1

Check the change against the culling and work-avoidance specification.

- Is the expensive scope named with a measured frame/spike record?
- Is the change presentation-only or does it affect authoritative gameplay?
- Are offscreen actors, effects, ragdolls, shadows, projectiles, UI, and world
  chunks skipped when they cannot affect the current view?
- Are visible actors still complete and correctly replicated?
- Are LOD/update-rate reductions documented and visually reviewed?
- Is the UI using dirty regions, cached text/layout, batching, and culling?
- Is config polling event-driven or bounded?
- Are allocations, locks, texture binds, draw calls, and state changes reduced?
- Is there a before/after frame-time distribution with p50/p95/p99/p99.99/max?
- Is the 240 FPS gate checked per frame, not by one-second average?
- Is the exact pinned build/generation/run/journal attached?
- Are screenshots taken at the exact tick and linked as supplemental evidence?
- Does another agent review the change, with human final for quality/policy?

Report the exact issue, scope, evidence, expected gain, risk, and missing proof.
