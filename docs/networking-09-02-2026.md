# **MiMITA full client/server networking specification**

**Date:** July 18, 2026  
**Status:** Target architecture after full client/server conversion

---

# **1\. End goal**

There is NO\!\!\!\!\!\!\!\!\!\! separate local-play gameplay architecture.

There is only client/server gameplay.

Also everyone in the entire earth can play with anyone else, at any time, because of how awesome our networking is. 

USA to korea to germany to brazil to canada to russia to japan to nigeria to istanbul to saudi arabia to laos to malaysia to USA again. All live play all the time real time nothing is missing nothing is dropped it all just works. 

Worded better:

- Players in any supported country should be able to discover,  
- connect to, and play with one another without manual network setup.  
-   
- The game must remain responsive under realistic geographic latency,  
- jitter, packet loss, reordering, NAT restrictions, and temporary  
- connection interruption.  
-   
- Local input must feel immediate through prediction.  
-   
- Remote movement must remain readable through interpolation.  
-   
- Important gameplay events must be delivered, acknowledged,  
- deduplicated, or reconstructed.  
-   
- Replaceable state may be dropped when newer state supersedes it.  
-   
- The system must select the best available direct route, relay,  
- authority location, and replication rate for the lobby.  
-   
- The game cannot remove the physical latency caused by distance,  
- but it must minimize, conceal, and fairly resolve its effects.

Every game session contains:

```
one authoritative server
one or more clients
```

Even when one person plays alone:

```
mimita.exe --server
        +
mimita.exe client
```

The server may run:

* On the same computer as the player.  
* On another player’s computer.  
* On a dedicated server.  
* Through a relay when direct networking fails.

The gameplay architecture remains the same in every case.

There MUST BE NO be separate implementations for:

```
local gameplay
online gameplay
host gameplay
remote-client gameplay
dedicated-server gameplay
```

There is one gameplay simulation. ONLY ONE

The difference is only where the authoritative server process runs.

---

# **2\. Fundamental rule**

The networking pattern is always:

```
client captures intent (i wanna shoot)
        ↓
client predicts immediate feedback (muzle flash, sound)
        ↓
client sends request or input command (hey server i want to shoot at pos X dir Z at tick Y
        ↓
server validates it (ok cool)
        ↓
server performs authoritative simulation (itll do this bc im the server and im authoritive)
        ↓
server sends confirmed state and events (heres what game looks like rigt now)
        ↓
clients reconcile their predictions (ok i was just 0.01 off thats cool)
```

The client owns what the player attempted to do.

The server owns what became real.

---

# **3\. The host is still a client**

Starting a server does not make the host player authoritative.

The host computer runs two logical processes:

```
authoritative server process
visible game client
```

The visible client sends the same requests as every other client.

For example:

```
host presses shoot
    ↓
host client predicts the shot
    ↓
host client sends AttackRequest
    ↓
local server validates it
    ↓
server confirms and replicates it
```

The host normally has extremely low latency because the server is on the same computer.

However, the host is not allowed to bypass:

* Ammo validation.  
* Cooldown validation.  
* Movement validation.  
* Damage authority.  
* Death authority.  
* Match authority.  
* Projectile authority.

This prevents the architecture from becoming:

```
host code path
remote-client code path
```

There MUST be one path. 

There can ONLY BE  1 PATH……………………. 

---

# **4\. Simulation tick rate**

## **4.01. Interpolation** 

This is bad

- No adaptive jitter buffer. High-latency connections get suboptimal feel.

We need to have adaptive everuthing, if high lag then high interp or somethign. Idk 

## **4.1 Fixed gameplay simulation**

Both the server and every client target:

```
60 simulation ticks per second
```

Therefore:

```c
constexpr uint32_t kSimulationHz = 60;
constexpr double kFixedDeltaTime = 1.0 / 60.0;
```

The server simulates authoritative reality at 60 Hz.

The client simulates:

* Local movement prediction.  
* Remote interpolation.  
* Predicted projectiles.  
* Replicated projectiles.  
* Loose-object physics.  
* Animation state.  
* Effects timing.  
* Input processing.

At the same fixed 60 Hz gameplay step.

Todo 7 18 2026 i feel like computers are gonna get good enough to handle 120hz or 240hz+ tick rate, or even serverless where its just clients 1:1 agree with each other, so eventually i want to do that, move to variable tick/serverless architecture, but for now i just want this to freakin work lois

---

## **4.2 Matching tick rate does not mean matching tick number**

A client and server both simulate at 60 Hz, but they do not necessarily execute the exact same numbered tick at the same physical instant.

Example:

```
client currently predicts tick 10,006
server currently processes tick 10,001
```

The client may be several ticks ahead because:

* The client predicts immediately.  
* Packets require time to reach the server.  
* The server processes the request after arrival.  
* Server responses require time to return.

Every packet that depends on simulation time must include a tick number.

For example:

```c
uint64_t clientSimulationTick;
uint64_t serverSimulationTick;
```

The network system maintains an estimate of:

```
client tick
server tick
estimated tick offset
round-trip time
one-way client to server latency estimate
```

The client does not constantly force its local tick to equal the latest received server tick.

Instead, it estimates which server tick corresponds to its current local simulation time.

---

## **4.3 Rendering is separate**

Rendering is not locked to 60 FPS.

A client may render at:

```
30 FPS
60 FPS
144 FPS
240 FPS
500 FPS
```

The gameplay simulation remains 60 Hz.

DONT TIE ANTTHING IMPORTANT TO FRAME RATE BC if its higher frame rate then higher simulation and thtst not good

Rendering interpolates LINEARLY AND SMOOTHLY between the previous and current simulation states.

Conceptually:

```c
while (accumulator >= fixedDeltaTime)
{
    simulateTick(fixedDeltaTime);
    accumulator -= fixedDeltaTime;
}

float renderAlpha = accumulator / fixedDeltaTime;
renderInterpolatedWorld(renderAlpha);
```

Therefore:

```
simulation rate ≠ rendering rate
```

A fast monitor produces smoother rendering without changing gameplay timing.

---

# **5\. Simulation rate versus sending rate**

Simulation and network transmission are different systems.

An entity can simulate at 60 Hz while being transmitted at 10 Hz.

Example:

```
server tick 1:
simulate grenade

server tick 2:
simulate grenade

server tick 3:
simulate grenade
send projectile snapshot

server tick 4:
simulate grenade

server tick 5:
simulate grenade

server tick 6:
simulate grenade
send projectile snapshot 
todo 7 18 2026, this means sending it out to clients? or it gets sent to the server?
```

7 18 2026 1010 explained

- means **server sends it to relevant clients**. Client-to-server input transmission is a separate rate.  
- Reliable does not mean instant. It means sent promptly, retransmitted until acknowledged, and deduplicated.  
- 

The grenade simulated six times.

Only two snapshots were transmitted.

Clients continue simulating between received snapshots. Linear interpolation between them 

Snapshots correct the client’s simulation instead of directly driving every rendered frame.

---

# **6\. Default rate model**

These values are starting targets, not permanent hardcoded rules.

```
Authoritative server simulation:     60 Hz
Client prediction simulation:        60 Hz
Loose-object simulation:             60 Hz
Projectile simulation:               60 Hz
Player input sampling:               60 Hz
Input command transmission:          up to 60 Hz, bundled
Player state snapshots:              1–60 Hz
if SUPER far = 1 hz, if super close = 60hz
Nearby important object snapshots:   15–60 Hz
Projectile correction snapshots:     10–60 Hz
Medium-distance NPC snapshots:       5–10 Hz
Far NPC snapshots:                   1–5 Hz
close npc snapshots: 30-60hz
Reliable gameplay events:            immediately
Coordinator heartbeat:               much slower, such as every few seconds, like 5, 10, 15 secs idk, heartbeat shows this server still alive keep reserving our spot for this server 
```

These rates must be configurable.

The simulation must not be rewritten when a transmission rate changes.

---

# **7\. Input commands**

Clients do not continuously send trusted positions as commands. Oh how i wish they could. Eventually i wnat to do it like that, serverless/authorityless. But thats later. Justmake ti work client/server way first.

Clients send player intent.

THIS IS PSUEDOCODE NOT EXACT 7 18 2026 PROB MAKE IT EXACT TODO

Also todo make the stuff clients send as small as possible, smallest bytes possible

This generic input command may contain:

```c
struct InputCommand
{
    uint32_t commandSequence;
    uint64_t clientSimulationTick;

    float moveX;
    float moveY;

    float lookYaw;
    float lookPitch;

    bool jumpPressed;
    bool dashPressed;
    bool downDashPressed;
    bool freezePressed;

    bool attackPressed;
    bool attackHeld;
    bool reloadPressed;

    uint8_t requestedWeaponSlot;
};
```

The command says:

```
At client tick 10,000,
I was attempting to move in this direction,
look in this direction,
and perform these actions.
```

It does not say:

```
My final position is definitely here.
My dash definitely succeeded.
I definitely hit another player.
```

---

# **8\. Input sequence numbers**

Every input command has an increasing sequence number.

Example:

```
command 5001
command 5002
command 5003
command 5004
```

Sequence numbers let the server determine:

* Whether a command is new.  
* Whether it is duplicated.  
* Whether commands were lost.  
* Which commands have already been processed.  
* Which prediction state the client may discard.

The server includes the last processed input sequence in player snapshots.

```c
uint32_t lastProcessedInputSequence;
```

When the local client receives this value, it knows:

```
The server has processed everything through command 5004.
```

The client may remove commands 5004 and earlier from its prediction history.

---

# **9\. Input packet bundling**

Inputs are generated at 60 Hz.

They may be transmitted in packets containing multiple recent commands.

Example packet:

```c
struct InputCommandPacket
{
    uint32_t packetSequence;

    InputCommand commands[3];
    uint8_t commandCount;
};
```

A packet may resend a small number of recent input commands.

Example:

```
packet A:
commands 5001, 5002, 5003

packet B:
commands 5002, 5003, 5004

packet C:
commands 5003, 5004, 5005
```

This provides lightweight redundancy.

If packet A is lost, commands 5002 and 5003 may still arrive in packet B.

The server ignores duplicate command sequences.

This is useful for time-sensitive movement commands that should not wait for reliable retransmission.

---

# **10\. Network channel types**

Not every message should use the same delivery semantics.

The transport exposes logical channels.

7 20 2026 heres this

- P0 — Build real transport channels  
-   
- IGameTransport currently only exposes:  
-   
- send()  
- poll()  
- connected()  
- close()  
-   
- That is a good start, but it does not yet express the full protocol requirements.  
-   
- Add logical channels:  
-   
- Channel	Used for  
- Unreliable sequenced	Movement and projectile snapshots  
- Reliable ordered	Join, map change, spawn, death, inventory  
- Reliable event-deduplicated	Damage, explosions, effects, projectile destruction  
- Control	Ping, ACKs, connection migration, disconnect  
-   
- Also add:  
-   
- Packet sequence  
- Latest received sequence  
- 32- or 64-bit ACK bitfield  
- Loss measurement  
- Reliable retransmission queues  
- Queue limits  
- Priority  
- Congestion response  
- Fragmentation/reassembly only where unavoidable  
- Per-channel bandwidth metrics  
-   
- You already have reliable event acknowledgements, but that mechanism needs to become a complete reusable transport facility rather than being added event by event.

## **10.1 Unreliable sequenced**

Use for rapidly replaced state:

* Player snapshots.  
* Projectile correction snapshots.  
* Look direction.  
* Velocity updates.  
* NPC transforms.  
* Ping measurements.

Properties:

```
A newer packet replaces an older packet.
Late packets may be discarded.
Occasional loss is acceptable.
```

---

## **10.2 Reliable ordered**

Use for state transitions that 	MUST ABSOLUTELY arrive in order:

* Join acceptance.  
* Spawn.  
* Death.  
* Respawn.  
  * TODO THIS ISNT INSTANT RESPAWN WE NEED TO 1000% MAKE THIS ISNTANT RESPAWN ON CLIENT/SERVER ARCHITECTURE IT DOES A FAKE RESPAWN RIGHT NOW 7 18 2026  1001  
* Inventory changes.  
* Weapon equip confirmation.  
* Map changes.  
* Match-state transitions.  
* Chat.  
* Server settings changes.

Properties:

```
Must arrive.
Must remain ordered.
```

---

## **10.3 Reliable unordered or event-deduplicated**

Use for events that must arrive but do not necessarily need global ordering:

* Confirmed explosion.  
* Confirmed damage result.  
* Projectile destruction.  
* Effect spawn with a unique ID.  
* Administrative notification.

Every event has a unique ID so duplicates can be ignored.

---

## **10.4 Deaths and respawns**

Ok so 7 18 2026 im noticing a issue that has to do with death and respawns. Go hard on this and make sure that it just loads teh same stuff everu single time when u die/respawn.

Grenade works on first life, breaks secondlife. Something works first time u join, breaks next time. Acnt have that.

---

# **11\. Generic request architecture**

Gameplay systems send generic requests.

They do not create unique networking paths for every individual weapon or ability. THERE IS JUST 1\. IDEALLY 1 REQUEST FOR EACH FUNCTION. AND MORE IDEALLY 1 REQUEST HANDLES EVERYTHING but idk how to do that 7 18 2026

Examples:

```
InputCommand
AttackRequest
ReloadRequest
EquipRequest
RespawnRequest
InteractRequest
ServerCommandRequest
```

There should not ever ever ever be:

```
GrenadeLauncherPacket
RocketLauncherPacket
ShotgunPacket
RevolverPacket
RevolverAimPacket
RevolverReloadPacketNPCReloadPacket
```

All weapons use `AttackRequest`.

All (insert function) use (function)Request.

