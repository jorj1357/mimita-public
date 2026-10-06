2026 10 06 1009 todo jorj fill in full ultra detailed of how counter strike mode should work 

MiMITA Counter-Strike Mode — Full Specification v1

Date: 2026-10-06
Status: Draft v1 / authoritative target behavior pending human edits
Purpose: Define the intended Counter-Strike mode behavior, architecture, configuration ownership, NPC behavior, runtime observability, and acceptance conditions.

1. End goal

Counter-Strike mode exists primarily to prove that MiMITA's generalized actor, NPC, movement, combat, objective, and networking systems work together in a complete competitive game mode.

Counter-Strike mode is not intended to become the final identity of MiMITA.

It is a reference implementation and stress test for generalized systems that later modes can reuse.

The mode should prove that MiMITA can support:

    human players;

    NPC players;

    teams;

    round-based gameplay;

    one-life rounds;

    objective behavior;

    tactical navigation;

    perception;

    combat;

    team coordination;

    bombs;

    grenades;

    spectating;

    authoritative client/server networking;

    configurable actor presets;

    configurable gamemode rules;

    scalable player counts;

    deterministic or controlled runtime tests;

observable behavior through structured logs.

The central rule is:

    Counter-Strike should not invent isolated systems.

    Counter-Strike should compose generalized MiMITA systems.

The mode should initially feel broadly like Counter-Strike as remembered and intended by the project owner.

It should:

    be slower and more deliberate than normal MiMITA;

    have one-life round consequences;

    use first-person gameplay;

    have low HUD information;

    rely more on positioning, awareness, movement, and combat decisions;

    use bomb planting/defusing as the primary objective;

    allow elimination as another round-ending path;

    provide difficult NPC opponents.

It should not:

    become excessively realistic;

    become intentionally slow merely for realism;

    require simulation-specific code that cannot be reused elsewhere;

    fork movement, damage, weapon, physics, or networking into Counter-Strike-only implementations;

    become the only place NPC behavior works.

Counter-Strike is the proving ground.

NPC behavior built for this mode must be reusable in:

    Zombie Tower;

    Infinite Dungeon Slayer;

    Payload;

    RPG modes;

    Sandbox;

    future team modes;

    future PvE modes;

    future user-created modes.

2. Fundamental architecture rule

Counter-Strike mode is assembled from generalized owners.

Conceptually:

    Counter-Strike Gamemode
        ↓
        Teams / Roles
            ↓
            Actor Preset
                ↓
            Behavior Preset / NPC Policy
                ↓
            Shared Actor Lifecycle
                ↓
            Human Input OR NPC Brain
                ↓
            Actor Intent
                ↓
            Navigation / Weapons / Interaction
                ↓
        Shared Fixed-Tick Movement / Physics / Damage
            ↓
        Authoritative Server
        ↓
    Replication / HUD / Audio / Effects

There must not be an alternate Counter-Strike actor implementation.

A Counter-Strike actor is still a normal actor.

The gamemode changes the actor's configuration and goals.

3. Architectural invariants

These are hard rules unless this specification is deliberately revised.

3.1 Shared actor execution

Humans and NPCs MUST ultimately use the same actor execution systems for:

    movement;

    collision;

    physics;

    weapon use;

    damage;

    health;

    death;

    animation execution;

    interaction;

    knockback;

    objective interaction where applicable.

NPCs decide what they want to do.

Humans provide input.

Both become generalized actor intent.

    Conceptually:

    Human input
        ↓
    ActorIntent
        ↓
    Shared Actor Execution

    NPC Brain
        ↓
    ActorIntent
        ↓
    Shared Actor Execution

There MUST NOT be:

    CounterStrikeNpcMovement()
    CounterStrikeNpcDamage()
    CounterStrikePlayerPhysics()
    CounterStrikeWeaponSystem()

unless the function merely configures or composes generalized systems.

4. Mode purpose as an NPC benchmark

A major success criterion is:

    NPCs should be difficult enough that the project owner
    loses considerably more often than wins.

Losses should remain understandable.

The human should usually be able to identify why they lost:

    bad positioning;

    missed shots;

    poor timing;

    exposing themselves;

    failing to rotate;

    failing to support teammates;

    failing to protect/defuse the bomb;

    moving predictably;

    taking an unnecessary fight;

    being outplayed.

Difficulty MUST NOT primarily come from:

    extra health;

    impossible damage;

    perfect omniscience;

    seeing directly through walls;

    impossible reaction times;

    impossible firing rates;

    hidden stat bonuses.

NPC difficulty should primarily emerge from:

    better decisions;

    better positioning;

    better consistency;

    better aim;

    better grenade usage;

    better teamwork;

    better prediction;

    better movement;

    better use of information.

5. Initial user flow

Selecting Counter-Strike should follow this flow:

    select Counter-Strike
        ↓
    load authoritative map, todo 2026 10 06 1255 make it so only supported maps are available for counter strike mode, and add a map editor so we can add spawns and bomb sites so we can make any existing map a valid counter strike map
        ↓
    show team selection
        ↓
    player chooses CT or T
        ↓
    player immediately spawns at that team's spawn
        ↓
    warmup / intermission
        ↓
    3
    2
    1
    GO
        ↓
    first live round

6. Team selection

Before entering active round play, the player selects:

    Counter-Terrorists
    Terrorists

The UI should show:

    team names;

    current human count;

    current NPC count;

    names of all actors on each team; 

    total team actor count;

    whether the team is joinable/full.

Example:

COUNTER-TERRORISTS
Humans: 3
coolusername123, jorj1357, admin
NPCs: 2
Total: 5

TERRORISTS
Humans: 0
NPCs: 5
Total: 5

Default target:

    5 versus 5
    10 total actors

The architecture MUST NOT assume 5v5 permanently.

It should support:

    1v1
    5v5
    10v10
    20v20
    100v100
    999v999

subject to performance and networking capacity.

Default casual mode may allow uneven teams.

Competitive variants may enforce balancing.

7. Human joining and NPC replacement

If a team has NPCs filling available actor slots:

    human joins team
        ↓
    one NPC may be removed
        ↓
    human occupies that actor slot

    NPCs function as population fillers.

Example:

before:
CT = 1 human + 4 NPC

new human joins CT

after:
CT = 2 humans + 3 NPC

    If a team is completely occupied by humans and has reached configured capacity:

joining that team is rejected

The player may:

    join the other team;

    remain spectator;

    wait for an opening.

8. Team switching

