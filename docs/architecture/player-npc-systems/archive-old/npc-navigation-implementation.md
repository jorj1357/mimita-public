# MiMITA NPC navigation implementation contract

Date: 2026-10-06
Status: Draft v1; implementation contract for review before production migration
Audience: MiMITA contributors, AI coding agents, reviewers, and the project owner
Related feature: `docs/features/gamemodes/counterstrike.md`
Related architecture: `docs/architecture/player-npc-systems/npc-movement.md`
Library inventory: `external/npc-navigation-sources.md`
Migration plan: `docs/specs/20261006plan.md`

## Purpose

This document converts the broad NPC movement and Counter-Strike behavior goals
into an implementation contract that a human and an AI agent can execute
without repeatedly rediscovering ownership, scope, assumptions, or evidence
requirements.

It exists because a broad goal such as “make the NPC navigation better” is not
specific enough to safely change a live game. A contributor must be able to
answer, before editing code:

1. What behavior is desired?
2. What behavior exists now?
3. Which system owns the desired behavior?
4. Which system is allowed to change it?
5. Which systems must not change?
6. What is the smallest experiment that proves the approach?
7. What evidence allows migration to continue?
8. What evidence requires stopping, simplifying, or deleting code?

This document is the operational contract for the Recast/Detour navigation
investigation and migration. It does not make the migration complete merely by
describing it.

## How to use this document

Every navigation-related task must begin by classifying itself as one of:

- diagnosis of existing behavior;
- documentation/specification clarification;
- isolated library experiment;
- navigation backend implementation;
- movement arbitration implementation;
- traversal implementation;
- dynamic geometry implementation;
- local avoidance experiment;
- tactical NPC behavior implementation;
- runtime validation;
- cleanup/deletion after migration.

The contributor must then state:

```text
Task class:
Current owner:
Desired owner:
Current behavior:
Desired behavior:
Out of scope:
First evidence needed:
Smallest safe change:
Deletion or migration gate:
```

If any field is unknown, the first task is investigation, not implementation.

## Non-negotiable principles

### One owner per concept

Every concept has one authoritative production owner:

- strategic objective;
- actor goal;
- navigation route;
- traversal link;
- local avoidance suggestion;
- final movement intent;
- physics result;
- target selection;
- perception/LOS;
- team information;
- damage;
- server authority;
- navigation diagnostics.

Temporary comparison implementations may coexist only when:

1. one implementation is explicitly authoritative;
2. the other is observational;
3. both receive identical inputs;
4. differences are logged;
5. the comparison has a dated deletion/decision gate.

“Temporary fallback” is not a permanent ownership model.

### Navigation produces intent, not physics

Navigation may produce:

- a reachable/unreachable result;
- a corridor;
- path corners;
- a desired direction;
- a preferred velocity;
- a traversal-link request;
- route cost and route metadata.

Navigation must not directly write:

- actor position;
- final velocity;
- gravity;
- grounded state;
- jump state;
- dash state;
- knockback;
- collision response;
- health;
- weapon state;
- combat results.

The shared actor movement and physics system remains the only final movement
executor.

### Behavior is not proven by compilation

The following are separate claims:

- source ownership is correct;
- the code builds and links;
- a unit/component test passes;
- the real executable loaded the intended configuration;
- the real server generated a route;
- the actor received navigation intent;
- physics applied the intent;
- the actor visibly reached the destination;
- a human judged the behavior acceptable.

No claim may silently combine these evidence types.

### Diagnose before adding mechanisms

When an NPC walks into a wall, do not immediately add another escape branch.
First identify the first missing or incorrect stage:

```text
goal
  -> request
  -> navmesh availability
  -> nearest start polygon
  -> nearest destination polygon
  -> corridor
  -> next corner
  -> traversal decision
  -> movement arbitration
  -> ActorIntent
  -> physics
  -> collision result
  -> progress
  -> route feedback
```

The fix belongs at the first incorrect stage, not necessarily at the visible
symptom.

## Current implementation snapshot

The current MiMITA code contains a working but layered custom system:

```text
server target selection
  -> perception and LOS
  -> memory and belief
  -> utility goal / legacy state selection
  -> NpcGoal
  -> NpcNavigator
       -> NpcNavGraph for custom long-range routing
       -> planLocalPath for local A*
       -> wall avoidance
       -> commitment and progress tracking
       -> backtrack and area escape
  -> NpcTraversalExecutor
  -> ActorIntent / InputState
  -> physicsMainUpdate
  -> shared collision and movement
```

Current source owners include:

- `src/network/server-npcs.cpp`: authoritative target selection and NPC server loop;
- `src/npc/npc.cpp`: sensing orchestration, goal mapping, movement arbitration by ordering, and physics call;
- `src/npc/npc-perception.*`: perception, LOS-related state, memory, and prediction;
- `src/npc/npc-utility.*`: utility goal scoring and selection;
- `src/npc/npc-state-machine.*` and `src/npc/npc-states.cpp`: legacy state selection and tactical movement;
- `src/npc/npc-nav-graph.*`: custom collision-derived graph and A* routes;
- `src/npc/npc-navigator.*`: local pathing, route cache, commitment, progress, and local recovery;
- `src/npc/npc-navigation.*`: wall probes, raycasts, stuck checks, obstacle checks, cover probes, and ground queries;
- `src/npc/npc-traversal.*`: inferred walk/jump/drop/dash traversal;
- `src/physics/movement/*` and `src/physics/physics-mini.cpp`: shared movement and collision execution;
- `src/debug/structured-log.*`: canonical structured diagnostics.

