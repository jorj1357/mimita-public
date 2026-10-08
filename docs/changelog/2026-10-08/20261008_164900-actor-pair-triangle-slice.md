# Local actor-pair triangle collision slice

Date: 2026-10-08
Status: PASS_WITH_HUMAN_REVIEW

## Implemented

- Added `ActorCollisionParticipant` and `ActorPairCollisionSummary` to the shared actor triangle solver.
- Added stable actor-ID pair ordering and reusable thread-local scratch for participant geometry and pair ordering.
- Added triangle-vs-triangle SAT narrowphase using body and weapon collision triangles.
- Aggregated contacts per actor pair before applying one bounded depenetration and one velocity response.
- Reused existing hot collision bounce/friction configuration, with bounded restitution and friction response.
- Added sampled `actor-collision.contact` and `actor-collision.response` structured events containing actor IDs, kinds, lifecycle IDs, body-part labels, triangle contact counts, penetration, normal, point, velocity, restitution, friction, and impulse.
- Added the local fixed-tick participant pass for one human player plus all live NPCs, covering local player/NPC, NPC/NPC, and player/player when multiple local player bodies are supplied by the caller.
- Added `actor-collision.swarm-summary` metrics for pair count, broadphase pairs, triangle candidates, contacts, responses, and maximum penetration.

## Files changed by this session

- `src/physics/movement/actor-triangle-solver.h`
- `src/physics/movement/actor-triangle-solver.cpp`
- `src/npc/npc.h`
- `src/npc/npc.cpp`

## Build and executable evidence

- Fresh timestamped build succeeded: `mimita-20261008T1710-actor-pairs.exe`.
- Build result: success, one changed translation unit compiled, 548 cached units skipped.
- `--versioninfo` succeeded.
- Canonical journal: `logs/10-08-2026/20261008_164754/events.jsonl`.
- The journal contains executable identity/startup evidence only. No actor contact event was expected because `--versioninfo` does not run gameplay. No deterministic, swarm, or synthetic collision test was run per request.
- `git diff --check` found no new whitespace errors; only line-ending conversion warnings were reported for modified files.

## Not yet proven / remaining

- Human gameplay acceptance is still required for actual player/NPC and NPC/NPC contact feel, standing, bounce, and dense behavior.
- Dedicated-server player bodies do not yet expose the same triangle participant provider, so server-authoritative multiplayer player/player and player/NPC collision is not claimed.
- Ragdoll participation, moving support inheritance, lifecycle reconciliation, and network pose transport remain future slices.
- The pair geometry currently uses a bounded two-pass local solve; mass presets and full manifold/support policy still need the server-owned actor collision configuration and authority integration.

## Existing work preserved

The repository already contained unrelated user edits and earlier changelog files. They were preserved and were not reset, overwritten, or claimed as part of this session.
