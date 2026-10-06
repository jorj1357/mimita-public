9 8 2026 1019 est jorj todo - explain 

npc movment not super good right now
we need to ahve like super duper advanced jukes and stuff
record humanmovement, and replay it if ur in that part of the map 
or just general functions , figure out what is going on  as a human in movement optiosn we pick, adn then do those things 

2026 10 06 1333 jorj todo - 
now we are just gonna use C:\mimita-v9\external\npc-navigation-sources.md
external libraries for 
well, we havnet implemented it yet
but i want to use thees for npc navigation
instaed of doing it all oruselves 

ok heree is v1 2026 10 06 1354 jorj

MiMITA NPC Movement, Navigation, Juking, and Human-Like Motion Specification v1

Date: 2026-10-06
Status: Draft v1 / target architecture and behavior
Primary document: docs/architecture/player-npc-systems/npc-movement.md
Navigation source inventory: external/npc-navigation-sources.md

1. End goal

MiMITA NPC movement should eventually be sufficiently human-like that, from movement alone, it is difficult to determine whether an actor is controlled by:

    human
    or
    NPC

NPCs should not merely:

    find target
    → walk toward target
    → shoot

They should exhibit:

    purposeful travel;

    smooth physical movement;

    tactical positioning;

    intentional acceleration/deceleration;

    momentum management;

    combat strafing;

    juking;

    traversal;

    prediction;

    route variation;

    human-like imperfections;

    context-sensitive movement choices;

    advanced reusable movement techniques;

    adaptation to opponents.

The target is not merely:

    NPC reaches destination

The target is:

    NPC moves like an extremely competent player
    while still obeying the same physical rules as players.

2. Fundamental movement rule

There must be a distinction between:

    WHY am I moving there?

    WHERE should I travel?

    HOW do I physically move there?

These are separate responsibilities.

Conceptually:

    Game / Tactical Objective
            ↓
    Actor Brain
    "where/why do I want to go?"
            ↓
    Navigation Backend
    "what traversable corridor gets me there?"
            ↓
    Local Movement / Combat / Avoidance
    "what direction should I move this moment?"
            ↓
    ActorIntent
            ↓
    Shared Actor Movement
            ↓
    Shared Physics / Collision

No subsystem should silently take ownership of all three layers.

3. Desired visible behavior

    A good NPC should look purposeful.

When traveling:

    movement should usually be direct;

    useful net displacement should remain high;

    unnecessary stopping should be uncommon;

    turns should look intentional;

    NPC should not constantly reverse;

    NPC should not grind against geometry;

    NPC should not twitch left/right every frame;

    NPC should not jump randomly without purpose.

An NPC may stop because:

    stopping provides tactical advantage;

    it is managing momentum;

    it is holding an angle;

    it is waiting for a teammate;

    it is interacting with an objective;

    it is attempting to juke or deceive an opponent;

    it has reached its intended destination.

Stopping should have a reason.

4. Human-like movement goal

NPC movement should eventually include the kinds of small behaviors human players naturally develop:

    animation canceling;

    movement canceling;

    tiny directional corrections;

    momentum manipulation;

    strafing;

    baiting;

    stopping suddenly;

    changing rhythm;

    corner checking;

    peeking;

    retreating;

    pushing;

    pursuit;

    route switching;

    jump timing;

    crouch timing;

    air strafing;

    dashes where the movement preset permits them.

The NPC should not appear as if it is executing a rigid scripted locomotion animation.

It should appear to be actively controlling an actor.

5. Human-like does not mean random

    Unpredictability is desirable.

Randomness by itself is not.

Bad:

    left
    right
    left
    right
    jump
    spin
    left
    right

with no relationship to the situation.

Good:

enemy predicts left dodge
    → NPC recognizes opponent tendency
    → NPC changes rhythm
    → NPC moves somewhere harder to hit

Movement variation must serve an objective.

The core question is:

    Which available movement places this actor
    in the most advantageous future state?

6. What advanced movement looks like

An advanced NPC may understand patterns such as:

    enemy low health
    enemy low ammo
    enemy historically retreats right
            ↓
    predict retreat
            ↓
    cut off that route
            ↓
    aim projectile where enemy will probably move

An NPC may learn:

    this opponent frequently dodges right at low health

and later exploit that tendency.

The goal is for NPCs to become difficult because they:

    observe;

    remember;

    predict;

    adapt;

not because they secretly violate game rules.

7. Movement should remain smooth

    All actors should visually move smoothly.

NPC movement must not look:

    frame-by-frame;

    indecisive;

    twitchy;

    discontinuous.

Decision-making may occur at lower rates than physics, but movement execution remains smooth through the normal shared fixed-tick actor system.

8. Configurable movement personality

    Not every NPC must move identically.

Behavior profiles may configure:

    jump frequency;

    aggression;

    corner style;

    preferred range;

    juke frequency;

    willingness to retreat;

    willingness to chase;

    movement unpredictability;

    route variation;

    combat strafe behavior;

    movement commitment;

    reaction time.

Examples:

    calm
    aggressive
    defensive
    unpredictable
    movement-heavy
    sniper
    zombie
    brute

These tune generalized systems.

They do not create entirely separate movement implementations.

9. Global navigation versus physical movement

    Navigation is fundamentally GPS.