This snapshot is evidence of current ownership, not permission to preserve all
of it forever. The migration goal is to reduce overlapping owners.

## Target architecture

```text
Gamemode / role / team objective
                |
                v
           TeamBrain
                |
                v
           ActorBrain
                |
       NavigationRequest
                |
                v
      NavigationSystem backend
       Recast + Detour adapter
                |
       NavigationResult/corridor
                |
                v
       Traversal + local suggestions
                |
                v
        MovementIntentArbiter
                |
                v
           ActorIntent
                |
                v
       Shared actor execution
                |
                v
        Shared physics/collision
                |
                v
          Actual actor result
                |
                +---- feedback: progress, collision, blockage, success/failure
```

## Ownership contract

### TeamBrain

TeamBrain may own:

- team-level enemy reports;
- team assignments;
- bomb-carrier information;
- team objective priorities;
- rotation requests;
- squad-level route preferences;
- support and regroup information.

TeamBrain must not:

- set actor position;
- set actor velocity;
- issue physics corrections;
- directly mutate an actor's final movement input;
- run a second navigation implementation.

### ActorBrain

ActorBrain may own:

- actor goal selection;
- combat versus travel decisions;
- preferred range;
- cover selection requests;
- retreat decisions;
- whether to hold, push, rotate, plant, defuse, recover, or search;
- difficulty-dependent reaction and risk preferences.

ActorBrain must not:

- perform navmesh generation;
- directly solve collision;
- directly set actor position or velocity;
- bypass ActorIntent;
- use hidden perfect information unless the active behavior profile explicitly allows it.

### NavigationSystem

NavigationSystem owns:

- navigation geometry representation;
- Recast generation or loading;
- Detour queries;
- nearest reachable polygons;
- path corridors;
- straight-path corners;
- query filters;
- route cost;
- route validity;
- navmesh/tile versions;
- navigation-specific debug visualization;
- navigation failure classification.

NavigationSystem does not own:

- tactical goals;
- target selection;
- combat movement choice;
- aiming;
- weapon use;
- final velocity;
- physics;
- collision response;
- damage;
- actor lifecycle.

### TraversalSystem

TraversalSystem owns:

- interpretation of a selected traversal link;
- capability checks;
- link attempt state;
- link attempt timeout;
- success/failure classification;
- request to replan after failure.

TraversalSystem does not own the physics that makes the jump, dash, drop, or
rocket jump happen.

### MovementIntentArbiter

MovementIntentArbiter is the one owner that resolves competing movement
suggestions into one final `ActorIntent` for a simulation tick.

Inputs may include:

- objective travel;
- navigation corridor direction;
- traversal request;
- combat strafe or juke;
- local actor avoidance;
- safety escape;
- deliberate hold position;
- retreat;
- scripted movement primitive.

No contributor may add another direct `moveDir` override without routing it
through this arbitration owner and documenting its priority and interruption
rules.

### Shared actor execution

The shared actor system owns:

- input translation;
- movement configuration;
- fixed-tick movement;
- gravity;
- jumping;
- dashing;
- air movement;
- crouching;
- collision;
- knockback;
- grounded state;
- final position and velocity.

NPCs and human players must reach this owner through compatible actor intent.

## Backend selection

The navigation backend must be explicit:

```text
custom  = current MiMITA navigation; authoritative during transition
recast  = Recast/Detour backend; authoritative only after approval
compare = custom authoritative, Recast observational
```

The backend selection must not be inferred from game-mode name. It must be a
general navigation configuration or development override.

The first production migration must not silently run two competing routes.
Compare mode is diagnostic and must record which backend controls movement.

## General external-library adoption

The end state is that **every mode** uses the Recast/Detour backend for route
planning; no mode is allowed to keep a private navigator. The promotion order in
the migration plan still applies, but the target is global, not
Counter-Strike-only.

Requirements for general adoption:

1. The backend is chosen by general navigation configuration, not by gamemode
   name. A mode provides geometry (its map), not a navigation implementation.
2. Every supported map must be able to produce a versioned navmesh (offline
   artifact or runtime bake) before NPCs route on it. The first acceptance map is
   `dust2cyberiav4`.
3. The custom global/local route owners are deleted only after the deletion gates
   in this document pass on the representative maps.
4. Group/team movement (see `npc-group-behavior.md`) consumes navigator output; it
   must not compute its own route.
5. Local avoidance (see `npc-local-avoidance.md`) is an optional
   preferred-velocity provider layered on top of the navigator; it does not own
   routes.

This makes "NPCs use external libraries" a property of the shared navigation
owner rather than a mode-specific switch.

## Navigation backend interface

The exact C++ spelling may change after the Recast API is integrated, but the
semantic contract must remain equivalent to the following.

### Navigation capabilities

```text
canWalk
canJump
canDrop
canCrouch
canDash
canWallJump
canRocketJump
canClimb
canTeleport
canFly
maxStepHeight
maxJumpHeight
maxDropHeight
actorRadius
actorHeight
```

Capabilities belong to the actor/role/preset configuration. They must not be
duplicated independently in the navigation backend and movement system.

### Navigation request

Every request must conceptually contain:

