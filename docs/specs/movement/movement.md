MiMITA Player Movement Specification
Date: July 21, 2026
Status: Target default movement behavior and migration reference
Scope: Player-controlled movement, collision, touch resets, knockback interaction, prediction, validation, and replication
________________


1. End goal
MiMITA movement MUST!!!!!!!!!!!!!!!! feel immediate, controllable, chainable, and intentionally arcade-like. Make it weird and jank. Exploitable. No hitstun no animation locks no nothing that stops u from  doing a new valid input every frame at 999fps 
The default movement style is based on direct player control rather than realistic inertia:
* WASD changes horizontal velocity instantly.
   * Not vertical 
   * U kep ur current vertical velocity 
* Vertical velocity is preserved unless a vertical ability or knockback changes it.
* Players may keep explosion or knockback momentum until they intentionally press movement.
* Touching literally anything restores movement abilities.!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
* Players can intentionally chain jumps, dashes, down dashes, freeze, explosions, walls, players, weapons, projectiles, slopes, and future moving objects.
* The movement is supposed to feel ridiculous, expressive, and exploitable in a good way.
   * Like an anime battle 
* The current local collision feel is the reference behavior and must not be accidentally simplified during networking work.
   * As of 7 21 2026 its good local collisions BUT
   * We need
   * Client server architecture
   * For movement too
   * Also its nice to ahve it defined waht even we want 
MiMITA movement is not trying to simulate realistic human movement.
It is trying to create a highly expressive system where players discover techniques and combine them, similar to how movement systems in games like Super Smash Bros. Melee create depth through chaining, canceling, timing, and player invention.
The initial networking model is intentionally client-trusting for ordinary movement:
The client is initially authoritative over ordinary movement outcomes. The server performs sanity checks, collision correction, lifecycle validation, and impossible-state rejection.
Later, stricter anti-cheat and server authority may be added.
For now, the priority is:
make movement fun
make movement immediate
make movement replicate correctly
reject only obviously impossible movement
The protocol and shared movement structures must still be designed so stronger server authority, input acknowledgement, rewind, and replay can be enabled later without rewriting the movement game or changing its feel.
________________


2. Fundamental movement rule
The core movement contract is:
previous MovementState
+ one MovementCommand
+ MovementConfig
+ collision/contact results
+ external movement events
= next MovementState
+ MovementStepEvents
Movement rules must be independent from:
* Rendering
* Audio
* Effects
* Network transport
* Client/server identity
* Replay identity
* Input polling libraries
* Global singleton state
The same reusable gameplay functions should be callable by:
* The visible client
* A listen server
* A dedicated server
* Replay simulation
* Tests
* Future authoritative prediction and replay systems
* NPCS!!!!!!!!!!!!!!!!
Client and server wrappers may have different authority responsibilities, but movement math and movement definitions must not be separately reimplemented.
* Never ever 
* EVER EVER EVE NEVER EVER  RE DEFINE IT IF IT ALREADT EXISTS 
There must be one definition of movement behavior.
one movement rule
one movement configuration
one collision meaning
one set of formulas
Never create separate copies for:
client movement
server movement
host movement
offline movement
replay movement
Those systems should call the same reusable movement functions.
Client and server literally should ideally be dumb
All the math and logic exists in these other separate files and then server/client files both just call functions from those files 
________________


3. Simulation timing and coordinate rules

Timing precedence:

* Input is sampled every render frame so local intent is recognized with the
  minimum possible latency.
* Local prediction and local display may update every render frame.
* Authoritative gameplay movement, collision, and damage use the fixed 60 Hz
  simulation tick.
* Network replication sends movement snapshots at up to 60 Hz; remote clients
  may see movement on the next received snapshot.
* Remote rendering interpolates or predicts received state and never changes
  authoritative gameplay state.
* Client prediction, server simulation, replay, and tests call the same shared
  movement rules.

If informal notes conflict with a normative section, the normative section wins.
If two normative sections conflict, the task is `NEEDS_SPEC_DECISION` until the
specification is clarified.

Default gameplay simulation:
constexpr uint32_t MOVEMENT_SIMULATION_HZ = 60;
constexpr float MOVEMENT_FIXED_DT = 1.0f / 60.0f;
Rendering is separate and may run at any frame rate.
But in game tick rate is 60hz
Later, we will make it like adjustable, like 120hz 240hz 999hz whatever 
But just do 60hz for now 
A player may render at:
30 FPS
60 FPS
144 FPS
240 FPS
500 FPS
999 FPS
Gameplay movement still simulates at 60 fixed ticks per second.
Coordinate rules:
* World Z is vertical.
* Horizontal movement uses world X/Y.
* Camera-relative movement ignores camera pitch for horizontal direction.
   * So if ur looking up, u dont walk up/down, u just keep walking forward. U only move along x/y , horizontal axis, using the walk 
