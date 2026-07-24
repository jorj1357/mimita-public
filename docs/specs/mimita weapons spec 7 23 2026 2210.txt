Weapon system spec after full client/server architecture change
Date: 7/18/2026
End goal
There is no separate local-play gameplay path.
There is only client/server gameplay.
Even when one player is playing alone:
* mimita.exe --server runs the authoritative server.
* The visible game runs as a client.
* The client sends requests to the server.
* The server validates those requests.
* The server updates authoritative state.
* The server replicates confirmed results to every client.
The host player follows the same gameplay rules as every remote player.
The host has extremely low latency because the server is running on the same computer, but the host is not allowed to bypass server validation.
The pattern is always:
client intent
    ↓
client prediction
- for explosions and instant feedback
- so rocket launcher instantly knocks me back, collisions instantly move me, etc
    ↓
server validation
    ↓
authoritative simulation
    ↓
server replication
    ↓
client reconciliation

________________
Core weapon architecture
Every weapon is a different configuration of the same WeaponDefinition struct.
There should not be completely separate systems such as:
GrenadeLauncherWeapon
RocketLauncherWeapon
ShotgunWeapon
RevolverWeapon
Instead, all weapons use the same reusable systems:
psuedocode 7 18 2026, not final:


weapon definition
weapon runtime
fire validation
hitscan system
projectile system
melee system
damage system
knockback system
explosion system
effects system
client prediction
server replication
The weapon definition selects which systems are used and supplies their configuration.
________________


WeaponDefinition
WeaponDefinition contains permanent weapon configuration.
It describes the rules of the weapon.
It does not contain current ammo, current cooldown, current reload progress, or other changing state.
Conceptual structure:
enum class FireType
{
    Hitscan,
    Projectile,
    Melee
};


enum class SpreadType
{
    None,
    FixedPattern
};


enum class TriggerType
{
    SemiAutomatic,
    Automatic,
};


struct AmmoDefinition
{
    bool enabled = true; // false means infinite ammo like for a sword or melee object


    int magazineSize = 1;
    int reserveSize = 0;


    float reloadDurationSeconds = 1.0f; // default, i think weapon changes this? 7 18 2026 
    int ammoConsumedPerShot = 1;
};


struct FireDefinition
{
    FireType type = FireType::Hitscan;
 TriggerType triggerType= TriggerType::SemiAutomatic;


 float fireDelaySeconds = 0.2f; // defualt again


    int shotCount = 1;


    SpreadType spreadType = SpreadType::None;
    float spreadDegrees = 0.0f;
};


struct HitscanDefinition
{
    bool enabled = false;


    float range = 1000.0f;
    float radius = 0.0f; // radius of hitscan beam, i think controlled by weapons.json or a similar hot-reloadable thing? 
};


struct ProjectileDefinition
{
    bool enabled = false;


    float speed = 40.0f; // all defaults
    float radius = 0.2f;
    float gravityScale = 1.0f; // this IS THE 1 PLACE THAT OWNS GRAVITY NOWHERE ELSE 


    bool bounceEnabled = false;
    float bounceRestitution = 0.5f;
    float friction = 0.2f;


    float maximumLifetimeSeconds = 10.0f;
};


struct MeleeDefinition
{
    bool enabled = false;


    float range = 2.0f;
    float radius = 0.5f;
    Shape shape = Shape::Block // shape block, shape sphere, shape elognated sphere, shape cylinder, shape cone... etc 7 18 2026
    float activeDurationTicks = 6.0f; // after this much ticks, instantly delete it
    Style style = Style::Godball // style hitbox, like smash, and style godball means, check if we're near a plauer, if we are, check if godball is touching them/intersecting them, if we are, check the speeed/force/impact/angle etc of the touch from this tick to prev tick, then do damage based on that force value, every tick we are intersecting them
};


struct DamageDefinition
{
    bool enabled = true;


    float directDamage = 0.0f;
    float bodyPartMultiplier = 1.0f; // default 1.0f, but for head, torso, legs etc, arms 
};


struct KnockbackDefinition
{
    bool enabled = false;


    float directForce = 0.0f;
    float selfForceMultiplier = 1.0f; // need different knockback values for X/Y (horizontal) and Z (up/down, Z is up in mimita)
};


struct ExplosionDefinition
{
    bool enabled = false;


    float radius = 0.0f;
    float damage = 0.0f;
    float knockbackForce = 0.0f;


    bool explodeOnImpact = false;
    bool explodeOnFuse = false;


    float fuseSeconds = 0.0f;
};


struct RecoilDefinition
{
    bool enabled = false;


    float strength = 0.0f;
};


struct WeaponEffectsDefinition
{
    std::string fireSound; // put relateive paths foe these
    std::string muzzleEffect;
    std::string projectileTrailEffect;
    std::string impactEffect;
    std::string explosionEffect; // call the single effect part system i think? for any effect, should just be 1 effect part that handles sounds and hitfx vfx and stuff, 
    std::string explosionSound;
};


struct WeaponDefinition
{
    std::string id; // 1,2,3,4,5,6.. 
    std::string displayName; // godball,revolver, shotgun, rocket launcher... 


    AmmoDefinition ammo;
    FireDefinition fire;


    HitscanDefinition hitscan;
    ProjectileDefinition projectile;
    MeleeDefinition melee;


    DamageDefinition damage;
    KnockbackDefinition knockback;
    ExplosionDefinition explosion;


    RecoilDefinition recoil;
    WeaponEffectsDefinition effects;
};