The navigation system, whatever it is implemneted using, whether its an external library, new code made in the repo, or a combination of both, bc we are working on adding npc navigation libraries 2026 10 06 1356 jorj,  should answer:

    I am here.

    I want to reach there.

    What traversable corridor/path should I use?

It should not directly own:

    player/NPC physics;

    jumping execution;

    crouching execution;

    dash execution;

    wall jumping;

    air strafing;

    rocket jumping;

    actor collision response;

    combat aiming;

    tactical decision-making.

Those remain MiMITA systems.

10. External navigation libraries

Current local external sources:

    Recast Navigation

    external/recastnavigation/

Pinned commit:

    9f4ce64

Modules:

    Recast
    Detour
    DetourTileCache
    DetourCrowd
    DebugUtils

Preferred role:

    global navigation backend

11. Recast responsibility

    Recast should primarily convert MiMITA's collision geometry into navigable spatial data.

Conceptually:

    MiMITA collision triangles
            ↓
    Recast
            ↓
    navigation mesh

    Recast should help determine:

    walkable areas;

    unwalkable areas;

    ramps;

    stairs;

    floors;

    connected surfaces;

    navigation regions.

12. Detour responsibility

Detour should primarily perform runtime queries over the generated navigation representation.

Conceptually:

    current position
    +
    goal position
    +
    navigation mesh
            ↓
    Detour
            ↓
    path corridor

    Detour should help with:

    path queries;

    nearest reachable point;

    alternate routes;

    path corridors;

    dynamic route adjustment;

    multi-floor navigation;

    route recovery.

13. DetourTileCache

DetourTileCache should be investigated for:

    tiled navigation;

    dynamic obstacles;

    temporary blockers;

    partial rebuilds;

    live map changes.

Potential use:

    map edit
    → affected navigation tile invalidated
    → rebuild tile
    → existing NPCs repath

The whole navigation world should not require full reconstruction because one small object changed.

14. DetourCrowd

DetourCrowd may be investigated later.

It is not automatically the desired local avoidance implementation.

Potential uses:

    local crowd movement;

    corridor following;

    agent separation;

    velocity planning.

It must not become the actor physics owner.

MiMITA remains responsible for the final physical movement.

15. RVO2 / ORCA

Local source:

    external/rvo2/

Pinned commit:

    75822bb

RVO2 may be useful for local multi-agent avoidance.

Potential architecture:

    Detour
    global desired route
            ↓
    RVO2 / ORCA
    locally suggested safe velocity
            ↓
    MiMITA movement
            ↓
    MiMITA physics
            ↓
    actual resulting motion

RVO2 does not own:

    collision;

    final position;

    actor velocity;

    jumping;

    combat;

    physics.

It may only provide useful local steering information.

16. MicroPather

Local source:

    external/micropather/

Pinned commit:

    33a3b84

MicroPather is an optional comparison/reference for graph search.

It is not a collision-to-navmesh solution.

Possible role:

    standalone A* experiments;

    graph solver comparison;

    educational/reference implementation.

    It should not be integrated merely because it exists.

17. Library integration rule

External libraries are tools.

They are not architectural authorities.

Use each library only for the problem it solves well.

Do not force:

    Recast
    RVO2
    MicroPather

into a subsystem merely because they are available.

Integration should answer:

    Does this reduce custom code?
    Does this improve reliability?
    Does this improve runtime behavior?
    Does this provide a proven implementation?
    Does it preserve MiMITA's ownership model?

If not, do not use it.

18. Source of truth for navigation geometry

MiMITA's actual world collision triangles are the navigation geometry source of truth.

Conceptually:

    authoritative world triangles
            ↓
    navigation generation

The navmesh must correspond to the geometry actors physically collide with.

Do not maintain an unrelated hand-authored navigation world that silently diverges from collision.

19. Navigation mesh generation

Initial target:

    map loads
            ↓
    collision triangles loaded
            ↓
    Recast builds navmesh
            ↓
    navmesh available to NPCs

The generated navigation data should be cacheable.

Potential cache:

    map hash
    +
    navigation-generation configuration hash
            ↓
    cached navmesh

If hashes match:

    load cache

rather than rebuilding unnecessarily.

20. Tiled / chunked navigation

Navigation should be tiled/chunked.

Reasons:

    large maps;

    procedural worlds;

    map editing;

    destruction;

    dynamic objects;

    partial rebuild;

    memory scaling;

    Infinite Dungeon Slayer;

    Infinite Zombie Tower;

    future massive worlds.

A local map edit should invalidate relevant navigation tiles rather than the entire world.

21. Live navigation rebuilds

Long-term:

    map edited
    or
    navigation-relevant geometry changes
            ↓
    affected tiles marked dirty
            ↓
    rebuild
            ↓
    NPCs receive updated path information

NPCs must handle route invalidation cleanly.

22. One navigation representation

Preferred initial philosophy:

    one strong shared navigation representation

    rather than separate unrelated navigation systems for every actor type.

Actors may interpret the same world differently depending on capability.

Examples:

    human-sized actor
    crawler
    large brute
    flying actor
    vehicle

Whether one exact navmesh can efficiently serve every actor size remains an engineering question.

This is:

NEEDS_LIBRARY_EXPERIMENT

Recast supports agent-radius/height assumptions, so the project should test whether:

    one base mesh + capability filtering;

    multiple agent meshes;

    layered navigation representations;

best fit MiMITA.

