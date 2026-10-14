# General NPC Navigation and Counter-Strike Behavior Migration Plan

## Summary

Build a general, server-authoritative NPC navigation and behavior system around Recast/Detour while preserving MiMITA’s existing physics, collision, actor movement, and traversal execution.

The first complete slice targets `dust2cyberiav4` and Counter-Strike behavior:

- NPCs move decisively toward tactical goals.
- NPCs use server-known tactical information for high difficulty.
- NPCs select routes, positions, and combat actions.
- NPCs hold useful angles and wait for enemies.
- NPCs avoid walls, oscillation, repeated reversals, and deadlocks.
- NPCs replan after combat, obstruction, or dynamic-world changes.
- Every major decision is proven through the real executable and `events.jsonl`.

The architecture must remain general enough to later support walking, flying, hovering, driving, climbing, teleporting, and other locomotion types.

The first migration uses `compare` mode. Existing custom navigation remains authoritative until Recast/Detour behavior is proven.

## Core architecture

Reuse the existing NPC owners rather than creating a parallel AI system:

- `NpcSystem` and existing brain/state code continue selecting targets, objectives, combat states, and tactical intent.
- `NavigationRequest` becomes the common request surface for all locomotion types.
- A MiMITA-owned navigation adapter hides Recast/Detour types from gameplay code.
- Recast generates navigation data.
- Detour performs nearest-point queries, path searches, corridors, filters, and next-corner selection.
- DetourTileCache handles bounded dynamic tile updates.
- Existing MiMITA movement physics remains responsible for final position, velocity, gravity, collision, jumping, dashing, and grounding.
- Traversal links remain capability-aware and actor-specific.
- DetourCrowd and RVO2 are optional later providers of local avoidance suggestions only.

Introduce or formalize these general concepts:

- `NavigationAgentProfile`: radius, standing height, crouching height, step height, slope limit, clearance, movement mode, and coordinate scale.
- `MovementCapabilities`: walk, jump, drop, crouch, dash, climb, fly, hover, drive, teleport, or custom abilities.
- `NavigationRequest`: actor identity, start, destination, goal type, agent profile, capabilities, team/mode filters, tactical constraints, and navmesh version.
- `NavigationResult`: backend, route identity, navmesh version, success/failure category, corridor/corners, traversal links, fallback source, and timing.
- `TraversalLink`: stable link ID, entry/exit points, semantic type, required capability, execution parameters, and actor eligibility.
- `LocomotionAdapter`: converts navigation output into movement intent for walking, flying, hovering, driving, or other actor-specific movement systems.

The navigation layer may provide a desired direction or preferred velocity. It must never directly set actor position, final velocity, grounded state, jump state, or collision results.

## Phases

### Phase 0 — Integration readiness and evidence foundation

Before changing NPC behavior:

- Trace the current owners in `NpcNavigator`, `NpcNavGraph`, `NpcNavigation`, `NpcTraversal`, server NPC updates, and shared movement.
- Record which current systems produce goals, paths, movement directions, recovery behavior, and traversal actions.
- Confirm the canonical build path through `build.py` and the existing toolchain.
- Pin and integrate the cloned Recast/Detour source through the MiMITA-owned build.
- Keep Recast demo, sample, and standalone test targets outside the game executable.
- Define the first actor profile from authoritative player/NPC collision data and GLB/avatar geometry.
- Record world units, up axis, coordinate orientation, radius, height, step height, slope, clearance, and jump envelope.
- Define offline navmesh artifacts and runtime-bake fallback.
- Define dynamic tile invalidation and bounded update policy.
- Define server-only navigation authority.
- Define the explicit backend modes: `custom`, `compare`, and `recast`.

Add or verify bounded StructuredLogger events:

- `npc.navmesh.load-start`
- `npc.navmesh.load-success`
- `npc.navmesh.bake-start`
- `npc.navmesh.bake-success`
- `npc.navmesh.bake-failed`
- `npc.navmesh.tile-dirty`
- `npc.navmesh.tile-updated`
- `npc.nav.requested`
- `npc.nav.result`
- `npc.nav.fallback`
- `npc.traversal.selected`
- `npc.movement-decision`
- `npc.movement-progress`
- `npc.stuck`
- `npc.goal-changed`

Every event must identify, where applicable:

- process role;
- NPC ID;
- round/run ID;
- backend;
- map identity;
- navmesh version;
- actor profile;
- request/result ID;
- goal and destination;
- failure category;
- route timing;
- fallback source;
- position and progress values.

Phase 0 passes only when a reviewer can identify the first failure from source and `events.jsonl`.

### Phase 1 — External-library build and isolated navigation proof

Implement the MiMITA-owned Recast/Detour adapter without changing production NPC behavior.