During casual mode:

    team switching is allowed before active competitive play begins;

    spectators may later join teams if capacity permits.

Target rule:

    team switching locks after the first live round begins

Exact casual/competitive exceptions may later become configurable.

9. Spectator team

Spectator is a real game state/team concept but does not participate in round combat.

A spectator:

    does not occupy a playing-team combat slot unless configured otherwise;

    does not spawn as an active combat actor;

    may observe the world;

    may later join CT/T when permitted.

Dead players enter spectator behavior until the next round.

todo 2026 10 06 we also need to make the commands 
"team_status", "team_list"  and "team_pick" behave ideally

e.g. team_status just gives information about ur team
team_list shows all available teams for that gamemode, like, sorted alphabetically, and with numbers. 
so CT = 1, spectator = 2, T = 3, not hardcoded to those positions, just as a result of being sorted alphabetically. as well as spectator being a valid team in this mode, so it should be listed as well
then, team_pick should work like "team_pick 1" = u are a CT now. "team_pick 3" u are a terrorist now. "team_pick 2" u are now on spectators

10. Ownership model

The following ownership boundaries should be treated as authoritative.

    10.1 Gamemode owns

    The Counter-Strike gamemode configuration owns:

    team definitions;

    round lifecycle;

    intermission duration;

    countdown duration;

    round duration;

    score;

    rounds-to-win;

    win conditions;

    objective rules;

    match victory;

    spectator rules;

    warmup/intermission behavior;

    map selection;

    friendly-fire policy;

    team-size policy;

    mode-level NPC strategy where appropriate;

    objective presentation;

    mode-specific presentation policy that is not already owned elsewhere.

11. Role owns

A role represents an actor's purpose and team identity.

Examples:

    counter_terrorist
    terrorist
    spectator

A role may define:

    team;

    spawn group;

    actor preset reference;

    avatar identity or avatar family;

    objective permissions;

    NPC role/objective policy;

    allowed objective interactions.

Example:

Terrorist
    team = T
    spawn = terrorist_spawn
    may_carry_bomb = true
    may_plant_bomb = true
    may_defuse_bomb = false

Counter-Terrorist:

team = CT
    spawn = counter_terrorist_spawn
    may_carry_bomb = false
    may_plant_bomb = false
    may_defuse_bomb = true

12. Actor preset owns

Actor presets define how the actor physically and visually operates.

The Counter-Strike actor preset should own:

    FOV;

    forced camera mode;

    movement preset;

    health;

    allowed weapon set;

    weapon overrides;

    presentation restrictions;

    outline policy;

    healthbar policy;

    allowed abilities;

    disabled abilities;

    other actor-level constraints.

Target:

    FOV = 70
    camera = forced first-person
    movement = movement-heavy / Counter-Strike movement
    dash = disabled
    down dash = disabled
    freeze = disabled

13. Role and actor preset consolidation

Current architecture separates:

    Role
    ActorPreset

        This MAY be reconsidered.

        This MAY be reconsidered.

        This MAY be reconsidered.

The project owner jorj as of 2026 10 06 1301 todo figure this out
 currently suspects that roles and actor presets may be unnecessarily separate.

Possible future target:

    ActorRolePreset

containing both:

    semantic role;

    physical actor configuration.

However, this MUST NOT be merged casually.

Before merging, determine whether there are legitimate reusable cases such as:

    same actor preset
    different team/objective role

or:

    same role
    different physical actor preset

Until proven otherwise, the current separation remains valid.

    This is a NEEDS_ARCHITECTURE_REVIEW item.

14. No duplicated ownership

A configuration value MUST have one owner.

Do not define the same authoritative value in:

    Gamemode
    AND Role
    AND ActorPreset

without explicit precedence.

Examples:

    FOV should not simultaneously be independently authored in:

    gamemode.json
    roles.json
    actor-preset.json

    unless one clearly overrides another by documented rule.

Every duplicated configuration source must have:

    explicit precedence
    OR
    be removed

15. Counter-Strike player movement

Counter-Strike movement should feel heavier than normal MiMITA.

It should not remove the identity of MiMITA movement completely.

Target characteristics:

    lower acceleration;

    lower air acceleration;

    momentum persists after input release;

    slower deceleration;

    reduced air control;

    jumping allowed;

    bunny hopping possible;

    no automatic hold-to-jump behavior used by standard movement;

    no dash;

    no down dash;

    no freeze;

    crouch support required;

    fall damage required.

Approximate design concept:

    ground target speed ≈ 30
    maximum speed cap ≈ 50

Exact values belong to the movement preset and remain tunable.

16. Movement acceleration

The player should not instantly reach maximum velocity.

Approximate intended feel:

    press movement
        ↓
    accelerate progressively
        ↓
    reach target speed after roughly ~1 second

This is a feel target, not necessarily an exact one-second invariant.

Releasing movement should not instantly zero velocity.

17. Air movement

Air control should be weaker than normal MiMITA.

Players may still influence movement in air.

Counter-Strike mode should not use the extremely strong default movement characteristics.

Air behavior should be controlled entirely through the movement preset rather than Counter-Strike-specific movement code.

18. Jumping

Jumping is permitted.

Target behavior:

pressing jump causes a jump when legal;

bunny hopping is possible;

holding jump should NOT automatically continuously jump in the same manner as the normal MiMITA movement preset.

Exact input semantics remain configurable.

19. Crouch

jorj 2026 10 06 1302 todo: crouching just does not exist right now, prob bind it to left control, press down = crouch , press up = no more crouch
and edit how fast you go into crouch, if there is like a fatigue, e.g. u cant crouch over and over super quick, a cooldown can be editable in a .json contrlling crouch behvaior, etc.

Counter-Strike mode requires crouching.

Crouch should eventually affect:

    actor stance;

    collision/body pose;

    movement speed;

    weapon/aim stability if desired;

    ability to move through low spaces;

    animation.

Implementation must be generalized.

    Do not add a Counter-Strike-only crouch.

20. Fall damage

Counter-Strike mode should support fall damage.

    Fall damage should be produced through a generalized actor/physics damage rule.

Counter-Strike may configure the thresholds and scale.

21. Camera

Counter-Strike mode uses:

    forced first-person
    70-degree FOV

The actor preset owns this behavior.

The gamemode should not contain hardcoded:

    if (mode == COUNTERSTRIKE)
        fov = 70;

The correct architecture is:

    Gamemode
        ↓
    Role
        ↓
    ActorPreset
        ↓
    Camera configuration

22. RMB aim / zoom