________________
Example weapon definitions
Revolver
fire type: hitscan
trigger type: semi automatic
shot count: 1
spread: none
magazine: 6
reserve: 1337
fire delay: 0.2 seconds
direct damage: enabled
knockback: enabled
recoil: enabled
projectile: disabled
melee: disabled
explosion: disabled
Shotgun
fire type: hitscan
trigger type: semi automatic
shot count: 15
spread: fixed pattern
magazine: configurable
reserve: configurable
direct damage: applied separately for every ray
knockback: accumulated from all confirmed pellet hits
projectile: disabled
melee: disabled
explosion: disabled
Grenade launcher
fire type: projectile
trigger type: semi automatic
shot count: 1
spread: none
magazine: 4
reserve: 1337
projectile speed: configurable
projectile gravity: enabled
projectile collision: sphere
projectile bounce: enabled
direct damage: optional
explosion: enabled
explosion on fuse: enabled
explosion on impact: configurable
knockback: enabled
recoil: enabled
Sword
fire type: melee
trigger type: semi automatic
shot count: 1
ammo: disabled
spread: none
projectile: disabled
hitscan: disabled
melee collision volume: enabled
direct damage: enabled
knockback: enabled
explosion: disabled
shotCount means how many attack traces or projectiles one trigger press creates.
For a shotgun, shotCount = 15 creates 15 hitscan rays.
For a grenade launcher, shotCount = 1 creates one projectile.
For a sword, shotCount = 1 creates one melee attack volume.
________________


Reload 
Ok so 
* 7 18 2026 this might not need to be in ths section?
* But
* All weapons 
* Auto relaod, when unequipped
* So,
* Reveolver boom boom boom!
* 6 in clip to 3 in clip
* Switch to shotgun = INSTANT start revovler auto reload in backgrund
* Then, when i equip it, it is fullt laoded again 
* Also, rocket launcher adn grenade launcher load 1 rocket/grenade at a time, like tf2 


Reload issue 7 22 2026
Ok so also
* 7 22 2026 
* The 
* There should be a single reload function
* Todo define it fullt here i dotn know what it looks like or where it is in the code
* I think ideally , write it in the google doc like the specifics treat this google doc like the super duper brain for mimita and then make the code match the doc as best it can, and if it cant then the doc is meesd up 
* But
* Reload has a issue


Important finding already visible
The spawn processing does this:
player.weaponRuntimes.clear();
Then rebuilds each runtime from the authoritative spawn entries:
reconcileAuthoritativeWeaponRuntime(
    mpContext,
    player,
    slot.weaponDefNetworkId,
    slot.magazineAmmo,
    slot.reserveAmmo,
    ...
);
That is reasonable only if:
1. Every weapon is present in the spawn packet.
2. Every network weapon ID maps correctly.
3. The reconciler creates the local runtime with both ammo values.
4. No later generic weapon initialization recreates it using defaults or zeroes.
5. A stale state revision cannot overwrite newer ammo.
The authoritative packet structures do contain separate magazine and reserve values, and reload results send both values back.
So the reserve ammo is probably being lost in one of these places:
server weapon initialization
spawn packet population
network-ID → weapon-ID mapping
authoritative runtime reconciliation
local runtime recreation
HUD reading the wrong runtime
fire confirmation applying incomplete state
Reload architecture: your intended rule is correct
There should be one canonical reload transition, conceptually:
ReloadResult tryStartReload(
    WeaponRuntime& runtime,
    const WeaponDefinition& definition,
    uint64_t currentTick,
    ReloadReason reason);
Both manual and automatic reload should call it:
tryStartReload(runtime, definition, tick, ReloadReason::Manual);
tryStartReload(runtime, definition, tick, ReloadReason::EmptyMagazine);
tryStartReload(runtime, definition, tick, ReloadReason::Unequipped);
Then one canonical completion function:
ReloadCompletionResult completeReload(
    WeaponRuntime& runtime,
    const WeaponDefinition& definition,
    uint64_t currentTick);
It should own all actual ammo math:
needed = magazineSize - magazineAmmo;
transferred = min(needed, reserveAmmo);


magazineAmmo += transferred;
reserveAmmo -= transferred;
No rocket-specific, grenade-specific, shotgun-specific, client-only, or server-only ammo-transfer formulas.
Your weapons specification already says all weapons should be configurations of shared reusable systems, not separate weapon implementations.


Aim system


Default: camforward
* Origin: muzzle barrel tip
* End: cam forward
* True parallel to cam forward, no farpoint
* I think this is like CS? Or tf2? Idk 
* But keep it like this 


Others but dont use 7 18 2026 
* Crosshair
   * The spot in world u click on is where ur shot goes
   * Like roblox, click on smth behind u = shoot behind u 
* Worldcrosshair
   * The spot from the muzzle tip to the spot on world that cam forward hits, that spot is where the gun bullet goes
   * But the corsshair is on that spot in the world
* Farpoint
   * 999999.0f away point
   * Not true parallel so it sucks 
WeaponRuntime
WeaponRuntime stores the changing state of one weapon owned by one player.
Conceptual structure:
struct WeaponRuntime
{
    int currentMagazineAmmo = 0;
    int currentReserveAmmo = 0;


    uint64_t nextAllowedFireTick = 0;
    uint64_t reloadCompleteTick = 0;


    bool isReloading = false;


    uint32_t nextAttackRequestId = 1;
    uint32_t nextConfirmedShotId = 1;
};
Example:
WeaponDefinition:
grenade launcher magazine size = 4


Player 1 WeaponRuntime:
current magazine ammo = 2
is reloading = false


Player 2 WeaponRuntime:
current magazine ammo = 4
is reloading = true
The definition is shared.
The runtime is unique for each player weapon instance.
________________


AttackRequest
When a client wants to fire/attack (with any weapon, even melee even godball), it sends an AttackRequest.
Conceptual structure:
struct AttackRequest
{
    uint32_t requestId;


    EntityId playerId;
    WeaponId weaponId;


    uint64_t clientSimulationTick;


    glm::vec3 aimOrigin;
    glm::vec3 aimDirection;