Tasks:

- Add Recast, Detour, and DetourTileCache to the canonical MiMITA build.
- Expose only the adapter’s public interface to NPC code.
- Convert authoritative GLB/collision triangles into Recast input.
- Build a minimal test navmesh.
- Load and validate the resulting Detour navmesh.
- Query nearest polygons.
- Query a path between two points.
- Generate a straight path and inspect the corridor.
- Visualize polygons, corridor, start, destination, and failure location.
- Validate coordinate conversion and agent-profile dimensions.

Tests:

- Flat walkable surface.
- Wall obstruction.
- Ramp and stairs.
- Multi-floor surfaces.
- Narrow corridor.
- No-path case.
- Invalid start or destination.
- Navmesh cache mismatch.
- Runtime bake fallback.
- Tile rebuild after a temporary obstacle.

Gate:

- Real MiMITA geometry produces a valid, inspectable route.
- No game actor uses the new backend yet.
- Build evidence and navigation-query evidence are recorded separately.

### Phase 2 — Compare mode against current navigation

Run the current custom navigator and Recast/Detour with identical requests.

Tasks:

- Preserve custom navigation as authoritative.
- Send the same start, destination, actor profile, capabilities, and filters to both backends.
- Compare success/failure, route length, corner count, path validity, timing, and endpoint reachability.
- Add fixed-seed repeatability.
- Record route differences in `events.jsonl`.
- Add debug visualization for both routes.
- Classify differences as:
  - expected geometric improvement;
  - missing traversal link;
  - bad geometry extraction;
  - profile mismatch;
  - dynamic-world mismatch;
  - custom-system bug;
  - Recast/Detour adapter bug.

Gate:

- Differences are understood and categorized.
- No silent backend switching occurs.
- The custom backend remains authoritative.

### Phase 3 — Recast route output into existing movement

Use Detour for route planning while retaining existing MiMITA movement execution.

Tasks:

- Convert Detour’s next corner into existing movement intent.
- Preserve shared physics and collision.
- Preserve existing jump, dash, gravity, and grounding owners.
- Keep wall avoidance and stuck recovery explicit during transition.
- Prevent the route provider from teleporting or directly integrating actors.
- Add route-corridor retention and partial replanning.
- Replan on target changes, stale navmesh versions, dynamic obstruction, combat interruption, or no progress.
- Add bounded fallback to custom navigation or hold state.

Acceptance scenarios:

- NPC moves to a visible destination.
- NPC routes around a wall.
- NPC reaches a ramp or stairs.
- NPC stops at a tactical destination.
- NPC does not repeatedly reverse direction.
- NPC does not continuously turn into a wall.
- NPC recovers from a blocked route.
- NPC never bypasses physics.

Gate:

- Recast/Detour can route a real NPC through the actual executable while MiMITA remains authoritative for movement.

### Phase 4 — Counter-Strike tactical behavior on `dust2cyberiav4`

Make the NPC behavior resemble a difficult professional Counter-Strike team.

The first difficulty profile uses competitive omniscience: the server may provide authoritative enemy and tactical information to the NPC brain. The behavior must still apply reaction delay, movement commitment, weapon constraints, and tactical positioning so it appears deliberate rather than visually cheating.

Implement:

- team-level objective selection;
- attack and defense roles;
- spawn-to-site routing;
- tactical destination selection;
- cover and angle selection;
- last-known enemy positions;
- enemy approach prediction;
- holding an angle;
- waiting with bounded patience;
- repositioning after danger;
- coordinated pushes;
- combat interruption;
- route resumption after combat;
- objective continuation after target loss;
- bounded route diversity;
- tactical fallback when no route exists.

The navigation layer remains responsible only for spatial route feasibility. The brain remains responsible for why the NPC is moving and where it wants to be.

First acceptance scenarios:

- NPCs leave spawn.
- NPCs route toward tactical positions.
- NPCs do not run into walls.
- NPCs do not oscillate or turn around repeatedly.
- NPCs reach useful holding angles.
- NPCs wait for enemy contact.
- NPCs begin fights from meaningful positions.
- NPCs replan after contact.
- NPCs resume the objective after combat.
- NPCs behave consistently across repeated fixed-seed rounds.

Initial measurable targets across ten fixed-seed rounds:

- 100% of NPC routes have a result or explicit hold/fallback reason.
- 100% of route results include backend, actor profile, and navmesh version.
- At least 95% of travel segments make positive progress.
- Zero unrecovered wall-intersection or collision-loop failures.
- No more than one unintended reversal per NPC per 30 seconds.
- No unexplained oscillation longer than two seconds.
- At least 90% of NPCs reach a tactical destination or valid combat hold point before first engagement.
- At least 90% of surviving NPCs resume a valid route after combat interruption.
- No navigation or tile-update server-tick budget violations.
- Human review confirms the result resembles a coordinated Counter-Strike match.