```text
request id
actor id
simulation tick
map id
navmesh version requested
start position
destination position
goal kind/reason
capabilities
team or mode route filter
route diversity seed
maximum acceptable query distance
```

The request must not contain a final actor velocity or a teleport command.

### Navigation result

Every result must conceptually contain:

```text
request id
backend name
navmesh version
reachable
failure reason
nearest start polygon
nearest destination polygon
polygon corridor
straight-path corners
next waypoint
route length
route cost
next traversal link, if any
route validity
replan reason
```

The result is a suggestion for actor movement. It is not proof that physics
will successfully execute the route.

### Failure categories

Failures must be classified rather than collapsed into `no path`:

- no navigation data;
- start outside navigation;
- destination outside navigation;
- destination unreachable;
- invalid capability;
- corridor invalidated;
- dynamic tile pending;
- query budget exceeded;
- stale navmesh version;
- traversal link unavailable;
- actor made insufficient progress;
- backend error.

## Recast/Detour responsibilities

### Recast

Recast receives MiMITA's authoritative collision triangles and produces
navigation data for an explicitly configured agent profile.

The generated navigation data must correspond to the collision geometry actors
actually use. A separate hand-authored navigation world must not silently
diverge from gameplay collision.

### Detour

Detour performs runtime navigation queries over generated navigation data:

- nearest polygon;
- route query;
- corridor maintenance;
- straight-path extraction;
- query filtering;
- route cost;
- off-mesh connection discovery;
- route validity checks.

### DetourTileCache

TileCache may be used for bounded temporary obstacles and partial rebuilds. It
must not be assumed to solve every arbitrary destructible geometry update.

MiMITA must still own:

- geometry-change events;
- affected-tile selection;
- asynchronous build scheduling;
- version publication;
- stale route invalidation;
- safe behavior while a rebuild is pending.

### DetourCrowd

DetourCrowd is not part of the first migration. It may later provide an
avoidance suggestion, but its internal agent integration must not become the
owner of MiMITA position, velocity, collision, jumping, or dashing.

### RVO2

RVO2 is not a global navigation backend. It may later be tested for local
agent-vs-agent avoidance after global route following is reliable.

### MicroPather

MicroPather remains an isolated generic A* comparison candidate. It should not
be added to the production path if Recast/Detour already owns the required
navigation representation.

## Navigation data lifecycle

### Initial map load

The target sequence is:

```text
map selected
  -> collision geometry loaded
  -> geometry hash computed
  -> navigation configuration hash computed
  -> cached compatible navmesh searched
  -> compatible cache loaded OR navmesh built
  -> navmesh version published
  -> NPC navigation requests allowed
```

NPCs must not receive a successful-looking route from incompatible geometry.

### Cache identity

A navigation cache must include enough identity to reject stale data:

- map id;
- collision geometry hash;
- navigation-generation settings hash;
- Recast/Detour version or pinned revision;
- agent profile/schema version;
- coordinate-system version.

### Dynamic changes

The target sequence is:

```text
geometry change
  -> affected region/tile identified
  -> navmesh region marked dirty
  -> temporary blocker applied if appropriate
  -> asynchronous rebuild scheduled
  -> new navmesh version produced
  -> version published at a safe boundary
  -> affected corridors invalidated
  -> NPCs replan
```

No full-world synchronous rebuild may block a fixed 60 Hz gameplay tick.

## Route query and corridor rules

### Start and destination projection

The backend must explicitly report whether start and destination were:

- already inside a valid polygon;
- projected to a nearby polygon;
- projected beyond the allowed tolerance;
- not found.

Silent projection over a large distance is prohibited because it can make an
NPC appear intelligent while hiding a bad spawn or map anchor.

### Corridor retention

A valid corridor should remain active until one of these happens:

- actor reaches the destination tolerance;
- goal changes;
- corridor becomes invalid;
- relevant geometry changes;
- actor leaves the corridor beyond tolerance;
- traversal fails;
- sustained progress failure occurs;
- tactical decision explicitly chooses a different route.

Do not regenerate a long-distance route from a body-relative short waypoint on
every tick.

### Route diversity

Multiple valid corridors may be returned or selected using a stable actor/squad
seed. Variation must be deterministic or controlled, not random every tick.

Route diversity may consider:

- alternate sides of a wall;
- congestion;
- team assignments;
- recent route use;
- known danger;
- tactical flank preference.

These costs must be supplied by the brain or route policy. Detour must not
invent Counter-Strike strategy.

## Traversal-link contract

Traversal links are semantic route edges, not direct physics commands.

Supported conceptual link kinds:

- `WALK`;
- `JUMP`;
- `DROP`;
- `CROUCH`;
- `DASH`;
- `ROCKET_JUMP`;
- `CLIMB`;
- `TELEPORT`;
- `CUSTOM`.

Each link requires:

- stable link id;
- start and end positions;
- endpoint tolerance;
- capability requirement;
- preferred input direction;
- maximum attempt duration;
- success observation;
- failure observation;
- retry/replan policy;
- optional authoring metadata.

The execution sequence is:

```text
Detour selects link
  -> TraversalSystem validates capability
  -> MovementIntentArbiter requests normal input
  -> shared physics executes input
  -> observed movement determines success/failure
  -> corridor continues OR route replans
```

The navigation system must not animate an actor across a jump link by directly
interpolating its transform.

## Strategic and tactical decision contract