Do not assume one mesh is sufficient if runtime evidence proves otherwise.

23. Actor capabilities

Every actor should expose movement capabilities.

Example:

    canWalk
    canJump
    canDrop
    canCrouch
    canWallJump
    canDash
    canFly
    canHover
    canCrawl
    canRocketJump
    canUseLadder
    vehicleType
    maxStepHeight
    maxJumpDistance
    maxDropHeight

Navigation may use these to decide what traversal links are legal.

24. Special traversal

A path is more than flat walking.

MiMITA requires traversal such as:

    low-obstacle jump;

    gap jump;

    ledge drop;

    crouch passage;

    ladder;

    wall jump;

    dash traversal;

    rocket jump;

    flying traversal;

    hovering;

    crawling;

    vehicle-specific traversal.

These should be represented in a generalized way.

25. Traversal links

Possible target:

    normal nav corridor
        ↓
    special traversal link
        ↓
    normal nav corridor

Example:

    walk
    → JUMP_GAP
    → land
    → continue

The navigation system may identify:

this route requires jump traversal

MiMITA owns:

how jumping is physically executed

26. Capability filtering

An actor may only receive paths it can actually execute.

Example:

    Zombie:
    canJump = false

    route containing jump link
    → invalid for Zombie

Another actor:

    movement-heavy player:
    canJump = true

same route
→ valid

27. Low obstacle traversal

If an NPC's lower body is blocked but sufficient upper clearance exists:

    low obstacle
            ↓
    jump
            ↓
    continue route

This should be deliberate traversal behavior.

It should not require waiting until the actor is completely stuck.

28. Gap jumping

Navigation should eventually understand traversable gaps.

    Requirements include:

    reachable takeoff point;

    actor jump capability;

    sufficient horizontal/vertical jump capacity;

    valid landing area;

    clear trajectory.

The library may help represent the link.

MiMITA determines actual physical feasibility and executes the jump.

29. Ledge drops

    NPCs may intentionally drop from ledges when:

    drop is physically legal;

    drop is tactically acceptable;

    expected damage/risk is acceptable;

    route benefit justifies it.

Do not make actors incapable of descending merely because a normal walking connection does not exist.

30. Crouch traversal

Future navigation may identify low-clearance corridors requiring crouch.

Example:

    standing height blocked
    crouched height clear
            ↓
    CROUCH traversal link

MiMITA crouch mechanics perform the actual movement.

31. Wall jumps

Modes that permit wall jumping may expose wall-jump traversal.

Navigation may determine:

route possible using wall jump

but must not execute it directly.

MiMITA movement performs:

jump
→ wall contact
→ jump input
→ resulting physics

32. Rocket jumping

Rocket jumping is a possible advanced traversal capability.

A navigation solution may eventually identify:

    destination unreachable by normal movement
    but reachable using rocket jump

The movement/combat system must then execute the required inputs.

This is advanced future behavior, not a requirement for the first Recast integration.

33. Flying actors

Flying actors still need spatial understanding.

They may use:

    collision triangles;

    world bounds;

    obstacle queries;

    target information;

but probably should not be forced into ground navmesh walking semantics.

Recast may still provide useful ground/world reference information, but a true flying navigation system may require a different representation.

This is:

FUTURE / NEEDS DESIGN

34. Hovering actors

    Hovering actors may combine:

    ground-route information;

    fixed hover height;

    obstacle avoidance;

    local steering.

Again, reuse Recast information where useful without forcing it into an inappropriate problem.

35. Crawling actors

Crawling actors may share the same navigation world but use:

    lower actor height;

    different clearance;

    different movement speed;

    different traversal permissions.

Whether this requires separate Recast agent profiles should be experimentally determined.

36. Vehicles

Vehicles require path planning but also have:

    turning radius;

    acceleration;

    braking;

    larger footprint;

    different collision;

    inability to rotate in place;

    potentially different terrain restrictions.

The shared world data may remain useful.

Vehicle-specific path execution is a later subsystem.

37. Local agent avoidance

NPCs should not permanently block one another.

When many actors approach the same doorway, acceptable outcomes include:

    smooth local flow;

    queue;

    alternating passage;

    small physical shoves;

    alternate routes.

    Unacceptable:

    permanent deadlock;

    every NPC occupying same target point;

    endless left/right oscillation;

    actors refusing to approach each other at all.

38. Physical actor collision remains real

2026 10 06 1412 jorj todo actor vs actor collision isnt reall working yet so we need to fix that also networking representaiton of hybrid movement mode and ragdoll is choppy so we need to ensure that  its smooth as well

NPCs should be able to approach other actors closely.

Avoidance should not create huge invisible bubbles.

MiMITA should eventually support proper actor-vs-actor physical collision.

Actors may:

body block;

touch;

push;

shove;

according to physics rules.

Local avoidance should help actors avoid unnecessary collisions.

It should not erase physical interaction.

39. Local avoidance architecture

Possible architecture:

Detour path corridor
        ↓
desired travel direction
        ↓
RVO2 / ORCA
suggest local collision-avoiding velocity
        ↓
combat/traversal constraints
        ↓
ActorIntent
        ↓
MiMITA physics

The final position always comes from MiMITA actor execution and physics.

40. Tight doorway behavior

Example:

20 NPCs
→ one narrow doorway

Desired:

actors approach;

