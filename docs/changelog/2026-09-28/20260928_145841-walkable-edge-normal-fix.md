// 2026-09-28T14:58:41Z
/* purpose
* record the walkable-floor edge normal correction
* preserve JSON and spark behavior
* does NOT claim human gameplay acceptance
*/

# Task

- Summary: Prevent a rounded sweep from producing a sideways bounce when it
  reaches the edge or vertex of a walkable floor triangle.
- Status: CODE_COMPLETE / DEV_BUILD_VERIFIED / DETERMINISTIC_TEST_PASS /
  HUMAN_REVIEW_REQUIRED
- Date, time, timezone: 2026-09-28T14:58:41Z, ISO 8601 UTC

# Implementation

- `src/physics/movement/physics-collision-glb-sweep.cpp`: when the contacted
  world triangle is walkable, edge and point sweep hits now use the oriented
  triangle face normal. Non-walkable surfaces retain their rounded edge and
  point normals.
- `src/physics/movement/physics-collision-stress.cpp`: added a flat-floor edge
  test requiring an upward normal, while preserving the existing mesh sweep,
  thin-wall, and old-contact-leaving tests.
- No JSON files, spark code, rounded shape sizes, or thickness settings changed.

# Validation

- Live dev build `0227` compiled the change.
- `C:\mimita-v9\.dev\builds\0227\mimita.exe --collision-selftest` passed:
  walkable floor edge keeps face normal, mesh limb floor contact, swept thin
  wall crossing, limb leaving old contact, and the complete collision self-test.
- Human gameplay acceptance remains required on a flat floor and triangle seam.