Navigation begins only after a brain has selected a travel goal.

Examples of goals:

- reach bombsite A;
- support bomb carrier;
- defend bombsite;
- rotate to teammate report;
- hunt last known position;
- reach cover;
- retreat to a safe position;
- hold a selected position;
- patrol/explore when no objective is urgent.

The goal must contain:

- goal kind;
- destination or destination provider;
- arrival tolerance;
- priority;
- interruption policy;
- resume policy;
- information confidence;
- tactical reason.

The navigation backend receives the chosen destination. It does not choose
between planting, fighting, retreating, or rotating.

## Combat interruption and resumption

### Normal travel

```text
goal selected
  -> route queried
  -> corridor followed
  -> local movement suggestion
  -> ActorIntent
```

### Visible enemy appears

The brain may temporarily select combat movement. It must record:

- previous strategic goal;
- previous navigation destination;
- whether the corridor remains valid;
- combat reason;
- expected resume condition.

### Enemy disappears or dies

The actor must either:

- resume the existing corridor if it remains valid; or
- issue a new request if the goal, information, or route changed.

Combat movement must not silently erase the long-term objective.

Remembered or radar-only information must not automatically receive the same
movement authority as a genuinely visible target unless the active behavior
profile explicitly says so.

## Movement arbitration contract

Every movement source produces a suggestion rather than directly overwriting
the final movement input.

Conceptual suggestions:

```text
objective suggestion
navigation suggestion
traversal suggestion
combat suggestion
avoidance suggestion
safety/recovery suggestion
hold-position suggestion
```

The arbiter selects one coherent result containing:

- final movement direction;
- look direction;
- jump request;
- dash request;
- down-dash request;
- crouch request;
- fire request;
- selected source/reason;
- suppressed suggestions and why they lost;
- route/traversal identifiers.

Contextual priority is allowed, but the priority must be explicit and logged.

Example default priority:

```text
immediate physics safety
  > required traversal input
  > urgent combat survival
  > deliberate combat movement
  > objective navigation
  > local avoidance
  > exploration/patrol
```

The brain may change the priority by selecting a different tactical action;
individual subsystems must not secretly change the final order.

## Progress, blockage, and recovery

### Progress owner

One progress monitor must own the authoritative definition of navigation
progress. Other systems may report observations to it but must not maintain
independent contradictory stuck timers.

Progress should include:

- net distance toward meaningful goal;
- movement along current corridor;
- time spent attempting traversal;
- wall-contact duration;
- recent route repetition;
- reversals;
- actual velocity versus requested direction.

### Stuck definition

An actor is navigation-stuck only when:

- it is expected to move;
- it has meaningful movement intent;
- it is not deliberately holding or interacting;
- it has insufficient useful progress for the configured window;
- the failure is not explained by a valid combat or physics interruption.

### Recovery ladder

Recovery should be bounded and escalate:

1. continue the corridor briefly;
2. verify physical blockage;
3. request local correction;
4. mark the failed area/edge temporarily;
5. requery or replan;
6. choose a bounded open-space escape;
7. attempt a legal traversal link if capability permits;
8. notify the brain if the objective remains unreachable.

No recovery step may repeat forever without changing state or producing a
diagnostic event.

## Local avoidance and actor collision

Local avoidance is not global navigation and is not physical collision.

The intended future flow is:

```text
Detour corridor
  -> desired travel direction
  -> optional RVO2/DetourCrowd suggestion
  -> combat/traversal constraints
  -> MovementIntentArbiter
  -> ActorIntent
  -> MiMITA physics and actor collision
```

RVO2 or DetourCrowd must not create invisible actor bubbles so large that NPCs
cannot approach targets, doorways, or combat positions.

Physical actor-vs-actor collision remains a MiMITA physics responsibility.
Avoidance may reduce unnecessary contact; it must not erase legitimate contact.

## Server authority and determinism

The server owns authoritative NPC decisions, route requests, traversal choices,
movement execution, and final results.

Clients may observe and render replicated state, but must not independently
replace the server's authoritative NPC route.

For stable results:

- process authoritative NPC decisions at fixed 60 Hz or an explicitly bounded
  decision cadence;
- use stable actor-id ordering for multi-agent processing;
- use stable equal-cost route tie-breaking;
- record navmesh version and request inputs;
- avoid unordered iteration when it affects a choice;
- separate render timing from gameplay decisions;
- test repeated runs with the same map, seed, and inputs.

The external library is not presumed deterministic merely because the same
source is used. MiMITA must verify the relevant behavior.

## Configuration ownership

Configuration must have one source of truth.

### Navigation-generation configuration

Owns:

- voxel/cell size;
- tile size;
- walkable slope;
- agent radius and height;
- step height;
- build version;
- cache behavior;
- dynamic tile limits.

This configuration affects generated nav data and should not be casually
hot-reloaded while gameplay is using the old representation.

### Navigation-runtime configuration

Owns:

- query tolerances;
- replan thresholds;
- arrival tolerance;
- corridor validation interval;
- route diversity policy;
- temporary blocker duration;
- debug visualization.

These values may be hot-reloadable where safe.

### Behavior configuration

Owns:

- aggression;
- preferred range;
- retreat behavior;
- memory duration;
- team support weights;
- objective priorities;
- combat movement weights.

It must not duplicate navmesh geometry-generation values.

## Runtime diagnostics contract