    glm::vec3 predictedMuzzlePosition;
};
The request contains intent.
It does not contain trusted damage values.
The client must never send:
I dealt 100 damage
I killed player 2
My grenade exploded here and damaged these players
The client instead sends:
I attempted to fire this weapon
from this origin
in this direction
at this simulation tick
The server determines the result.
________________


Full grenade launcher flow
Phase 1: client input
On client simulation tick 1000:
Player presses attack.
The client reads the currently equipped weapon definition.
The client sees:
fire type = projectile
shot count = 1
projectile enabled = true
explosion enabled = true
The client performs lightweight prediction checks:
Does my predicted state say I am alive?
Does my predicted state say I have ammo?
Does my predicted state say the cooldown is finished?
Am I not currently reloading?
These checks are only for responsiveness.
The server will repeat the real checks.
If these lightweight checks pass, instantly spawn/do this request, for the client only, replicate to others later.
E.g. do knockback to self instantly, spawn projectile instantly, do explosions instantly for instant rocket launcher rocket jumping
________________


Phase 2: create the attack request
The client creates:
request ID: 481
weapon: grenade_launcher
client tick: 1000
aim origin: muzzle position
aim direction: normalized XYZ direction
predicted muzzle position: current weapon muzzle
The client sends this request to the server.
sendAttackRequest(request);

________________
Phase 3: instant client prediction
The client does not wait for the server response before showing immediate feedback.
The client immediately predicts:
temporary ammo decreases from 4 to 3
fire animation plays
muzzle flash appears
fire sound plays
recoil is applied
predicted grenade is spawned
The predicted grenade receives:
prediction owner: local client
source attack request ID: 481
position: predicted muzzle position
velocity: aim direction × projectile speed
The grenade begins moving immediately using the shared loose-object physics system.
THERE IS ONLY 1 SYSTEM LIKE THIS. THER IS NO OTHER ONES. CLIENT, AND SERVER, BOTH USE THIS SAME PHYSICS SIMULATION SYSTEM.
simulateLooseObject(
    predictedGrenade.state,
    predictedGrenade.physicsConfig,
    clientPhysicsWorld,
    fixedDeltaTime);
The predicted grenade may:
move
fall
collide
bounce
roll
reach its fuse time
However, its state is still predicted.
The client does not have final authority over damage or death.
________________


Phase 4: server receives request
The server receives attack request 481.
The server finds:
player
equipped weapon
weapon definition
authoritative weapon runtime
authoritative player state
The server validates:
Does the player exist?
Is the player connected?
Is the player alive?
Is grenade_launcher actually equipped?
Does the player have at least one ammo?
Has the authoritative fire cooldown completed?
Is the player not already reloading?
Is request ID 481 new?
Is the aim direction finite and normalized?
Is the muzzle position reasonably close to the real player weapon?
Is the request recent enough?
If any important check fails, the server rejects the request.
If all checks pass, the server accepts it.
________________


Phase 5: server accepts the shot
The server updates authoritative weapon runtime:
authoritative ammo: 4 → 3
next allowed fire tick: current tick + fire delay
The server creates an authoritative projectile.
Todo 7 18 2026 can we make this naming more similar? Looseobject is a projectile, like, projectile fits within looseobject 
ProjectileEntity grenade =
    projectileSystem.spawnProjectile(
        grenadeLauncherDefinition,
        authoritativeMuzzlePosition,
        validatedAimDirection,
        playerId);
The authoritative grenade stores:
server projectile ID
owner player ID
source request ID 481
weapon definition ID
spawn tick
position
velocity
remaining fuse ticks
collision shape
bounce properties
explosion configuration
The server sends AttackConfirmed to all clients.
request ID: 481
server projectile ID: 9821
server spawn tick: 1001
authoritative position
authoritative velocity
authoritative ammo: 3

________________
Phase 6: local client reconciles prediction
The firing client receives confirmation for request 481.
Remember we alreadt fired and have knockback and stuff. Now we just have small corrections that are server authoritative. 7 18 2026 maybe? 
It searches for the predicted grenade whose source request ID is 481.
It finds the predicted grenade and associates it with server projectile 9821.
predicted projectile
        becomes
confirmed network projectile
The client compares:
predicted position
authoritative position


predicted velocity
authoritative velocity
If the difference is small:
smoothly blend toward server state
If the difference is large:
correct toward server state more aggressively
The client does not spawn a second visible grenade.
The existing predicted grenade is adopted by the authoritative projectile.
________________


Phase 7: other clients receive the shot
Other clients did not predict this grenade because they did not fire it.
When they receive AttackConfirmed, they create a replicated grenade:
server projectile ID: 9821
position: authoritative spawn position
velocity: authoritative spawn velocity
They simulate it locally using the same loose-object physics system.
AGAIN JUST 1 . THERE IS JUST 1 WE USE EVERWHERE EVERUONE USES THE SAME VALUE S
Dont hardcode new values dont make another function that is different for multiplayer vs local vs single vs  etc. just 1. Its jsut 1 that server/clients call.
This reduces bandwidth and produces smooth movement.
________________


Shared projectile physics
The server and clients use the same deterministic loose-object physics logic!!!!!!!!!!!!!!!!
The simulation function does not know whether it is running on a client or server.
LooseObjectStepResult simulateLooseObject(
    LooseObjectState& state,
    const LooseObjectConfig& config,
    const PhysicsWorld& world,
    float fixedDeltaTime);
It performs:
gravity
velocity integration
continuous collision sweep
collision response
bounce
friction
rolling
penetration prevention
sleep detection
It returns collision information.
struct LooseObjectStepResult
{
    bool collided;
    EntityId collidedEntity;
    glm::vec3 collisionPoint;
    glm::vec3 collisionNormal;
    float impactSpeed;
};
It does not:
send network packets
apply weapon damage
deduct ammo
decide whether the caller is authoritative
spawn visual effects
know what a grenade launcher is

