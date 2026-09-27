// 2026-09-27T19:18:02Z
/* purpose
* record the collision-only fix for body mesh contacts that appear too far away
* preserve the black body-contact spark as an unchanged downstream visual
* does NOT claim live gameplay or human acceptance
*/

# Task

- Summary: Make body-part collision use the actual limb mesh across the full
  movement from the previous pose to the current pose, so a limb cannot cross a
  thin world surface between fixed ticks and enter a block.
- Status: CODE_COMPLETE / BUILD_VERIFIED / DETERMINISTIC_TEST_PASS /
  HUMAN_REVIEW_REQUIRED
- Date, time, timezone: 2026-09-27T19:18:02Z, ISO 8601 UTC

# Scope

- The `bodyContactSpark` effect was not changed. Its long black shape remains a
  visualization of the root-to-contact point and is downstream of collision.
- The user's pre-existing edits to `config/collision.json` and `config/hitfx.json`
  were preserved.

# Implementation

- `src/physics/movement/physics-collision-glb-body.cpp`: refresh current model
  transforms without destroying each part's previous transform, so the mesh
  collision pass receives a real old-pose-to-new-pose sweep on every pass.
- `src/physics/movement/physics-collision-mesh.cpp`: query the union of the old
  and new part bounds, sample every loaded body collider triangle along its
  swept path, oppose the part's travel for swept response, and test all loaded
  limb triangles rather than an arbitrary first-512 prefix.
- `src/physics/movement/physics-collision-stress.cpp`: add a deterministic thin
  wall crossing test where the limb starts before the wall and ends beyond it.

# Validation

- Source/build: `python build_agent.py` completed with `Status: SUCCESS`.
- Deterministic test with mesh collision enabled temporarily: existing collision
  stress cases passed, `mesh limb triangle hits floor` passed, and `swept limb
  crosses thin wall` passed; final result was `[COLLISION SELFTEST] PASS`.
- Deterministic test with the user's final `bodyMeshCollision: false` setting:
  existing non-mesh collision stress cases passed; mesh tests were correctly
  skipped by configuration.
- Human/runtime acceptance: not performed. Gameplay still needs a manual check
  with `bodyMeshCollision: true` against the pictured cylinder/block scene.