* W means horizontal camera forward.
* S means horizontal camera backward.
* A means camera-relative left.
* D means camera-relative right.
* Diagonal input is normalized before applying normal movement or dash direction.
Gameplay results must not depend on rendering frame rate.!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
* This game is supposed to b e good good good  on ANY device
* Good device
* Bad device
* Etc
* Anything 
* Anywhere 
* U can run this on a potato ideally at AT LEAST 240fps 
Rendering should interpolate smoothly between simulation states so movement does not appear as:
player is here
then instantly there
then instantly there again
But animations and instant snapping is fun if we have sweep collisions and arms/limbs interact withthe world bc the should. Like ur arms legs head torso and weapon all should collide with the world and authoritatively move ur position 
The initial target preserves the current local movement tick ordering unless an implementation audit proves that a small ordering change is required for shared simulation.
Any ordering change that affects feel must be explicitly documented and tested.
________________


4. Canonical default values
The initial default movement configuration is: (these are all float values i think? In the cpp code? 7 21 2026) 
Setting
	Default
	Simulation rate
	60 Hz
	Fixed timestep
	1 / 60 second
	Ground movement speed
	20.0
	Air movement speed
	20.0
	Gravity
	-58.0
	Jump strength
	19.0
	Jump buffer
	0.12 seconds
	Coyote time
	0.001 seconds
	Air jumps
	1
	Dash horizontal impulse
	50.0
	Down-dash vertical speed
	-100.0
	Freeze maximum duration
	5.0 seconds
	Maximum fall speed
	400.0
	Maximum external horizontal impulse speed
	Initially current canonical value
	External impulse decay
	Initially current canonical value
	Walkable slope dot
	Current local collision value
	Collision skin
	Current local collision value
	Maximum step height
	Approximately 0.25 (but lowk i dont know if we evenuse this? 7 21 2026)
	Player radius
	Preserve current local value
	Player height
	Preserve current local value
	The existing local collision configuration is the source of truth until deliberately revised.!!!!!!!!!!!!!!!!!!!!!!!!!
* BUT STILL DONT MAKE IT 
* OK THIS IS GOOD AS OF 7 21 2026
* IT CANNOT just be full local.
* It needs to be 
* Client server
* All things need to be good and awesome for client/server architecture
* Movement, weapons, collisions, npcs, etc 
* Mimita is working when i can play with someone on the other side of the world + 7 other people inbetween us all in different countries, and we can all chat, and move around and shoot each other REAL TIME and it WORKS no crashing nonothing 
Values should live in one canonical movement configuration source.
Client and server must not keep conflicting copies such as:
client dash = 50
server dash = 100
Future movement modes may add:
* Acceleration
* Momentum-preserving air control
* Quake-like movement
* Source-like movement
* Glide movement
Cuz 7 20 2026 i tried to add the bhop cs style movement
* I prob just didnt do it super good
* But i didnt like it thaaaat much
* But i still do want to have that as an option eventually 
* But its whateve r
The default mode remains instant horizontal velocity control.
________________


5. Movement command model
OK , CLIENTS HAVE INSTANT MOVEMENT ON THEIR CLEINTS. IT DOES NOT WAIT FOR TICKS IT WAITS FOR FRAMES, ITS PER FRAME. IF UR AT 999 FPS U CAN MOVE 999 TIMES PER SECOND ON UR SCREEN, IT JUST GETS TRANSMITTED TO SERVER AT A MAX OF (tick rate) SO TICKRATE IS 60HZ AS OF 7 21 2026 ok 
No waiting
NO WAITING for movement 
One fixed-tick movement command conceptually contains:
struct MovementCommand
{
    uint32_t sequence = 0;
    uint64_t clientSimulationTick = 0;


    uint32_t spawnGeneration = 0;
    uint16_t transformEpoch = 0;


    float moveX = 0.0f;
    float moveY = 0.0f;


    float lookYaw = 0.0f;
    float lookPitch = 0.0f;


    glm::vec3 horizontalCameraForward{
        1.0f,
        0.0f,
        0.0f
    };


    bool jumpHeld = false;
    bool dashPressed = false;
    bool downDashPressed = false;
    bool freezeHeld = false;


    bool movementDirectionPressed = false;
    bool movementDirectionChanged = false;
};
The runtime structure may use normal types.
A later network representation may quantize and pack values to reduce packet size. And probably should 7 21 2026 
Action classification:
* WASD direction: held state plus edge/change detection
   * Input up/down
   * For dash stuff 
* Jump: held
* Dash: edge-triggered
* Down dash: edge-triggered
* Freeze: held
* Movement direction change: edge/change state
Dash requires a fresh Shift press.
Down dash requires a fresh Q press.
The movement command does not claim:
* Final position
* Final velocity
* Successful collision
* Successful dash
* Successful jump
* Final knockback result
During the initial client-trusting phase, reported position and velocity may remain in network packets.
They must remain clearly separated from the reusable movement-command contract.
________________