Right mouse button may reduce the field of view for aiming.

    Target behavior currently exists conceptually.

Example:

    normal FOV = 70
    RMB aim FOV = configured lower value

Transition should be smooth.

Exact target FOV and transition duration remain configurable.

23. Mouse aim

Mouse sensitivity remains user-controlled.

Turn speed is not artificially limited by the gamemode unless a future weapon or state explicitly requires it.

24. Weapon aim philosophy

Weapon accuracy should not primarily use random spread.

The intended model is:

    where the weapon/right arm physically aims
        =
    where the shot goes

Accuracy differences should emerge through:

    recoil;

    arm orientation;

    movement;

    stance;

    player input;

    NPC aim quality.

Random recoil/spread should be minimized or disabled for core Counter-Strike firearms unless deliberately added later.

25. Initial weapon set

Initial required firearm set:

    Revolver
    Shotgun
    Hitscan Rifle

Required grenades:

    Frag Grenade
    Smoke Grenade
    Fire Grenade
    Darkbang

Future:

    Knife / melee
    additional rifles
    additional firearms

26. Revolver

Counter-Strike revolver should behave broadly like a heavy Desert Eagle-style handgun.

Current desired starting values:

    magazine = 6
    reserve = 36

Fire rate should be considerably slower than normal MiMITA revolver behavior.

Approximate target:

    ~3x slower than normal

Exact rate should be measured/tuned rather than hardcoded from this sentence.

Reload should feel similar to a heavy handgun.

27. Shotgun

Shotgun remains hitscan.

Exact magazine/reserve and damage values remain configurable through the Counter-Strike actor preset weapon override.

NO Random pellet spread: it should be DETERMINISTIC, FIXED, SQUARE PATTERN, and should not be the primary accuracy model unless specifically desired later.

Potential model:

deterministic pellet directions
+
physical weapon aim/recoil

28. Hitscan rifle

Rifle is:

    hitscan
    long range
    thin/zero-thickness trace

Starting target range:

    ~1000 meters

or effectively long enough that normal Counter-Strike maps are not range-limited.

It should behave as an actual thin ray rather than a broad beam volume.

29. Weapon beam/tracer presentation

Tracers or beams may remain visually visible.

    Reason:

    They provide useful readability about:

    where shots traveled;

    how fights are unfolding;

    where aim is directed.

However, their collision thickness MUST NOT depend on their rendered thickness.

    Rendering may show a visible tracer while gameplay remains an infinitely thin hitscan ray.

30. Weapon recoil

Recoil should physically alter weapon/right-arm aim.

Example:

    fire
        ↓
    arm/weapon rotates or moves
        ↓
    next shot goes where arm currently points

The game should absolutely avoid at all costs:

    hidden random cone spread

when a physically observable recoil system can produce inaccuracy instead.

31. Headshots

Headshots use a damage multiplier.

Exact multiplier is configurable.

Body-part hit detection should use the generalized body/damage system.

Do not create Counter-Strike-only head collision logic.

32. Knockback

Standard Counter-Strike firearm hits should generally not produce large MiMITA-style knockback.

Target:

    firearm knockback ≈ none/minimal

unless explicitly configured.

33. Weapon switching

Weapon switching should not allow immediate firing on the exact same tick.

Target equip delay:

approximately 15–36 simulation ticks

Final value should be tuned.

The weapon must complete its equip/readiness window before firing.

34. Reload cancellation

todo define better 2026 10 06 1305 jorj - like, shoot all bullets out of revolver, equip shotgun = revolver auto reloads bc u unequipped it, as a result of u equipping shotgun. it would auto reload even if u just uneqeuipped the revolver anyway. so its like, we need to  define more better behavior here

Initial target:

reload cancellation = disabled

This may later be revisited.

35. Penetration

Wall penetration is not required for v1.

Initial target:

penetration = off

later we can do penetration 

36. Presentation philosophy

Counter-Strike mode intentionally removes much of MiMITA's normal hit feedback.

The player should receive relatively grounded visual feedback.

Enabled:

    blood;

    killfeed;

    ragdolls;

    weapon firing;

    tracers/beams if configured.

Disabled:

    damage numbers;

    hit markers;

    hit sounds;

    large red damage spheres/effects;

    obvious arcade damage confirmation.

Health bars should not reveal opponents through walls.

Preferably health bars are disabled entirely during active play.

37. Team friendly fire

Friendly fire exists.

Players may shoot teammates.

The game should not prevent the shot merely because the target is allied.

The damage policy is configurable.

Default Counter-Strike target:

friendly fire = enabled

NPC targeting, however, MUST NOT intentionally choose teammates as hostile targets.

Accidental friendly damage is different from intentional targeting.

38. Team-select state

When first entering Counter-Strike:

    TEAM SELECT

The player has not yet entered active team play.

UI shows both teams and their occupancy.

Player selects CT/T.

After selection:

    spawn immediately at selected team spawn

39. Intermission / warmup

Duration:

    30 seconds

Warmup behavior:

    movement allowed;

    shooting allowed;

    infinite lives;

    instant respawn;

    normal team actors present;

    NPCs active;

    weapons usable;

    warmup is intentionally low consequence.

Warmup exists to let players:

    move;

    shoot;

    verify settings;

    practice;

    wait for the match.

The bomb objective does not need to determine round victory during warmup.

40. Countdown

Every live round begins with:

3
2
1
GO!!!

During:

3
2
1

all active combat actors are frozen from movement.

They should not be able to gain a timing advantage.

At:

GO!!!

all active actors become movable on the same authoritative round transition.
exact same server tick, every single actor is able to move, all at the same time, no one is ahead/behind.

41. Active round

During active round:

    dead players do not respawn;

    CT/T pursue their team objectives;

    bomb may be carried/planted/defused;

    elimination may end the round;

    HUD remains minimal;

NPCs actively navigate, fight, communicate, and pursue objectives.

42. Round victory

Terrorists win if:

    all Counter-Terrorists are eliminated
    OR
    bomb explodes successfully

Counter-Terrorists win if:

    all Terrorists are eliminated before a planted bomb requires resolution
    OR
    bomb is defused
    OR
    round timer expires without successful plant

43. Planted bomb and elimination

If bomb is already planted:

    eliminating the Terrorist team does NOT automatically cancel the bomb

CT must still defuse before explosion.

    The planted objective continues independently of Terrorist survival.

44. Round timeout

If the round timer reaches zero and bomb has not been planted:

    CT wins

If the bomb has already been planted:

    bomb timer remains authoritative