All navigation diagnostics use `StructuredLogger` and the canonical:

```text
logs/<date>/<run>/events.jsonl
```

Do not create a navigation-specific unmanaged debug file.

### Required state-change events

- `npc.navigation-backend-selected`;
- `npc.navigation-data-loaded`;
- `npc.navigation-data-built`;
- `npc.navigation-tile-invalidated`;
- `npc.navigation-request`;
- `npc.nav-plan-created`;
- `npc.nav-plan-failed`;
- `npc.nav-replan`;
- `npc.nav-corridor-invalidated`;
- `npc.traversal-selected`;
- `npc.traversal-succeeded`;
- `npc.traversal-failed`;
- `npc.movement-intent-selected`;
- `npc.stuck`;
- `npc.stuck-recovery`.

### Required navigation-request fields

- actor id;
- simulation tick;
- map id;
- backend;
- navmesh version;
- start position;
- destination position;
- goal kind/reason;
- capabilities;
- start polygon;
- destination polygon;
- success/failure;
- failure reason;
- corridor polygon count;
- route length/cost;
- replan reason.

### Required movement sample fields

- actor id;
- tick;
- position;
- actual velocity;
- goal;
- destination;
- current waypoint;
- corridor state;
- desired navigation direction;
- combat suggestion;
- avoidance suggestion;
- final ActorIntent;
- distance moved;
- net progress;
- stuck state;
- traversal state;
- actual physics result.

Default logging must be bounded by state changes, fixed sample windows, or
scenario summaries. Per-tick logging is allowed only under an explicit
diagnostic mode.

## Debug visualization contract

When an NPC is selected, the runtime should be able to show:

- current goal;
- current target and perception state;
- navmesh version;
- current polygon;
- destination polygon;
- corridor;
- path corners;
- next waypoint;
- traversal link;
- desired direction;
- local avoidance suggestion;
- final ActorIntent;
- actual velocity;
- stuck timer;
- current replan reason.

Suggested colors:

- green: valid route;
- blue: path corners;
- yellow: current polygon;
- purple: destination;
- magenta: traversal link;
- orange: avoidance direction;
- red: blocked LOS.

Visualization is evidence of state, not a substitute for runtime behavior.

## Human/AI collaboration protocol

This section exists to handle ambiguous requests, changing goals, and
insufficient human context.

### When the human request is vague

The AI must translate the request into:

```text
Observed symptom:
Desired visible result:
Affected actor/mode/map:
Likely owner:
Evidence currently available:
Evidence missing:
Smallest investigation:
Possible implementation choices:
Decision required from human:
```

The AI should make progress on read-only investigation without waiting for
permission. It must stop before a materially different behavior or scope is
chosen.

### When the goal changes during work

The AI must classify the new request as one of:

- clarification of the current goal;
- additive requirement;
- replacement requirement;
- unrelated request;
- implementation detail;
- new future scope.

If it replaces the goal, the previous plan must not be silently continued. The
AI should state:

```text
Previous goal:
New goal:
What remains valid:
What is now out of scope:
What files/owners are affected:
```

### When the human asks for “make it better”

The AI must not convert that directly into code. It must ask or infer a
measurable visible outcome, such as:

- leaves spawn within a defined duration;
- reaches Site A within a defined duration;
- stops wall grinding;
- reduces reversals;
- improves route success;
- reduces repeated jump attempts;
- maintains positive net progress;
- wins more rounds for a stated tactical reason.

If the user does not specify a metric, the AI may propose one but must label it
as an assumption.

### Before editing code

The AI must report:

1. current owner;
2. proposed owner;
3. current-to-target gap;
4. exact files likely to change;
5. exact files explicitly protected from change;
6. expected runtime evidence;
7. deletion or rollback plan;
8. whether the change is reversible;
9. whether a human choice is required.

### After editing code

The AI must report separately:

- source changes;
- build result;
- component/unit-test result;
- real runtime result;
- JSONL evidence;
- human visual/gameplay review;
- remaining uncertainty.

### When previous attempts exist

The AI must inspect previous changelogs and regressions before adding a new
mechanism. It must answer:

- What did the previous attempt believe?
- What evidence supported it?
- What did runtime actually prove?
- Is the old mechanism still active?
- Did it create another owner?
- Can it be simplified or deleted?

Repeated attempts must not accumulate duplicate recovery systems without an
explicit ownership decision.

## Implementation phases and gates

### Phase 0 — integration readiness

Phase 0 is now the approved preparation contract for beginning the external
navigation-library integration. It records the project decisions that are
known, selects safe defaults for the decisions that were not yet explicit,
and defines what must be proven before production NPC behavior changes.

#### Decisions recorded by the project owner

The following decisions are authoritative for the first implementation slice:

1. Navigation data must support both offline-generated and runtime-generated
   navmeshes.
2. The long-term target is every supported map, rather than a permanently
   hand-authored whitelist. `dust2cyberiav4` is the first acceptance map.
3. The system must support static and dynamic worlds.
4. Agent radius, agent height, step height, slope limits, and related geometry
   values must be derived from the actual MiMITA actor and collision contracts,
   not copied from a generic Recast sample.
5. Dynamic navigation updates should be bounded to relevant regions. When an
   obstacle is created or changed near an NPC's active route, destination, or
   imminent traversal, the affected tile region is marked dirty and rebuilt or
   refreshed. A whole-world rebuild must not be the normal per-obstacle path.