6. Movement state model
The deterministic movement state should conceptually contain:
struct MovementState
{
    glm::vec3 position{0.0f};


    glm::vec3 baseVelocity{0.0f};
    glm::vec3 externalImpulse{0.0f};


    float yaw = 0.0f;
    float sizeScale = 1.0f;


    bool alive = true;


    GroundState ground;
    JumpState jump;
    DashState dash;
    DownDashState downDash;
    FreezeState freeze;


    DashMomentumProtection dashMomentumProtection;
    ContactState contact;
};
Base velocity
Base velocity is owned by ordinary movement and player abilities:
* WASD movement
* Gravity
* Jump
* Air jump
* Dash
* Down dash
* Future direct movement abilities
External impulse
External impulse is owned by outside forces:
* Rocket knockback
* Grenade knockback
* Weapon recoil
* Enemy attacks
* Melee
* Environmental force
* Moving structures
* Future hazards
Normal WASD input may overwrite horizontal base velocity and cancel horizontal external impulse.
Vertical base velocity and vertical external impulse are not erased by ordinary WASD movement.
One-tick events such as: ALSO THESE I KNOW, it sas one tick, these to the client feel LIKE ONE FRAME. LIKE INSTANT. The just get transmitted to the server AS IF THE only happened for 1 tick. NO ACTUAL WAITING FOR 1 TICK 
didJump
didDash
didDownDash
didFreeze
landed
touchedWall
must not be confused with persistent movement state.
________________


7. Walking and direct horizontal control
Default movement uses instant horizontal velocity assignment. It ASSIGNS ur verlocities. No influence no acceleration it just does it.
ALSO
I think, like 7 21 2026 i think 
Ueah , ok so the revolver for example, when u shoot it, it pushes u back in the opposite direction u shot it from. So shoot forward = knocknack impulse backward
When u press WASD, it overwrites that external velocitt and sets it to 0. So ur just like at no knockback at all bc u pressed WASD once. I like that, later we build it so it doenst do that , like give an option or a mode where it doesnt do that, but i like that for now.
When ordinary WASD movement takes control:
baseVelocity.x =
    normalizedWishDirection.x * selectedMoveSpeed;


baseVelocity.y =
    normalizedWishDirection.y * selectedMoveSpeed;
Where:
selectedMoveSpeed =
    onGround
        ? groundMoveSpeed
        : airMoveSpeed;
Initial values:
groundMoveSpeed = 20.0f;
airMoveSpeed = 20.0f;
Rules:
1. WASD affects X/Y.
2. WASD does not overwrite Z.
   1. Z is like carried over from all the other sources of velocity changing things 
3. Diagonal movement is normalized.
4. Ground and air use the same instant-control behavior.!!!!!!!!!!!
5. Pressing or intentionally changing movement direction cancels horizontal knockback momentum.
6. No movement input allows existing velocity and external impulse to continue.
7. Jump does not cancel knockback.
8. Dash does not cancel knockback.
9. Freeze suppresses horizontal motion but does not delete stored knockback.
10. Weapon use does not cancel knockback unless that weapon applies another force.
   1. If shooting forward = knockback impulse backward of idk force 50
   2. That means
   3. Shoot forward = pushed back
   4. So , turn around, shoot that way = pushed again.
   5. But both should acncel each other out kinda. 
Example:
Player is not holding WASD.


Explosion launches player through the air.


Player continues moving smoothly from knockback.


Player presses W.


Horizontal movement becomes camera-forward speed 20.


Vertical movement remains unchanged.
Future movement modes may support acceleration or preserved air momentum.
They must not silently replace the default instant movement style.
________________


8. Dash and dash-momentum protection
Dash is omnidirectional. NOT RIGHT NOW IT SUCKS RIGHT NOW but ideallt is like that 
Dash works:
* On the ground
* In the air
* On slopes
* Near walls
* Near other collidable objects
Input:
Fresh Left Shift press
Dash does not repeat while Shift remains held.
The player must release and press Shift again for another dash attempt.
Dash direction
1. If WASD input exists, dash uses the normalized camera-relative WASD direction.
2. If no WASD input exists, dash uses normalized horizontal camera forward.
3. Camera pitch does not point the horizontal dash upward or downward.
Examples:
Shift
→ dash forward


A + Shift
→ dash left


W + A + Shift
→ dash forward-left


S + D + Shift
→ dash backward-right
Default effect:
baseVelocity.x += dashDirection.x * 50.0f;
baseVelocity.y += dashDirection.y * 50.0f;
Dash adds horizontal velocity.
It does not replace prior velocity.
It does not delete external impulse. Rather it adds to it.
Dash-momentum protection
The WASD combination used to trigger dash must not immediately overwrite the dash merely because those keys remain held.
Example:
Hold W+A.


Press Shift.


Dash forward-left.