The normal round timer no longer determines CT victory.

45. Simultaneous elimination

    NEEDS_SPEC_DECISION

Possible rule:

if both teams become eliminated on the same authoritative tick before plant, resolve according to objective state or draw;

if bomb planted, planted bomb remains authoritative.

Final tie resolution requires explicit decision.

2026 10 06 1307 jorj  - i think here juts call it a draw, do another round, no one wins that round, just try again 

46. Round result

Round result displays:

    TERRORISTS WIN

or

    COUNTER-TERRORISTS WIN

The score increments.

After result presentation:

new round
→ respawn everyone
→ reset objective state
→ 3
→ 2
→ 1
→ GO

47. Match victory

Initial target:

    first team to 8 round wins

wins the match.

This remains configurable.

After match victory:

    display winner;

    return to intermission/warmup;

reset match score when new match begins.

48. Death

During an active round:

    death
        ↓
    combat actor becomes dead
        ↓
    player becomes spectator

No active respawn until the next round.

This must be server-authoritative.

49. Spectator behavior

Dead players:

    can free-fly;

    can observe the map;

    can observe enemies;

    can observe teammates;

    cannot affect active gameplay;

    cannot deal damage;

    cannot interact with bomb/objectives;

    cannot block movement;

    cannot collide with actors.

Other dead spectators may optionally see each other's ghost representations.

50. Ghost networking

Spectator ghost movement is low priority.

It may:

    not replicate at all

or:

    replicate at reduced rate

Example:

    active actor:
    high-rate simulation/replication

    spectator ghost:
    low-rate cosmetic replication

Exact policy is performance-dependent.

Spectator replication must never ever ever consume significant bandwidth needed by active combat.

51. Spectator communication

Initial Counter-Strike rule:

    dead players communicate with dead players

    living players communicate with living players

This should be configurable per gamemode.

    Voice/text communication architecture, when later implemented, bc right now 2026 10 06 1308 we only have text chat no voice chat yet, should support this same team/life-state filtering.

52. Vote skip

Any player may potentially initiate or participate in vote-skip.

Purpose:

    avoid extended dead-time or stalled rounds.

    Exact voting thresholds and cooldowns are TBD.

2026 10 06 1309 jorj - we def need to have a threshold that is not like 50% of people or more. like, if u want to skip, it should be like 30-40% vote yes, then it happens. if u want to vote kick, again, 30-40%. so that people who dont vote wont  stall the vote 

53. Respawn boundary

At the beginning of a new round:

    dead state clears;

    spectator state clears;

    actor respawns at correct team spawn;

    health resets;

    inventory/loadout resets;

    bomb state resets;

    transient effects reset;

previous-life packets/events must not affect the new actor life.

A player MUST NEVER respawn at the opposite team's spawn because of stale state.

54. Bomb assignment

At live round start:

    one living Terrorist is selected randomly

to carry the bomb.

    The carrier must be a valid Terrorist actor.

Humans and NPCs are both eligible unless configured otherwise.

55. Bomb carrier

The bomb behaves as a real objective item associated with the actor.

The carrier may:

    move;

    fight;

    reach a bomb site;

    plant.

If carrier dies:

    bomb drops at or near death location

56. Bomb disconnect behavior

If carrier disconnects:

    bomb drops

If dropping is impossible or results in an unreachable location:

    recover bomb automatically

Fallback:

    assign bomb to a valid living Terrorist

    or return it to a valid recoverable world position.

Exact recovery priority is configurable.

57. Bomb pickup

Only Terrorists may pick up a dropped bomb.

Potential pickup modes:

walk within ~0.3 m
OR
press F while within interaction range

2026 10 06 1310 jorj - todo furhter implement this, already theres a thing that pops up when u get close to the bomb but i have onyl tested as CT, not T, so i havent picked it up ever , but we need more clearer GUI for this, like , world space gui billboard in the world like a damage number how it faces ur screen 

Preferred design should support explicit interaction through generalized Interact.

CT cannot pick up the bomb.

58. Planting

A Terrorist carrying the bomb may plant while inside a valid bombsite.

Interaction:

hold F

ensure that this si very very visible, like, it should show a popup like "Hold F to plant bomb" as soon as u get into a valid bomb placing space 

Target plant duration:

3 seconds

Planting interrupts if:

    player moves more than, for now, 0.5 meters, configurable later;

    player releases F;

    leaves site;

    dies;

    becomes invalid;

bomb changes owner.

59. Bomb planted

Once planted:

    bomb state = planted

Target explosion time:

    40 seconds

The planted bomb belongs to the site where it was planted.

60. Bomb timer presentation

NEEDS_SPEC_DECISION

Possible behaviors:

A:

    show exact seconds remaining

B:

    use sound/visual cues without exact number

Current preference appears to lean toward less exact HUD information.

Until decided, the system should internally track exact authoritative ticks regardless of presentation.

61. Defusing

Only CT may defuse.

Interaction:

hold F within interaction range

Current expected defuse duration:

    ~5 seconds

subject to configuration.

Defuse interrupts if:

    F released;

    CT leaves range;

    CT dies;

objective becomes invalid.

62. Bomb explosion

At authoritative explosion tick:

    Terrorists win

Explosion should use generalized explosion systems if the bomb produces physical effects.

Round result remains gamemode-owned.

63. Bomb world representation

The bomb should exist as an understandable world object.

It should not require a giant arcade marker.

Possible presentation:

    bomb model;

    subtle glow/pulse;

    interaction prompt;

    team-specific HUD state.

Dropped bomb should be discoverable without becoming visually overwhelming.

64. Bombsite data

Map-specific objective data lives in the map configuration.

Target:

config/maps/<map>.json

2026 10 06 1312 jojr - todo expand this into map editor i think, cuz that would be awesome to be able to work with this stuff 

Bomb sites include:

    id
    position/volume
    radius or volume geometry
    plant eligibility
    debug visibility

65. Map spawn authoring

Current authoring may use Blender node names such as:

    spawnpoint.CT
    spawnpoint.T

    or

    spawnpoint.counterterrorist
    spawnpoint.terrorist

Exact naming should match the actual repo convention.
also i dont know what the exact naming is  2026 10 06 1315 jorj 

Long-term target:

    MiMITA map editor

should allow visual placement of:

    CT spawn regions;

    T spawn regions;

    bombsites;

    objective areas;

    navigation metadata;

    other map entities;

    anything else that could potentially be desired to be editable through a physical world editor like a map editor

66. Map JSON validity

Map configuration supports hot reload.