________________
Server projectile simulation
The server simulates every active authoritative grenade every server tick.
Target server simulation rate:
60 simulation ticks per second
The projectile simulation may be 60 Hz even when network snapshots are sent less frequently.
Example:
server tick 1: simulate
server tick 2: simulate
server tick 3: simulate and send snapshot
server tick 4: simulate
server tick 5: simulate
server tick 6: simulate and send snapshot
The projectile is still simulated at 60 Hz.
Only the network transmission rate is lower.
________________


Melee weapons 
Continuous Physical-Contact Weapons
Purpose
Godball and Swordsword are not temporary melee hitboxes.
They are persistent physical weapon shapes whose motion and overlap with player body parts create damage and knockback.
Both weapons use one reusable system:
PhysicalContactWeaponSystem
There must not be separate systems such as:
GodballDamageSystem
SwordTouchDamageSystem
SwordHitboxSystem
Godball and Swordsword are different configurations of the same generic physical-contact weapon system.
The existing weapon architecture still applies:
WeaponDefinition
↓
generic weapon runtime
↓
shared physical-contact simulation
↓
predicted local feedback
↓
authoritative server damage and knockback
↓
generic confirmed contact event
↓
client reconciliation
The overall client/server rule remains unchanged:
client predicts immediate feedback
server owns real damage, knockback, health, death, and score

________________
1. Physical-contact weapon family
Add a generic weapon execution family:
enum class WeaponExecutionType
{
    Hitscan,
    Projectile,
    PhysicalContact
};
PhysicalContact means:
the weapon exists continuously
its shape moves through the world
its previous and current transforms are known
the system sweeps between those transforms
the system also checks current overlap
damage may occur every simulation tick while intersecting
Physical-contact weapons are not required to create a new AttackRequest every tick.
Equipping and controlling the weapon determines its physical transform.
The contact system evaluates its interaction every fixed gameplay tick.
________________


2. Shared physical-contact definition
Conceptual configuration:
enum class PhysicalWeaponShape
{
    Sphere,
    Capsule
};


enum class PhysicalWeaponWorldResponse
{
    NonBlocking,
    BlockingSlide
};


struct PhysicalContactWeaponDefinition
{
    bool enabled = false;
    bool alwaysActive = true;


    PhysicalWeaponShape shape =
        PhysicalWeaponShape::Sphere;


    PhysicalWeaponWorldResponse worldResponse =
        PhysicalWeaponWorldResponse::NonBlocking;


    float radius = 0.5f;


    // Used by capsule weapons.
    float capsuleLength = 0.0f;


    bool sweepBetweenTicks = true;
    bool damageEveryIntersectingTick = true;


    float minimumDamagePerTick = 5.0f;
    float relativeSpeedDamageScale = 1.0f;
    float angularSpeedDamageScale = 0.0f;
    float penetrationDamageScale = 0.0f;
    float maximumDamagePerTick = 100.0f;


    float knockbackScale = 1.0f;
    float maximumKnockbackPerTick = 100.0f;


    bool canDamageOwner = false;
    bool canDamageTeammates = true;
};
The exact field names may differ.
There must still be one configuration source and one implementation.
Do not hardcode Godball or Swordsword names inside the shared contact solver.
________________


3. Fixed-tick contact evaluation
Physical-contact weapons are evaluated at the fixed gameplay rate:
60 simulation ticks per second
For each weapon, store:
previous authoritative transform
current authoritative transform
previous predicted transform
current predicted transform
Each simulation tick performs:
1. Construct the weapon shape at the previous transform.
2. Construct the weapon shape at the current transform.
3. Build a conservative swept volume between them.
4. Broad-phase query nearby living player bodies.
5. Sweep against candidate body-part shapes.
6. Test current overlap at the final transform.
7. Deduplicate results for the same target body part and tick.
8. Calculate relative contact motion.
9. Produce one contact sample for each valid touched body part.
This handles both:
fast weapon movement that crosses a player between ticks
and:
a weapon that remains inside a player over several ticks
Do not rely only on the current-frame overlap.
That would allow tunneling.
Do not rely only on a sweep-entry event.
That would fail to apply continuous damage while the weapon remains intersecting.
________________


4. Contact episode
A continuous intersection between one weapon and one target is called a:
PhysicalContactEpisode
Example:
Godball starts intersecting Player 2 on tick 13.
It remains intersecting on ticks 14, 15, 16, and 17.
It stops intersecting before or during tick 18.
Represent the episode as:
start tick: 13
end tick: 18
damaging sample ticks: 13, 14, 15, 16, 17
Use a consistent half-open tick interval:
[startTick, endTick)
Therefore:
startTick = 13
endTick = 18
sampleCount = 5
This avoids ambiguity over whether the stopping tick itself dealt damage.
Conceptual identity:
struct PhysicalContactEpisodeKey
{
    EntityId weaponEntityId;
    EntityId ownerPlayerId;
    EntityId targetPlayerId;
    uint32_t targetSpawnGeneration;
    uint16_t targetTransformEpoch;
};
A body-part identifier may be stored inside episode samples rather than in the episode key, because one weapon may intersect several body parts during one continuous episode.
Each episode also has a stable server-created ID:
contactEpisodeId
This ID allows prediction, confirmation, deduplication, and correction.
________________


5. Contact sample per tick
The simulation still calculates the physical result every tick.
Conceptual sample:
struct PhysicalContactTickSample
{
    uint64_t simulationTick;


    BodyPartId bodyPart;


    glm::vec3 contactPoint;
    glm::vec3 contactNormal;


    glm::vec3 weaponPointVelocity;
    glm::vec3 targetPointVelocity;
    glm::vec3 relativeVelocity;