Continue holding W+A.


Dash momentum remains.


Release W+A.


Momentum remains.


Later press D.


D is a new movement press.


Normal movement now overwrites X/Y velocity.!!!!!!!!!!
Exact rule:
* Record the movement direction active when dash begins.
* While the same movement combination remains continuously held, walking does not overwrite dash momentum.
* Releasing the movement keys does not erase dash momentum.
* A later fresh movement press ends protection.
* A meaningful movement-direction change ends protection.
* Lifecycle resets clear dash-momentum protection.
* Touching something resets dash availability but does not erase existing velocity.
Ground dash is intentionally allowed.
Ground friction and collision may make ground dash weaker on flat ground while creating interesting effects on slopes and edges.
Dash consumes dash availability once.
Touching anything restores dash availability. Bc its an ability and touching anything restores all abilities. 
________________


9. Jump, held jump, and wall climbing
Default jump strength:
baseVelocity.z =
    19.0f * jumpSizeScale;
Keep:
Jump buffer: 0.12 seconds
Coyote time: 0.001 seconds
Air jumps: 1
Jump is a held action:
While Space remains held, the movement system attempts to jump whenever a valid jump resource becomes available.
The client does not need to send a new jump event every tick. Like absolutely dont send it every single tick. AND AGAIN clients jump INSTANT on their screen. Its per frame. We just send out info 60hz tick rate max. So we dont clog servers. 
It may send:
jumpHeld = true;
The movement simulation decides when a jump is valid.
Ground jump
A valid ground jump occurs when:
player is grounded
or
player is inside the configured coyote window
Ground jump:
* Sets vertical base velocity to jump strength
* Leaves horizontal base velocity unchanged
* Leaves external impulse unchanged
* Consumes the current ground-jump opportunity
* Makes one air jump available
* Produces a ground-jump event
Air jump
The player has one air jump.
A valid air jump:
* Sets vertical base velocity to jump strength
* Does not overwrite horizontal velocity
* Does not delete external impulse
* Consumes the air jump
* Produces an air-jump event
Touch-reset climbing
Touching any valid gameplay contact restores jump resources. Bc jump is an ability, definitely air jump is an ability, ground jump im not sure? 7 21 2026 todo figure out and define better 
Therefore:
Hold Space against wall
→ touch wall
→ jump restores
→ held Space attempts jump
→ player jumps again
→ touch wall again
→ repeat
This creates wall climbing.
This behavior is intentional.
A single tick must not execute the same restored resource more than once.
AND FOR CLIENT INSTANT FEEL, HTE SAME FRAME CANT DO IT MORE THAN ONCE. 1 frame = 1 action. So u can do bunches of actions over and over with a super quick framereate but it only sends 1 time to the server 60hz rate 
________________


10. Down dash
Down dash is the active downward ability.
The old Ground Return ability is obsolete. NOT GOOD!!!!!!!!!1
It should be removed after compatibility checks prove nothing still depends on it.
Input:
Fresh Q press
u have to let go of Q then press it again to do another down dash if u want to do it 
Default result:
baseVelocity.z = -100.0f;
Rules:
* Works in the air
* Works while grounded
* Preserves horizontal base velocity
* Preserves horizontal external impulse
* Replaces current vertical base velocity with -100
   * It REPLACES it
   * It SETS ur Z velocity to -100.  No influence no additions. It SETS it.
* Does not erase vertical external impulse unless collision requires it
* Consumes down-dash availability
* Touching anything restores down-dash availability
* Holding Q does not repeatedly down dash
* Every down dash requires a fresh Q press
Grounded down dash is intentional.
On flat ground it may resolve immediately.
On slopes, edges, seams, moving objects, or unusual geometry, it may launch the player strongly. This is AWESOME and we want that. That is good jank, interaction between systems that wasnt intended but found.
Those interactions are part of the movement system.
Do not automatically remove them because they look unrealistic. We lowk prefer things that look unrealistic. Bc on the inside i feel unrealistic. All the time people telling me u cant do this. Cant do that. Nope the server thinks ur hacking. Nope its boots on the ground no floating no flying around. Nope this that that adn this no fun no fun allowed. SHUTUp!!!!!!!!!!!!!!!1 this game  for fun. Max max max fun. Not  for realism. Realism fits WITHIN fun,  sometimes fun is NOT realist.c 
________________


11. Freeze
Freeze is a held movement ability activated with E.
Core goals:
* Immediate horizontal stop
   * THE FRAME u press freeze, it does it 
* Vertical movement remains active
* Horizontal external knockback is retained
* Freeze weakens over five seconds
* Touching anything restores full freeze
* Releasing ends freeze immediately
* Releasing and pressing again in midair does not recharge it
Activation
When freeze activates with full availability: (touching world makes it go back to full avialabilitty)
baseVelocity.x = 0.0f;
baseVelocity.y = 0.0f;


