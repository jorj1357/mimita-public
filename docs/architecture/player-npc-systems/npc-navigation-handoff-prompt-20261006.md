# NPC navigation implementation handoff prompt

This file is a copy/paste prompt for the next AI agent working in
`C:\mimita-v9`. It is intentionally operational: read it together with the
two authoritative navigation documents below, inspect the repository, and
continue the implementation from the verified frontier.

## Copy/paste prompt

```text
You are continuing implementation of MiMITA's general NPC navigation and
Counter-Strike behavior migration in C:\mimita-v9.

Read these first and treat them as the current design authority:

1. C:\mimita-v9\docs\architecture\player-npc-systems\npc-navigation-implementation.md
2. C:\mimita-v9\docs\architecture\player-npc-systems\npc-nav-external-lib-20261006.md
3. C:\mimita-v9\docs\workflows\runtime-scenario-validation.md
4. C:\mimita-v9\docs\specs\debug-logging\debug-logging.md
5. C:\mimita-v9\docs\operations\build-and-exe\build-and-exe.md
6. C:\mimita-v9\docs\operations\task-completion\task-completion.md
7. C:\mimita-v9\docs\skills\logging-checker-v1.md
8. C:\mimita-v9\docs\skills\asset-checker-v1.md

## Mission

Continue the full-slice migration toward a general, server-authoritative
navigation and behavior architecture. The first acceptance target is
Counter-Strike NPC behavior on `dust2cyberiav4`. NPCs should eventually route
like difficult professional Counter-Strike players: leave spawn decisively,
avoid walls and oscillation, reach tactical positions, hold angles, wait for
contact, fight from useful positions, replan after combat, and resume the
objective.

The architecture must remain expandable to flying, hovering, driving,
climbing, swimming, teleporting, and custom traversal later. Do not force
those locomotion types into the ground-navigation implementation.

## Verified current state

The following facts are proven and should not be re-discovered or overstated:

- The acceptance map exists at:
  `C:\mimita-v9\assets\maps\dust2cyberiav4.glb`
- The canonical runtime request is:
  `assets/maps/dust2cyberiav4.glb`
- A real server run resolved that path to the exact absolute file above.
- The file size in that run was 8,628,028 bytes.
- The server parsed the GLB and produced 7,442 collision triangles.
- The server found 2 spawn points and finite map bounds.
- Recast/Detour was built into the MiMITA executable and a runtime bake
  succeeded with navmesh version 1.
- Recast/Detour is currently compare-only. The existing custom navigator is
  still authoritative for NPC movement.
- The current Recast implementation is in:
  `src/npc/recast-navigation.h`
  `src/npc/recast-navigation.cpp`
- The existing compare integration is in:
  `src/npc/npc-navigator.h`
  `src/npc/npc-navigator.cpp`
  `src/npc/npc.cpp`
  `src/network/server-npcs.cpp`
- Recast, Detour, and DetourTileCache sources are already vendored under:
  `external/recastnavigation/`
- The canonical build integration is already in `build.py`.
- Structured map-load evidence was added to:
  `src/world/world-gltf-loader.cpp`
  `src/network/server-world.cpp`
- The world structured-log category is enabled in:
  `config/debuglogger.json`

The most recent verified executable was:
`mimita-20261006T-dust2-map-load-v1.exe`

The most useful runtime journal was:
`logs/10-06-2026/20261006_160133/events.jsonl`

That journal proves this event chain:

1. `map.server-load-start`
2. `map.server-load-success`
3. `npc.navmesh.bake-success`
4. `npc.nav.compare`

The active blocker is not map discovery or file loading. Long Dust 2 Recast
queries still produce `failure=path_not_found`. Investigate and fix the first
incorrect step in Recast geometry/profile/connectivity/query preparation.

## Non-negotiable ownership rules

- The server owns NPC navigation authority and tactical truth.
- NPC brain/state code owns why and where the actor wants to go.
- The navigation adapter owns spatial feasibility and route data.
- Recast generates navmesh data.
- Detour performs nearest-poly, path, corridor, filter, and corner queries.
- DetourTileCache is for bounded dynamic tile updates later.
- MiMITA movement/physics remains authoritative for final position, velocity,
  gravity, collision, grounding, jumping, dashing, and execution.
- Navigation may output a desired direction or preferred velocity, but must
  never teleport or directly set final actor state.
- Traversal links remain capability-aware and actor-specific; traversal
  execution remains in MiMITA's traversal/movement owner.
- DetourCrowd and RVO2 are later optional local-avoidance suggestions only.
- Do not create a parallel NPC brain, parallel movement motor, or competing
  logger when an existing owner can be extended.
- Do not delete the custom navigator until the promotion/deletion gates in the
  implementation contract are actually proven.

## What to do next

Work through the following in order, continuing as far as evidence supports:

### A. Reproduce the real failure with fresh evidence

1. Inspect the current dirty worktree first. Preserve unrelated user edits;
   do not reset, clean, delete, or rewrite them.
2. Build a newly named executable using the canonical build process.
3. Run that exact executable with `--versioninfo` and capture the printed
   `EVENTS_JSONL_PATH`.
4. Run the real server scenario with compare mode enabled:

   `MIMITA_NPC_NAV_COMPARE=1 mimita-<new-name>.exe --server --bind 127.0.0.1:0 --map dust2cyberiav4 --gamemode counterstrike --npcs 2 --timeout 5 --no-map-rotation --no-discord-notification`

5. Read the same `events.jsonl` while the process is active and after it ends.
6. Identify the first incorrect or missing event. Do not infer from a build
   success or from a synthetic self-test alone.

### B. Diagnose `path_not_found`

Instrument the existing Recast adapter with bounded, searchable diagnostics
where needed. At minimum make the evidence distinguish:

- invalid or non-finite start/destination;
- nearest-start failure;
- nearest-destination failure;
- start and destination polygon references;
- disconnected polygon regions;
- zero-poly or truncated corridor;
- filter/area/flag rejection;
- geometry orientation or coordinate-scale mismatch;
- profile radius/height/step/slope mismatch;
- insufficient query extents;
- route found but straight-path extraction failed.

Compare the authoritative collision triangles, Recast bounds, walkable-area
count, polygon count, start/destination positions, nearest points, and route
result. Preserve the existing map-loading and navmesh success events, but add
only bounded diagnostics that explain the failure.

Do not silently widen the agent profile, disable erosion, or change the
coordinate system merely to make one query pass. Measure the geometry and
explain the choice in the changelog.

### C. Prove isolated route quality on the real map

Add or complete the smallest useful isolated query proof using the actual
Dust 2 collision geometry. It must cover at least:

- same-floor route;
- route around a wall or building;
- ramp/stairs if present;
- invalid start or destination;
- disconnected/no-path result;
- one route that currently reports `path_not_found`.

Keep isolated query evidence separate from real NPC movement evidence. If
visual debug output is practical, show Recast polygons, start, destination,
nearest points, corridor, and failure location.

### D. Improve the compare contract

For identical navigation requests, record custom and Recast results with:

- actor/profile identity;
- start and destination;
- backend;
- navmesh version;
- success/failure category;
- polygon/corner counts;
- route length;
- endpoint reachability;
- query timing;
- fallback source;
- classification of the difference.

The custom route remains authoritative. There must be no silent backend
switching. If Recast cannot route, record the explicit reason and preserve the
custom result or bounded hold behavior.

### E. Only then move toward route-following

After real-map query failures are understood and isolated routes are sound,
implement the next safe slice: feed Recast's next corner into existing
movement intent while retaining MiMITA physics and collision. Add corridor
retention, partial replanning, stale-navmesh rejection, stuck detection, and
bounded fallback. Do not make Recast authoritative globally yet.

### F. Continue toward Counter-Strike behavior

Once route following is stable, continue through tactical behavior in the
existing brain/state owners:

- team objective and role selection;
- spawn-to-site routing;
- cover and angle selection;
- last-known enemy position;
- holding and bounded patience;
- coordinated pushes;
- combat interruption and route resumption;
- tactical fallback and bounded route diversity.

Keep navigation responsible for “how can this actor get there?” and the brain
responsible for “where and why should this actor go?”.

## Required evidence discipline

For every executable-changing phase:

1. Source evidence: exact owners/functions changed.
2. Build evidence: newly named executable and build result.
3. Version evidence: exact executable and `EVENTS_JSONL_PATH`.
4. Runtime journal evidence: exact event chain in the same file.
5. Test evidence: focused checks or isolated query result.
6. Human acceptance: visual/gameplay observations, clearly separated from
   machine proof.

Always inspect `events.jsonl` after the real run. A successful build is not
proof that NPC behavior changed. When behavior fails, report the first missing
or incorrect event, not just the final symptom.

Use the existing StructuredLogger and canonical append-only JSONL output.
Do not add unmanaged debug files or unbounded per-tick logging. Include run,
process, map, backend, profile, navmesh version, request/result identity,
reason, timing, and relevant positions in navigation events.

## Acceptance targets

Use the thresholds in the implementation contract as the target gate:

- 100% of routes have a result or explicit hold/fallback reason.
- 100% of route results identify backend, profile, and navmesh version.
- At least 95% of travel segments make positive progress.
- Zero unrecovered wall-intersection or collision-loop failures.
- No more than one unintended reversal per NPC per 30 seconds.
- No unexplained oscillation longer than two seconds.
- At least 90% reach a tactical destination or valid combat hold point before
  first engagement.
- At least 90% of survivors resume a valid route after combat interruption.
- No server tick or tile-update budget violation.
- Human review resembles a coordinated Counter-Strike match.

Do not claim these targets pass until a fixed-seed multi-round run and human
review actually support them.

## Scope exclusions for now

- Do not implement erosion/destructible-world integration in this handoff.
- Do not rebuild the entire navmesh for every dynamic change.
- Do not promote Recast globally because its bake succeeds.
- Do not add DetourCrowd or RVO2 before global route following is reliable.
- Do not force flying, hovering, vehicle, or swimming actors into this ground
  navmesh.
- Do not delete custom navigation yet.

## Completion behavior

Continue making safe, in-scope progress without waiting for clarification when
the choice is reversible and consistent with the two navigation documents.
Stop and report a clear decision request only when the choice would materially
change gameplay authority, public data formats, or the long-term architecture.

At the end, create exactly one changelog for your session under
`docs/changelog/2026-10-06/`, preserve unrelated dirty work, and report exact
paths plus separate source/build/runtime/human evidence.
```

## Purpose and non-goals

This handoff is for an AI agent who will implement and verify the next
navigation slice. It does not replace the two navigation contracts, the
runtime-validation workflow, or the debug-logging specification. It does not
authorize destructive cleanup, Recast promotion, deletion of custom
navigation, or changes to erosion/destructible-world systems.