    float inwardSpeed;
    float tangentialSpeed;
    float angularContribution;
    float penetrationDepth;


    float calculatedDamage;
    glm::vec3 calculatedImpulse;
};
The server does not need to transmit every raw field.
These samples describe how the authoritative total was calculated.
________________


6. Relative force and damage
Damage is based on relative physical interaction, not merely on weapon ownership.
At the contact point:
relativeVelocity =
    weaponPointVelocity
    - targetPointVelocity;
The inward component is conceptually:
inwardSpeed =
    max(
        0.0f,
        dot(relativeVelocity, -contactNormal)
    );
For rotating weapons, the velocity at the contact point must include angular motion:
weaponPointVelocity =
    weaponLinearVelocity
    + cross(
        weaponAngularVelocity,
        contactPoint - weaponCenter
    );
Conceptual per-tick damage:
damage =
    minimumDamagePerTick
    + inwardSpeed * relativeSpeedDamageScale
    + angularContribution * angularSpeedDamageScale
    + penetrationDepth * penetrationDamageScale;
Then:
damage *= bodyPartMultiplier;
damage = clamp(
    damage,
    minimumDamagePerTick,
    maximumDamagePerTick
);
Exact values remain configurable.
Godball is intentionally extremely powerful.
It may have a minimum damage of approximately:
5–10 damage per intersecting tick
before additional force scaling.
Because players normally have approximately 100 health, sustained Godball intersection should kill very quickly.
That is intended.
________________


7. Continued overlap
A weapon may already be internally overlapping a target body part.
The solver must still calculate a useful contact direction.
Preferred evidence order:
1. Current overlap penetration normal.
2. Sweep contact normal from previous to current transform.
3. Weapon-center motion toward the body-part center.
4. Relative contact-point velocity.
5. Stable fallback from the previous valid episode normal.
Do not use a random normal.
Do not allow floating-point noise to reverse knockback direction every tick.
If relative motion is almost zero, the configured minimum damage may still apply.
This preserves the intended behavior:
holding Godball inside a player remains damaging every tick
Force-scaled damage and knockback may be smaller when there is little relative motion, but minimum damage still applies.
________________


8. Godball definition
Godball uses:
shape: sphere
world response: non-blocking
player response: overlap damage and knockback
always active: yes
damage every intersecting tick: yes
continuous sweep: yes
Godball is an engine-created sphere represented by:
center point
radius
previous center
current center
It is not required to use a visible mesh for collision.
World interaction
Godball does not collide with world geometry.
It passes through:
walls
floors
ceilings
map objects
World geometry does not stop, slide, bounce, or redirect Godball.
Player interaction
Players still collide with its damaging influence.
More precisely:
Godball itself is not physically blocked by players.
Godball detects player-body intersection.
Godball applies damage and force to the player.
The contacted player may be knocked away.
Godball continues on its existing path.
Fast motion uses a swept sphere from the previous center to the current center.
Current overlap uses a sphere-versus-body-part query.
________________


9. Swordsword definition
Swordsword uses:
shape: capsule along blade
world response: blocking slide
player response: overlap damage and knockback
always active: yes
damage every intersecting tick: yes
continuous sweep: yes
The capsule is defined from the actual blade transform.
Conceptually:
capsule point A = blade base
capsule point B = blade tip
capsule radius = configured blade thickness
Do not use:
distance from sword owner
distance from player root
temporary attack box
owner-centered radius
World interaction
Swordsword is a physical world object.
When the blade collides with a wall:
the blade cannot pass through the wall
the weapon stops at contact
remaining motion slides along the surface
the collision affects the wielder’s movement
The collision response must use sweep-and-slide or a semantically equivalent continuous collision method.
The sword is approximately twice the size of the player, so collision with map geometry is a major gameplay interaction rather than only a visual adjustment.
Player interaction
Swordsword is always damaging.
Damage may result from:
swinging the sword into someone
running into someone
falling onto someone
being knocked into someone
another player falling onto the blade
holding the blade inside someone
rotating while intersecting someone
There is no required attack-active window.
Attack input may control animation or weapon movement, but it does not enable or disable damage.
________________


10. Broad-phase performance
Do not compare every physical weapon against every body part of every player every tick.
Use a broad phase:
previous weapon shape
+
current weapon shape
↓
conservative swept AABB
↓
spatial query for nearby players
↓
detailed body-part checks only for candidates
Possible spatial structures:
uniform spatial grid
BVH
dynamic AABB tree
existing world/entity broad phase
For a small match, a simple bounded spatial grid is acceptable.
The broad phase must not alter gameplay results.
It only reduces the candidate set.
Candidate filtering
Before detailed tests, reject:
dead players
wrong lifecycle
disconnected players
owner when self-damage is disabled
teammates when friendly damage is disabled
players outside swept bounds
Detailed checks then operate against:
head
torso
arms
legs
other configured body-part shapes

________________
11. Client prediction
The local client predicts physical contact every 60 Hz using the same shared contact solver.
For every predicted damaging sample, the client may immediately show:
one hitmarker
one hit sound
one damage number
one impact effect
one brief target reaction
predicted knockback presentation
Therefore, a five-tick intersection may intentionally produce:
five very fast hitmarkers
five very fast hit sounds
five predicted impact effects
This is desired.
It should feel extremely rapid and satisfying.
The presentation system may apply safety limits to avoid audio clipping, but it must preserve the perception of one rapid hit per damaging tick.
Possible presentation handling:
allow every tick event
slightly vary pitch
slightly vary volume
limit only pathological duplicate playback
Do not collapse local predicted hit feedback into one slow hitmarker.
Prediction ownership
The local client may predict:
contact detection
per-tick damage number
per-tick hitmarker
per-tick hit sound
per-tick visual reaction
temporary target knockback presentation
The local client does not own:
confirmed health
confirmed death
confirmed score
authoritative remote-player movement
final damage total
final knockback total