freeze.active = true;
freeze.timer = 0.0f;
freeze.available = false;
Vertical base velocity remains.
Vertical external impulse remains.
Horizontal pass-through curve
float u =
    clamp(
        freeze.timer / 5.0f,
        0.0f,
        1.0f
    );


float horizontalPassThrough =
    pow(u, 4.0f);
WE HAVE A FUNCTION FOR THE FREEZE STREGNTH I HAVE NO IDE AWHERE IT IS BUT 7 21 2026 THE REPO HAS THE FUNCTION FOR FREEZE STRegnth drop off. So just do that again. 
Approximate behavior:
Time held
	Horizontal force allowed
	0 sec
	0%
	1 sec
	0.16%
	2 sec
	2.56%
	3 sec
	12.96%
	4 sec
	40.96%
	5 sec
	100%
	Freeze remains extremely strong early.
It becomes much weaker near seconds four and five.
Applied movement while frozen
Conceptually:
appliedHorizontalVelocity =
    baseVelocity.xy * horizontalPassThrough
    +
    externalImpulse.xy * horizontalPassThrough;
Vertical movement is unaffected:
appliedVerticalVelocity =
    baseVelocity.z
    +
    externalImpulse.z;
This means the player can:
hold freeze
shoot downward
receive vertical knockback
launch upward
Stored external impulse
Freeze does not delete external horizontal impulse.
External impulse continues normal decay while freeze is held.
Example:
Player has external impulse 1000.


Player activates freeze.


Horizontal movement is almost fully suppressed, and decays over the freeze's strength decay curve.


Vertical movement remains active.


starting impulse of 1000 continues decaying internally.


Freeze becomes weaker.


More of the remaining impulse starts moving the player.


At five seconds, remaining horizontal impulse passes through fully.
Release and recharge
* Releasing E immediately ends active freeze.
* Re-pressing E without touching something does not restore full strength. It doenst even let u do a freeze bc u havent reset the ability.
   * Todo addd a dry fire for abilities? Like a weak effect thats like “No ability!” 
* At five seconds, holding E may continue, but freeze has no remaining horizontal suppression.
* Touching anything restores full freeze strength.
* If E remains held during contact, freeze  DOES NOT return to full strength.
   * Freeze requires a new fresh E press to begin freeze again
* If E is not held, contact restores availability for the next press likenormal. It restores regardless of E held or not
Freeze does not suppress:
* Gravity
* Jump velocity
* Down dash
* Rocket jump Z force
* Grenade jump Z force
* Vertical recoil
* Vertical knockback
________________


12. Universal contact reset
Universal contact reset is a defining MiMITA movement rule.
Touching anything that participates in gameplay contact resets all touch-reset movement abilities.
Qualifying contacts include:
* Ground
* Wall
* Ceiling
* Slope
* Step
* Static map geometry
* Moving platform
* Moving structure
* Ladder
* Water
* Another player
* Player root body
* Player body part
* Weapon collision
* Friendly weapon
* Enemy weapon
* Own weapon when it creates real contact
* Projectile contact
* Friendly projectile
* Enemy projectile
* Own projectile
* Rolling grenade
* Explosion radius influence
* Future collidable gameplay objects
A qualifying contact restores:
air jump
dash
down dash
freeze
future TouchReset abilities
If Space is held when contact restores jump:
the movement simulation attempts another jump
andthen it should work if its doable


like a wall jump in mimita means 
walk into the wall
hold space
u will climb the wall bc u keep resetting ur jump over and over and over


todo 7 21 2026 how to send htis to server without bugging it?
i know we just send jump held true/false, we cant send like abilities reset over and over, matbe like abilities allowed , jump = true? everu tick? idk 
If E is held when contact restores freeze:
freeze timer resets to 0
freeze strength returns to full
BUT U DONT GET THE FREEZE FULL STRENGTH BACK UNLESS U LET GO OF FREEZE THEN PRESS IT AGAIN


so 


1. hold E, for 5 sec, go to 0% freeze strength
2. touch the ground, still holding E
3. now im at 100% freeze strength, but the current freeze i have , from when i went to 0% strength, is still active, so i have 0% stregnth. 
Explosion influence counts as a contact reset even without direct projectile-body contact.
The explosion should emit a deduplicated reset event.
A duplicate network event must not apply unrelated effects twice.
Dead players do not receive or consume movement resets. Bc like. U know. Theyre dead. Cant move if ur dead. 
________________


13. Collision and world interaction
The current local collision feel is the reference.
Preserve:
* Root capsule collision
* Body collision
* Weapon collision
* Sweep and slide
* Depenetration
* Ground recovery
* Step-up
* Slope interaction
* Ceiling response
* Wall response
* Stuck recovery
* Existing collision skin
* Existing tolerances
* Existing size scaling where intended
The shared movement simulation should query an abstract collision interface.
It should not directly depend on a rendering world or headless server type.
Conceptual interface TODO THIS NOT FINAL 7 21 2026 MAKE FINAL
class MovementCollisionWorld
{
public:
    virtual CapsuleSweepResult sweepPlayerCapsule(
        const CapsuleSweepInput& input
    ) const = 0;