If edited config becomes invalid:

    keep last valid version

Do not replace working runtime config with malformed data.

The error should be visible in structured diagnostics.

67. NPC information model

NPCs use perception, memory, team information, and game-state information.

An NPC may know:

    its own position;

    health;

    weapon;

    ammo;

    objective;

    bomb state;

    teammate positions;

    teammate deaths;

    bombsite locations;

    round state;

    information directly perceived;

    team-shared enemy reports.

NPCs MUST NOT automatically use perfect hidden information as immediate aim targets, unless otherwise configured to do so.

68. Enemy information

The server technically knows every actor's exact transform.

The NPC brain may have access to authoritative data internally, but behavior must intentionally restrict how it uses that data.

An NPC may know that an enemy exists somewhere through shared/team information without behaving as if it has direct line of sight.

Example:

    enemy known to be around B

≠

    NPC aims perfectly through wall at exact head location

69. Vision

for npcs, Visible enemy requires appropriate perception.

Conceptually:

    alive
    +
    within range
    +
    within FOV
    +
    line of sight
    =
    visible target

Exact tuning is configurable.

70. Memory

Initial target:

    enemy memory ≈ 15 seconds

measured in fixed simulation ticks.

Initial v1 may use relatively simple memory:

    known
    or
    forgotten

Later versions may support:

    confidence
    uncertainty
    predicted area
    position variance

71. Forbidden NPC knowledge

NPC must not directly know:

    future player movement;

    exact future aim;

    unseen target actions before they occur;

    impossible wall-penetrating visual information.

NPC may use:

    prediction based on observed velocity;

    teammate reports;

    remembered positions;

    general objective logic.

72. No-visible-enemy behavior

When an NPC has no visible hostile:

    MOVE

A core design requirement:

    cover meaningful net ground

The NPC should not spend most of its time stationary or oscillating unless the tactical role explicitly calls for holding.

The NPC should, in counter strike mode, as of 2026 10 06 1317, spend most of its time, if there is not an observable enemy or other event such as bomb being planted, team member dies, or enemy is observed, covering as much ground as it can. exploring as much of the map as it can

73. T no-visible-enemy priorities

Possible hierarchy:

if carrying bomb:
    choose/commit to bombsite
    move with team
    reach site
    plant
    defend planted bomb

if not carrying bomb:
    support bomb carrier
    move with team
    clear routes
    fight encountered enemies
    recover dropped bomb when appropriate

NPCs should generally avoid wandering without purpose.

74. CT no-visible-enemy priorities

Possible hierarchy:

    defend likely sites
    hold useful positions
    patrol
    respond to teammate information
    rotate
    retake if bomb planted

Exact tactical strategy remains expandable.

75. Team proximity

NPCs should generally be aware of teammate locations.

They should tend to move in a way that allows mutual support.

This does not mean every NPC stacks into one blob.

Target:

    coordinated
    but spatially distributed

76. Net movement requirement

For an NPC actively traveling toward an objective:

    distance to useful destination should generally decrease over time

A useful runtime measure:

    net_progress_toward_goal

should remain positive over reasonable windows unless:

    combat interrupts;

    path changes;

    temporary obstacle;

    tactical hold;

    retreat;

    objective changes.

77. Bad navigation definition

Bad navigation includes:

    walking continuously into a wall;

    repeatedly returning to the same obstacle;

    left/right oscillation;

    moving several meters while producing almost zero net displacement;

    getting trapped in corners;

    getting trapped between crate and wall;

    failing to traverse obvious low obstacles;

    repeatedly turning around on a ramp;

    repeatedly climbing halfway up and reversing;

    unnecessary jump spam;

    circling a waypoint;

    overshooting destination;

    endlessly replanning without moving.

These are specification violations.

78. Good navigation definition

Good navigation includes:

    sustained net travel;

    deliberate heading;

    routing around full-height walls;

    navigating ramps;

    navigating stairs;

    traversing doorways;

    using alternate routes;

    jumping low obstacles when needed;

    escaping corners;

    escaping narrow traps;

    stopping correctly at destination;

    resuming movement after obstruction;

    avoiding endless oscillation.

79. Low obstacles

If:

    legs blocked
    AND
    upper body path clear

    and obstacle is traversable:

    NPC may jump over it

If:

    full body blocked

the NPC should route around rather than repeatedly jump into it.

80. Corners and traps

NPCs must have a generalized escape behavior for situations where:

    trying to move
    BUT
    remaining inside nearly same local area

Recovery should seek open space.

It should not:

wall push
→ step backward
→ wall push
→ step backward

forever.

81. Multiple NPC route diversity

Squads should not necessarily choose the identical microscopic path.

NPCs may choose:

    alternate corridors;

    different sides of obstacles;

    slightly different spacing;

    different tactical paths.

Variation should be deterministic or controlled rather than random every tick.

82. Seeing an enemy

When hostile becomes visible:

    perception detects hostile
        ↓
    reaction delay
        ↓
    combat goal/action selected
        ↓
    aim
        ↓
    movement
        ↓
    fire if appropriate

83. Reaction time

    Reaction delay is behavior/difficulty controlled.

    Hard NPCs react faster.

    Easy NPCs react slower.

Reaction time should never become literally impossible unless a special mode deliberately requests that behavior.

84. Aim

Aim quality should be configurable.

Possible variables:

    aim error;

    aim smoothing;

    reaction delay;

    target leading;

    vertical/head targeting preference;

    recoil recovery;

    firing timing.

Hard NPCs should be more consistent rather than secretly cheating.

85. Combat movement

When enemy is visible, combat movement may temporarily override travel steering.

Possible combat behaviors:

    strafe;

    maintain preferred range;

    push;

    retreat;

    hold angle;

    reposition;

    move to cover;

    jump when tactically useful.

Combat movement must still use shared actor movement.

86. Losing sight

When enemy leaves visibility:

    remember last-known position

NPC may:

    pursue the enemy;

    search the area of the last known position;

    hold its current zone/area;

    predict where the enemy would go, and go there;

or return to objective.

Current approximate memory window:

    15 seconds

Exact behavior belongs to NPC behavior policy.

87. Reload logic

    NPC should reload when tactically appropriate.

    NPC should avoid obviously suicidal reload behavior when possible.

Possible factors:

    magazine remaining;

    reserve ammo;

    enemy visible;

    distance;

    cover;

    recent threat.

88. Weapon switching

NPC selects weapon based on:

    range;

    ammo;

    enemy distance;

    weapon readiness;

    role;

    tactical context.