The server reads the weapon definition and executes the correct reusable systems.

The serevr DOES NOT have different logic, neither does client. Client and server both use the same exact functions for logic and physics. The logic supersedes both client and server. The both just call the functions. 

---

# **12\. AttackRequest**

THIS IS THE 1 DEFINITION OF AttackRequest 7 18 2026

Also todo do we need pendingattackrequests? In the code it sas taht, also “mpSendAttackRequest” it says that but i thought it was just 1:1 with spec? Idk whatever   
As long as we have it somewhere 

```c
struct AttackRequest
{
    uint32_t requestId;

    EntityId claimedPlayerId;
    WeaponId weaponId;

    uint64_t clientSimulationTick;
    uint32_t inputCommandSequence;

    glm::vec3 aimOrigin;
    glm::vec3 aimDirection;
    glm::vec3 predictedMuzzlePosition;

    uint32_t deterministicSeed;
};
```

The client sends:

```
I attempted to use this weapon
at this tick
from this approximate origin
in this direction.
```

The client does not send trusted:

* Damage.  
* Target identity.  
* Death.  
* Score.  
* Authoritative ammo.  
* Explosion results.  
* Final projectile position.

The server determines those values.

7 20 2026 update

- The target should become:  
-   
- InputCommand  
- AttackRequest  
- ReloadRequest  
- EquipRequest  
- RespawnRequest  
- InteractRequest  
-   
- For weapons:  
-   
- AttackRequest  
-     ↓  
- server finds WeaponDefinition  
-     ↓  
- Hitscan | Projectile | Melee  
-     ↓  
- Damage \+ Knockback \+ Explosion  
-     ↓  
- generic confirmed events  
-   
- Delete the old packet handlers only after every weapon passes the generic path.  
-   
- This must work for:  
-   
- Revolver  
- Shotgun and AA12  
- Rocket launcher  
- Grenade launcher  
- Swordsword  
- Godball  
- NPC attacks  
- Every life after respawning, not only the first life  
-   
- Your weapon spec explicitly requires one generic request and shared reusable systems rather than weapon-specific networking.

---

# **13\. Client prediction**

The client predicts actions that need immediate feedback.

Examples:

* Movement.  
  * I move instantly on my screen, replicate to others after validated   
  * Also do that for other plaurs, because we use same functions everywhere, client and server use smae functions, it should be fine, minimal corrections needed   
* Jump.  
* Dash.  
* Down dash.  
* Freeze.  
* Weapon recoil.  
* Muzzle flash.  
* Fire sound.  
* Predicted ammo display.  
* Predicted weapon cooldown.  
* Projectile spawn.  
* Projectile movement.  
* Explosion visual.  
* Local rocket-jump impulse.  
* Local hitmarker.  
* Local collision response.

Prediction exists to make the game feel immediate.

Prediction does not make the client authoritative.

Every predicted object or action stores:

```
request ID
client tick
prediction status
associated server entity ID, once confirmed
```

Prediction states:

```c
enum class PredictionStatus
{
    Pending,
    Confirmed,
    Rejected
};
```

---

# **14\. Server validation**

The server validates requests against authoritative state.

For an attack, the server checks:

* Does the player exist?  
* Is this connection authenticated as that player?  
* Was the player alive at the relevant tick?  
* Was the weapon equipped at the relevant tick?  
* Was sufficient ammo available?  
* Was the cooldown complete?  
* Was the player reloading?  
* Is this request ID new?  
* Is the requested tick plausible?  
* Is the direction finite and valid?  
* Is the muzzle position plausible?  
* Is the requested action physically possible?  
* Is the packet too old to evaluate safely?

The server should not reject reasonable actions because of tiny harmless differences.

The server should reject:

* Impossible firing speed.  
* Impossible velocity.  
* Impossible range.  
* Thick-wall shots unsupported by rewind state.  
* Duplicate requests.  
* Extremely stale requests.  
* Invalid numbers such as NaN or infinity.  
* Requests from dead or disconnected players.  
* Requests for unavailable weapons.

---

# **15\. Historical state and tick-specific validation**

The server must not validate every delayed request only against the server’s current state.

Example:

```
client tick 481:
grenade launcher equipped
client fires

client tick 490:
client switches to shotgun

server receives AttackRequest for tick 481
while the current weapon is now shotgun
```

The server should determine which weapon was equipped at tick 481\.

It should not reject the attack merely because the player switched weapons afterward.

The server maintains a short history buffer for relevant authoritative state:

```c
struct PlayerHistoryFrame
{
    uint64_t serverTick;

    glm::vec3 position;
    glm::vec3 velocity;

    float lookYaw;
    float lookPitch;

    WeaponId equippedWeapon;

    bool alive;
    bool dashing;
    bool frozen;

    CollisionPose collisionPose;
};
```

The history window only needs to cover a limited recent period.

For example:

```
250–500 milliseconds
```

The exact value remains configurable. Also prob should work based on ticks

---

# **16\. Server-side rewind for hits**

For hitscan and melee validation, the server may reconstruct where players were when the attacker fired.

Flow:

```
1. Client sends attack tick.
2. Server maps client tick to estimated server tick.
3. Server loads historical target transforms for that tick.
4. Server performs authoritative hit test.
5. Server restores current simulation state.
6. Server applies confirmed damage.
```

This means the shooter is evaluated against what was reasonably visible when they fired, rather than only where the target is when the packet arrives.

The server should bias toward accepting plausible hits when:

* The target was visibly hittable on the shooter’s predicted screen.  
* The request arrived within the allowed rewind window.  
* The shot does not pass through blocking world geometry.  
* The weapon timing and range are valid.

---

# **17\. Server simulation order**

Every authoritative server tick uses a defined order.

Example:

```
1. Receive and decode incoming packets.
2. Authenticate packet ownership.
3. Add input commands and requests to queues.
4. Process connection and join state.
5. Process player input commands.
6. Validate requested gameplay actions.
7. Simulate player movement.
8. Simulate loose objects and projectiles.
9. Generate collision and impact events.
10. Resolve explosions.
11. Resolve damage and knockback.
12. Resolve health, death, and respawn.
13. Update gamemode and score.
14. Build snapshots and reliable events.
15. Send outgoing network data.
16. Record history required for rewind and debugging.
```

Every system should know which phase it belongs to.

This prevents results from depending on accidental function-call order.

---

# **18\. Client simulation order**

Every client simulation tick uses a matching predictable order.

Example:

```
1. Poll input.
2. Create InputCommand.
3. Create gameplay requests from edge-triggered input.
4. Apply local movement prediction.
5. Apply local weapon prediction.
6. Simulate predicted and replicated loose objects.
7. Consume incoming server events.
8. Reconcile local predicted player state.
9. Reconcile predicted projectiles.
10. Update remote interpolation targets.
11. Update gameplay animation state.
12. Record prediction history.
13. Queue outgoing input and requests.
```

Rendering happens separately after simulation.

---

# **19\. Shared gameplay code**

Client and server call the same reusable simulation functions EVERY SINGLE TIME WITHOUT FAIL NO EXCEPTIONS THERE ARE NO DEUPLICATES THERE ARE NO  DOUBLE WRITE THERE IS NO RE DEFINING FUNCTIONS THAT ALREADT EXIST

Examples:

```c
simulateCharacterMovement(...)
simulateLooseObject(...)
generateSpreadDirections(...)
calculateExplosionFalloff(...)
calculateKnockbackImpulse(...)
performHitscanTrace(...)
performMeleeQuery(...)
```

These functions receive explicit state and configuration.

They do not know whether they are running on:

* Client.  
* Listen host.  
* Dedicated server.  
* Replay.  
* Testing environment.

For example:

```c
LooseObjectStepResult simulateLooseObject(
    LooseObjectState& state,
    const LooseObjectConfig& config,
    const PhysicsWorld& world,
    float fixedDeltaTime);
```

This function does not:

* Send packets.  
* Deduct ammo.  
* Apply final damage.  
* Spawn graphical effects.  
* Decide authority.  
* Contain grenade-launcher-specific logic.

Authority is handled by the caller.

---

# **20\. Player movement prediction**

When the local player presses movement input:

```
client captures input
client immediately simulates movement
client stores command in prediction history
client sends command to server
server simulates same command
server sends authoritative player snapshot
client reconciles
```

The local player should not wait for a server round trip before moving.

Also dont make client  wait for server to confirm that other plaures were hit by a rocket. Just instant predict it and correct quietly after, like the server tick after 

Also hi 7 20 2026 heres this 

- P0 — Complete movement prediction and reconciliation  
-   
- The final movement path must be:  
-   
- client sends input sequence  
- client predicts movement  
- server runs same movement function  
- server acknowledges last processed sequence  
- client rewinds to server state  
- client replays unacknowledged inputs  
- visual model smooths correction  
-   
- The current packet still carries client position and velocity values. That can be used as diagnostic/prediction information, but authoritative movement must eventually be derived from input commands, not accepted transforms.  
-   
- Required additions:  
-   
- Increasing input-command sequence  
- Bundles containing recent redundant inputs  
- lastProcessedInputSequence in snapshots  
- Prediction-state history  
- Replay of unacknowledged commands  
- RTT-aware validation windows  
- Proper dash, freeze, down-dash and knockback replay  
- Packet loss and reordering handling  
-   
- Until this works, international latency will create more movement disagreements than necessary.

---

# **21\. PlayerSnapshot**

This is psuedocode not final todo 7 18 2026 

```c
struct PlayerSnapshot
{
    EntityId playerId;

    uint64_t serverTick;
    uint32_t lastProcessedInputSequence;

    glm::vec3 position;
    glm::vec3 velocity;

    float lookYaw;
    float lookPitch;

    PlayerMovementState movementState;

    float health;
    bool alive;

    WeaponId equippedWeapon;
};
```

The local player uses this for reconciliation.

Remote players use it as an interpolation target.

---

# **22\. Local-player reconciliation**

The local client keeps a history of:

```
input command
predicted state before command
predicted state after command
```

When an authoritative snapshot arrives:

```
1. Find the state associated with lastProcessedInputSequence.
2. Compare predicted state with server state.
3. Replace historical base state with authoritative state.
4. Replay all unacknowledged inputs after that sequence.
5. Produce corrected current predicted state.
6. Smooth the visible transform when appropriate.
```

Conceptually:

```c
state = authoritativeSnapshot.state;

for (const InputCommand& command : unacknowledgedCommands)
{
    simulateCharacterMovement(state, command, fixedDeltaTime);
}
```

This allows the local player to remain responsive while respecting the server.

---

# **23\. Movement correction levels**

Corrections use error thresholds.

## **Small error**

Example:

```
predicted position differs by 1 meter
```

Action:

```
smoothly blend the visible model toward corrected state
```

Do not visibly snap.

Possible: todo maybe show server disagree effect? Maybe not? This too small 

---

## **Medium error**

Example:

```
predicted position differs by 30 meters
```

Action:

```
perform a faster correction
replay remaining input
show optional disagreement diagnostics
```

Maybe show server disagree effect idk  7 18 2026   
A bigger one?

---

## **Major error**

Example:

```
client predicts being on the opposite side of a wall
client predicts impossible velocity
client fell far outside the valid map
```

Action:

```
replace state with authoritative state
clear invalid prediction
snap or very rapidly correct
```

The gameplay collision state must be corrected immediately even when the visible model is smoothed.

Absoltuelt show  server disagree effect

This is for like, deaths taht didnt happen

Shots taht didnt actuallt damage u, 

Positions that are greater than liek 100m in correction required 

With big sound

Bigger correction needed \= bigger effect \+ sound 

---

# **24\. Remote-player interpolation**

Remote players are not normally predicted from their private input.

Their snapshots arrive periodically.

The client keeps a short interpolation buffer.

Example:

```
snapshot server tick 100
snapshot server tick 102
snapshot server tick 104
```

The renderer displays a slightly delayed point between known snapshots.

```
render time = estimated server time - interpolation delay
```

This produces smooth movement despite packet spacing and jitter.

Use linear interpolation as the initial standard:

```c
renderPosition = lerp(
    olderSnapshot.position,
    newerSnapshot.position,
    interpolationAlpha);
```

Rotation should use appropriate angular interpolation.

---

# **25\. Remote extrapolation**

If a new snapshot is slightly late, the client may extrapolate briefly using known velocity.

Extrapolation must be limited.

Example:

```
maximum extrapolation: 100 milliseconds
```

After the limit:

* Stop extending the motion.  
* Hold or gradually slow.  
* Wait for authoritative state.  
* Do not let the player travel indefinitely based on old information.  
* Todo: put like server disagree effect where its like a sphere that surrounds them? Like shows that we dont know where theure about to go? idk

---

# **26\. Projectile replication**

Authoritative projectiles are simulated by the server at 60 Hz.

ENSURE ITS 60HZ 

DEBUG LOG SPEC TODO NEED TO MAKE BETTER 7 18 2026 BUT NEED TO LOG EXACT WHY ITS NOT AT 60HZ RIGHT NOW AND THEN FIX 

Clients also simulate them at 60 Hz using the same projectile and loose-object physics.

The server sends:

* Projectile spawn event.  
* Periodic projectile correction snapshots.  
* Projectile impact event.  
* Projectile explosion event.  
* Projectile destruction event.

It does not need to send the complete projectile transform every server tick.

---

# **27\. ProjectileSpawned**

Todo maybe move to looseobject? Because a projectile is just looseobject with like different values, like can explode, do damage, do knockback, has velocity has angular rotation etc

```c
struct ProjectileSpawned
{
    EntityId projectileId;

    EntityId ownerPlayerId;
    WeaponId sourceWeaponId;

    uint32_t sourceAttackRequestId;

    uint64_t serverSpawnTick;

    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 angularVelocity;

    ProjectileConfigId configId;
    uint32_t deterministicSeed;
};
```