    virtual ContactSet queryPlayerBodyContacts(
        const BodyContactInput& input
    ) const = 0;


    virtual ContactSet queryWeaponContacts(
        const WeaponContactInput& input
    ) const = 0;


    virtual GroundQueryResult queryGround(
        const GroundQueryInput& input
    ) const = 0;


    virtual DynamicContactSet queryDynamicGameplayContacts(
        const DynamicContactInput& input
    ) const = 0;
};
Client and server may store the map differently.
Their collision semantics must match. EXACTLY MUST MATCH NO DIFFERENCE. BOTH IDEALLY USE THE SAME EXACT FUNCTIONS
Step-up
Step-up remains enabled.
Initial maximum step height:
maxStepHeight = 0.25f;
Slopes
Down dash against slopes is intentionally allowed to create launches.
Do not add safety logic that removes slope launches unless the behavior is proven to be a collision bug.
Prefer like 99/100 times to add UNSAFE logic. Things that if abused or exploited  allow for huge movement speed increases or “Unbalanced” gameplau. I want unbalanced. Its in the name of fun spirit of fun. Unbalanced again is something that fits within the broader  idea of “fun”. So if its fun then keep it.
Body and weapon collision
Body and weapon collision are gameplay.
They are not presentation-only.
They must eventually exist in multiplayer authority and contact replication. Cuz right now the dont.
BUT 
LIKE OK
This is defined better in collision spec here: https://docs.google.com/document/d/1uTxOkciwKHtSzBm4rKxxMqMshNGttK9TOk8ju3uBu-E/edit?tab=t.uo9zem1su2ua 
But
Mimita characters are like minecraft characters. 
Head, arms, torso, legs.
But each of these limbs interacts with the world directly. 
So ur arm being on a ledge =  u wont fall. Ur arm is a auhtoriitve position setter of ur player’s position.
Same with ur weapon.
If ur shotgun is held in front of u. And u walk into a wall. U should NOT be able to just go into the wall. The shotgun is welded to ur arm ur body etc. and it doenst just let u walk into walls. 7 21 2026 this behavior is not present in client/server architecture new update but we do later. 
ALSO THIS M FAVORITE PART
Reloads and other weapon poses have weird ridiculous  poses and values. I think for the shotgun reload the gun literallt goes off to ur right and floats there for like 1.5 seconds. 
U should 1000000% BE ABLE TO HANG on things using thta. Its welded to ur arm right? Ok and ur arm is authoitirve to ur position right?
Ok this means shotgun should be able to hover 5 meters to ur right and u should be able to just hang there fora s long as the reload pose is alive, so u can kinda float
Touching bodies and weapons also triggers universal reset. A plauers weapons touching world triggers universal abilities reset too.
________________


14. Knockback and external impulse
All knockback enters movement through explicit external impulse state or an equivalent event timeline.
Sources include:
* Rocket explosion
* Grenade explosion
* Projectile contact
* Weapon recoil
* Enemy attack
* Melee
* Environmental hazard
* Moving structure
Conceptual application:
externalImpulse += calculatedImpulse;
Rules:
* No WASD means external impulse keeps moving the player.
* Fresh WASD input cancels horizontal external impulse.
* WASD does not cancel vertical external impulse.
* Jump does not cancel external impulse.
* Dash does not cancel external impulse.
* Down dash does not cancel horizontal external impulse.
* Freeze suppresses horizontal external impulse.
* Freeze does not delete external impulse.
* Freeze does not suppress vertical external impulse.
* External impulse continues normal decay while frozen.
Explosion knockback may do two things:
apply external impulse
reset movement abilities
Those are separate results of one explosion.
Predicted and authoritative knockback must use stable IDs.
Duplicate confirmations must not apply force twice.
________________


15. Death, respawn, teleport, and reconnect
Dead players:
* Do not run normal movement
* Do not jump
* Do not dash
* Do not down dash
* Do not freeze
* Do not consume contact-reset events
* Do not accumulate movement commands for later replay
Respawn creates a clean movement state:
position = authoritative spawn position


base velocity = spawn velocity
normally zero


external impulse = zero


ground state = recomputed


air jump = available


dash = available


down dash = available


freeze = fully available


freeze timer = zero


dash-momentum protection = cleared


old contact events = cleared


old prediction history = cleared