Example:

    far target → rifle/revolver
    close target → shotgun

89. TeamBrain

NPC squads should eventually behave as a coordinated group.

Ideal TeamBrain capabilities include:

    enemy reports;

    bomb information;

    site assignments;

    attack site choice;

    defending sites;

    rotation;

    retake;

    support;

    avoiding excessive stacking;

    responding to teammate deaths;

    coordinating grenade usage.

90. Team information sharing

If one NPC sees an enemy:

    team may receive a report

A report may contain:

    enemy identity;

    approximate position;

    last-seen tick;

    confidence;

    site/area.

This does not mean every teammate instantly aims through walls at the exact transform.

91. Teammate death response

If teammate dies at Site B:

NPCs may infer:

    enemy activity likely near Site B

CT may rotate.

T may:

    avoid;

    support;

    trade;

    continue objective;

    change route.

Exact utility weighting remains configurable.

92. Hive-mind target behavior

Long-term NPC teams may behave with strong shared intelligence.

The goal is:

    better coordinated than many random human teams

while remaining understandable.

Too-perfect behavior should be avoided where it destroys readability or fairness.

93. Difficulty model

Difficulty should primarily adjust:

    reaction delay;

    aim error;

    aim consistency;

    decision quality;

    weapon selection;

    grenade usage;

    memory;

    prediction;

    positioning;

    teamwork;

    movement efficiency;

    tactical aggression;

    willingness to retreat;

    ability to exploit mistakes.

94. Easy NPC

Example tendencies:

    slower reactions;

    larger aim error;

    worse grenade usage;

    simpler tactics;

    weaker team response;

    more predictable paths;

    slower information response.

95. Normal NPC

Example:

    human-like reaction;

    moderate aim;

    useful objective play;

    reasonable team response;

    occasional mistakes.

96. Hard NPC

Example:

    strong aim;

    fast but plausible reaction;

    strong path choice;

    strong teamwork;

    uses utility effectively;

    punishes predictable player behavior;

    rarely gets stuck;

    maintains objective awareness.

97. Rage / Expert NPC

Target:

    project owner should lose substantially more than win

    Expert NPCs:

    miss less;

    waste fewer actions;

    coordinate more effectively;

    predict better;

    use grenades intelligently;

    choose surprising positions;

    capitalize quickly on mistakes.

They still MUST NOT rely primarily on:

    extra HP;

    direct hidden-wall aim;

    impossible speed;

    impossible damage.

98. Grenade architecture

Grenades use generalized projectile and area-effect systems.

Counter-Strike should configure them.

Do not create one entirely different projectile framework for each grenade.

99. Frag grenade

Expected:

    thrown projectile;

    bounce;

    fuse;

    explosion;

    radial damage;

    falloff;

    optional friendly/self damage;

    line-of-sight checks for explosion damaage where appropriate.

100. Smoke grenade

Expected:

    thrown projectile;

    detonation;

    smoke volume;

    timed duration visible on the smoke cloud itself, e.g. 9.82 seconds left, but , internally it is measured in ticks, so, if the smoke has a 10 sec timer, its actually just 600 ticks , 2026 10 06 1321 jorj todo define this better ;

    visual obstruction.

Eventually perception should respect smoke.

NPC should not see perfectly through a valid smoke volume.

101. Fire grenade

Expected:

    thrown projectile;

    creates area-of-effect fire;

    duration;

    periodic damage;

    spatial denial.

NPCs should understand:

    fire area = dangerous

    and avoid unnecessarily walking into it.

102. Darkbang

Darkbang replaces traditional white flashbang behavior.

Reason:

    bright white flash is unpleasant  to jorj

Instead:

    screen darkens toward black based on exposure;

    audio becomes broad/noisy/muffled;

    other sounds become obscured based on effect strength;

    effect duration depends on exposure.

    Gameplay semantics remain broadly flashbang-like.

103. Grenade line-of-sight

Darkbang/frag behavior may consider:

    distance;

    view direction;

    obstacles;

    line-of-sight.

Exact formula is configurable.

104. NPC grenade usage

NPC should evaluate:

    target location;

    teammate positions;

    self position;

    wall collision;

    benefit;

    risk;

    duplicate utility;

    objective context.

NPC should reject grenade throws likely to:

    hit a nearby wall immediately;

    heavily damage self;

    heavily damage teammate;

    duplicate a smoke or fire grenade already covering the same location without reason;

    provide no tactical benefit.

105. Grenade inventory

Grenades are finite per round.

    They reset according to round loadout.

    Exact quantity is configurable.

106. Hot-reloadable configuration

The following should generally be live-editable/hot-reloadable where technically safe:

    FOV;

    movement tuning;

    round duration;

    intermission duration;

    countdown;

    rounds-to-win;

    team capacities;

    weapon values;

    reload values;

    fire rates;

    grenade values;

    NPC aim;

    NPC reaction delay;

    NPC memory duration;

    NPC preferred range;

    NPC behavior weights;

    bomb timers;

    bombsite definitions;

    navigation tuning;

    map selection where controlled transitions exist;

    presentation flags.

107. Architectural invariants are not tuning knobs

2026 10 06 1323 jorj - todo explain the direction id like to go in where, everything is hot reloadable, id love to ahve this stuff all be editable live. especially like, you can edit one .cpp file live, compile live, and it updates behavior live. that would be awesome. just need to make a full spec for that as well 

These should NOT be casually hot-reconfigured as ordinary gameplay values:

    server authority;

    one shared actor execution path;

    one damage owner;

    one navigation owner;

    fixed simulation ownership;

    objective ownership;

    logging contract;

    request/event authority;

    human/NPC shared actor execution.

108. Navigation owner

There should be ONLY ONE generalized navigation owner. 
Ensure there are not multiple owners for the same function, anywhere, at all, for any time, or any reason, as of 2026 10 06.

bad example
problem: npc wont jump over obstacle
solution: add a new state e.g. checkIfObstacle and add a new if/then statement to its movement block

good example
problem: npc wont jump over obstacle
solution: search current code for unnecessary complexity, ensure that we are 99% confident that, there is not already a function for NPCs to jump over obstacles, and only if we are confident and have evidence that we are not going to duplicate an already existing function, then we add that new function. otherwise, prefer to extend and strengthen existing functions 

Strategic systems provide:

    destination / goal

    Navigation determines:

    how to get there

Movement execution determines:

    how actor physically performs that movement

TeamBrain MUST NOT directly move actors. it only gives information that actors can use to move themselves.