local spacing develops;

some queue;

some may route elsewhere;

physical movement remains stable;

actors eventually pass through.

Do not require all actors to follow the exact same centerline at the exact same time.

41. Movement hierarchy

NPC movement should be driven by the highest-level objective first.

Example Counter-Strike Terrorist:

Goal:
WIN ROUND
    ↓
Role:
TERRORIST
    ↓
Current objective:
I HAVE BOMB
    ↓
Strategic action:
PLANT BOMB
    ↓
Travel goal:
BOMBSITE
    ↓
Navigation:
CORRIDOR TO SITE
    ↓
Local movement:
FOLLOW CORRIDOR
    ↓
Combat interruption:
ENEMY APPEARS

The actor should not simply maximize raw movement distance.

Movement should maximize progress toward the current meaningful objective.

42. Example Terrorist hierarchy

If carrying bomb:

survive
+
reach viable bombsite
+
plant

is a major priority.

If another teammate has bomb:

support carrier
+
control space
+
fight threats

If bomb is dropped:

someone should recover bomb

If bomb route becomes impossible:

reassess objective

43. Combat can interrupt travel

A navigation route is not sacred.

Example:

path says north
enemy appears west

Combat may temporarily become more important than exact corridor following.

After combat:

resume/recompute objective travel

Do not destroy the long-term goal merely because short-term movement changed.

44. Movement ownership order

A conceptual order:

1. determine strategic objective
2. determine travel goal
3. compute global navigation corridor
4. evaluate immediate traversal requirement
5. evaluate combat movement
6. evaluate local actor avoidance
7. produce ActorIntent
8. shared actor execution
9. shared physics determines actual result

Exact implementation order may vary, but ownership must remain clear.

45. Physical safety

Movement requests must always remain physically legal.

No high-level system should command:

walk through wall

and expect physics to solve it indefinitely.

If physical execution repeatedly fails, information must return upward:

movement failed
→ navigation learns route is blocked
→ repath/reassess

46. Combat movement

When fighting an enemy, an NPC should have access to movement options including:

strafe left;

strafe right;

retreat;

push;

circle;

parallel strafe;

sudden stop;

jump;

crouch;

fake direction;

reverse;

hold;

peek;

retreat to cover;

chase;

cut off escape route.

These are reusable combat-movement primitives.

47. Combat movement depends on context

NPC should consider:

own health;

enemy health;

own weapon;

enemy weapon;

ammo;

reload state;

range;

nearby cover;

nearby teammates;

enemy velocity;

enemy movement tendencies;

objective urgency;

geometry.

Example:

enemy:
shotgun
full health

NPC:
long-range weapon

→ increase distance

Another:

enemy:
knife
low health

NPC:
revolver

→ maintain distance and fire

48. Parallel strafing / tracking movement

NPCs should be capable of matching an enemy's screen-relative movement.

Example:

enemy moves left across NPC view
        ↓
NPC moves left in parallel
        ↓
relative horizontal aim movement decreases

This can create extremely strong tracking behavior.

The NPC may intentionally move to simplify its own aiming problem.

49. Predictive combat movement

NPCs may predict where opponents are likely to move.

Potential inputs:

current velocity;

acceleration;

health;

ammo;

weapon;

nearby cover;

repeated behavior history;

map geometry.

Example:

enemy low health
+
enemy usually retreats right
+
right-side cover available
        ↓
predict right retreat
        ↓
pre-aim / intercept

50. Opponent behavior modeling

Long-term NPCs may maintain opponent-specific behavioral statistics.

Examples:

dodges_right_when_low_health = 78%
jumps_after_rocket_shot = 64%
retreats_when_magazine_empty = 81%
prefers_left_side_of_doorway = 67%

These are predictions, not privileged knowledge.

NPC may exploit repeated player tendencies.

51. Difficulty and prediction

Higher difficulty may improve:

pattern recognition;

consistency;

amount of usable opponent history;

reaction speed;

prediction accuracy;

tactical movement selection.

It must not simply become:

read player's future inputs

The NPC should infer likely behavior from known state.

52. Juking goal

The goal of juking is:

reduce opponent's ability to predict and damage the NPC

It is not:

move randomly

A juke is successful if it:

causes missed shots;

creates positional advantage;

disrupts enemy tracking;

buys reload time;

creates escape space;

improves attack opportunity.

53. Juke complexity

A juke may be:

simple;

subtle;

flashy;

technically complex.

Example simple:

move right
→ sudden stop

Example complex:

left strafe
→ jump cancel
→ reverse
→ down dash
→ air strafe

Both are valid if tactically useful.

54. Hand-authored movement primitives

MiMITA should support reusable movement primitives.

Examples:

left-right duel strafe
corner peek
doorway bait
retreat strafe
circle strafe
jump peek
dash retreat
down-dash cancel
wall-jump escape
parallel tracking strafe

A primitive should describe intent/motion pattern rather than hard-coded world coordinates.

55. Human-recorded movement

MiMITA should eventually record human movement examples for NPC use.

Potential captured information:

simulation tick
map
position
velocity
look direction
movement input
jump
dash
down dash
crouch
weapon
enemy relative position
enemy velocity
enemy weapon
enemy health
own health
geometry context
goal direction
nearby walls
available space

56. Recorded movement is not raw playback only

Exact input playback is useful for some purposes, but is too brittle as the only representation.