The firing client matches `sourceAttackRequestId` to its predicted projectile.

Other clients create a new replicated projectile.

---

# **28\. Projectile correction snapshots**

```c
struct ProjectileSnapshot
{
    EntityId projectileId;
    uint64_t serverTick;

    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 angularVelocity;

    bool sleeping;
};
```

The network snapshot is a correction target.

It is not directly assigned to the visual object every render frame.

Wrong approach:

```c
projectile.position = newestNetworkPosition;
```

This causes visible jumping every time a packet arrives.

Correct approach:

```
simulate projectile locally every tick
compare with authoritative snapshot
correct error smoothly linearly when small
snap only when severely wrong
```

---

# **29\. Projectile reconciliation**

## **Small error**

```
Client grenade is slightly lower than server grenade.
```

Action:

```
Add a small correction velocity or position offset over several ticks.
```

---

## **Medium error**

```
Client grenade bounced differently and is one meter away.
```

Action:

```
Correct more strongly over a short interval.
Update velocity toward the authoritative velocity.
```

---

## **Major error**

```
Client grenade is across the map.
Server grenade is still near the shooter.
```

Action:

```
Replace simulation state with server state.
Clear invalid contact history.
Resume simulation from the authoritative state.
```

Correction should be linear and predictable before adding more complex smoothing.

---

# **30\. Deterministic inputs**

EVERY SINGLE TIME the server and client should use the same:

* Weapon definition.  
* Projectile definition.  
* Collision shape.  
* Gravity value.  
* Friction.  
* Restitution.  
* Fuse duration.  
* Spread seed.  
* Fixed delta time.  
* Collision rules.

THERE IS 0 EXCEPTIONS 

CLIENT \= SERVER \= CLIENT \= SERVER \= CLIENT \= SERVER

No different functions

No different definitions no diffrent hardcoded values its just the config values thats it thats the 1 thign 

Random behavior must use explicit seeds.

The client must not generate an unrelated random spread pattern from the server.

Example:

```c
uint32_t deterministicSeed;
```

The seed produces the same shotgun pattern or projectile spin on both sides.

Perfect bit-for-bit physics determinism is not required initially.

Periodic authoritative correction remains necessary.

---

# **31\. Collision authority**

Clients may predict collisions for immediate feedback.

The server confirms final collision results.

Smooth linear  corrections if needed at all. 

For a projectile:

```
client predicts:
grenade touched wall and bounced

server determines:
authoritative contact point
authoritative bounce velocity
authoritative explosion or continuation
```

For local player movement:

```
client predicts:
weapon or body contacted wall
movement was blocked

server determines:
final legal transform
final velocity
```

Clients must not send:

```
I collided, therefore this result is final.
```

They send intent and simulate prediction.

---

# **32\. Damage authority**

Only the server changes authoritative health. ONLY the seerver.

- 7 20 2026 i doooo want to, in the future, have a more client trusted architecture  
- But for now , just do it in a way that works  
- Make ti work first THEN break it

The client may predict:

* Hitmarker.  
* Damage number presentation.  
* Screen effect.  
* Temporary reaction.  
* Predicted death presentation, when desired.

But the server owns:

* Final damage amount.  
* Final target.  
* Health.  
* Death.  
* Kill credit.  
* Score.  
* Respawn eligibility.

If client and server disagree, use effectpart function to do a server disagreement effect, bigger effect for bigger disagreement 

A client never sends:

```
DamagePlayer(target, 500)
```

Instead, the server derives damage from:

```
validated attack
validated collision or hit
weapon definition
target body part
distance/falloff
authoritative game rules
```

---

# **33\. Knockback authority**

Local knockback  MUST be predicted immediately.

Recorrect it later  IF NEEDED, BUT DO IT IMMEDIATELY

Ideally no recorrection is neede because AGAIN BOTH  CLIENT \= SERVER \= CLIENT \= SERVE

Both use the same functions

Both dont even have functions 

Both literally just read the same functions from the same files

The functions for knockback, damage, movement, etc, tehse supersede client and server

Client and server are dumb in this way, in the way that the both just ask those files for thier functions and run them, and dont do anything else.

Examples:

* Rocket jumping.  
* Grenade jumping.  
* Recoil impulse.  
* Being hit by a nearby explosion.

The client predicts the impulse so movement feels immediate.

The server calculates and applies the authoritative impulse.

When server state arrives:

```
small difference:
blend and continue

large difference:
replay movement from authoritative impulse

invalid prediction:
correct to server state
```

Remote-player knockback is PREDICTED, AND TREATED THE SAME, displayed from authoritative replicated state.

So small correction \= small change

Medium \= medium

Big \= big

But still, its INSTANT 

---

# **34\. Health and death flow**

Example:

```
1. Server confirms explosion.
2. Server calculates 80 damage to Player 2.
3. Server changes health from 100 to 20.
4. Server emits DamageConfirmed.
5. all other Clients update Player 2’s health presentation.
```

If health reaches zero:

```
0. predict death happens before sevrer confirms it. rollback if it doenst, WITH A HUGE SERVEDISAGREEMENT effect. becuase that should be part of the game its super cool and intersting 7 18 2026. 
1. Server marks player dead.
2. Server assigns kill credit to killer/aggressor/attacker
3. Server updates score/kills/deaths for plauers involved.
4. Server emits PlayerDied.
5. Clients play death effects.
6. Server determines respawn eligibility.
```

Clients do not independently finalize death.

---

# **35\. RespawnRequest**

```c
struct RespawnRequest
{
    uint32_t requestId;
    uint64_t clientSimulationTick;
};
```

Pressing Space may send a respawn request. 

PREDICT THAT THEYRE ABLE TO RESPAWN IMMEDIATELY, MAKE THIS SUPER CLIENT AUTHROITIVE

IF UR ALREADY DEAD, ITS CONFIMRED U DIED, WHY WAIT.

Its veru frustrating for me when i  press space to respawn instantlt but i cant instant respawn, i want instant respawn, no waiting, just trust client unless obviously wrong

The server validates:

* Player is dead.  
* Respawn is permitted by the gamemode.  
* Respawn delay is complete, when applicable.  
  * There is 0 delay by default  
  * I mean  
  * 3 sec automatic respawn  
  * But press space \= instant respawn   
* Spawn location is valid.  
  * Its just “spawnlocation” in the map  
  * Pick one of them

The server sends:

```c
struct PlayerRespawned
{
    EntityId playerId;
    uint64_t serverTick;

    glm::vec3 spawnPosition;
    glm::vec3 spawnVelocity;

    float health;

    InventorySnapshot inventory;
};
```

The respawn event resets authoritative and predicted transient state:

* Death state.  
* Movement state.  
* Animation state.  
* Weapon pose state.  
* Cooldown state when required.  
* Effects attached to the old body.  
* Prediction history associated with the old life.

This raw link is the link to the death/respawn spec, i think we might not need it? Just need to continue working on making this verison in this networking doc how it should be, 

It should be instant press space \= i am spawneed, like as close to 0ms in real life as possible

This