6. Traversal links are capability-aware and actor-specific. The shared world
   may provide link definitions, but each actor decides which links it can use
   based on its movement capabilities, current state, role, and policy.
7. Navigation decisions run on the authoritative server for now. Clients may
   receive debug or presentation data but must not authoritatively choose NPC
   routes.
8. Existing movement remains authoritative during the initial migration.
   DetourCrowd and RVO2 are optional later tools, not immediate replacements
   for MiMITA physics, movement, jumping, or collision authority.

#### Recommended defaults for unresolved decisions

These defaults are part of the plan unless a later human decision records a
different policy:

- Use Recast for navmesh generation, Detour for queries and corridor
  following, and DetourTileCache for bounded dynamic-tile updates.
- Keep navmesh generation behind a MiMITA-owned adapter. External library
  types must not spread through gameplay, network, JSON, or actor code.
- Prefer offline navmesh artifacts for stable map geometry, with runtime bake
  as the fallback for missing, incompatible, edited, or procedurally changed
  maps. The cache identity must include map identity, geometry revision,
  generation parameters, library revision, and coordinate/unit convention.
- Use a `custom` baseline, a `compare` mode, and an explicit `recast` mode.
  `compare` must not alter the authoritative result. Promotion to `recast`
  happens per server preset or test scenario before it becomes the global
  default.
- Use the server's fixed 60 Hz simulation as the authority. Navmesh baking and
  tile rebuilding may run off the gameplay tick, but route-result publication
  must occur at a deterministic safe boundary and never mutate collision
  state from a render callback.
- Treat a dynamic obstacle as navigation geometry only when it blocks or
  changes a traversable surface. Nearby irrelevant changes should not dirty a
  tile. A route query may request a local refresh when its corridor intersects
  a dirty or stale region, subject to a bounded time and work budget.
- Let the MiMITA build own the integration target and pin the external source
  revision. Recast/Detour demo, sample, and standalone test targets must not
  become part of the game executable. The adapter owns include and target
  boundaries; gameplay links only to the adapter target.

#### Geometry and actor-profile readiness

Before the first real-map bake, record the measured values for the player/NPC
collision profile from the authoritative actor and collision implementation:

- capsule or equivalent footprint radius;
- standing height;
- crouching height, if crouching is supported by the first slice;
- maximum walkable slope;
- maximum step or ledge height;
- minimum traversable clearance;
- jump start/landing envelope, if jump links are enabled;
- world-unit-to-Recast-unit conversion;
- map-coordinate handedness and up axis.

The profile must be generated from the same collision/avatar contract used by
the actor at runtime. GLB model bounds may inform the visual envelope, but a
render mesh must not silently become the gameplay collision authority. If
different actor classes need different radii or heights, the navmesh/profile
contract must state whether they share one conservative mesh or use separate
agent-profile meshes.

#### Static and dynamic data lifecycle

The first implementation must support this lifecycle:

1. Identify the map and geometry revision.
2. Load a compatible offline navmesh when one exists.
3. Otherwise collect authoritative collision geometry and bake a runtime
   navmesh before NPC route requests are enabled.
4. Publish a versioned navmesh snapshot to the server navigation owner.
5. Mark only affected tiles dirty when dynamic geometry changes.
6. Rebuild or refresh those tiles within a bounded work budget.
7. Keep the previous valid snapshot available until the replacement is valid.
8. Reject route results whose snapshot version is no longer current.
9. Replan only affected actors, with rate limits and a safe fallback when a
   replacement is temporarily unavailable.

The system must define what happens when runtime baking is not finished:
NPCs remain in a safe non-teleporting state, retain a valid previous route if
one exists, or use a bounded direct/hold behavior. They must not invent a
route through unknown geometry.

#### Rollout and fallback contract

The rollout order is:

1. `custom`: existing navigator remains authoritative.
2. `compare`: custom remains authoritative while Recast/Detour receives the
   same request and reports route, failure, cost, version, and timing.
3. `recast`: Recast/Detour supplies the route, but the existing MiMITA motor,
   collision, and traversal executor remain authoritative.
4. Optional local-avoidance experiment: evaluate DetourCrowd or RVO2 against
   the existing actor-collision behavior in isolated scenarios.
5. Later targeted migration: promote only the components that measurably
   improve behavior and retain explicit fallback ownership.

Fallback must be explicit in the request/result contract and diagnostics. A
failed Recast query may fall back to the custom navigator only when the server
policy permits it; the result must identify `recast`, `custom`, or `hold` as
the actual source. A fallback is a recovery mechanism, not permission to keep
two permanently competing production owners.

#### Traversal-link ownership

World geometry and map/objective systems may publish candidate traversal links
with stable IDs and endpoint metadata. The actor navigation capability profile
selects whether a link is usable. The actor owns the final selection and
execution state, including:

- whether the actor can jump, drop, crouch, dash, climb, or use a custom link;
- whether the current weapon, role, health, or combat state permits it;
- the movement input and action command used to execute it;
- success, failure, interruption, and cooldown;
- replanning after the actor reaches, misses, or is displaced from the link.

This preserves shared map knowledge while allowing different actors to take
different legal routes.

#### Phase 0 deliverables

- current ownership map for `NpcNavigator`, `NpcNavGraph`, `NpcNavigation`,
  `NpcTraversal`, shared movement, and the future Recast adapter;