109. Behavior owner

NPC strategic/tactical decisions should have a clear owner.

Example boundary:

TeamBrain
    team-level strategy

ActorBrain
    actor-level goal/action

Navigator
    route/path

ActorIntent
    requested action

Shared Actor Execution
    physical execution

110. Runtime evidence requirement

Build success does NOT prove Counter-Strike behavior.

Selftests do NOT prove Counter-Strike gameplay.

Correct behavior should be proven using:

    source evidence
    +
    runtime evidence
    +
    human acceptance

111. Canonical logging

All relevant diagnostics use:

    StructuredLogger

as of 2026 10 06 1327 the Canonical journal:

    logs/<date>/<run>/events.jsonl

Do not create isolated debug text files for individual Counter-Strike systems.

112. Diagnostic rule

When behavior is ambiguous:

    visible/runtime symptom
        ↓
    identify responsible owners
        ↓
    instrument owner boundaries
        ↓
    run actual executable/scenario
        ↓
    read active events.jsonl
        ↓
    find first divergence
        ↓
    fix smallest responsible owner
        ↓
    rerun same scenario

113. Required NPC logging

Useful bounded events include:

    npc.target-changed
    npc.perception
    npc.memory
    npc.goal-changed
    npc.action-changed
    npc.navigation-request
    npc.nav-plan-created
    npc.nav-plan-failed
    npc.nav-replan
    npc.wall-avoid
    npc.stuck
    npc.stuck-recovery
    npc.jump
    npc.weapon-switched
    npc.shot
    npc.damage
    npc.death
    npc.respawn
    npc.team-report
    npc.objective-state

Do not emit huge per-frame spam by default.

Use:

    aggregated behavior, e.g. collect all information from tick 1 to 60, then, on tick 61, print all information in a summary block, that we just got from tick 1 to 60, then, restart the process again from tick 61 to 120, indefinitely, todo explain better 2026 10 06 1328 jorj 

    state-change events;

    bounded periodic samples;

    scenario summaries.

114. Movement decision sample

A periodic NPC movement sample should ideally contain:

    npc id
    team
    tick
    position
    goal
    goal position
    input direction
    desired direction
    actual velocity
    distance moved
    net progress toward goal
    path state
    visible target
    current action
    stuck state
    jump state
    replan reason

115. Repeated-attempt rule

If a bug is on attempt 2 or later, the AI must automatically inspect previous attempts.

Before adding code it should state:

    What did previous attempt believe?

    What evidence supported it?

    Did human/runtime evidence prove the target behavior?

    What mechanism was added?

    Is that mechanism still active?

    Is it necessary?

    Can it be simplified or deleted?

A new attempt does NOT imply a new mechanism must be added.

Deletion and simplification are valid fixes.

116. Diagnosis-before-implementation

For broken existing behavior, default workflow is:

    diagnose first
    implement second

The AI should not immediately respond to:

    NPC walks into wall

by adding another wall-escape algorithm.

It should first establish:

    where expected behavior diverged

using code + runtime evidence.

117. Runtime scenarios

MiMITA should support controlled gameplay scenarios for repeatable testing.

Example:

    scenario:
    ct_spawn_to_site_a

Setup:

    map = dust2cyberiav4
    mode = counterstrike
    seed = fixed
    actor = CT NPC
    goal = Site A
    enemies = none
    duration = 30 seconds

Expected measurements:

    reached_goal = true
    positive net progress
    low reversals
    low stuck time
    reasonable replan count
    no wall grinding

118. Required scenario: straight travel

Test:

    NPC starts away from objective
    no enemies
    clear route

NPC MUST:

    travel toward objective;

    produce positive net progress;

    stop at destination.

119. Required scenario: wall route

Test:

    goal behind full-height wall

    NPC MUST:

    route around wall;

    not repeatedly jump against it;

    not grind against wall;

    eventually regain forward progress.

120. Required scenario: low obstacle

Test:

    leg-level obstacle
    upper-body clearance

    NPC SHOULD:

    jump/traverse obstacle;

    continue route.

121. Required scenario: corner trap

Test:

    NPC placed between wall + crate/corner

    NPC MUST:

    leave local trap;

    avoid endless reversal loop.

122. Required scenario: enemy interrupt

Test:

    NPC traveling to objective
    enemy becomes visible

Expected:

travel
    → perception
    → reaction
    → combat
    → enemy disappears
    → memory/search
    → return to objective

123. Required scenario: team targeting

NPC MUST:

    target hostile actors;

    not intentionally target teammates;

    fight humans and NPC enemies using same hostility rules.

124. Required scenario: bomb carrier death

Expected:

    T carries bomb
    → T dies
    → bomb drops
    → valid T can recover it

125. Required scenario: plant

Expected:

    T carrying bomb enters site
    → holds interaction
    → plant progress
    → planted

    Leaving site interrupts.

126. Required scenario: defuse

Expected:

    CT enters range
    → holds interaction
    → defuse progress
    → bomb defused
    → CT round win

127. Required scenario: bomb explosion

Expected:

    plant
    → timer reaches zero
    → explosion
    → T round win

128. Required scenario: death spectator

Expected:

human dies
    → no active respawn
    → spectator freecam
    → world continues rendering
    → next round restores active actor

129. Required scenario: warmup

Expected:

intermission
    → movement allowed
    → combat allowed
    → death
    → immediate respawn

130. Required scenario: countdown

Expected:

3
2
1

all playing actors remain frozen.

At:

GO!!!

all release.

131. Human acceptance

Some behavior cannot be declared gold by automation alone.

Human must verify:

    movement feels suitably heavy;

    FOV looks correct;

    first-person presentation works;

    aim/recoil feels understandable;

    NPC navigation looks intentional;

    NPC difficulty feels strong but understandable;

    NPCs do not appear omniscient;

    bomb interactions are readable;

    death/spectating is not frustrating;

    round pacing feels good;

    visual presentation matches desired low-HUD style.

132. Build/test/runtime status reporting

Every agent implementing Counter-Strike behavior should report separately:

    BUILD
    SELFTESTS
    RUNTIME SCENARIO
    JSONL EVIDENCE
    HUMAN ACCEPTANCE

Never collapse these into:

    fixed

unless all relevant acceptance layers are complete.

133. Counter-Strike mode must not fork networking

Counter-Strike uses standard client/server networking.

    It does not have special Counter-Strike transport logic.

Round/objective state replicates through generalized gamemode/objective networking.

    Actors use shared movement, attack, damage, spawn, death, and replication paths.