________________
12. Server authority
The authoritative server evaluates the same physical-contact simulation at 60 Hz.
The server owns:
whether contact existed
episode start tick
episode end tick
body parts contacted
damage for each tick
total damage
knockback for each tick
total impulse
health
death
score
The client never sends:
I intersected Player 2 for five ticks
I dealt 100 damage
I killed Player 2
The client may report or replicate weapon transforms and control intent according to the generic network architecture.
The server reconstructs and validates physical contact from authoritative weapon and player state.
________________


13. Network batching
The server must not send one reliable confirmed-damage event for every intersecting tick.
The server still simulates and applies authoritative damage every tick.
Network confirmation is aggregated by contact episode.
Example:
started intersecting: tick 13
last damaging tick: tick 17
stopped intersecting: tick 18
The server sends one episode confirmation:
contact existed during [13, 18)
five authoritative damage samples
total confirmed damage
total confirmed impulse
Conceptual packet/event:
struct PhysicalContactEpisodeConfirmed
{
    uint64_t contactEpisodeId;


    EntityId weaponEntityId;
    EntityId ownerPlayerId;
    EntityId targetPlayerId;


    uint32_t targetSpawnGeneration;
    uint16_t targetTransformEpoch;


    uint64_t startTick;
    uint64_t endTickExclusive;


    uint16_t sampleCount;


    float totalDamage;
    glm::vec3 totalImpulse;


    uint32_t bodyPartMask;


    float minimumTickDamage;
    float maximumTickDamage;


    bool targetDied;
};
Exact serialization should remain compact.
The server may include compressed per-tick information when required for accurate reconciliation, but it must not send five separate reliable events for a five-tick episode.
________________


14. Open episode updates
Waiting until contact ends could delay confirmation during a long intersection.
Therefore, the server may send bounded cumulative episode updates.
Example:
episode starts at tick 13


server sends cumulative update at tick 16:
[13, 16)
3 samples
total damage so far


contact continues


server sends final update at tick 18:
[13, 18)
5 total samples
final total damage
episode closed
Updates use the same:
contactEpisodeId
Each update contains cumulative totals or a clearly defined non-overlapping tick range.
Do not accidentally apply earlier confirmed damage twice.
Preferred semantics:
cumulative totals
+
latest confirmed endTickExclusive
The client stores the last confirmed cumulative state and applies only the difference.
Possible update frequency:
10–20 Hz
while simulation remains:
60 Hz
This limits bandwidth without delaying long contacts until they end.
________________


15. Prediction reconciliation
The local client records predicted samples by:
predicted contact episode
target
body part
simulation tick
predicted damage
predicted impulse
When confirmation arrives, match by:
owner
weapon entity
target lifecycle
contact episode
tick range
Compare:
predicted start tick
authoritative start tick


predicted end tick
authoritative end tick


predicted sample count
authoritative sample count


predicted total damage
authoritative total damage


predicted total impulse
authoritative total impulse
Exact or near match
adopt confirmation
do not replay hitmarkers
do not replay hit sounds
update confirmed totals silently
Small disagreement
preserve predicted presentation
quietly correct damage total
quietly correct knockback
Meaningful disagreement
Show the existing server-correction/disagreement effect.
Examples:
client predicted five contact ticks
server confirmed three


client predicted torso contact
server confirmed arm contact


client predicted 80 damage
server confirmed 35


client predicted contact
server confirmed none
Do not play all confirmed hit effects again after already showing predicted effects.
Completely missing local prediction
For observers or missed prediction:
play confirmed effects based on the episode sample count
Presentation may reproduce a rapid burst representing the confirmed tick count.
________________


16. Episode lifecycle
An episode starts when:
no active episode existed for this weapon/target
and
a valid damaging sweep or overlap occurs
An episode continues when:
the same weapon and target remain in valid contact
on the next simulation tick
An episode ends when:
no valid damaging contact exists for that tick
the target dies
the target respawns
the weapon is unequipped
the weapon is destroyed
the owner disconnects
the lifecycle changes
an authoritative teleport invalidates the contact
A later intersection creates a new episode ID.
Do not reuse the prior episode merely because the same weapon hits the same player again later.
________________


17. Multiple body parts
One weapon may intersect more than one body part in one tick.
The system must avoid accidental duplicate full damage.
Choose a configurable policy.
Recommended initial policy:
calculate every body-part sample
select the highest-priority or highest-damage sample for base damage
use all valid contacts to calculate stable impulse direction
record a body-part mask for presentation
Alternative weapons may explicitly allow multi-part damage.
Do not silently multiply full damage because the same sphere overlaps:
torso
arm
head
during one tick unless the weapon definition intentionally enables that behavior.
Godball and Swordsword should initially apply at most one base damage sample per target per tick.
The selected body-part multiplier should come from the strongest valid contact for that tick.
________________


18. Knockback batching
Authoritative knockback is calculated each tick.
Each tick produces:
glm::vec3 tickImpulse;
The server applies the impulse to the target’s shared movement external-impulse state at the correct simulation phase.
The network episode accumulates:
totalImpulse += tickImpulse;
The confirmation event carries the accumulated impulse for reconciliation.
Do not wait until the episode ends to apply server-authoritative knockback.
The target must move physically during the contact episode.
Only the network confirmation is batched.
Simulation is not batched.
________________


19. Universal movement reset
Every valid Godball or Swordsword physical contact is also a generic movement contact.
It must feed:
MovementContactKind::Weapon
through the shared movement-contact reset system.
Therefore, touching either weapon restores:
air jump
dash
down dash
freeze availability
future touch-reset abilities
This contact reset must be deduplicated by lifecycle and contact episode/tick identity.
It must not erase the knockback impulse produced by the weapon.
________________