- build integration decision and pinned external revisions;
- measured actor geometry/profile table from authoritative collision code;
- map geometry-source inventory for `dust2cyberiav4`;
- offline-navmesh artifact format and runtime-bake fallback contract;
- static/dynamic tile invalidation and bounded-refresh policy;
- explicit backend selection and fallback configuration;
- server-only authority and safe-boundary publication rules;
- route-owner, movement-intent, navmesh-version, and fallback diagnostics;
- representative Counter-Strike runtime trace using the current custom path;
- first acceptance scenario list and threshold recording format.

#### Phase 0 gate

The gate passes only when a reviewer can answer, from source and
`events.jsonl`:

- which system authored the route;
- which navmesh version and actor profile it used;
- whether the route came from offline data, runtime bake, custom fallback, or
  a hold state;
- which movement/traversal owner executed the result;
- the first divergence when the NPC fails to progress; and
- whether the server remained authoritative.

No production backend switch is allowed until this evidence exists.

### Phase 1 — isolated Recast build/query proof

Deliver:

- isolated backend adapter;
- one minimal collision world;
- one generated navmesh;
- one Detour path query;
- one visualized corridor;
- no production movement switch.

Gate:

- real MiMITA collision triangles produce a valid, inspectable route.

### Phase 2 — existing motor integration

Deliver:

- Detour next-corner result converted into the existing ActorIntent path;
- shared physics unchanged;
- one NPC follows the route in the real executable;
- runtime JSONL evidence.

Gate:

- the NPC reaches a real destination without teleporting or bypassing physics.

### Phase 3 — compare mode

Deliver:

- custom and Recast receive identical requests;
- one backend remains authoritative;
- route differences are bounded and logged;
- repeated fixed-seed comparison.

Gate:

- differences are understood, accepted, or assigned to a concrete follow-up.

### Phase 4 — real Counter-Strike map

Deliver:

- `dust2cyberiav4` as the first acceptance map, with the same pipeline designed
  to generalize to every supported map;
- spawn-to-site scenarios;
- doorway, ramp, wall, and objective routes;
- visible route diagnostics;
- live `events.jsonl` evidence.

Gate:

- real NPCs leave spawn, make positive progress, route around obstacles, and
  resume objectives after a combat interruption.

#### First acceptance behavior and proposed thresholds

The qualitative target is Counter-Strike-like team behavior: NPCs leave spawn,
choose sensible routes toward the enemy or objective, hold useful angles,
avoid repetitive wall collisions and turn loops, and enter fights in a way
that resembles a coordinated human match. “Looks human” remains a human
acceptance judgment; the following measurements make obvious failures
observable and repeatable.

For an initial `dust2cyberiav4` acceptance run, record at least ten fixed-seed
rounds with the same server preset and actor profiles. The proposed starting
thresholds are:

- 100% of NPCs receive a valid route or an explicit hold/fallback reason;
- 100% of route results identify the navmesh version, backend, and actor
  profile;
- 95% or more of planned travel segments make positive progress toward the
  active goal within the configured observation window;
- zero unrecovered wall-intersection or collision-loop failures;
- no more than one unintended route reversal per NPC per 30 seconds, excluding
  combat-driven replans and valid traversal failures;
- no repeated left/right or forward/back oscillation lasting more than two
  seconds without a logged blockage, target change, or traversal decision;
- at least 90% of NPCs reach their assigned tactical destination or a valid
  combat hold point before the round's first engagement window;
- after an enemy appears, NPCs stop or reposition at a valid angle rather than
  blindly continuing through the opponent;
- after a combat interruption, at least 90% of surviving NPCs resume a valid
  objective or tactical route;
- no server-tick budget violation caused by navigation or tile updates in the
  acceptance trace.

These are initial engineering thresholds, not a substitute for the project
owner's final visual review. Any threshold that fails must be classified as a
route-generation, traversal, movement-execution, combat-decision, dynamic-
update, or presentation problem before tuning begins.

### Phase 5 — migration and deletion

Deliver:

- Recast/Detour becomes the single production global navigation owner;
- callers are migrated;
- old custom global/local route owners are deleted or reduced to explicitly
  justified responsibilities;
- tests name the new owner.

Gate:

- source, build, runtime, performance, and human evidence all pass.

### Phase 6 — optional local avoidance

Only after global navigation is reliable:

- test no-avoidance baseline;
- test RVO2 or DetourCrowd suggestion;
- compare doorway throughput, deadlocks, collisions, net progress, and cost;
- integrate only if it measurably improves behavior.

### Phase 7 — advanced movement

Only after reliable route following:

- semantic traversal links;
- crouch links;
- dash links;
- wall jumps;
- rocket jumps;
- advanced combat movement;
- human movement primitives;
- opponent adaptation.

## Current-to-target migration map

| Current system | First treatment | Eventual treatment |
|---|---|---|
| `NpcNavGraph` | Keep as custom comparison backend | Delete after Recast parity |
| `planLocalPath()` | Keep for compare mode | Delete when Detour corridor owns route following |
| `NpcNavigator` | Preserve goal/cache shell while replacing backend | Retain as MiMITA navigation facade or simplify |
| `NpcNavigation::wallAvoidDirection` | Keep as local safety during transition | Move into one arbiter/avoidance owner |
| `NpcNavigation::isStuck` | Instrument and compare | Consolidate into progress owner |
| `updateAreaEscape` | Keep as bounded recovery | Feed recovery suggestion to arbiter |
| `NpcTraversalExecutor` | Keep but add semantic link input | Make link execution explicit |
| `computeStateMovement` | Preserve behavior while tracing | Reduce direct movement ownership |
| `makeNavGoal` | Keep compatibility adapter | Convert to `NavigationRequest` producer |
| shared physics | Do not replace | Remains final owner |