old command history = cleared or generation-gated
Every life uses: TODO CONFIRM THIS IDK IF THIS IS THE ACTUAL NAME OF THE SPAWN THING BU I KNOW WE HAV  A AUTHORITIVE SPAWN FUNCTION AND WE NEED TO USE ONLT THAT 7 21 2026 
spawnGeneration
Teleports, reconnects, map transitions, and authoritative position replacement use:
TODO CONFIRM THIS IDK IF THIS IS THE ACTUAL NAME OF THE SPAWN THING BU I KNOW WE HAV  A AUTHORITIVE TRANSFORM EPOCH thing AND WE NEED TO USE ONLT THAT 7 21 2026 


transformEpoch
Old-life commands and events must not affect a new life.
A player must be able to:
join
move
use abilities
die
respawn
move again
use abilities again
repeat forever

________________
16. Initial client-authoritative networking model
The first target prioritizes responsiveness and implementation speed.
For ordinary movement:
1. Client captures movement input.


2. Client runs movement immediately.


3. Client sends intent and resulting movement state.


4. Server validates ownership, lifecycle, finite values,
   plausible speed, trajectory, bounds, and impossible transitions.


5. Server performs collision correction or rejects impossible state.


6. Server distributes accepted movement state.


7. Other clients interpolate that player.
The server initially trusts reasonable movement outcomes.
The server still rejects or corrects:
* NaN
* Infinity
* Impossible map position
* Movement while dead
* Old-life movement
* Wrong transform epoch
* Extreme unsupported velocity
* Huge teleport
* Movement through obviously blocking geometry
* Wrong player ownership
* Malformed input
* Impossible lifecycle transitions
This phase is intentionally not strong anti-cheat.
It must be documented honestly as a client-trusting movement policy.
The server remains authoritative over:
* Health
* Damage
* Death
* Respawn generation
* Match state
* Score
* Inventory
* Ammo
* Weapon cooldown
* Authoritative projectiles
* Authoritative explosions
Later wehn i can figure it out  movement can be more server authoritative but for now its fine. I just want to run around and have fun. I dont want to have desync either like if  someone on high ping is desncrhonizing from where the actually are and im getting shot behind walls and stuff that sucks. But whatver 
________________


17. Shared simulation and future authority
Even during the client-trusting phase, movement should be represented through shared structures and reusable functions. AND DONT DUPLCIATE ALREDT EXISTING things . client server both use SAME FUNCTIONS client server dont know dontcare if its client or server the just both do the same math both do the same functions. Same numbers etc.
Target API:
MovementStepResult simulateCharacterMovement(
    MovementState& state,
    const MovementCommand& command,
    const MovementConfig& config,
    const MovementCollisionWorld& world,
    std::span<const MovementExternalEvent> externalEvents,
    float fixedDt
);
The function owns:
* Movement math
* Timers
* Ability availability
* Freeze pass-through
* Universal resets
* Collision response
* Movement events
The function does not:
* Send packets
* Render
* Play sounds
* Spawn visual effects
* Decide authority
* Read GLFW
* Know client versus server
* Know host versus dedicated server
Future stricter movement authority may use:
client creates sequenced commands


client predicts shared movement


server runs the same movement function


server acknowledges processed sequence


client rewinds to authoritative state


client replays unacknowledged commands


visible model smooths correction
This future upgrade must not redesign:
* Walking
* Jump
* Dash
* Down dash
* Freeze
* Universal reset
* Collision feel
* Knockback behavior
________________


18. Replication, prediction, and correction
Local player
Local movement always reacts immediately.
The player never waits for a round trip to:
* Walk
* Jump
* Air jump
* Dash
* Down dash
* Freeze
* Receive predicted self-knockback
* Receive predicted explosion reset
Remote players
Remote players use:
* Snapshot interpolation
* Short interpolation buffer
* Linear position interpolation initially
* Angular interpolation
* Limited extrapolation
* Hard lifecycle reset for respawn or teleport
Remote private inputs do not normally need to be predicted.
Correction levels
Small disagreement
* Correct quietly
* Smooth visible model
* Smooth camera offset
* Avoid changing gameplay feel
* Server disagreement visuals link: https://docs.google.com/document/d/1uTxOkciwKHtSzBm4rKxxMqMshNGttK9TOk8ju3uBu-E/edit?tab=t.81l3pnizgdad 
*  do these 
Medium disagreement
* Correct gameplay state
* Smooth visual representation
* Record diagnostics
* Server disagreement visuals link: https://docs.google.com/document/d/1uTxOkciwKHtSzBm4rKxxMqMshNGttK9TOk8ju3uBu-E/edit?tab=t.81l3pnizgdad 
*  do these but bigger
Major disagreement
* Replace invalid gameplay state
* Clear invalid prediction
* Clear invalid contact history
* Show optional server-disagreement effect
* Never leave collision on the wrong side of a wall
* Server disagreement visuals link: https://docs.google.com/document/d/1uTxOkciwKHtSzBm4rKxxMqMshNGttK9TOk8ju3uBu-E/edit?tab=t.81l3pnizgdad 
*  do these but HUGEly huge effect
Future replay must preserve:
* Dash-momentum protection
* Held-jump behavior
* Touch resets
* Explosion resets
* External impulse
* Freeze timer
* Freeze pass-through
* Spawn generation
* Transform epoch
________________