- [https://docs.google.com/document/d/1uTxOkciwKHtSzBm4rKxxMqMshNGttK9TOk8ju3uBu-E/edit?tab=t.luksvv2s1r88](https://docs.google.com/document/d/1uTxOkciwKHtSzBm4rKxxMqMshNGttK9TOk8ju3uBu-E/edit?tab=t.luksvv2s1r88) 

---

# **36\. Snapshot model**

A snapshot represents an authoritative state at a specific server tick.

A world snapshot may contain:

```c
struct WorldSnapshot
{
    uint32_t snapshotSequence;
    uint64_t serverTick;

    uint32_t acknowledgedClientPacketSequence;

    std::vector<PlayerSnapshot> players;
    std::vector<ProjectileSnapshot> projectiles;
    std::vector<LooseObjectSnapshot> looseObjects;
    std::vector<NpcSnapshot> npcs;
};
```

Large snapshots may later use:

* Delta compression.  
* Entity interest management.  
* Quantized coordinates.  
* Snapshot baselines.  
* Separate frequency groups.

Correctness comes before compression. Correct and working but inefficient beats compressed and broken.

---

# **37\. Snapshot IDs and ordering**

Every snapshot has an increasing sequence number.

Clients discard snapshots older than the newest already processed snapshot.

Example:

```
received snapshot 201
received snapshot 203
received snapshot 202
```

Snapshot 202 is stale and may be discarded.

Reliable events remain separately deduplicated by event ID.

---

# **38\. Entity IDs**

Every networked entity has a server-assigned ID.

Examples:

```
player ID
NPC ID
projectile ID
loose-object ID
effect event ID
explosion ID
damage event ID
```

Entity IDs are not raw memory pointers.

They must remain valid across packet transmission.

Conceptually:

```c
using EntityId = uint32_t;
```

A generation value may later prevent reuse bugs:

```c
struct NetworkEntityId
{
    uint32_t index;
    uint32_t generation;
};
```

---

# **39\. Request and event lifecycle**

Every client request has a unique request ID.

Possible states:

```
pending
confirmed
rejected
timed out
```

Every server event has a unique event ID.

Clients store recently processed event IDs to reject duplicates.

Example chain:

```
AttackRequest ID 481
Projectile ID 9821
Explosion ID 15502
DamageEvent ID 19003
DeathEvent ID 19400
```

These IDs connect prediction to authoritative outcomes.

---

# **40\. Request rejection**

The server returns an explicit rejection message.

```c
struct RequestRejected
{
    uint32_t requestId;
    RequestType requestType;
    RejectReason reason;

    uint64_t serverTick;
};
```

Weapon-specific correction data may be included:

```c
struct AttackRejected
{
    uint32_t requestId;
    AttackRejectReason reason;

    int authoritativeMagazineAmmo;
    int authoritativeReserveAmmo;
    uint64_t authoritativeNextFireTick;
};
```

The client then:

* Finds the prediction.  
* Marks it rejected.  
* Restores predicted ammo.  
* Restores predicted cooldown.  
* Removes or fades predicted projectile/effect.  
* Corrects movement impulses caused by the rejected action.  
* Records a disagreement diagnostic.

One trigger press must create one request.

The client must not repeatedly create new attack requests every tick while waiting.

---

# **41\. Edge-triggered versus held actions**

Actions are classified.

## **Edge-triggered**

Generated once when the button changes from up to down:

* Semi-automatic attack.  
* Dash.  
* Down dash.  
* Freeze.  
* Reload.  
* Equip slot.  
* Respawn.

## **Held**

Represented while the button remains down:

* Automatic weapon fire.  
* Charge attack.  
* Jump.  
  * Hold space down \= attempt jump as much as u can  
  * Client jumps everu frame they can/every tick? Idk how it works but it spams jump effect and its cool  
  * But, dont send that spam to server, just send jumping \= true   
* Continuous beam.  
* Movement direction.  
* Aim state.

An automatic weapon may generate attacks according to its local predicted cooldown while held.

Each generated shot still receives a unique request ID.

---

# **42\. Reliable event examples**

The server sends discrete events for things that occurred once.

7 18 2026 i dont understand, sends to all clients? Idk 

Examples:

```
AttackConfirmed
AttackRejected
ProjectileSpawned
ProjectileImpacted
ExplosionConfirmed
DamageConfirmed
PlayerDied
PlayerRespawned
WeaponEquipped
MapChanged
MatchStarted
MatchEnded
PlayerJoined
PlayerDisconnected
```

A snapshot answers:

```
What is the current state?
```

An event answers:

```
What happened?
```

Both are useful.

---

# **43\. Interest management**

Not every client needs every entity at the same frequency.

The server determines relevance.

Initial example:

```
local player:
authoritative correction snapshots at high priority

players within 50 meters:
20–30 Hz

important projectiles nearby:
15–30 Hz

medium-distance players and NPCs:
5–15 Hz

far NPCs:
1–5 Hz

dormant objects:
on-change only
```

Simulation remains 60 Hz on the server.

Only replication frequency changes.

Critical events are not delayed merely because an entity is far away.

Example:

```
A distant player dies.
PlayerDied still arrives reliably.
```

---

# **44\. Priority model**

Each replicating entity may receive a priority score.

Possible inputs:

```
distance from client
visibility
whether entity is attacking client
whether entity is a dangerous projectile
whether state changed recently
time since last update
gamemode importance
```

High-priority entities receive updates first when bandwidth is limited.

The system should avoid starvation by increasing priority as time since last update grows.

---

# **45\. Bandwidth degradation**

When bandwidth or server load is constrained, reduce transmission before reducing authoritative simulation.

Preferred degradation order:

```
1. Reduce far-object snapshot frequency.
2. Reduce cosmetic replication.
3. Increase quantization or compression.
4. Reduce medium-priority snapshot frequency.
5. Preserve local player, nearby enemies, attacks, damage, and deaths.
```

The target architecture keeps server gameplay simulation at 60 Hz.

If the server physically cannot maintain 60 Hz, it must report the problem clearly rather than silently changing gameplay speed.

## **45.1 Lag-compensation goal**

Lag compensation exists so players can interact fairly despite:

```
geographic distance
packet travel time
jitter
packet loss
different client and server tick positions
```

Lag compensation does not remove latency.

It determines which historical or current world state the server should use when validating an action.

The server remains authoritative.

The client sends:

```
what action was attempted
the client simulation tick
the input command sequence
the approximate origin
the direction
the relevant request ID
```

The server maps the client tick to an estimated server tick and selects the correct validation method for the action type.

There must not be one universal rewind rule for every interaction.

Different actions require different treatment.

---

## **45.2 Time mapping**

Every latency-sensitive request includes:

```c
uint64_t clientSimulationTick;
uint32_t inputCommandSequence;
```

The server maintains an estimate of:

```
client tick offset
round-trip time
client-to-server travel time
jitter
clock drift
```

Conceptually:

```c
estimatedServerActionTick =
    mapClientTickToServerTick(
        clientSimulationTick,
        connectionTickOffset);
```

The estimated tick must be clamped to the server’s available history window.

Do not allow a client to request arbitrary ancient world states.

The server logs:

```
requested client tick
estimated server tick
current server tick
rewind amount in ticks
rewind amount in milliseconds
whether the value was clamped
```

---

## **45.3 Historical state buffer**

The server maintains a bounded history of state required for validation.

History may include:

```
player root position
player velocity
body-part collision poses
look direction
equipped weapon
alive/dead state
movement state
dash state
freeze state
spawn generation
moving-platform transforms
important world-door or obstacle states
```

Conceptual frame:

```c
struct PlayerHistoryFrame
{
    uint64_t serverTick;

    glm::vec3 position;
    glm::vec3 velocity;

    float lookYaw;
    float lookPitch;

    CollisionPose collisionPose;

    WeaponId equippedWeapon;

    bool alive;
    bool dashing;
    bool frozen;

    uint32_t spawnGeneration;
};
```

History is stored once per authoritative server tick.

History retention must be configurable.

Initial target:

```
casual maximum history:
250 milliseconds

competitive maximum history:
150–200 milliseconds

development maximum:
configurable for testing
```

The existence of longer history does not mean every action is permitted to use all of it.

Each action category has its own maximum rewind.

---

## **45.4 Hitscan weapons**

Hitscan weapons use server-side rewind.

Examples:

```
revolver
shotgun pellets
instant beam weapons
future rifles
```

Flow:

```
1. Client predicts the shot immediately.
2. Client sends AttackRequest with fire tick.
3. Server validates ammo, cooldown, weapon, and origin.
4. Server estimates the authoritative fire tick.
5. Server loads historical target collision poses.
6. Server performs the hitscan trace against those poses.
7. Server verifies blocking world geometry.
8. Server restores current state.
9. Server applies confirmed damage and knockback.
```

The server rewinds:

```
target collision poses
relevant moving blockers when history exists
attacker muzzle pose when required
```

The server does not rewind:

```
unrelated effects
visual particles
the entire current simulation
every loose object in the map
```

Each shotgun pellet uses the same rewind tick and deterministic spread seed.

The server must avoid counting the same pellet twice.

---

## **45.5 Melee attacks**

Melee uses a shorter and stricter rewind window than hitscan.

Melee attacks have:

```
windup
active window
recovery
moving collision volume
attacker movement
target movement
```

Flow:

```
1. Client sends melee request and attack-start tick.
2. Server validates the attack.
3. Server reconstructs attacker and target poses over the active window.
4. Server evaluates the melee volume at relevant ticks.
5. Server confirms the first valid or configured repeated contacts.
6. Server applies authoritative damage and knockback.
```

The server may evaluate several historical ticks:

```
active tick 1
active tick 2
active tick 3
...
```

Do not test only one static pose when the melee weapon moved through an arc.

Melee maximum rewind should normally be lower than hitscan because deeply rewinding close-range physical contact feels unfair to the defender.

Suggested initial maximum:

```
100–150 milliseconds
```

---

## **45.6 Fast projectiles**

Fast projectiles include projectiles that travel far during one network round trip.

Examples:

```
rockets
very fast energy projectiles
high-speed launched objects
```

The server rewinds only the launch conditions.

Flow:

```
1. Client predicts projectile immediately.
2. Client sends attack tick, origin, and direction.
3. Server reconstructs the attacker muzzle at the estimated fire tick.
4. Server validates that the launch origin and direction were possible.
5. Server spawns the authoritative projectile.
6. Server simulates it forward from the accepted launch state.
```

The server may fast-forward the new projectile from the historical launch tick toward the current server tick.

Example:

```
estimated launch tick = 1000
current server tick = 1004

server spawns at tick 1000 state
server simulates projectile through ticks 1001–1004
projectile enters current world at its resulting state
```

Fast-forward work must be bounded.

Do not rewind the entire live world for every projectile simulation tick.

If historical collision geometry is required during fast-forward, use a limited historical query only for relevant dynamic entities.

Static world geometry does not need rewinding.

---

## **45.7 Slow projectiles**

Slow projectiles include:

```
grenades
slow energy balls
thrown objects
rolling explosives
```

The server validates the historical launch origin, then simulates the projectile forward authoritatively.

After launch, the projectile interacts with the current progressing authoritative world.

The server does not repeatedly rewind targets to the shooter’s original fire time whenever the projectile reaches them.

A slow projectile hitting a player one second later is evaluated at the authoritative impact time.

Flow:

```
historical launch validation
        ↓
authoritative forward simulation
        ↓
authoritative collision at impact tick
        ↓
damage/explosion at impact tick
```

This keeps projectiles understandable and prevents them from hitting where a target was long before impact.

---

## **45.8 Projectile fast-forward limits**

Projectile catch-up must be bounded to prevent one delayed packet from causing unlimited server work.

Configurable limits include:

```
maximum launch rewind ticks
maximum projectile catch-up ticks
maximum catch-up collision queries
maximum catch-up CPU time
```

If a request exceeds the supported limit:

```
clamp to oldest supported tick
or
reject with request_too_old
```

Do not silently spawn the projectile from an impossible position.

Log:

```
requested launch tick
accepted launch tick
catch-up ticks
catch-up duration
collision queries
final catch-up position
whether catch-up was clamped
```

---

## **45.9 Explosions**

Explosions use the authoritative projectile position and authoritative explosion tick.

Examples:

```
rocket impact
grenade fuse
grenade direct hit
explosive loose object
```

The server determines:

```
explosion position
explosion tick
targets inside radius
line of sight
damage falloff
knockback falloff
self-damage
death
```

The firing client may predict:

```
explosion effect
explosion sound
camera shake
self-knockback
temporary hitmarker
temporary remote reaction
```

Confirmed remote damage uses authoritative server evaluation.

For projectile explosions, do not rewind targets all the way back to the original fire tick.

Normally evaluate targets at:

```
authoritative explosion tick
```

A very small historical correction may be allowed to account for server processing and mapped tick error, but it must be bounded separately from hitscan rewind.

---

## **45.10 Rocket jumping**

Rocket jumping must feel immediate for the firing player.

Flow:

```
1. Client predicts rocket spawn.
2. Client predicts impact or explosion.
3. Client predicts local self-knockback immediately.
4. Server validates launch and explosion.
5. Server calculates authoritative self-knockback.
6. Client reconciles movement from authoritative state.
```

The client may predict:

```
horizontal impulse
vertical impulse
velocity change
movement after the impulse
```

The server owns:

```
whether the rocket existed
where it exploded
whether the player was in range
final impulse
final movement state
```

Small impulse differences should be corrected through input replay and smooth visual correction.

Large invalid rocket jumps must restore authoritative movement state.

Do not delay local rocket-jump movement until the explosion confirmation returns.

---

## **45.11 Grenade jumping**

Grenade jumping follows the same authority pattern as rocket jumping, but the grenade may:

```
bounce
roll
settle
move because of collision
explode from fuse
explode from direct impact
```

Both client and server simulate the grenade using the same loose-object physics.

The local client predicts:

```
grenade movement
bounce
fuse
explosion
self-knockback
```

The server confirms:

```
authoritative grenade path
authoritative explosion tick
authoritative explosion position
authoritative self-knockback
```

The server does not rewind the grenade explosion back to when the grenade was fired.

It uses the authoritative explosion state.

If client and server grenade paths differ:

```
small difference:
smooth projectile correction

medium difference:
stronger correction

major difference:
replace projectile state
```

Movement reconciliation then uses the confirmed explosion impulse.

---

## **45.12 Player-to-player collisions**

Player-to-player physical collision uses current authoritative simulation.

Do not deeply rewind players and physically resolve an old collision inside the current world.

Examples:

```
players body-blocking
players touching in a doorway
soft pushing
solid-player collision
player landing on another player
```

The local client may predict nearby physical interaction for responsiveness.

The server resolves the current collision using:

```
current authoritative player states
current velocity
current collision shapes
stable entity ordering
bounded depenetration
bounded impulse
```

The client then reconciles.

Historical player positions may be consulted for diagnostics or plausibility, but they should not normally be inserted into the current physical simulation.

This avoids:

```
players being pushed by old positions
retroactive body blocking
historical collision launching current players
large disagreement chains
```

---

## **45.13 Damage, body parts, and historical poses**

When rewind is used, damage must use the body part that existed in the accepted historical pose.

Examples:

```
head
torso
arm
leg
hand
foot
```

The server records:

```
historical target pose
body part hit
hit position
hit normal
distance
damage multiplier
final damage
```

Do not:

```
rewind the root position
but use current body-part transforms
```

That would create mismatched hitboxes.

Spawn generation must also match.

A request targeting spawn generation 4 must not damage generation 5 after the player has respawned.

---

## **45.14 Fairness limits and failure behavior**

Lag compensation must balance the shooter and defender.

It must not always accept whatever the shooter saw.

The server rejects or clamps actions when:

```
request is older than permitted rewind
client tick mapping is implausible
origin is outside historical muzzle tolerance
direction is invalid
weapon was not equipped
player was dead
ammo or cooldown was invalid
shot passes through blocking world geometry
target belongs to another spawn generation
required history is unavailable
```

Suggested behavior:

```
within rewind limit:
evaluate historical state

slightly beyond limit:
clamp to oldest permitted state when configured

far beyond limit:
reject as request_too_old
```

Every accepted or rejected compensated action logs:

```
action category
requested tick
accepted tick
rewind milliseconds
history source
target pose
result
clamp reason
reject reason
prediction difference
```

The permanent rules are:

```
Hitscan rewinds target poses.

Melee rewinds a short active window.

Projectiles rewind their launch, not their whole lifetime.

Fast projectiles may be boundedly fast-forwarded.

Slow projectiles collide at authoritative impact time.

Explosions use authoritative explosion position and tick.

Rocket and grenade self-knockback are predicted immediately.

Player-to-player physical collisions use current authoritative state.

The server owns final damage, knockback, death, and movement.

Every rewind is bounded, measurable, and weapon-category aware.
```

---

# **46\. Server tick overruns**

The server measures each tick duration.

Target:

```
16.6667 milliseconds per tick
```

If a tick takes longer:

* Record the overrun.  
* Do not use a wildly variable delta time.  
  * Its 60hz  
  * Its just 60hz  
  * It doenst change  
  * Idk wh  
  * It doesnt change  
  * It doenst care  about anything else any performance etc its just a fixed value of 60 do simulation 60hz  60 time s a second  
  * It doesntcange doenst care about rendering it just goes 60 times a second   
* Continue fixed-step simulation.  
* Limit runaway catch-up work.  
* Report sustained overload.  
* Reduce optional network work where possible.  
* Preserve authoritative correctness.

Diagnostics should include:

```
current server Hz
average tick time
maximum tick time
number of overruns
entity counts
packet rates
bandwidth
```

---

# **47\. Connection architecture**

The user-facing goal is:

```
paste code
click join
play
```

THATS it thats all

Nothing else no codes no 

No help menu

No port hosting

No nothing 

Users should not need to understand:

* IP addresses.  
* `ipconfig`.  
* Port forwarding.  
* Router configuration.  
* NAT.  
* UDP hole punching.  
* ICE.  
* STUN.  
* TURN.  
* Relay routing.

Those systems are implementation details.

## **47.1 Worldwide networking goal**

Players in any supported country should be able to:

```
open MiMITA
find or create a session
press Join
connect automatically
play without port forwarding
```

The player should not need to understand:

```
server regions
IP addresses
routing
NAT
ICE
STUN
TURN
relays
packet loss
authority placement
```

The infrastructure automatically selects:

```
best gameplay server region
best relay route
best coordinator endpoint
best snapshot rate
best interpolation delay
best matchmaking region
```

The worldwide goal does not mean eliminating geographic latency.

That is physically impossible.

The goal is:

```
minimize travel distance
avoid bad internet routes
keep local input immediate
keep remote players readable
recover from packet loss
choose fair authority placement
```

---

## **47.2 Global infrastructure layers**

Worldwide MiMITA infrastructure has separate layers:

```
global discovery and control plane
regional gameplay servers
regional relay edges
regional download/content mirrors
monitoring and health systems
```

Conceptually:

```
PLAYER
   ↓
nearest coordinator/API edge
   ↓
global session directory
   ↓
best gameplay region selected
   ↓
nearest relay edge or direct route
   ↓
authoritative gameplay server
```

These systems have separate responsibilities.

The coordinator discovers sessions.

The relay forwards packets.

The gameplay server simulates the match.

The content mirror distributes files.

Do not combine all responsibilities into one server process.

---

## **47.3 Recommended global regions**

Assuming sufficient money and player population, begin with region groups that cover major population and routing centers.

Suggested top-level regions:

```
North America East
North America Central
North America West

South America East
South America West, later

Western Europe
Central Europe
Northern Europe, later
Eastern Europe, later

Middle East

Africa North
Africa South
Africa West, later
Africa East, later

India

East Asia
Southeast Asia

Oceania
```

Example city-level deployment targets:

```
North America East:
Virginia, New York, Toronto, or Montreal

North America Central:
Chicago, Dallas, or Kansas City

North America West:
Los Angeles, Seattle, or San Jose

South America East:
São Paulo

South America West:
Santiago or Lima

Western Europe:
London, Paris, or Amsterdam

Central Europe:
Frankfurt or Warsaw

Northern Europe:
Stockholm or Helsinki

Middle East:
Dubai, Bahrain, or Riyadh

Africa North:
Cairo or nearby regional hub

Africa South:
Johannesburg or Cape Town

Africa West:
Lagos or Accra

Africa East:
Nairobi

India:
Mumbai, Delhi, or Chennai

East Asia:
Tokyo, Seoul, Hong Kong, or Taipei

Southeast Asia:
Singapore, Jakarta, or Kuala Lumpur

Oceania:
Sydney, Melbourne, or Auckland
```

The exact provider city matters less than measured real-player routing.

Do not choose regions only because a cloud dashboard labels them geographically close.

Measure actual:

```
RTT
jitter
packet loss
route stability
regional capacity
```

---

## **47.4 Region identifiers**

Every infrastructure location receives a stable region ID.

```c
enum class RegionId
{
    NorthAmericaEast,
    NorthAmericaCentral,
    NorthAmericaWest,

    SouthAmericaEast,
    SouthAmericaWest,

    EuropeWest,
    EuropeCentral,
    EuropeNorth,
    EuropeEast,

    MiddleEast,

    AfricaNorth,
    AfricaSouth,
    AfricaWest,
    AfricaEast,

    India,

    EastAsia,
    SoutheastAsia,

    Oceania
};
```

Do not identify regions only through display strings.

A server record contains:

```c
struct RegionDescriptor
{
    RegionId id;

    std::string displayName;
    std::string countryCode;
    std::string metroName;

    glm::vec2 approximateCoordinates;

    bool gameplayAvailable;
    bool relayAvailable;
    bool coordinatorAvailable;
};
```

---

## **47.5 Client region probing**

Before matchmaking or joining a public match, the client probes nearby regions.

A probe measures:

```c
struct RegionProbeResult
{
    RegionId region;

    float latestRttMs;
    float smoothedRttMs;
    float jitterMs;
    float packetLossPercent;

    float routeStabilityScore;

    bool reachable;
    uint64_t measuredAt;
};
```

The client should probe several plausible regions, not every server in the world continuously.

Example for a Michigan player:

```
North America Central
North America East
North America West
Europe West
```

Example for a player in Malaysia:

```
Southeast Asia
East Asia
India
Oceania
Middle East
```

Probe packets must be:

```
small
rate limited
unauthenticated or minimally authenticated
safe against amplification abuse
```

---

## **47.6 Region score**

Do not choose a region using ping alone.

Region selection should account for:

```
median player RTT
worst-player RTT
jitter
packet loss
route stability
server load
available capacity
server health
required gamemode resources
```

Conceptual score:

```c
float regionScore =
    medianLatencyScore     * 0.30f +
    worstLatencyScore      * 0.25f +
    jitterScore            * 0.15f +
    packetLossScore        * 0.10f +
    routeStabilityScore    * 0.10f +
    availableCapacityScore * 0.10f;
```

The exact weights remain configurable.

The best region is:

```
the healthiest region that gives the fairest
acceptable connection to the whole lobby
```

Not necessarily:

```
the lowest ping region for the party leader
```

---

## **47.7 Party and lobby region selection**

When a party contains players from multiple countries:

```
1. Every client submits signed recent region probes.
2. Coordinator validates probe freshness and plausibility.
3. Coordinator calculates candidate region scores.
4. Coordinator removes unavailable or overloaded regions.
5. Coordinator selects the fairest remaining region.
6. Clients receive the region choice before joining.
```

Example:

```
Player A: Michigan
Player B: Germany
Player C: Brazil
Player D: Japan
```

Possible measurements:

```
Virginia:
A 25 ms
B 95 ms
C 130 ms
D 180 ms

Frankfurt:
A 105 ms
B 18 ms
C 190 ms
D 215 ms

Dallas:
A 35 ms
B 125 ms
C 115 ms
D 165 ms
```

Dallas may be selected because it produces the best compromise even though it is not best for any single player.

---

## **47.8 Public matchmaking regions**

Automatic matchmaking should first search within the player’s good regions.

Example search expansion:

```
0–15 seconds:
excellent regions only

15–30 seconds:
good neighboring regions

30–60 seconds:
playable wider regions

after 60 seconds:
worldwide regions when enabled
```

The player may choose:

```
Nearby only
Balanced
Worldwide
```

Recommended behavior:

```
competitive:
prefer nearby and fair regions

casual:
allow wider regions sooner

private friend lobby:
allow any chosen region

sandbox:
allow high latency with warning
```

Worldwide matchmaking should not silently place a player into a 250 ms competitive match when a closer match is likely to appear shortly.

---

## **47.9 Dedicated authoritative gameplay servers**

Official global matches should normally run on dedicated authoritative servers.

Each gameplay server runs:

```
the same 60 Hz authoritative simulation
the same validation
the same gameplay definitions
the same packet protocol
the same collision and weapon systems
```

The region changes only where the process runs.

There must not be:

```
Europe gameplay rules
Japan gameplay rules
Brazil gameplay rules
```

There is one game simulation.

A gameplay server should have:

```
stable CPU performance
sufficient upload bandwidth
low packet-processing latency
monotonic clock
health reporting
crash restart
metrics
log upload
DDoS protection
```

---

## **47.10 Regional server pools**

Do not assign every match to one giant server in a region.

Each region contains a pool:

```
region
├── server allocator
├── available gameplay workers
├── active gameplay workers
├── relay edges
└── health monitor
```

A worker may host:

```
one large match
or
several small matches
```

depending on measured CPU and bandwidth budgets.

The allocator selects a worker based on:

```
current CPU load
tick-time history
memory
network bandwidth
match size
gamemode
map
software version
health status
```

Never place a new match on a server already failing to maintain the 60 Hz tick budget.

---

## **47.11 Match allocation flow**

Public match creation:

```
1. Matchmaker forms a lobby.
2. Lobby region probes are compared.
3. Best healthy region is selected.
4. Regional allocator reserves a gameplay worker.
5. Worker creates a unique match session.
6. Coordinator issues signed join tokens.
7. Clients connect directly or through relay.
8. Worker installs initial match state.
9. Match begins after required clients are ready.
```

The reservation includes:

```c
struct MatchAllocation
{
    MatchId matchId;
    RegionId region;
    ServerInstanceId serverInstance;

    NetworkEndpoint directEndpoint;
    RelayRouteId relayRoute;

    ProtocolVersion protocol;
    ContentManifestHash contentHash;

    uint64_t expirationTime;
};
```

---

## **47.12 Relay edge network**

Every major gameplay region should have relay capacity.

A player preferably connects:

```
directly to gameplay server
```

When direct routing is unavailable, unstable, or undesirable:

```
player
→ nearest relay edge
→ gameplay server
```

For long-distance routes, later architecture may support:

```
player
→ nearest regional edge
→ private or optimized backbone
→ gameplay region edge
→ gameplay server
```

This can avoid poor public internet routes.

The relay does not:

```
simulate gameplay
validate damage
own player state
run weapons
decide collisions
```

It only forwards authenticated encrypted packets.

---

## **47.13 Anycast and nearest-edge entry**

Global services may expose one logical hostname or network address.

The network routes the player toward a nearby healthy edge.

Example:

```
api.mimita.fun
relay.mimita.fun
```

The player does not manually select:

```
relay-17-singapore.mimita.fun
```

Nearest-edge entry is useful for:

```
login
session discovery
region probing
join-token requests
relay allocation
```

Gameplay packets still travel to the selected authoritative match region.

The nearest API edge is not automatically the gameplay authority.

---

## **47.14 Coordinator architecture**

The coordinator should be globally available but should not simulate matches.

Coordinator responsibilities:

```
accounts and temporary sessions
server browser
party and lobby records
matchmaking
region probe collection
match allocation
join tokens
relay assignments
server heartbeats
host migration coordination
```

Architecture:

```
global logical coordinator
        ↓
regional stateless API instances
        ↓
replicated shared session storage
```

Regional API instances should be disposable.

If one fails:

```
DNS or traffic routing sends players to another healthy instance
```

Do not require every request to travel to one Michigan VPS.

---

## **47.15 Global data ownership**

Separate globally persistent data from temporary match data.

Persistent data:

```
accounts
cosmetics
friends
settings
verified MMR
bans
ownership records
```

Temporary session data:

```
room codes
join tokens
party membership
server allocation
current relay route
server heartbeat
```

Authoritative live match state:

```
players
positions
projectiles
damage
score
gamemode
match timer
```

Live match state stays primarily on the gameplay server.

Do not write every 60 Hz position update into a global database.

---

## **47.16 Content distribution**

Worldwide players must download required content from a nearby content edge.

Content includes:

```
game updates
maps
models
textures
sounds
weapon definitions
animation definitions
mod packages
```

Use:

```
content hashes
immutable versioned packages
regional CDN caches
resume support
parallel downloads
compression
```

Join flow should not download a map from the individual host’s home computer when official or cached content exists.

Flow:

```
client detects missing hash
→ requests signed content manifest
→ downloads from nearest healthy mirror
→ verifies hash
→ installs session-safe copy
→ joins match
```

---

## **47.17 Cross-region private lobbies**

Friends may intentionally play across distant regions.

For private lobbies:

```
show candidate region results
show predicted connection quality
recommend fairest region
allow host override
```

Example:

```
Recommended region: North America West

Michigan: 72 ms
Tokyo: 98 ms
Sydney: 135 ms
```

Do not block the match solely because latency is high.

Warn clearly:

```
This is a long-distance lobby.
Some remote interactions may feel delayed.
Local movement will remain immediate.
```

---

## **47.18 Cross-region competitive matches**

Competitive matchmaking should use stricter limits.

Possible initial requirements:

```
maximum median RTT:
configurable

maximum worst-player RTT:
configurable

maximum packet loss:
configurable

maximum jitter:
configurable

server tick health:
must remain stable
```

Example initial targets:

```
preferred RTT:
under 80 ms

acceptable RTT:
80–140 ms

high-latency exception:
140–180 ms

above 180 ms:
normally avoid for verified competition
```

These are starting targets and require real testing.

Private matches may ignore these limits.

Ranked matchmaking should not.

---

## **47.19 Network-quality classes**

Every client connection receives a quality classification.

```c
enum class ConnectionQuality
{
    Excellent,
    Good,
    Fair,
    Poor,
    Critical
};
```

Possible initial classification:

```
Excellent:
low RTT, low jitter, almost no loss

Good:
moderate RTT, stable route

Fair:
noticeable latency or jitter but playable

Poor:
frequent corrections or packet loss

Critical:
gameplay continuity cannot be maintained
```

This class controls:

```
interpolation buffer
snapshot frequency
cosmetic traffic
extrapolation limit
compression
warning UI
reconnection policy
```

The classification should use a rolling measurement window.

One bad packet must not instantly classify the connection as critical.

---

## **47.20 Adaptive replication by connection**

Different clients in the same match may receive different snapshot rates.

Example:

```
Client A:
30 Hz nearby player snapshots

Client B:
20 Hz nearby player snapshots

Client C:
15 Hz nearby player snapshots
with larger interpolation buffer
```

The server simulation remains 60 Hz for everyone.

Only transmission changes.

Priority order:

```
local authoritative correction
dangerous nearby projectiles
nearby opponents
damage and death events
gamemode-critical objects
distant players
NPCs
cosmetic effects
```

Reliable gameplay events must not be discarded to preserve cosmetic traffic.

---

## **47.21 Adaptive interpolation by route quality**

Remote interpolation delay should adapt to snapshot rate and jitter.

Conceptually:

```c
interpolationDelay =
    expectedSnapshotInterval
    + jitterMargin
    + safetyMargin;
```

Stable connection:

```
small buffer
lower visible delay
```

Unstable international connection:

```
larger buffer
higher visible delay
smoother movement
```

Changes to interpolation delay should happen gradually.

Do not resize the buffer aggressively every packet.

The player should see smooth motion rather than the buffer itself moving back and forth.

---

## **47.22 Route monitoring and migration**

The game should continue measuring routes after the match begins.

Monitor:

```
RTT
jitter
packet loss
relay delay
server packet queue
server health
regional outage
```

If a route degrades:

```
1. Increase interpolation safety temporarily.
2. Reduce low-priority traffic.
3. Test alternate relay path.
4. Migrate transport route when beneficial.
5. Preserve the same gameplay session.
```

Transport-route migration should not require restarting the match.

Example:

```
direct path becomes unstable
→ switch to relay
→ connection ID remains valid
→ server keeps player session
```

---

## **47.23 Gameplay server migration**

A match may need migration when:

```
server is failing
region suffers outage
server tick health becomes critical
host authority disconnects
maintenance requires evacuation
```

Migration requires periodic checkpoints containing:

```
server tick
random state
players
health
inventories
projectiles
loose objects
NPCs
gamemode
score
timers
pending reliable events
```

Possible flow:

```
1. Select replacement server.
2. Transfer latest checkpoint.
3. Transfer recent event journal.
4. Pause or buffer new inputs briefly.
5. Restore authoritative simulation.
6. Issue new connection route.
7. Clients reconnect or migrate transport.
8. Replay buffered inputs.
9. Resume match.
```

Regional migration may increase latency, so prefer another healthy server in the same region first.

---

## **47.24 Regional failure hierarchy**

When infrastructure fails, use this order:

```
1. Another gameplay worker in same location.
2. Another location in same region.
3. Neighboring region.
4. Peer-hosted authority when supported.
5. End match with recoverable result.
```

Example:

```
Frankfurt unavailable
→ Amsterdam
→ London
→ Warsaw
→ peer authority
```

The exact order depends on measured lobby routes.

Do not hardcode one universal geographic fallback list.

---

## **47.25 Capacity scaling**

Every region reports:

```
available gameplay workers
active matches
average CPU
95th percentile tick time
memory
outbound bandwidth
relay bandwidth
queue time
failure rate
```

Autoscaling may:

```
start more gameplay workers
stop unused workers
add relay capacity
reject new matches from unhealthy pools
redirect matchmaking to neighboring regions
```

Scale before servers become overloaded.

Do not wait until authoritative ticks are already missing.

---

## **47.26 Region population and consolidation**

Not every region will have enough players at all times.

When a region has low population:

```
combine matchmaking with neighboring regions
keep relay and discovery available
run fewer gameplay workers
allow friend-hosted sessions
```

Example:

```
Africa East has too few queued players
→ compare Middle East, India, and Africa South
→ choose based on measured lobby quality
```

Do not permanently force a country into one predefined region.

A player’s best region can change by:

```
ISP
city
time of day
route outage
undersea cable issues
provider congestion
```

Measured networking wins over geographic labels.

---

## **47.27 Country support and legal boundaries**

A supported country requires more than a nearby server.

Support may depend on:

```
network reachability
payment and account availability
local laws
data-handling requirements
sanctions or service restrictions
content rules
age requirements
```

The networking system should represent availability separately:

```c
struct CountryAvailability
{
    CountryCode country;

    bool accountServiceAvailable;
    bool matchmakingAvailable;
    bool gameplayAvailable;
    bool voiceAvailable;
    bool paymentsAvailable;
};
```

Do not make gameplay simulation itself country-specific.

Legal and service availability belongs to infrastructure and account policy.

---

## **47.28 Security and DDoS boundaries**

Public gameplay server addresses should be protected behind:

```
provider network filtering
rate limiting
packet authentication
connection tokens
relay shielding when appropriate
automatic attack detection
server replacement
```

Clients must not be allowed to cause unlimited work through:

```
fake join requests
oversized packets
invalid entity arrays
attack-request floods
reliable-message floods
region-probe floods
```

Dedicated official servers reduce the risk of exposing player home addresses.

Private peer-hosted sessions may still use relays to conceal host addresses.

---

## **47.29 Global monitoring**

Every region reports comparable metrics.

Required metrics:

```
connection success rate
ICE direct success rate
relay usage rate
join duration
RTT by country and ISP
jitter
packet loss
disconnect rate
reconnect success
server tick overruns
snapshot delivery delay
correction frequency
match migration success
regional capacity
```

Dashboards should answer:

```
Can players connect?

Where are connections failing?

Which region is overloaded?

Which routes have high packet loss?

Which ISP-to-region pairs are bad?

Are players correcting too frequently?

Can servers maintain 60 Hz?
```

This integrates with the debug logging specification.

---

## **47.30 Worldwide testing matrix**

Do not claim worldwide support from local testing.

Automated and real-player tests should cover:

```
same-city connection
same-country connection
same-continent connection
cross-continent connection
high stable latency
high jitter
packet loss
packet duplication
packet reordering
temporary outage
relay fallback
route migration
server migration
```

Example simulated network profiles:

```
Profile A:
30 ms RTT
2 ms jitter
0.1% loss

Profile B:
90 ms RTT
8 ms jitter
0.5% loss

Profile C:
150 ms RTT
20 ms jitter
1% loss

Profile D:
220 ms RTT
35 ms jitter
3% loss

Profile E:
temporary 2-second outage
followed by recovery
```

Test every major feature under each profile:

```
walking
jumping
dash
freeze
hitscan
melee
rocket
grenade
rocket jumping
grenade jumping
damage
death
respawn
map change
join in progress
```

---

## **47.31 Worldwide success conditions**

The worldwide architecture is successful when:

```
players never need manual port forwarding

a failed direct connection automatically tries relay

region selection considers the whole lobby

local input appears immediately

remote movement remains readable under supported jitter

replaceable packet loss does not corrupt state

important events are acknowledged or reconstructed

duplicate events do not apply twice

temporary connection loss enters reconnection

server overload is detected before gameplay silently slows

official matches run on healthy regional authority

content downloads come from nearby mirrors

regional outage has a defined fallback

global metrics identify country-specific failures
```

Suggested measurable targets:

```
connection establishment success:
greater than 99% where network access is supported

reliable event integrity:
no lost confirmed damage, death, spawn, or map transition

duplicate authoritative events:
0

server simulation:
60 Hz target with measured overrun limits

local input presentation:
same local simulation tick

unexpected match termination:
tracked by region, provider, ISP, and cause

reconnection success:
measured and improved continuously
```

---

## **47.32 Best final global architecture**

The long-term worldwide architecture is:

```
GLOBAL CONTROL PLANE
accounts
parties
matchmaking
session discovery
region selection
join tokens
server allocation

REGIONAL GAMEPLAY PLANE
60 Hz authoritative gameplay servers
regional match allocators
regional server pools
regional checkpoints

GLOBAL RELAY PLANE
nearest packet edges
direct-route fallback
IP separation
optimized long-distance routing

CONTENT PLANE
regional CDN mirrors
versioned assets
hash verification

OBSERVABILITY PLANE
global metrics
regional health
network-quality analysis
debug-log collection
```

The permanent rules are:

```
Players choose who they want to play with.

The infrastructure chooses where the match should run.

Geography suggests candidate regions.

Real measurements select the final region.

The server closest to one player is not automatically fairest.

Official global matches use regional authoritative servers.

Private matches may use peer authority.

Relays fix reachability and routing, not gameplay authority.

The gameplay simulation remains identical in every region.

Local input is predicted immediately.

Remote state is interpolated adaptively.

Critical events are delivered or reconstructed.

The internet may drop packets.

The match must not lose its truth because a packet was dropped.
```

---

# **48\. Server startup**

User flow:

```
1. Open MiMITA.
2. click Play.
3. click online/community servers.
4. Select server settings. NPCs on/off, server name, map u want, gamemode u want, player limit, etc
5. Press Start.
6. Receive a server code. dont join server yet! 
7. Join the server as a client when u click "Connect to this server" button underneath the generated code. 
```

Starting the server launches or activates:

```
mimita.exe --server
```

The server process may remain running when the host client leaves the match.

If host leaves and stops the server, we need todo 7 18 2026 architecture where other players can become the host, hostmigration

# **48.1. PLAYERS HOST THEIR OWN SERVERS FIRST BEFORE CLOUD SERVERS**

7 20 2026 update

- Self-hosted servers  
-   
- This should come before building an enormous official cloud fleet.  
-   
- Your spec already calls for a configurable coordinator, direct invites, community listings and a bundled dedicated server.  
-   
- Required self-host package  
-   
- Produce:  
-   
- mimita-server.exe        Windows  
- mimita-server            Linux  
- Docker image             Linux hosting  
- server-config.json  
-   
- Configuration:  
-   
- {  
-   "name": "Sydney Movement Server",  
-   "region": "oce-sydney",  
-   "map": "funworld3",  
-   "gamemode": "endless",  
-   "max\_players": 16,  
-   "public": true,  
-   "coordinator\_url": "https://api.mimita.fun",  
-   "require\_relay": false  
- }  
-   
- Add:  
-   
- Linux socket support rather than only Winsock  
- Graceful restart  
- Structured logs  
- Crash restart  
- Configurable ports  
- Automatic coordinator registration  
- Heartbeats  
- Version and content hashes  
- Password/protected-session support  
- Ban list and admin permissions  
- Server console  
- Docker health check  
- Server-browser metadata  
- Signed mimita://join/... links  
-   
- The inspected server is currently tied directly to Winsock and Windows process APIs, so Linux server portability remains significant work.  
-   
- Open coordinator  
-   
- Publish a small coordinator package that communities can run:  
-   
- mimita-coordinator  
- ├── room registry  
- ├── server heartbeats  
- ├── ICE signaling  
- ├── join-token signing  
- ├── TURN credential issuance  
- └── server browser API  
-   
- Clients can default to mimita.fun, while allowing:  
-   
- coordinator\_url=https://community.example  
-   
- Direct IP joining should remain available as an emergency fallback.  
-   
- That means MiMITA remains playable even if your company, website or primary coordinator disappears.\!\!\!\!\!\!\!\!\!\!\!\!\!\!\!\!\!\!\!\!\!\!\!\!\!\!\!\!\!\!\!\!\!\!\!\!\!\!\!\!\!\!  
- 7 20 2026 the long term goal with networking is that, even if ALL servers are down, mimita.fun servers are down, i stopped working on it, etc etc. this game is STILL fully modifiable, still FULLY working online play, still FULLY code-able and fixable even from someone who doesnt really know what the are doing (like me) etc.   
  - Bc i wish sooooo bad   
  - Need for madness had open source multilpauer   
  - Like that would eb so fucking sicjkkkkkkk i wnana plau with people  
  - And AND IN GAME  
  - IN GAME SERVER BROWSER  
  - AND OUT OF GAME NOTIFICATIONS  
  - Discord  
  - Email  
  - Etc  
  - EMAIL me bro   
  - When someoens online EMAIL me  
  - When i open the game SHHOW ME HOW MUHCH PPL ARE ONLINE dude thats HUGE please  
  - Like green dot, 3714 people are online, and like turquoise flashing dot, 31 people are in matchmaking queue RIGHT NOW\!  
  - Like u can join RIGHT NOW\!\!\!  
  - All should be in the game, the .exe the whatever, so even as a beginner who knows nothing, u can clearly see ok people are actually playing this and stuff 

# 

# 48.2. Worldwide reliability proof 

Heres this 7 20 2026

- Worldwide reliability proof  
-   
- Do not call it worldwide until automated tests prove it.  
-   
- Your target test matrix should run every important mechanic under:  
-   
- Profile	RTT	Jitter	Loss  
- Local	30 ms	2 ms	0.1%  
- Regional	90 ms	8 ms	0.5%  
- International	150 ms	20 ms	1%  
- Extreme	220 ms	35 ms	3%  
- Outage	2-second disconnect followed by recovery		  
-   
- Test:  
-   
- Movement  
- Dash, freeze and down-dash  
- Hitscan  
- Shotguns  
- Melee  
- Rockets and grenades  
- Rocket/grenade jumping  
- Death and instant respawn  
- Rejoining  
- Duplicate and reordered packets  
- TURN fallback  
- Route change  
- Server restart  
- Join-in-progress  
- Map change  
-   
- These profiles and success conditions are already clearly defined in the spec.  
-   
- Release gates:  
-   
- \>99% supported-network connection success  
- 0 duplicate damage/death/spawn events  
- 0 permanently lost confirmed gameplay events  
- 60 Hz authority maintained within measured limits  
- automatic TURN fallback  
- successful temporary-outage reconnection  
- no manual port forwarding

# 48.3. People online at 2AM, u can play at any time

7 20 2026

- The “people online at 2 AM” part  
-   
- Networking makes a match reachable. It does not create players.  
-   
- The approximate population math is:  
-   
- average concurrent players  
- ≈ daily active players × average session minutes ÷ 1,440  
-   
- Examples:  
-   
- 2,400 DAU × 60 minutes ÷ 1,440  
- ≈ 100 average concurrent players  
-   
- 4,800 DAU × 30 minutes ÷ 1,440  
- ≈ 100 average concurrent players  
-   
- Because players are divided by region, gamemode and skill level, stable instant matchmaking usually requires more than the raw average.  
-   
- To make low-population hours feel alive:  
-   
- Use one broad casual/endless queue initially  
- Expand matchmaking regions gradually  
- Let players enter an NPC fighting lobby while waiting  
- Keep public community servers visible  
- Support join-in-progress  
- Make rounds extremely short  
- Avoid splitting the population across too many modes  
- Run official always-on servers  
- Let community servers cover countries where you have no official region  
-   
- The strongest early structure is probably:  
-   
- Endless \= main global population pool  
- Duels \= fast opt-in matchmaking  
- Competitive \= added after sufficient population  
- Private/self-hosted \= always available

7 20 2026 bonus implementation order from AI thanks

- Exact implementation order  
- Milestone 1 — Reliable online gameplay  
- Finish generic AttackRequest.  
- Delete legacy weapon packet paths.  
- Finish input sequencing and reconciliation.  
- Finish reliable/unreliable logical channels.  
- Add packet authentication and encryption.  
- Test every mechanic through death and respawn.  
- Pass simulated 220 ms, jitter, loss and outage profiles.  
- Milestone 2 — Worldwide private rooms  
- Valid HTTPS coordinator.  
- Short-lived signed join and TURN credentials.  
- Multiple STUN/TURN endpoints.  
- Automatic direct-to-relay fallback.  
- Reconnect without losing player identity or match state.  
- Real tests between Michigan, Europe and Australia.  
- Milestone 3 — Self-hosted ecosystem  
- Windows and Linux dedicated packages.  
- Docker image.  
- Server browser and heartbeats.  
- Community coordinator package.  
- Configurable coordinator URL.  
- Direct and signed invite links.  
- Version/content compatibility checks.  
- Milestone 4 — Official global matchmaking  
- Region probes.  
- Party-aware region scoring.  
- Matchmaker.  
- Regional worker allocator.  
- Official server pools.  
- Regional relay edges.  
- Global metrics and alerts.  
- Rate limiting and DDoS boundaries.  
- Regional CDN and content manifests.  
- Autoscaling and regional failure fallback.  
- Milestone 5 — 24/7 population  
- One concentrated public queue.  
- Waiting/practice lobby.  
- Community-host incentives.  
- Player acquisition and retention loop.  
- Add modes only when concurrency supports them.  
-   
- The fastest route is not “deploy servers in every country immediately.” It is:  
-   
- one correct gameplay protocol  
- → worldwide room-code beta  
- → easy self-hosting  
- → community servers supply regional coverage  
- → official regional servers  
- → global matchmaking  
-   
- That converts the remaining system from unknown pieces into known, testable modules. The architecture is possible; the work left is now a finite queue rather than an unsolved problem.	

# 48.4. NO LOCALHOST

ALL WE 

- 7 22 2026 1218 all we do is just the same   
- 1 single function, room code joining function, hats all we do  
- We dont do localhost, juts call coordinator   
- Like i knowwwwww its ugh   
- But its fine  
- Coorndiator its like 0.000001 usd to make 1 room it dont matta 

---

# **49\. Initial server settings**

Initial settings include:

```
server name
map
gamemode
maximum players
public or password-protected
NPCs enabled or disabled
public listing enabled or disabled
```

Initial practical maximum:

```
16 players
```

The architecture should avoid assumptions that permanently limit it to 16\.

Larger capacities can be developed after correctness and profiling.

Do not claim 999 players are supported until they have been measured and tested.

---

# **50\. Server code**

The coordinator creates an easy-to-type server code.

Avoid confusing characters:

```
O
0
I
l
1
Y
```

Example:

```
ZMRJKLW
```

The code is only an identifier used to locate the active session.

It is not itself the private authentication credential.

Dont use the letter Y because developer’s keyboard Y key is broken.

---

# **51\. Coordinator responsibilities**

The coordinator service stores temporary session discovery information.

Public session information may include:

```json
{
  "code": "ZMRJKLW",
  "server_name": "MiMITA Server",
  "status": "online",
  "map": "funworldv3",
  "gamemode": "sandbox",
  "players": 3,
  "max_players": 16,
  "password_protected": false,
  "last_heartbeat": "2026-07-18T09:00:00-04:00"
}
```

Private connection information is not returned publicly.

The coordinator may assist with:

* Session discovery.  
* Temporary authentication.  
* ICE signaling.  
* Exchanging connection candidates.  
* Relay assignment.  
* Server-list heartbeat.  
* Join-token issuance.

The coordinator does not simulate gameplay.

---

# **52\. Join token**

When a player requests to join a room, the coordinator creates a temporary signed join token.

The token may include:

```
session ID
player session ID
expiration time
permissions
random nonce
cryptographic signature
```

Conceptual result:

```json
{
  "join_token": "signed-temporary-token"
}
```

The server accepts a joining connection only when the token is:

* Signed correctly.  
* Intended for this session.  
* Not expired.  
* Not already consumed when single-use.  
* Associated with the connecting player session.

This prevents random internet packets from impersonating players.

---

# **53\. Joining flow**

```
1. Player enters server code.
2. Client contacts coordinator.
3. Coordinator checks server heartbeat.
4. Coordinator verifies capacity and access rules.
5. Coordinator issues temporary connection credentials.
6. Host and joining client exchange ICE information.
7. Clients attempt direct connection.
8. If direct connection fails, use relay when available.
9. Client opens transport connection to authoritative server.
10. Server verifies join token.
11. Server sends protocol and content requirements.
12. Client verifies compatible game data.
13. Server sends initial world state.
14. Client acknowledges world load.
15. Server spawns or queues the player.
16. Player enters gameplay.
```

---

# **54\. ICE and connection establishment**

ICE tries available network routes.

Possible candidate types:

```
host candidate:
local network address

server-reflexive candidate:
public-facing route discovered through STUN

relay candidate:
traffic forwarded through a relay
```

Simplified explanation:

```
Both computers automatically test which route lets them exchange packets.
```

No user port forwarding should be required.

---

# **55\. Relay fallback**

Connection preference:

```
1. Direct connection when it works safely.
2. Relay connection when direct connection fails.
```

Correctness and ease of joining come before minimizing relay cost.

Relay usage can later be reduced through:

* Better direct-connect success.  
* Regional relay selection.  
* Packet compression.  
* Snapshot prioritization.  
* Bandwidth measurement.  
* Efficient protocols.

The player should not need to solve network configuration manually.

---

# **56\. IP privacy**

A direct peer-to-peer connection may expose network address information to the participating peers at the transport layer.

A room code or hash does not inherently hide an IP address.

To prevent peers from learning each other’s public network addresses, traffic must go through a relay.

Therefore:

```
direct connection:
lower infrastructure cost
may expose peer network addresses

relay connection:
higher infrastructure cost
better address separation
```

The interface should not display player IP addresses.

Server administration should identify users by:

* Player name.  
* Player ID.  
* Session ID.  
* Ping.  
* Connection state, great, good, fair, poor, critical

# **56.1. Security\!**

Hi 7 20 2026

- Major security work  
- Replace the current coordinator connection  
-   
- The client currently defaults to:  
-   
- http://107.191.48.226:3001  
-   
- and the HTTPS path disables certificate-name, date and CA validation.  
-   
- Before public release:  
-   
- Use https://api.mimita.fun  
- Require a valid certificate  
- Never ignore certificate failures  
- Sign short-lived join tokens  
- Bind tokens to room, server, player and expiration  
- Rotate signing keys safely  
- Give TURN credentials short expirations  
- Add API rate limits  
- Add replay prevention  
- Avoid logging complete secrets or SDP credentials  
- Authenticate and encrypt gameplay packets  
-   
- ICE discovers a route. ICE alone is not the gameplay-security layer.  
-   
- Add authenticated encryption above the transport, such as:  
-   
- Noise/DTLS handshake  
-         ↓  
- session keys  
-         ↓  
- AEAD-encrypted gameplay datagrams  
-         ↓  
- packet sequence prevents replay  
-   
- The server must reject a packet before doing expensive parsing when its authentication tag is invalid.  
-   
- Remove the single-point hardcoding  
-   
- The current configuration defaults to one STUN address and one TURN address.  
-   
- Production needs a returned endpoint list:  
-   
- {  
-   "stun": \[  
-     "stun-na-east.mimita.fun",  
-     "stun-eu-central.mimita.fun",  
-     "stun-ap-southeast.mimita.fun"  
-   \],  
-   "turn": \[  
-     {  
-       "host": "turn-na-east.mimita.fun",  
-       "priority": 10  
-     },  
-     {  
-       "host": "turn-eu-central.mimita.fun",  
-       "priority": 20  
-     }  
-   \]  
- }  
-   
- Clients should attempt multiple endpoints and report which route succeeded.

---

# **57\. Initial state synchronization**

After authentication, the server sends a complete initial state.

This includes:

* Server configuration.  
* Current map.  
* Gamemode state.  
* Current server tick.  
* Player list.  
* Player transforms.  
* Health and life state.  
* Inventories and equipped weapons.  
* Active projectiles.  
* Active loose objects.  
* NPC states.  
* Scores.  
* Match timer.  
* Relevant persistent effects.  
* Version and content hashes.

The joining client must not enter normal incremental snapshot processing before the initial state is installed.

But also dont lag the joining players game if the state is slow to load. Just get them in there. And make load times as low as possible 7 18 2026 

---

# **58\. Join-in-progress behavior**

Join behavior depends on gamemode.

```
Endless:
spawn immediately when state is ready

Casual round-based:
join immediately when safe, or wait for next short round

Competitive:
wait for a controlled round boundary

Spectator:
enter immediately as observer
```

A waiting practice space may exist, but it is still client/server gameplay. Like  the players taht are waiting to join canjust play a small mode vs NPCs in a lobby of some kind so theure not just sitting there doing nothing 

If an offline practice instance is shown while waiting, it runs through a private local server process rather than an unrelated local gameplay path.

---

# **59\. Content compatibility**

Before joining, client and server compare:

```
network protocol version
gameplay schema version
map hash
weapon-definition hash
animation-definition hash
required asset manifest
mod list, when applicable
```

If required content differs:

```
1. Identify mismatched content.
2. Download the server-approved version when permitted.
3. Preserve the user’s existing file rather than silently deleting it. also notify them that their file was found not valid, renamed to (file name) at (location) and u are downloading the valid file (file name) and then u will join 
4. Load the matching version for this session.
5. Recheck hashes.
```

The server should reject incompatible gameplay data rather than allowing clients to simulate different weapon or physics rules.

---

# **60\. Reconnection**

When connectivity is lost:

```
1. Client enters Reconnecting.
2. Client retains temporary session state.
3. Client attempts connection again.
4. Server reserves the player’s slot temporarily.
5. Valid reconnect token restores the player session.
6. Server sends authoritative recovery state.
7. Client clears invalid prediction history.
8. Gameplay resumes.
```

Reconnect data may preserve:

* Player identity.  
* Team.  
* Kills and deaths.  
* Inventory.  
* Session permissions.  
* Position, where safe.  
* Alive/dead state.  
* Current match membership.

Initial slot reservation target:

```
60 seconds
```

---

# **61\. Reconnect attempts**

Use bounded exponential backoff.

Example:

```
attempt 1 in 1 second
attempt 2 in 2 seconds
attempt 3 in 4 seconds
attempt 4 in 8 seconds
attempt 5 in 12 seconds
```

Cap the delay.

Do not continue forever without informing the user.

Display:

```
Connection lost.
Reconnect attempt 3/10…
```

---

# **62\. Disconnect behavior**

## **Remote player disconnects**

The server:

* Marks connection unavailable.  
* Reserves session temporarily.  
* Decides what happens to the player body.  
* Notifies other clients when necessary.  
* Removes the entity after timeout if reconnection fails.

## **Host client leaves**

If the host only closes or leaves the visible client:

```
the separate server process may continue
```

The server is not automatically stopped unless requested.

## **Authoritative server process ends**

All connected clients lose authority source.

Without migration support, the match ends.

BUT WITH MIGRATION SUPPORT WHICH WE TODO NEED TO ADD THE MATCH CAN KEEP GOING . 7 18 2026 so we need that 

---

# **63\. Host migration**

Host migration is a later architecture.

It is not merely selecting the player with the lowest ping.

To preserve the match, another machine needs enough authoritative state to recreate the server.

This requires one of:

```
every client maintains a recoverable server checkpoint
```

or:

```
a selected backup host receives periodic authoritative checkpoints
```

Checkpoint state includes:

* Server tick.  
* Random-number state.  
* Players.  
* Inventories.  
* Health.  
* Projectiles.  
* Loose objects.  
* NPCs.  
* Gamemode state.  
* Score.  
* Match timer.  
* Pending reliable events.  
* Session permissions.

Only after checkpoint replication exists can exact-state host migration be promised.

Initial implementation may display:

```
Host connection lost.
Match ended because host migration is not available yet.
```

Later flow:

```
Host left. Picking next best host…
New host: PlayerName
Restoring match…
```

---

# **64\. Server browser**

A public server may post a heartbeat to the coordinator.

The server browser can display:

* Server name.  
* Server code or Join button.  
* Map.  
* Gamemode.  
* Player count.  
* Maximum players.  
* Password-protected status.  
* Ping estimate relative to the person browsing servers.   
* Date/time listed.  
* Version compatibility.

The authoritative server still runs on the host or dedicated machine.

The server browser is discovery, not gameplay authority. 

The server browser simulates NO gameplay. Its a 1gb ram 20gb memory VPS its for website mostly so it cant do that stuff uet. 

---

# **65\. Portable and self-hosted discovery**

Long-term survival goal:

```
MiMITA should remain playable if mimita.fun disappears.
```

Support may include:

* Open-source coordinator.  
* Configurable coordinator address.  
* Direct connection invites.  
* Community server listings.  
* Portable signed connection invites.  
* Dedicated-server executable bundled with the game.

Possible invite:

```
mimita://join/v1/...
```

A decentralized public listing system is a separate future project.

It should not block implementing reliable client/server gameplay now.

---

# **66\. Dedicated server**

The same authoritative server simulation should run in:

```
mimita.exe --server
```

or a dedicated package such as:

```
mimita-server.exe
```

The dedicated server:

* Has no rendering requirement.  
* Runs the same 60 Hz gameplay simulation.  
* Loads the same gameplay definitions.  
* Uses the same packet protocol.  
* Uses the same validation.  
* Produces the same snapshots and events.  
* Does not create a new gameplay code path.

---

# **67\. Server console commands**

Initial commands:

```
server_help
server_info
server_stop
server_restart
server_leave
server_map
```

## **`server_help`**

Lists available commands and usage.

## **`server_info`**

Displays:

```
server code
server name
map
gamemode
uptime
current players
maximum players
connection type
server tick rate
snapshot rates
bandwidth
```

Player information example:

```
Jorj1357         0 ms (host client)
Builderman      42 ms
Quickscoper     71 ms
```

Do not display player IP addresses.

## **`server_stop`**

Stops the authoritative server and disconnects clients with a clear reason. 

“Disconnected from (server id)

Reason: Host stopped server”

## **`server_restart`**

Restarts using the same configuration and attempts to preserve the server listing and code where safe.

“Host restarted server. Attempting to reconnect to server (id)... (attempt N/N)”

## **`server_leave`**

Leaves the visible client while allowing the separate server process to continue.

## **`server_map`**

Displays current and available maps or requests a controlled map transition.

Server\_map\_list

Lists all maps available , in number form

So like

Funworld… 1

Chainofjudgement… 2

Aabbtest… 3

Writing server\_map 2 means “I request this server’s map be changed to Chainofjudgement”

So all plauerrs are gonna change to be on the map Chainofjudgement

---

# **68\. Map changes**

The server owns map changes.

Flow:

```
1. Server chooses next map.
2. Server emits MapChangePreparing.
3. Clients stop accepting normal gameplay input.
4. Server sends required map identity and hash.
5. Clients load and acknowledge.
6. Server resets world state.
7. Server sends initial map snapshot.
8. Server emits MapStarted.
9. Clients resume gameplay.
```

One client cannot independently change the authoritative map.

---

# **69\. NPC authority**

NPCs are server-authoritative.

Npcs use the same functions that players use. Move, collide, shoot, aim, etc. the are just like practice bots. NO CUSTOM LOGIC FOR NPCS 

The server owns:

* NPC spawn.  
* AI decisions.  
* Navigation.  
* Movement.  
* Attacks.  
* Damage.  
* Health.  
* Death.  
* Loot or score effects.

Clients render and interpolate NPC state.

Clients must not each create unrelated local NPC worlds.

A private one-player server still simulates NPCs on its server process.

---

# **70\. NPC update rates**

The server simulates NPC logic and physics at defined rates.

Important combat NPCs may require 60 Hz movement and collision simulation.

Expensive decision-making may run less frequently.

Example:

```
physics and combat resolution: 60 Hz
nearby AI decisions: 10–30 Hz
far AI decisions: 1–5 Hz
network snapshot transmission: based on relevance
```

Lower AI decision frequency does not mean the server stops simulating the entity’s physical movement.

---

# **71\. Effects replication**

Effects are produced from named gameplay events.

Examples:

```
weapon fired
projectile trail started
projectile impacted
explosion occurred
player damaged
player died
dash occurred
freeze occurred
movement direction changed
```

Local client prediction may start effects instantly.

The server event confirms and replicates them to other players. 

Idk how to do this but need instant effects as well, like instant world hit effects and instant explosion effects nad sounds. No delays allowed. Matbe do like a servr disagree effect as well? Or like server unconfirmed hit effect for a little/ not sure 

Effects should use unique event IDs to prevent duplicates.

The effects system does not decide gameplay authority.

---

# **72\. Animation replication**

The network does not need to transmit every bone every tick.

Transmit semantic animation state:

```
movement state
movement direction
grounded
vertical velocity
dash state
freeze state
weapon pose
attack event
reload state
death state
respawn generation
```

Remote clients reconstruct animation using the same animation system.

SAME ANIMATION SSTEM

THAT NPCS USE

PLRS USE

SERVER USERS

CLIENT USERS

Ok 

Server doesnt do animations, it just sas

Hi clients, at tick 2875, plr jorj1357 started moving, i confirmed it, so u all start simulating the animation using the same function that server has, client has, etc 

All u use that same function to simulate that plr’s animation, then when that plr stops moving, stop simulating that plr’s animation

Every respawn receives a new life or spawn generation ID so stale animation events from the prior body are ignored.

---

# **73\. Spawn generations**

Entities that can die and respawn need a generation value.

Example:

```c
struct SpawnIdentity
{
    EntityId playerId;
    uint32_t spawnGeneration;
};
```

Packets for generation 4 must not mutate the player after generation 5 has spawned.

This prevents:

* Old death events affecting the new body.  
* Old weapon poses returning after respawn.  
* Old freeze events attaching to the respawned player.  
* Old projectile ownership references being confused.

# **73.1. Spawning plauer function**

Its this 

 completeAuthoritativeSpawn is now the ONLY function that calls resetPlayerForSpawn. One spawn \= one generation increment.

7 18 2026 1707 just use htis  
Its just this 1 function  
BC WE HAVE DEATH ISSUES AND UGH SO JUST USE HTIS 1  
I ahvent tested if it works or not yet th o so ugh 

---

# **74\. Server disagreement feedback**

Corrections should be measurable and optionally visible.

A disagreement event contains:

```c
struct DisagreementEvent
{
    uint32_t eventId;

    EntityId affectedEntity;
    uint64_t serverTick;

    DisagreementType type;
    DisagreementSeverity severity;

    glm::vec3 predictedPosition;
    glm::vec3 authoritativePosition;

    std::string reasonCode;
};
```

Possible reasons:

```
attack_no_ammo
attack_cooldown
attack_invalid_origin
movement_impossible_velocity
movement_world_collision
projectile_position_mismatch
projectile_collision_mismatch
damage_occluded
stale_request
duplicate_request
```

Debug presentation may include:

* Directional sphere effect.  
* Text label.  
* Sound.  
* Console log.  
* Prediction error magnitude.  
* Request and entity IDs.

Normal small corrections should not spam the player.

Use large presentation only for meaningful disagreements or explicit debug mode.

These should be cool like interesting effects to help players learn the flow of the game and report bugs easier.

Like if u see a huge sphere that sas “insufficient\_ammo” whevner u shoot, its a lot easier to debug

Alsoit looksk super cool and fun, it has like real time  server tick/heartbeat  having an effect on the game, like a referee 

---

# **75\. Diagnostics and structured logs**

Every important network flow should be traceable by IDs. see debug spec 

Example:

```
[ATTACK REQUEST]
client=2 request=481 tick=1000 weapon=grenade_launcher

[ATTACK ACCEPT]
client=2 request=481 projectile=9821 serverTick=1002

[PROJECTILE CONFIRM]
client=2 request=481 projectile=9821 positionError=0.04

[EXPLOSION]
projectile=9821 explosion=15502 serverTick=1182

[DAMAGE]
explosion=15502 target=4 amount=80 health=20
```

Logs should include:

* Connection ID.  
* Player ID.  
* Packet sequence.  
* Request ID.  
* Entity ID.  
* Client tick.  
* Server tick.  
* Result.  
* Rejection reason.  
* Prediction error.  
* Ping.  
* Packet loss when available.

---

# **76\. Protocol versioning**

Every connection begins with protocol negotiation.

```c
struct ProtocolHello
{
    uint32_t protocolVersion;
    uint32_t minimumCompatibleVersion;

    uint64_t gameplaySchemaHash;
    uint64_t contentManifestHash;
};
```

Incompatible clients receive a clear reason.

Example:

```
Unable to join:
Your game uses network protocol 16.
This server requires protocol 18.
```

Do not attempt to decode incompatible packet layouts silently.

# **76.1. Network and game verrsioning**

Ok s

- 7 21 2026 1429 idk where to put this exactly 

---

# **77\. Packet safety**

Every packet must be validated before use.

Validate:

* Packet size.  
* Message type.  
* Version.  
* Entity IDs.  
* Array lengths.  
* String lengths.  
* Numeric ranges.  
* Finite vectors.  
* Sequence numbers.  
* Authentication.  
* Rate limits.  
* State eligibility.

Never trust a packet simply because it came from an established connection.

---

# **78\. Authentication and passwords**

Do not place server passwords directly in visible process arguments such as:

```
mimita.exe --server --password password12345
```

Use:

* Secure in-process configuration.  
* Prompted input.  
* Protected configuration storage.  
* Salted password verification.  
* Temporary join proofs.

The server does not need to store a plaintext password.

The coordinator should not expose private access information publicly.

---

# **79\. Rate limiting**

The server limits request rates.

Examples:

```
maximum input packets per second
maximum attack requests per second
maximum chat messages per interval
maximum join attempts
maximum invalid packets
maximum server commands
```

Weapon cooldown remains the gameplay rule.

Network rate limiting is a separate abuse and stability boundary.

A client sending thousands of duplicated attack requests must not consume unlimited server work.

---

# **80\. Timeouts and heartbeat**

Gameplay connections track:

```
last packet received
last valid input received
last acknowledged server packet
round-trip time
connection state
```

Possible connection states:

```c
enum class ConnectionState
{
    Disconnected,
    ResolvingCode,
    RequestingJoin,
    WaitingForJoinAccept,
    NatNegotiating,
    Connecting,
    Synchronizing,
    Connected,
    Reconnecting,
    DisconnectPending
};
```

A temporary packet gap does not immediately destroy the player session.

A sustained timeout begins reconnection handling.

---

# **81\. Ping calculation**

Ping is measured using timestamps or sequence acknowledgements.

Use a smoothed round-trip estimate.

Track:

```
current RTT
smoothed RTT
jitter
packet loss estimate
```

Display understandable ping in the scoreboard.

Do not use one abnormal packet as the permanent ping value.

---

# **82\. Packet acknowledgement**

Packets may include:

```c
uint32_t localPacketSequence;
uint32_t latestRemotePacketSequence;
uint32_t acknowledgementBitfield;
```

The acknowledgement bitfield reports receipt of recent packets.

This supports:

* Packet-loss measurement.  
* Reliable message retransmission.  
* Bandwidth diagnostics.  
* Congestion decisions.  
* Snapshot baseline selection.

---

# **83\. Congestion awareness**

7 28 2026 we r gonan need either a dedicated serve or fullt go to p2p connections bc the vps is the 1 server we have that does  that

- OK BUT I WANT TO DO FULL P2P NO SERVER NEEDED ok…  
- I wnat to do that so that this game exists and runs and works no matter where u are what ur doing  whatever server connections exist  no deidacetd server  
- But thats later, once we have deidcated servers , then we can work on doing pure p2p bc we understand how it works and its wokring 

The server should avoid blindly increasing outbound traffic when a client’s connection is overloaded.

Track:

* Sent bytes.  
* Acknowledged bytes.  
* Packet loss.  
* Queue growth.  
* RTT change.  
* Retransmissions.

When congestion rises:

* Preserve critical reliable events.  
* Reduce low-priority snapshot rate.  
* Avoid building an indefinitely growing reliable queue.  
* Disconnect with a clear reason if the connection cannot maintain minimum gameplay requirements.

---

# **84\. File responsibility boundaries**

Todo 7 28 2026 idk if this is actual accurate? Need to confirm that this is actually sttructure in the repo cuz i didnt write this part i thoguht files just get kinda made or not made 

## **`network-protocol.h`**

Does:

* Define packet and event types.  
* Define protocol version.  
* Define serialization-safe data structures.

Does not:

* Open sockets.  
* Simulate gameplay.  
* Apply damage.  
* Render effects.

---

## **`network-transport.h`**

Does:

* Define transport interface.  
* Send and receive byte packets.  
* Expose connection events.  
* Expose reliability channels.

Does not:

* Understand weapons.  
* Validate movement.  
* Spawn entities.  
* Apply gameplay rules.

---

## **`udp-transport.cpp`**

Does:

* Implement UDP transport.  
* Handle packet addressing.  
* Handle packet sequence and acknowledgement support.

Does not:

* Know about players or projectiles.  
* Process gameplay packets directly.

---

## **`ice-transport.cpp`**

Does:

* Perform ICE setup.  
* Gather candidates.  
* Establish direct or relayed transport.  
* Report connection state.

Does not:

* Authenticate gameplay.  
* Simulate the server.  
* Apply player state.

---

## **`client-network-session.cpp`**

Does:

* Manage client connection state.  
* Send input and requests.  
* Receive snapshots and events.  
* Maintain tick synchronization.  
* Route messages to client systems.

Does not:

* Decide authoritative damage.  
* Own server player state.  
* Contain weapon-specific networking branches.

---

## **`server-network-session.cpp`**

Does:

* Manage connected clients.  
* Authenticate packets.  
* Queue inputs and requests.  
* Build snapshots.  
* Replicate reliable events.  
* Track acknowledgements and connection health.

Does not:

* Contain all gameplay simulation.  
* Render anything.  
* Generate client prediction.

---

## **`client-prediction.cpp`**

Does:

* Store input history.  
* Simulate predicted state.  
* Reconcile authoritative snapshots.  
* Replay unacknowledged input.  
* Track prediction errors.

Does not:

* Send raw socket packets.  
* Decide confirmed death.  
* Modify authoritative server state.

---

## **`server-validation.cpp`**

Does:

* Validate generic requests.  
* Check state eligibility.  
* Check timing and plausibility.  
* Return accepted or rejected result.

Does not:

* Play effects.  
* Render UI.  
* Implement transport protocols.

---

## **`snapshot-system.cpp`**

Does:

* Select relevant entities.  
* Build snapshots.  
* Encode authoritative state.  
* Apply frequency and priority rules.

Does not:

* Simulate entities.  
* Apply damage.  
* Read player input.

---

## **`network-reconciliation.cpp`**

Does:

* Compare predicted and authoritative state.  
* Classify correction severity.  
* Produce corrected state.  
* Record disagreement events.

Does not:

* Decide server authority.  
* Perform weapon-specific behavior.  
* Open network connections.

---

## **`server-history.cpp`**

Does:

* Record recent authoritative states.  
* Retrieve state by server tick.  
* Support rewind validation.  
* Expire old history.

Does not:

* Render replays.  
* Permanently store full matches.  
* Accept client claims without validation.

---

# **85\. Main architectural prohibitions**

Do not create separate gameplay implementations for:

```
local
online
host
client
dedicated server
replay
```

Do not create weapon-specific networking systems.

Do not make rendering frame rate control gameplay timing.

Do not make snapshot transmission rate control simulation rate.

Do not trust client damage, health, death, or score.

Do not replace smooth simulated projectile motion with raw packet positions.

Do not use current server state when historical tick state is required.

Do not resend one button press as unlimited new requests.

Do not spawn duplicate effects when prediction becomes confirmation.

Do not let old-life packets affect a newly respawned player.

Do not expose IP addresses in normal UI.

Do not require port forwarding.

---

# **86\. Final target model**

```
CLIENT
60 Hz fixed simulation
captures input
predicts movement
predicts attacks
predicts projectiles
predicts local effects
sends intent and requests
receives snapshots and events
reconciles to authority
renders at any FPS

SERVER
60 Hz fixed simulation
receives input and requests
validates them
simulates authoritative movement
simulates authoritative projectiles
applies damage and knockback
owns health, death, score, and match
sends snapshots at selected rates
sends critical events immediately

NETWORK
does not run gameplay
moves commands, requests, snapshots, and events
uses different channels and sending rates
handles sequencing, acknowledgement, reliability, ICE, and relay

SHARED GAMEPLAY SYSTEMS
are called by both client and server
use the same definitions
use the same fixed delta time
do not know whether they are local or online
do not decide network authority internally
```

The central rule is:

```
Both client and server simulate at 60 Hz.

They do not need to send every simulated tick.

The client predicts immediately.

The server owns the final result.

Snapshots correct simulation; they do not replace simulation.
```