## Required runtime scenarios

Every implementation phase that changes behavior must use real runtime scenarios
where applicable.

### Clear travel

Setup:

- real executable/server;
- known map;
- one NPC;
- no enemies;
- goal at a known distance.

Expected:

- navigation data is loaded;
- path query succeeds;
- ActorIntent contains travel input;
- physics applies movement;
- positive net progress;
- destination reached.

### Full wall

Expected:

- route goes around wall;
- no repeated jump spam;
- no endless wall contact;
- corridor remains stable;
- destination is reached.

### Low obstacle

Expected:

- traversal link or configured obstacle policy is selected;
- one legal jump attempt occurs;
- physics determines the result;
- failed attempt produces replan/failure evidence;
- no repeated jump loop.

### Ramp/stairs

Expected:

- actor stays on walkable surface;
- no midpoint reversal;
- no artificial teleport or vertical correction;
- progress remains positive.

### Corner trap

Expected:

- stuck detector identifies insufficient progress;
- recovery chooses a bounded alternative;
- actor leaves the local area;
- recovery does not repeat forever.

### Doorway crowd

Expected:

- actors approach the doorway;
- physical collision remains real;
- local avoidance does not create giant invisible bubbles;
- no permanent deadlock;
- throughput and net movement are measured.

### Combat interruption

Expected:

```text
objective travel
  -> target visible
  -> combat decision
  -> combat movement
  -> target lost/dead
  -> objective resume/replan
```

The JSONL trace must show the reason for every ownership transition.

## Performance requirements

Measure separately:

- navmesh build time;
- tile rebuild time;
- query time;
- corridor update time;
- avoidance time;
- decision time;
- memory;
- allocations in fixed-tick paths;
- server tick cost;
- runtime logging cost.

Test at:

- 1 NPC;
- 10 NPCs;
- 30 NPCs;
- 100 NPCs;
- synthetic 1000-agent query/avoidance benchmark.

Do not claim support for a scale until it is measured on the authoritative
server path.

## Definition of implementation complete

The first Recast/Detour integration is complete only when all of these are
true:

1. MiMITA collision geometry produced the navigation data.
2. The backend is isolated behind a MiMITA adapter.
3. The backend selection is explicit.
4. The server owns authoritative route decisions.
5. The real NPC requested a route.
6. Detour returned a corridor or an explainable failure.
7. The result reached ActorIntent.
8. Shared physics executed the intent.
9. The real NPC visibly moved through a real map.
10. `events.jsonl` proves the state flow.
11. Wall, ramp, doorway, and destination scenarios pass.
12. No duplicate permanent navigation owner was added.
13. Human review confirms the visible result.

It is not complete because:

- the library compiled;
- a selftest passed;
- a path existed in a synthetic graph;
- a debug mesh was generated;
- a route was returned without physics execution.

## Deletion gates

Do not delete current navigation code until:

- the new backend passes the isolated proof;
- compare mode has run on representative scenarios;
- real-map runtime evidence exists;
- the new backend handles route failure;
- human acceptance confirms behavior;
- the old responsibility is no longer called in production;
- tests and diagnostics identify the new owner;
- rollback remains possible until the migration is accepted.

When deletion is approved, delete responsibilities rather than merely hiding
them behind another fallback.

## Open decisions requiring human approval

The Phase 0 decisions above resolve the initial direction for the following
questions:

- Recast/Detour is the intended backend to prove and progressively adopt.
- Static and dynamic geometry are both required.
- `dust2cyberiav4` is the first acceptance map; the architecture must
  generalize to every supported map.
- The initial actor profile comes from authoritative MiMITA collision/avatar
  data, with the measured values recorded before baking.
- Existing movement remains authoritative during the first migration slice.
- DetourCrowd and RVO2 are future experiments, not immediate replacements.

The following decisions still require explicit human approval or measurement
before the corresponding phase can pass:

1. The exact measured geometry values for each initial actor profile.
2. Whether jump links are required in the first `dust2cyberiav4` route slice,
   or are enabled immediately after basic walk routing.
3. The maximum navigation query, tile-update, memory, and server-tick budgets.
4. The exact offline navmesh artifact location and versioning format.
5. The exact dynamic-obstacle dirty-radius and rebuild scheduling policy after
   profiling representative maps.
6. Which current custom systems may remain after Recast/Detour becomes the
   global navigation owner.
7. Whether the proposed acceptance thresholds are approved as the first human
   review baseline.

If any decision changes the owner, scope, runtime behavior, or deletion plan,
record the decision in the relevant changelog and update this contract before
implementation continues.

## Final guiding rule

The goal is not to make an external library appear in the repository.

The goal is:

```text
human or AI chooses a meaningful objective
  -> navigation understands legal space
  -> movement intent is generated
  -> shared MiMITA physics executes it
  -> actual results feed back into the brain and navigation
```

An NPC is successful when it looks intelligent because it understands its goal,
space, capabilities, and opponent—not because it bypasses the rules available
to a human player.