20. Hitscan clarification
Revolver and Shotgun both use the exact muzzle tip as the ray origin.
origin = current weapon muzzle-tip transform
direction = normalized camera-forward direction
The direction is parallel to camera forward.
It does not aim toward a far point.
It does not begin at the camera.
If the muzzle is inside or behind blocking world geometry:
the shot is blocked immediately
Every shotgun pellet begins from the same muzzle origin.
Every pellet:
has its own deterministic spread direction
traces independently
stops at the first world or player-body hit
uses the hit body-part multiplier
contributes its own damage
contributes its own knockback
does not pierce
A shotgun trigger press still produces one generic AttackRequest.
The deterministic seed reconstructs all pellet directions on the server.
________________


21. Networking rule
Do not add weapon-specific packet types such as:
GodballContactPacket
SwordTouchPacket
SwordDamagePacket
GodballDamageTickPacket
Use generic events:
AttackRequest
PhysicalContactEpisodeConfirmed
DamageConfirmed
KnockbackConfirmed
MovementContact
or an equivalent smaller generic vocabulary.
The server reads WeaponDefinition to determine:
sphere or capsule
blocking or non-blocking
minimum damage
force scaling
knockback scaling
always active
continuous tick damage
Godball and Swordsword are configurations, not network architectures.
________________


22. Required deterministic tests
Add tests for:
Godball tunneling
previous sphere is before target
current sphere is past target
neither endpoint overlaps
sweep crosses torso
contact sample exists
Godball continuous overlap
starts tick 13
continues ticks 14–17
ends tick 18
episode is [13, 18)
sample count is 5
Godball minimum damage
very low relative speed
valid overlap
minimum damage still applies
Godball world pass-through
sphere crosses wall
transform is not blocked
player behind wall may only be contacted according to the intended player/world occlusion rule
The initial intended rule should be documented explicitly.
Swordsword world collision
capsule sweeps into wall
blade stops at contact
remaining motion slides along wall
wielder receives the intended movement response
Swordsword player contact
no attack input
player runs blade into target
damage occurs
Player falls onto sword
sword almost stationary
target has relative velocity into blade
damage and knockback occur
Rotation
blade center barely translates
blade rotates quickly through target
angular contact velocity produces damage
Body-part selection
multiple parts overlap in one tick
one configured base-damage sample applies
correct strongest body-part multiplier selected
Episode batching
five server damage ticks
one final episode confirmation
sampleCount = 5
startTick = 13
endTickExclusive = 18
Cumulative updates
update one confirms [13, 16)
final confirms [13, 18)
client applies only newly confirmed difference
Predicted feedback
five predicted contact ticks
five local hitmarker events
five local hit-sound events
one network episode confirmation
no duplicated confirmed presentation
Server disagreement
client predicts five samples
server confirms three
health uses server total
correction feedback occurs
predicted hit effects are not replayed
Lifecycle
episode begins
target dies or respawns
episode closes
old episode update cannot affect new life
Movement contact
weapon contact applies knockback
weapon contact restores touch-reset abilities
reset does not erase applied impulse

________________
23. Main rule
Godball and Swordsword use the same continuous physical-contact simulation.
Their important differences are configuration:
Godball:
sphere
non-blocking world response
always damaging
passes through geometry
high minimum tick damage


Swordsword:
capsule along blade
blocking-slide world response
always damaging
collision affects wielder movement
The physical simulation runs every 60 Hz tick.
The local client predicts one satisfying feedback event per damaging tick.
The server applies authoritative damage and knockback every tick.
The server batches confirmation by contact episode rather than sending one reliable damage event per tick.
For an episode:
started at tick 13
stopped at tick 18
the server confirms:
contact interval [13, 18)
five damaging samples
full authoritative damage
full authoritative impulse
body-part information
death result if applicable
There is one shared system, one server-authoritative result, and no weapon-specific networking path.


Grenade fuse flow
The server stores the grenade spawn tick.
Example:
spawn tick = 1001
fuse duration = 180 ticks
At server tick 1181:
current tick - spawn tick >= fuse duration
The server creates an authoritative explosion event.
Todo can we fit explosions into their own system? Or, within a bigger system?
Like explosions are just knockback + damage, knockback is just force, etc
ExplosionEvent explosion =
    explosionSystem.createExplosion(
        projectile.position,
        projectile.ownerPlayerId,
        projectile.weaponDefinition.explosion);
The server then destroys the authoritative grenade.
________________


Explosion damage flow
The explosion system receives:
position
radius
maximum damage
maximum knockback
damage falloff rules
knockback falloff rules
owner player ID
It finds entities inside the radius.
std::vector<EntityId> targets =
    physicsWorld.querySphere(explosion.position, explosion.radius);
For every possible target, the server checks:
Is it damageable?
Is it alive?
How far is it from the explosion?
Is there blocking world geometry?
What is the damage falloff?
What is the knockback falloff?
Is this the firing player?
Does self-damage use a different multiplier?
For each valid target, the server creates damage and knockback commands.
DamageCommand:
source player
target player
weapon ID
projectile ID
damage amount
damage position
damage direction


KnockbackCommand:
target player
impulse direction
impulse strength
source position

________________
Example of damaging another player
The grenade explodes at:
X: 20
Y: 4
Z: 10
Player 2 is two meters from the explosion.
The explosion radius is six meters.
The server calculates:
distance fraction = 2 / 6
damage multiplier = approximately 0.67
Suppose maximum damage is 120.
damage = 120 × 0.67
damage = approximately 80
The server checks line of sight.
If the explosion is not blocked:
Player 2 health: 100 → 20
The server also calculates knockback:
direction =
normalize(player position - explosion position)