19. Testing requirements
Deterministic movement tests
Test as of 7 21 2026: 
* Ground speed 20
* Air speed 20
* Instant WASD overwrite
* WASD preserves Z
* No input preserves momentum
* Fresh movement cancels horizontal knockback
* Jump preserves momentum
* Jump strength 19
* One air jump
* Held Space jumps after reset
* Wall climbing
* Omnidirectional dash
* Camera-forward dash fallback
* Dash adds 50
* Held dash direction does not erase dash
* New movement input erases dash momentum
* Shift requires a fresh press
* Down dash sets Z to -100
* Down dash preserves X/Y
* Grounded slope down dash
* Freeze immediately stops horizontal movement
* Freeze preserves vertical movement
* Fourth-power freeze curve
* Stored knockback remains during freeze
* Touch restores freeze
* Release ends freeze
* Re-press without touch does not recharge
* Every contact resets abilities
* Explosion resets abilities
* Duplicate explosion reset is deduplicated
* Step-up
* Slope behavior
* Body contact
* Weapon contact
* Player contact
* Projectile contact
* Death disables movement
* Respawn restores movement
* Old-life events are rejected
Network tests
Test:
* Two clients
* Same computer
* Different computers
* Direct ICE
* Relay when available
* 0 ms latency
* 50 ms latency
* 100 ms latency
* 250 ms latency
* 500 ms latency
* Jitter
* Packet loss
* Duplication
* Reordering
* Burst delivery
* Disconnect
* Reconnect
* Death and respawn repeated 50 times
Manual feel tests
Verify:
* Instant movement still feels the same
* Rocket knockback remains smooth
* Grenade knockback remains smooth
* Slope down dash still launches the player
* Wall-jump chains work
* Player contact resets abilities
* Weapon contact resets abilities
* Projectile contact resets abilities
* Explosion resets abilities
* Freeze plus downward shooting launches upward
* Dash follows camera-relative WASD
* Holding dash keys does not cancel dash
* New movement input takes control instantly
________________


20. Implementation and migration order
Stage 0 — Preserve behavior
* Keep the current movement-code report
* Keep rocket and grenade regression tests
* Add movement baseline tests
* Do not change feel
Stage 1 — Shared movement data
Introduce:
MovementCommand
MovementState
MovementConfig
MovementStepEvents
MovementExternalEvent
conversion helpers
No runtime behavior change.
Stage 2 — Shared movement kernel
Extract current local movement and collision behavior into reusable functions.
Apply intentional changes:
* Omnidirectional ground and air dash
* Dash-momentum protection
* Remove Ground Return
* Ground-capable down dash
* Horizontal-only freeze
* Fourth-power freeze curve
* Universal reset
* Explosion reset events
Do not change network authority in the same task.
Stage 3 — Client-trusting network integration
* Client simulates and reports movement
* Server validates sanity
* Server validates lifecycle
* Server validates bounds
* Server validates collision
* Server replicates accepted state
Stage 4 — Dynamic collision replication
Add consistent multiplayer contact for:
* Players
* Bodies
* Weapons
* Projectiles
* Explosions
* Moving structures
All relevant contacts trigger universal reset.
Stage 5 — Sequenced commands
Add:
* Input sequence
* Client tick
* Recent-command bundling
* Duplicate rejection
* Old-life rejection
* Held and edge action semantics
Stage 6 — Prediction history
Add:
* lastProcessedInputSequence
* Prediction history
* External event history
* Contact history
* Rewind
* Replay
* Render-only smoothing
Stage 7 — Optional stronger server authority
Later:
* Server derives movement from commands
* Client position becomes diagnostic
* Server acknowledges inputs
* Client rewinds and replays
* Anti-cheat becomes stricter
This is an authority upgrade, not a movement redesign.
Stage 8 — Worldwide validation
Test:
* High latency
* Jitter
* Loss
* Duplication
* Reordering
* Interruption
* Reconnect
* Repeated respawn
* Projectile and movement interactions
________________


Canonical design summary
instant horizontal player control


vertical momentum preserved


external knockback persists until WASD takes control


omnidirectional additive dash


dash momentum survives held trigger direction


new movement input overwrites horizontal velocity


one air jump


held jump attempts whenever contact restores it


Q down dash at -100


down dash works on ground and slopes


five-second horizontal-only freeze


vertical knockback always passes through freeze


freeze suppresses but does not delete horizontal impulse


touching literally anything resets every movement ability


explosions also reset abilities


current local collision feel is preserved


client movement is initially trusted after sanity validation


shared structures preserve a path to stronger authority later
This document is the target MiMITA movement reference.
Every future movement or networking task should compare its changes against this specification.