134. Non-host fairness

Counter-Strike must be tested when the human is NOT the host.

Important validation:

    hitscan;

    rockets if allowed later;

    player movement;

    deaths;

    spectating;

    bomb interaction;

    objective state.

The experience should not produce obvious:

rocket visually one meter away
but player dies

without corresponding authoritative/predicted explanation.

135. Shared fixed simulation

Counter-Strike follows the main networking specification.

Authoritative simulation:

60 Hz

todo variable tickrate 2026 10 06 1329 jorj but not sure where to write that and also i dont think we need that right now 

NPC movement and relevant combat physics execute through the same fixed simulation system as all other modes.

136. Server authority

Server owns:

    team assignment validity;

    round state;

    bomb state;

    health;

    death;

    objective result;

    match score;

    authoritative movement;

    damage;

    win/loss;

    NPC decisions.

Clients may predict appropriate local effects/actions as defined by networking spec.

137. Scaling

Counter-Strike architecture must not assume exactly 10 actors.

Desired long-term scalability:

    5v5
    10v10
    20v20
    ...

Potential extreme:

    999v999

must remain architecturally possible even if not currently performant.

Correctness comes before claiming support.

138. NPC performance scaling

AI decision rates may scale independently from physics.

Example:

    physical movement/collision:
    60 Hz

    near decision-making:
    high rate

    far decision-making:
    lower rate

Do not reduce authoritative movement physics merely because expensive high-level decisions run less often.

139. Counter-Strike is not the final NPC architecture

Any behavior added solely because:

    Counter-Strike needs it

should be examined for generalization.

Examples:

    plant objective
    defuse objective
    team rotate
    take cover
    hold position
    move to objective
    hunt last known location

should ideally become generalized objective/action primitives usable elsewhere.

140. Main prohibitions

Do NOT:

    create Counter-Strike-only physics
    create Counter-Strike-only damage
    create Counter-Strike-only player movement
    create Counter-Strike-only weapon execution
    create Counter-Strike-only networking
    duplicate actor configuration in several owners
    let NPC strategy directly teleport/move actors
    let AI see perfectly through walls as fake difficulty
    declare gameplay correct solely because a unit test passed
    add another workaround without understanding previous attempts

141. Final target model

COUNTER-STRIKE GAMEMODE

Game Rules
    rounds
    teams
    victory
    warmup
    countdown
    objective

Roles
    Counter-Terrorist
    Terrorist
    Spectator

Actor Preset
    first person
    FOV 70
    heavy movement
    no dash/down-dash/freeze
    CS weapons
    CS presentation

NPC Brain
    perception
    memory
    team information
    tactical goal
    objective goal
    weapon decision

TeamBrain
    shared information
    attack/defend
    rotations
    support
    bomb strategy

Navigation
    route generation
    obstacle traversal
    recovery
    sustained net movement

Shared Actor Execution
    movement
    physics
    shooting
    damage
    interaction

Server
    authoritative truth

Client
    input
    prediction
    presentation

StructuredLogger
    runtime evidence

142. Definition of done — v1

Counter-Strike v1 is considered functionally complete when a human can:

    select Counter-Strike;

    choose CT or T;

    spawn at the correct team spawn;

    participate in 30-second warmup/intermission;

    enter a synchronized 3-2-1-GO!!! state;

    play a complete one-life round;

    use the required weapons;

    experience the correct 70 FOV first-person heavy movement;

    experience low-feedback, minimal HUD Counter-Strike presentation;

    fight NPCs that navigate the  map competently;

    observe NPCs that do not intentionally attack teammates;

    observe NPCs pursuing team objectives;

    carry/drop/recover/plant the bomb;

    defuse the bomb;

    win by elimination/objective/timeout;

    die and enter spectator without respawning;

    respawn correctly on the next round;

    accumulate round score;

    finish the match at configured rounds-to-win;

    play as a non-host without severe fairness failures.

143. NPC-specific definition of done

NPC behavior is not complete merely when NPCs can move and shoot.

NPC behavior is complete enough for Counter-Strike v1 when:

    NPCs reliably leave spawn;

    NPCs make sustained net movement;

    NPCs traverse the actual map;

    NPCs do not repeatedly grind walls;

    NPCs escape corners/traps;

    NPCs traverse low obstacles;

    NPCs correctly distinguish teammates/enemies;

    NPCs react to visible enemies;

    NPCs do not aim perfectly through walls;

    NPCs use remembered information after losing sight;

    NPCs resume objectives after combat;

    NPCs understand basic T/CT objectives;

    NPCs support team activity;

    NPCs can carry/plant/defend/defuse where applicable;

    NPC behavior remains difficult enough to challenge  jorj

144. Evidence-first definition of done

For a behavior to be considered proven:

    Specification says what should happen
    +
    runtime scenario exercises actual system
    +
    events.jsonl shows expected state flow
    +
    human verifies visible behavior where required

Example:

    NPC should route around wall

is not proven by:

    navigation selftest PASS

It is proven when:

    real NPC
    real executable
    real map/scenario
    real navigation owner
    real movement
    real events.jsonl
    real positive progress
    real observed escape

145. Open decisions for v2 revision

These should be reviewed by contributors based on feedback and ideal behaviors vs observed behaviors: 

    Merge Role and ActorPreset or retain separation?

    Exact Counter-Strike weapon stats.

    Exact recoil/equip timings.

    Exact crouch behavior.

    Exact fall-damage curve.

    Exact bomb timer presentation.

    Exact simultaneous-elimination behavior.

    Exact NPC strategic site-selection behavior.

    Exact dead-player voice/text restrictions.

    Exact vote-skip thresholds.

    Exact NPC difficulty profiles.

    Exact grenade quantities.

    Exact smoke perception rules.

    Exact team communication model.

    Exact competitive team-balancing rules.

    Exact objective interaction ranges.

    Exact NPC memory representation beyond v1 boolean/simple memory.

    Exact route/team distribution strategy.

    Exact healthbar policy during active mode.

    Exact melee/knife behavior.

146. Permanent guiding rule

Counter-Strike is successful when it demonstrates that:

    MiMITA does not need a special game engine implementation
    for every new game mode.

    It has powerful generalized primitives.

    Counter-Strike is simply one configuration and composition
    of those primitives.

The long-term goal is that future modes can reuse the same systems without having to rebuild:

    movement
    NPC intelligence
    objectives
    weapons
    navigation
    damage
    teams
    rounds
    networking
    spectating
    logging

from scratch.