Example:

recorded move was performed in open area

Blindly replaying it in:

tight corridor

may fail.

Therefore movement recordings should be convertible into generalized patterns.

57. Normalized movement patterns

Preferred concept:

human recording
        ↓
normalize relative to:
goal
enemy
local geometry
actor orientation
        ↓
movement primitive

Example:

Instead of storing:

world X = 342
press A for 18 ticks
press Space at tick 9

store something conceptually like:

enemy-relative lateral juke:
move perpendicular left
for ~0.3 sec
jump near midpoint
reverse after landing

This can be adapted to other locations.

58. Exact recordings remain useful

Exact input streams should still be retainable.

Uses:

debugging;

deterministic replay;

movement benchmarking;

tool-assisted movement authoring;

reproducing player behavior;

building movement primitives.

A movement primitive may originate from a literal recorded sequence and later become generalized.

59. Context matching

Recorded patterns may be selected based on context.

Example context:

wide open area
enemy 10m ahead
enemy moving right
NPC low health
weapon ready

The system may choose a movement pattern recorded in a sufficiently similar context.

60. Space validation before executing a pattern

Before executing a complex movement primitive:

does geometry permit it?

The NPC must verify:

clearance;

landing area;

wall position;

movement capability;

available space.

If not:

reject primitive
→ choose another

61. Movement primitives should be interruptible

A movement primitive should not trap the NPC into completing something obviously bad.

Example:

NPC begins circle strafe
        ↓
rocket appears
        ↓
immediate threat
        ↓
cancel pattern

Current world state overrides stale recorded behavior.

62. Animation canceling / movement tech

MiMITA should eventually permit NPCs to use the same movement technology humans discover.

If humans can:

cancel an animation;

chain movement;

exploit timing;

perform advanced movement;

NPCs should be capable of learning or being configured to use the same legal mechanics.

NPCs must not use impossible inputs unavailable to humans.

63. Tool-assisted movement authoring

A future editor may allow manually authoring highly precise movement sequences.

Example:

tick 0-12: W
tick 13: jump
tick 14-25: W+A
tick 26: dash
...

These can be:

previewed;

replayed;

normalized;

converted into reusable movement skills.

64. One movement owner

MiMITA should avoid multiple systems simultaneously deciding final movement direction.

Bad architecture:

navigator steers
+
wall avoidance steers
+
TeamBrain steers
+
combat steers
+
RVO2 steers
+
stuck system steers

all independently modifying velocity.

Instead, systems should provide inputs/candidates into a single arbitration path.

65. Movement arbitration

Conceptually:

Strategic Goal
        ↓
Global Navigation Suggestion
        ↓
Traversal Requirement
        ↓
Combat Movement Suggestion
        ↓
Local Avoidance Suggestion
        ↓
Movement Arbiter
        ↓
ONE final ActorIntent

The actor receives one coherent movement intent for the tick.

66. Priority is contextual

There should not necessarily be one universal hard-coded priority order.

Example:

Normally:

objective travel
> random wandering

But if:

grenade about to explode

immediate safety may outrank planting the bomb.

If:

bomb has 0.5 sec left to plant
enemy far away

finishing plant may outrank repositioning.

The brain should choose based on utility/objective value.

67. Navigation is not tactical reasoning

Recast/Detour should not answer:

Should I plant the bomb or fight?

That is actor/team decision-making.

Navigation answers:

If you choose Site A,
here is a legal route to Site A.

This separation is fundamental.

68. Decision-tree / utility behavior

Higher-level decisions may use:

utility scoring;

behavior trees;

goal-oriented action planning;

custom policy;

future learned policy.

Example decision:

last T alive
low health
bomb available
enemy low health

options:
plant
fight
retreat
recover bomb

Navigation does not choose among these.

ActorBrain/TeamBrain does.

69. Map-aware decision-making

Navigation information may still assist high-level decisions.

Example:

Site A:
20m away
safe route

Site B:
80m away
enemy recently seen

→ high-level brain can use travel cost

Detour may provide route distance/cost.

The brain decides what that means tactically.

70. Alternate routes

NPCs should be able to choose alternate valid routes.

Reasons:

avoid predictability;

avoid congestion;

react to enemy presence;

flank;

recover from blockage;

spread squad.

Do not force every actor to use one identical shortest path.

71. Route cost

Future route cost may include more than distance.

Potential terms:

distance
danger
enemy sightings
smoke
fire
crowding
drop risk
jump difficulty
objective urgency
route predictability

Initial Recast integration can begin with simpler cost models.

72. Dynamic geometry

Navigation should eventually respond to:

moving doors;

moving platforms;

temporary obstacles;

editor changes;

destructible geometry;

placed objects.

Use DetourTileCache or another suitable system where it genuinely helps.

73. Temporary blockers

A temporary blocker should not necessarily cause full world rebuild.

Example:

crate placed in hallway

Navigation should:

recognize blockage;

update affected area;

find alternate path where possible.

74. Local physics feedback

Navigation's plan is only a prediction.

Actual physics may differ.

The movement system should report:

expected progress
actual progress

If actual progress repeatedly fails:

navigation invalid/stuck
→ reconsider route

75. Net movement as an important metric

When an actor intends to travel:

net useful displacement

is a critical measure.

Example:

path distance traveled = 80m
net progress toward goal = 2m

is usually bad.