### Phase 5 — Dynamic worlds and traversal

Implement dynamic navigation without rebuilding the entire world for every change.

Tasks:

- Track authoritative geometry revisions.
- Identify affected navmesh tiles from changed geometry.
- Mark only nearby relevant tiles dirty.
- Prioritize tiles intersecting active NPC corridors, destinations, or imminent traversal.
- Rebuild tiles asynchronously or at a safe server boundary.
- Keep the last valid navmesh snapshot until the replacement is ready.
- Reject stale route results.
- Replan only affected NPCs.
- Use explicit hold/custom fallback if a replacement is unavailable.
- Add doors, temporary blockers, moving objects, and changing cover to test scenarios.

Traversal:

- Map systems publish candidate links.
- Actor profiles decide link eligibility.
- Actor brains choose whether to use links.
- Traversal execution remains in MiMITA’s movement/traversal owner.
- Link success/failure updates the route corridor.
- A failed link causes bounded replan, not repeated attempts forever.

First traversal types:

- walk;
- step;
- drop;
- jump;
- crouch;
- dash;
- climb or custom only after the first types are proven.

### Phase 6 — Local avoidance experiments

Only after global route following is reliable:

- Establish a no-local-avoidance baseline.
- Test DetourCrowd as a preferred-velocity source only.
- Test RVO2 as a preferred-velocity source only.
- Feed suggestions through the movement-intent arbiter.
- Do not allow either library to own final actor integration.
- Compare doorway throughput, collision frequency, deadlocks, route progress, CPU cost, and determinism.
- Keep the provider only if it measurably improves NPC behavior.

### Phase 7 — General locomotion expansion

Generalize the navigation contract without contaminating the Counter-Strike implementation.

Add capability-specific adapters:

- ground walking;
- flying;
- hovering;
- driving;
- climbing;
- swimming;
- teleporting;
- custom scripted traversal.

Keep the shared layers:

- goals;
- navigation requests;
- map/world geometry identity;
- route validity;
- dynamic updates;
- diagnostics;
- fallback;
- authority;
- acceptance evidence.

Allow different navigation representations where Recast is unsuitable. Do not force flying or vehicle movement into a ground navmesh merely because the Counter-Strike implementation uses Recast.

## Runtime evidence workflow

For every implementation phase that changes the executable:

1. Build a newly named executable using the canonical build process.
2. Run that exact executable with `--versioninfo`.
3. Capture the printed `EVENTS_JSONL_PATH`.
4. Start the real server/client scenario using that executable.
5. Exercise the relevant NPC behavior.
6. Read the same `events.jsonl` file while the run is active and after it ends.
7. Verify the expected event chain:
   - goal created;
   - navigation request issued;
   - navmesh/profile selected;
   - route result returned;
   - traversal selected if needed;
   - movement intent produced;
   - positive progress recorded;
   - combat interruption or dynamic update handled;
   - replan or route completion recorded.
8. Identify the first missing or incorrect event when behavior fails.
9. Separate:
   - source evidence;
   - build evidence;
   - test evidence;
   - runtime journal evidence;
   - visual/human acceptance.
10. Record the result in the required changelog.

A successful build alone never counts as proof that NPC behavior changed.

## Deletion and promotion gates

Do not delete the custom navigator until:

- Recast/Detour handles all migrated route requests;
- compare mode has no unexplained divergence;
- dynamic update behavior is proven;
- traversal behavior is proven;
- route failures have explicit handling;
- server authority is preserved;
- performance budgets pass;
- repeated fixed-seed runs are stable;
- `events.jsonl` proves the new route owner;
- human review accepts the Counter-Strike behavior.

Promotion order:

1. custom authoritative;
2. compare;
3. Recast per test preset;
4. Recast per Counter-Strike server preset;
5. Recast global navigation owner;
6. optional local avoidance provider;
7. deletion of obsolete custom global/local path owners.

## Assumptions and defaults

- The first map is `dust2cyberiav4`.
- The first runtime authority is the server.
- The first migration mode is compare-first.
- Existing movement physics remains authoritative.
- Competitive omniscience is allowed for the first high-difficulty Counter-Strike profile.
- Recast and Detour are the preferred external libraries.
- DetourCrowd and RVO2 are later experiments.
- Offline navmeshes are preferred when valid; runtime baking is the fallback.
- Dynamic updates are local, versioned, bounded, and server-authoritative.
- GLB meshes inform geometry extraction, but gameplay collision remains authoritative.
- Navigation and behavior are separate: navigation answers “how can this actor get there?” and the NPC brain answers “where and why should this actor go?”