impulse =
direction × calculated knockback force
The server applies the authoritative knockback to Player 2.
The server sends an ExplosionConfirmed event to clients.
________________


ExplosionConfirmed
Conceptual packet:
struct ExplosionConfirmed
{
    uint32_t explosionId;
    uint32_t projectileId;


    EntityId ownerPlayerId;


    glm::vec3 position;


    uint64_t serverTick;


    std::vector<DamageResult> damageResults;
    std::vector<KnockbackResult> knockbackResults;
};
Clients use this event to:
play explosion visual
play explosion sound
remove the projectile
show confirmed hitmarkers
update confirmed health
apply or reconcile knockback
show death if the server confirmed death

________________
Predicted grenade explosion
The local client may predict the grenade fuse or impact so the explosion appears immediately.
The client may predict:
explosion particle
explosion sound
camera shake
temporary self-knockback
- later we will validate on server, and smooth quiet correction.
- which should be fine BECASUE SERVER AND CLIENT BOTH USE THE EXACT SAME FUNCTIONS
temporary hitmarker
But the client must mark this explosion as predicted.
In fact we should do the server-disagreement effect in some form to show that this is unconfirmed. But still do instant reactions
It does not permanently apply:
confirmed damage
confirmed death
confirmed score
confirmed projectile destruction
confirmed remote-player knockback
When the server confirmation arrives:
matching predicted explosion exists:
    adopt it
    avoid replaying duplicate effects
    correct damage and knockback


no matching prediction exists:
    spawn the confirmed explosion normally
If the server rejects the predicted explosion:
remove or fade incorrect predicted effects
restore predicted ammo
restore predicted cooldown if needed
correct projectile state
show server disagreement feedback when useful

________________
Attack rejection flow
The server may reject request 481 because:
no ammo
cooldown active
player dead
wrong weapon equipped
        - but if ur switcing weapons, like , u fired at tick 481, and u had gernade launcher equipped, but now at tick 501 u haev shotgun equipped, it doesnt reject just bc u haev shotgun equipped, it checks what uhad at that specific tick
duplicate request
        - todo , do this client, so we dont eat ammo and client shoots 15 times on their screen but sevrver only accpets 3 times, 
invalid direction
invalid muzzle position
request too old
The server sends:
struct AttackRejected
{
    uint32_t requestId;
    AttackRejectReason reason;


    int authoritativeAmmo;
    uint64_t authoritativeNextFireTick;
};
The client then:
finds predicted attack 481
removes or fades its predicted grenade
restores ammo to authoritative value
restores cooldown to authoritative value
corrects any predicted state
The client should not repeatedly subtract and refund ammo every few ticks.
One trigger press creates one unique request.
That request is either:
pending
confirmed
rejected
It must not be sent as a new shot every simulation tick while waiting for confirmation.
________________


Projectile snapshots
The server periodically sends projectile snapshots.
struct ProjectileSnapshot
{
    uint32_t projectileId;


    uint64_t serverTick;


    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 angularVelocity;


    bool sleeping;
};
Clients compare their locally simulated projectile with the server snapshot.
For small errors:
smooth correction
For medium errors:
faster correction
For major errors:
snap or rebuild from authoritative state
Clients should not overwrite smooth projectile movement every render frame with raw network positions.
Network state is used as a correction target, not as the only source of visible movement. 7 18 2026 todo i dont understand this 
________________


Important IDs
Every attack needs a client-created request ID.
Every accepted projectile needs a server-created projectile ID.
Every explosion needs a server-created explosion ID.
Example:
attack request ID: 481
server projectile ID: 9821
server explosion ID: 15502
These IDs allow the game to match:
predicted attack
confirmed attack
predicted projectile
authoritative projectile
predicted explosion
authoritative explosion
Without IDs, the client cannot reliably determine whether a network event is:
a confirmation
a duplicate
a correction
a completely different shot

________________
System ownership
Client owns
raw attack input
camera response
instant recoil
instant muzzle flash
instant fire sound
predicted ammo display
predicted cooldown display
predicted projectile visuals
predicted local knockback
predicted explosion effects
Server owns
whether the attack was accepted
authoritative ammo
authoritative cooldown
authoritative projectile existence
authoritative projectile position
authoritative collision
authoritative fuse timing
authoritative explosion
authoritative damage
authoritative knockback
authoritative health
authoritative death
authoritative score
Both simulate
projectile movement
gravity
collision prediction
bounce
friction
fuse countdown
Both use the same simulation code.
The server result wins when they disagree.
Clientn predicts grenade is lower than the server predicts = small correction next tick to where it actually is according to server, linear smooth interpolation.


Client predicts grenade is on the other side of the map = snap correction to where it actually is according to server, linear again
________________


Main rule
A weapon definition never contains network-specific behavior such as:
grenade launcher sends packet X
rocket launcher sends packet Y
shotgun sends packet Z
All weapons send the same generic attack request.
The server reads the weapon definition and selects the correct reusable execution path.
switch (definition.fire.type)
{
case FireType::Hitscan:
    hitscanSystem.execute(...);
    break;


case FireType::Projectile:
    projectileSystem.spawn(...);
    break;


case FireType::Melee:
    meleeSystem.execute(...);
    break;
}
Optional behaviors are then applied from configuration.
if (definition.explosion.enabled)
    projectile.attachExplosion(definition.explosion);


if (definition.projectile.bounceEnabled)
    projectile.attachBounce(definition.projectile);


if (definition.knockback.enabled)
    impact.attachKnockback(definition.knockback);
There should be no grenade-launcher-specific networking path.
There should only be:
generic attack request
generic server validation
generic projectile creation
generic shared physics
generic impact event
generic explosion event
generic damage
generic knockback
generic replication
The grenade launcher is simply one configuration that activates those systems.