Example:

path distance traveled = 65m
net progress toward goal = 58m

is usually good.

76. Intentional movement exceptions

Low net progress is not automatically a bug if actor is:

combat strafing;

defending;

holding;

juking;

circling target;

waiting for objective;

avoiding immediate danger.

The log must record the current reason for movement.

77. Stuck definition

An NPC may be considered stuck when:

it intends to make meaningful travel progress

but over a configurable time window:

its net useful movement remains too small

Single-frame collision does not equal stuck.

78. Stuck recovery

Recovery should escalate.

Possible progression:

local correction
→ alternate local direction
→ repath
→ alternate corridor
→ traverse obstacle
→ stronger escape behavior

Do not immediately jump every time a wall is detected.

79. Avoid repeated failed actions

If:

jump attempt failed repeatedly

do not keep issuing the exact same jump forever.

If:

route repeatedly fails at same location

the system should remember that failure for a bounded period and try something else.

80. Ramps

NPCs must reliably:

ascend;

descend;

maintain direction;

avoid halfway-up reversal loops;

recognize continuous walkable slope.

Ramp movement is a basic acceptance requirement.

81. Corners

NPC must not:

walk into corner
→ reverse
→ walk back
→ reverse

forever.

Local navigation should understand enough free space to escape.

82. NPC spacing

NPCs should consider teammate occupancy when moving.

Goals:

avoid exact overlap;

avoid huge clustering;

maintain usable firing lines;

avoid blocking critical routes;

remain close enough for team support.

83. Group movement

Squads should be capable of traveling together without occupying the same point.

Potential formations are not required initially.

Simple target:

same general objective
+
different local positions/routes

84. NPC collision

Long term:

active actor vs active actor collision

should function properly.

This allows:

body blocking;

pushing;

queueing;

shoving;

physical crowd interactions.

Navigation/avoidance should complement this rather than replace it.

85. Runtime observability

Every important movement investigation should be observable in:

logs/<date>/<run>/events.jsonl

using:

StructuredLogger

There should not be a separate bespoke NPC navigation text log.

86. Evidence-first debugging loop

When movement appears wrong:

visible problem
        ↓
read movement specification
        ↓
trace current owners
        ↓
identify missing runtime evidence
        ↓
add bounded diagnostics
        ↓
build
        ↓
run real executable / scenario
        ↓
read active events.jsonl
        ↓
find FIRST divergence
        ↓
fix responsible owner
        ↓
rerun exact scenario

87. Recast logging

Useful Recast/Detour records may include:

map id
map geometry hash
triangle count
triangle bounds
navmesh loaded/generated
cache hit/miss
tile count
tile id
generation duration
walkable area count
agent configuration

88. Path request logging

A navigation request should be able to record:

npc id
tick
map
start position
goal position
goal reason
navigation backend
actor capabilities
nearest start polygon
nearest target polygon
path success/failure
path corridor polygon count
route length
route cost
replan reason

89. Path execution logging

Bounded samples may include:

npc id
tick
position
velocity
current corridor polygon
next waypoint
distance to waypoint
distance to final goal
desired global direction
local avoidance direction
combat movement direction
final ActorIntent direction
actual resulting velocity
net progress

90. Traversal logging

When traversal occurs:

npc.traversal-selected

fields may include:

type
jump
drop
crouch
wall_jump
rocket_jump
etc.

start position
target position
capability check
reason
success/failure

91. Avoidance logging

For local agent avoidance:

nearby actor count
preferred velocity
avoidance velocity
selected velocity
closest actor distance
collision predicted

Only log at bounded rates or state changes.

92. Combat movement logging

Useful events:

npc.combat-movement-selected

Fields:

enemy
distance
own health
enemy health
own weapon
enemy weapon
movement primitive
reason
predicted enemy movement
desired relative range

93. Human-pattern logging

When selecting a recorded movement pattern:

npc.motion-pattern-selected

Fields:

pattern id
pattern type
source recording
context similarity
space validation
goal
enemy-relative direction
reason selected

94. Why logging exists

Logging should answer questions like:

Did the map triangles load?

Did Recast build anything?

Was the NPC inside a valid polygon?

Did Detour return a corridor?

Was the corridor useful?

Did local avoidance change the direction?

Did combat override travel?

What final ActorIntent was sent?

Did physics execute it?

Where did progress stop?

The goal is to identify the first wrong stage.

95. Runtime scenario system

NPC movement should be tested using real runtime scenarios.

A scenario defines:

map
gamemode
seed
actor configuration
spawn positions
goal
enemy arrangement
time limit
expected behavior

Then launches the real executable or server path.

96. Scenario: clear travel

Setup:

NPC
clear path
goal 50m away
no enemies

Expected:

path generated;

NPC travels directly;

positive net progress;

no unnecessary jumps;

reaches destination.

97. Scenario: full wall

Setup:

goal behind large wall

Expected:

path routes around wall;

no wall grinding;

no repeated wall jumps;

eventual progress resumes.

98. Scenario: low obstacle

Setup:

crate blocks legs
upper body clearance exists

Expected:

traversal jump selected;

obstacle crossed;

route continues.

99. Scenario: ramp

Setup:

goal at top of ramp

Expected:

actor climbs continuously;

no midpoint reversal;

reaches top.

100. Scenario: tight corner

Setup:

actor trapped between nearby geometry

