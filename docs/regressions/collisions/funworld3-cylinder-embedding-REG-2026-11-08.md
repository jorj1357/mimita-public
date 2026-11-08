# NPCs sometimes embed in a FunWorld3 cylinder

- Recorded: 2026-11-08 (human report; exact time not supplied)
- Status: REPORTED / NOT YET REPRODUCED
- Mode and map: Payload on FunWorld3

The human reports intermittent NPC embedding in cylinders. The authored
FunWorld3 map currently has one GLB chunk and two spawnpoints, with no
top-level cylinder entry. The reported cylinder may be mesh geometry, a
generated or physical object, or another contact source; the source is not
established.

The `20261108_141222` run recorded hundreds of stuck and recovery episodes,
but those records did not identify the contacted chunk, object, or triangle.
The episodes do not prove this particular embedding report.

Next capture: use the actual FunWorld3 route and affected actor type; record
the actor and body pose, world/object source, chunk and triangle or physical
entity ID, contact point and normal, candidate count, correction, and recovery
outcome. Preserve the fixed 60 Hz tick, cached broadphase, and contact budget.
Retain this status until a source-correlated live reproduction and human review.

Live source attempt, 2026-11-08: build
`2026-11-08T15:28:28.157458Z` activated `chunk-collision` generation 21
on server PID 17964 and client PID 29288. Both journals recorded applied
`collision.hot-correction` results, so the bounded shared correction routine
ran on both sides. The same server still recorded stuck-recovery episodes;
this is not evidence that the reported cylinder is fixed. A sampled recovery
record now includes up to four contact source/entity/surface IDs, points,
normals, and penetrations, but no authored GLB submesh name yet.
In this run, 431 contacts attached to sampled stuck-recovery episodes included
only 19 side-facing normals (`abs(normal.z) < 0.5`); the most frequent surfaces
were upward-facing. A stuck-recovery count alone is therefore a poor proxy for
the reported cylinder embed. The actual cylinder contact still needs a human
route and geometry-to-surface mapping.
