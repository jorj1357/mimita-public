# Actor corner collision owner switch

Date: 2026-09-27 19:22:40 -04:00 (EST)
Branch: afad20a-rebuild
Commit: 92cb30e7
Status: PASS_WITH_HUMAN_REVIEW

## Request

Fix the local player getting caught when an arm enters the corner of a block,
and mark duplicate collision owners for later deletion.

## Finding

The reported symptom was consistent with the legacy body/weapon phase in
`src/physics/movement/physics-collision-glb-body.cpp`: arm contacts were added
to root contacts and solved across up to three correction passes. The newer
actor-triangle solver already merges body, weapon, and world contacts into one
surface manifold and has a floor-plus-wall corner self-test.

## Change

`config/collision.json:7-11`

Old:

```json
// Toggle here (hot reload) to A/B test; NPCs keep the legacy path.
"actorTriangleSolver": false,
```

New:

```json
// Local players use this owner. The legacy path remains only as a guarded
// fallback/NPC path until its TODO-DELETE acceptance gates are complete.
"actorTriangleSolver": true,
```

The legacy local-player path was not deleted. Existing TODO-DELETE markers in
`physics-collision-glb-main.cpp:152-158,173-175` and
`physics-collision-glb-body.cpp:37-53` document the duplicate owner and its
human acceptance gates.

## Validation

- `git diff --check`: passed.
- `python build_agent.py`: exited 0 and reported `Status: SUCCESS`,
  `Nothing changed`; this was a JSON/hot-reload selection change, so no C++
  relink was required.
- Human gameplay corner test: still required. Push the local player's arms
  into a block corner and compare movement with the previous setting.
- Existing running processes were not stopped or replaced.

## Pre-existing or tool-created edits

- `config/analytics.json` was already modified by the executable launch during
  the investigation; it was not changed as part of this collision fix.
- `config/collision.json` is the intentional change from this session.