Expected:

escape selected;

actor leaves trap;

no indefinite oscillation.

101. Scenario: doorway crowd

Setup:

20 NPCs
single doorway

Expected:

actors continue making progress;

no permanent deadlock;

reasonable queue/flow;

physics remains final collision owner.

102. Scenario: opposing crowds

Setup:

group A moving east
group B moving west
narrow shared area

Purpose:

test RVO2/ORCA or alternate local avoidance.

Expected:

actors negotiate flow;

no giant invisible separation;

no permanent deadlock;

no catastrophic overlap.

103. Scenario: combat interruption

Setup:

NPC traveling to objective
enemy appears

Expected:

navigation travel
→ combat movement
→ fight
→ enemy disappears/dies
→ objective navigation resumes

104. Scenario: advanced juke

Setup:

enemy aiming at NPC
NPC has sufficient open area

Expected:

NPC selects valid juke;

movement has tactical purpose;

geometry permits it;

NPC does not devolve into random twitching.

105. Scenario: opponent adaptation

Future scenario:

human/replay repeatedly dodges right at low health

After sufficient observations:

NPC predicts right dodge above baseline

This proves adaptation rather than privileged future knowledge.

106. Scenario: map change / live editor

Future:

navigation geometry changes live

Expected:

relevant navigation tiles update;

NPC route invalidates cleanly;

new route generated;

server does not require full restart.

107. Performance tests

Navigation must eventually be profiled with:

1 NPC
10 NPCs
30 NPCs
100 NPCs
1000 NPCs

Do not claim a scale is supported without measurement.

Measure:

navmesh build time;

tile rebuild time;

path-query time;

avoidance time;

AI decision cost;

memory;

server tick time.

108. Navigation update frequency

Navigation queries do not necessarily run every 60 Hz tick.

Possible model:

physics:
60 Hz

movement execution:
60 Hz

path following:
high frequency

full repath:
event-driven / bounded

high-level decision:
lower frequency

Do not rebuild an entire path every frame without reason.

109. Replan triggers

Valid triggers may include:

goal changed;

corridor invalid;

geometry changed;

actor displaced significantly;

path blocked;

sustained progress failure;

tactical route change;

new objective.

A timer alone should not force constant unnecessary replanning.

110. Persistent travel goals

A long-distance objective should persist.

Do not regenerate:

"walk 2m forward"

every frame from current body position.

Example:

Bombsite A

remains the long-term destination until:

reached;

objective changes;

strategy changes;

route becomes invalid.

111. Human recordings as training/reference data

Recorded human behavior can be used in multiple ways:

exact deterministic replay;

movement-tech examples;

normalized movement primitives;

statistical behavior modeling;

NPC evaluation reference;

future machine-learning dataset.

Do not prematurely require machine learning.

A large amount of value can come from:

record
→ normalize
→ classify
→ reuse

112. Human baseline comparisons

NPC movement quality can be compared against human recordings.

Possible metrics:

net progress
time to destination
number of reversals
jump frequency
wall contact time
path efficiency
combat survival
hit avoidance
movement entropy
route diversity

The goal is not to blindly maximize every metric.

Use them to identify obviously non-human failure patterns.

113. Movement entropy / unpredictability

Future metric:

how predictable is this movement sequence?

Too low:

same movement every encounter

Too high:

random noise

Desired:

purposeful variation

114. Map-specific learning

NPCs may eventually learn:

common routes;

useful cover;

jump locations;

dangerous sightlines;

strong positions;

player habits per area.

This should augment generalized navigation.

It must not become required hand-written behavior for every map.

115. Navigation cache

Cached navigation data should be invalidated when relevant inputs change.

Potential cache key:

map geometry hash
navigation config hash
Recast version
agent/nav schema version

Never silently load navigation generated for incompatible geometry.

116. Debug visualization

MiMITA should be able to visualize navigation data.

Useful views:

navmesh polygons;

tile bounds;

current NPC corridor;

current waypoint;

off-mesh traversal link;

desired velocity;

avoidance velocity;

final movement vector;

goal position.

Debug visualization is not gameplay behavior.

117. External source isolation

Recast, RVO2, and MicroPather remain isolated until dependency review/integration gates are satisfied.

Preserve:

licenses;

upstream attribution;

pinned commit information.

Do not casually modify third-party source unless necessary.

Prefer a MiMITA adapter boundary around external APIs.

118. Adapter architecture

Preferred:

MiMITA
    ↓
NpcNavigationBackend interface
    ↓
RecastDetourBackend

rather than allowing all of MiMITA to call Detour directly.

This helps preserve the option to:

replace library;

compare implementations;

test fallback;

isolate third-party dependency.

119. Example backend interface

Conceptually:

buildWorldNavigation(...)
findPath(...)
findNearestReachable(...)
updateDynamicObstacle(...)
invalidateRegion(...)
debugDraw(...)

Exact API should be derived after inspecting Recast/Detour.

Do not prematurely force this pseudocode into production if the library suggests a better boundary.

120. Existing custom navigation

Existing MiMITA navigation should not be immediately deleted.

First determine:

what does existing code do well?
what does Recast replace?
what remains necessary?
what is redundant?

Potentially retain custom systems for:

movement execution;

special traversal;

physics feedback;

combat movement;

debugging.

Replace only responsibilities the external backend actually owns better.

121. Migration rule

Do not create:

old navigation
+
new navigation

both competing indefinitely.

During integration:

experiment
→ compare
→ choose owner
→ migrate
→ remove/reduce redundant owner

One responsibility should end with one production owner.

122. Integration proof

Recast integration is not complete when:

library compiled

It is complete when:

real map geometry
→ real navmesh
→ real Detour corridor
→ real NPC ActorIntent
→ real shared movement
→ real physics
→ successful visible navigation

and this is visible in events.jsonl.

123. RVO2 proof

RVO2 integration is not complete when:

sample program runs

It is complete when:

real MiMITA actors
→ local avoidance suggestion
→ shared ActorIntent
→ shared actor physics
→ improved crowd behavior

without replacing physical collision.

124. Do not confuse navigation with intelligence

A perfect pathfinder can still produce a stupid NPC.

A brilliant ActorBrain can still look stupid if navigation fails.

Therefore debug separately:

goal correctness
path correctness
local steering correctness
movement execution correctness
physics correctness

125. First-divergence debugging

When NPC movement is wrong, identify:

What was the intended goal?

Was that goal correct?

Was a path requested?

Was the requested path valid?

Did local steering alter it?

Did combat override it?

What ActorIntent was produced?

Did physics execute it?

Where did actual movement diverge?

Fix the earliest incorrect stage.

126. Repeated-attempt behavior

For attempt 2+:

Before implementing another movement mechanism:

read previous attempts;

identify the previous hypothesis;

identify the runtime evidence;

determine whether the mechanism actually improved the target behavior;

determine whether it is still necessary;

consider deletion/simplification.

Do not assume:

more movement systems
=
better movement

127. Main prohibitions

Do not:

build another full custom global navigation system if Recast/Detour already solves it better;

let Recast directly control actor physics;

let RVO2 directly set authoritative actor transforms;

let TeamBrain directly steer velocity;

let five systems independently modify movement;

rebuild path every tick without reason;

rely on random twitching as advanced movement;

give NPCs illegal player inputs;

give NPCs magical future knowledge;

consider a synthetic test equivalent to real gameplay;

treat successful library compilation as successful integration.

128. Initial implementation priority

Recommended order:

1. inspect current movement/navigation ownership
2. isolate Recast/Detour experiment
3. feed real MiMITA collision triangles into Recast
4. generate and visualize navmesh
5. perform Detour path query
6. adapt path corridor to existing ActorIntent/movement
7. run real map scenario
8. inspect events.jsonl
9. compare against existing navigation
10. migrate only if behavior improves

Then:

11. dynamic/tiled navigation
12. traversal links
13. RVO2 experiment
14. crowd handling
15. advanced combat movement
16. human-recorded movement primitives
17. opponent adaptation

129. First Recast acceptance gate

Do not proceed to fancy juking merely because Recast is available.

First prove:

NPC can travel around real MiMITA map

Requirements:

map triangles imported correctly;

navmesh corresponds to physical walkable surface;

path from spawn to objective exists;

corridor follows real geometry;

NPC follows it;

no repeated wall grinding;

no catastrophic oscillation;

positive net progress;

ramps/doorways work.

130. First RVO2 acceptance gate

Only investigate after global navigation works reliably.

Test:

multiple NPCs
same doorway

Compare:

without RVO2
vs
with RVO2

Measure:

throughput;

collisions;

deadlocks;

path efficiency;

net movement;

visual smoothness.

If RVO2 does not improve behavior:

do not integrate it

131. Advanced movement comes after reliable navigation

Fancy jukes do not compensate for broken pathfinding.

Order:

can reach destination reliably
        ↓
can traverse obstacles
        ↓
can handle crowds
        ↓
can fight while moving
        ↓
can juke
        ↓
can use recorded human movement
        ↓
can adapt to specific opponents

Each layer assumes the lower layers work.

132. Final target architecture

GAME / ROLE / TEAM OBJECTIVE
            ↓
       ActorBrain
            ↓
 Tactical movement decision
            ↓
     Travel destination
            ↓
  Recast / Detour backend
            ↓
      Path corridor
            ↓
 Traversal + local movement
            ↓
 Optional RVO2 suggestion
            ↓
 Combat movement / juke
            ↓
     Movement Arbiter
            ↓
        ActorIntent
            ↓
 Shared movement execution
            ↓
     Shared physics
            ↓
    Actual actor state

Feedback returns upward:

actual progress
collision
stuck state
route failure
combat result

so future decisions use reality rather than assuming the requested movement succeeded.

133. Final target behavior

A successful NPC should be able to:

understand objective
→ choose meaningful destination
→ find reliable route
→ traverse map
→ avoid teammates
→ react to enemies
→ fight while moving
→ predict opponent behavior
→ choose useful jukes
→ use advanced movement
→ recover when plans fail
→ resume objective

while always using the same physical actor rules available to humans.

134. Permanent guiding principle

The NPC should not look smart because it cheats.

The NPC should look smart because:

it knows its goal,
understands the available space,
understands its own movement capabilities,
understands what its opponent is likely to do,
and chooses strong actions accordingly.

Navigation tells the NPC where it can go.

MiMITA movement determines how it gets there.

Combat intelligence determines when the best route is not simply the shortest route.

Human movement recordings teach it additional ways that skilled players already know how to move.

All of these feed one shared actor execution system.