9 6 2026 

- End goal  
  - A single mode of behavior, like the current godball, that helps define all melee weapons later  
    - Like,   
    - Justholding the weapon should mean  
    - If someone touches it or walks into it \= the take dmaage, based on force  
    - Damage or knockback etc  
    - clicking/moving just does animations , so it doenst make the weapon behave diffrent, it just does a diffrent animation   
    - Also, because u can attack everu single tick, which i want to keep as true, no throttling , no artificial limits  
    - We need to send damage and stuff to the server in batches, like maybe at 10hz, or 5hz? Idk  
    - Instead of 60 sends, we onl do 10 sends or 5, and batches of them have the idk data inside of them  
    - Balance between ping/lag/instant stuff, and go hard on client sided prediction   
  - Mode 1  
    - Spy knife is this knife that , ur arm phsicallt moves it in the world  
    - If it is touched by someone, e.g. walking itno it, they take damage,not like click \= attack  
    - Hit from front \= 1 damage, big knockback against the victim  
    - Hit frmo back \= backstab, 999 damage, spy knife crit sound effect plaus   
  - Mode 2  
    - Clicking \= a hitbox appears and an animation plays that shows the hitbox and stuff, like exact like super smash bros melee  
  - Networking  
    - Batch hits because u can hit every single tick, at 60hz tick rate thats a lot, so server should not work super hard incase we hit a lot,  
    - Clients individually themselves can do more work just  make sur th server  only gets like important data not every single  hit request   
  - Behavior  
    - I go into mimita exe  
    - I equip the spy knife  
    - Its a physical object in m right hand, and its welded to m right arm on the hand  
    - I look up \= it moves up, as a result of the aimbody.json  thing that is there as of 9 6 2026  
    - Down \= same thing moves down  
    - I left click \= slash arm forward and do an attack animation	  
      - Todo ,head  torso and legs should be part of attack animaiton not just arms   
    - But the animatin is not what damages people, the knife itself damages peopel. Animation just mvoes the arm that holds the weapon  
    - I click over and oevr and over \= it starts again the animation every single time i click. Animation is measured in ticks, so not frame times or ms, its in ticks   
    - I walk up to  a player and slash them with the knife from the front \= the take damage like  3 times,   
      - Client side:  
        - I instant see damge numrs, damage getting hit effects , hear the sounds, see the hit effects, etc. its satisfying and predicted   
        - I instant predict their movement bc it instant applies knockback impulse, single tick that is then naturally decayed by other movement systems like gravity and the other player’s movement  
      - Their client side:  
        - They see me walking up to them and the knife phsicallt intersects their body, overlaps with a body part of therirs  
        - They get knockback applied to their body, and prob predict it as well like if both clients predict a hit then sure cool but ultiamte the sever is authoritative over damage and knockback if it even is applied at all   
      - Server side:  
        - The attacker only sends like, a single batch of hit claims,   
          - Dont send 60 individual hit claims, thats not good , batch it and send like 1 per 10 ticks/ or 1 per 15 ticks or 1 per 20 ticks idk   
        - then the server does like rollback and lag compensation so it sees is taht a valid hit according to the view of the world i had at the time? If so , then its a valid hit and gets applied.  
          - Todo maek prediction super duper good so i can predict na dhave next to  0 delay between hitting them/gettig hit and actually having the damage be applied 	  
  - Knife  
    - From front  
      - 1 damage HOT RELOADABLE IN WEAPONSJSON as of 9 6 2026  
      - And like  
      - 10 knockback   
      - From the direction i hit them from in the direction that the shouldgo.  
      - I hit them from underneath \= the get knocked back upward, bc attack from underneath \= mometum carries up  
      - Hit them from front \= knocked bakcward  
      - Hit them frmo back \= no knockback,this is a 999 damage instant kill, and has no throttle, if uhit them like 5 ticks in a row it should show 999 damage everu single time for u as the client, instant predicted effect and hitmarker and stuff should show, and hit sound as well   
- 9 6 2026 bad behavior current  
  - Visuals and stuff are all working fine, but, i just cant actually do damage to an npc. I see them take damage, i see the hit damage numbrs and i get crosshair and stuff, but it just doenst do damage to them, server not allowing it. Need to make it actually do damage to them,   
  - Check networking spec, and this spec and stuff 

# **MiMITA Physical Melee & Spy Knife Specification**

**Date:** 09-06-2026  
**Status:** Target behavior / architecture specification  
**Reference implementation:** Spy Knife  
**Primary mode:** Mode 1 — Physical Weapon Contact

---

# **1\. End Goal & Absolute Melee Rules**

MiMITA should have **one reusable physical melee behavior** that can later power weapons such as:

```
Spy Knife
large swords
chainsaws
clubs
bats
pistol whips
fists
weird physics weapons
future melee weapons
```

The fundamental rule is:

> **The physical weapon touching the victim causes the potential hit. Clicking does not inherently cause damage.**

For Mode 1:

```
physical weapon moves through world
        ↓
weapon damaging shape overlaps victim body
        ↓
melee contact exists
        ↓
client predicts result immediately
        ↓
contact is recorded
        ↓
contacts are network-batched
        ↓
server historically validates contact
        ↓
authoritative damage + knockback
        ↓
prediction confirmed or reconciled
```

Clicking only changes the physical pose/animation.

Example:

```
left click
    ↓
slash animation starts/restarts
    ↓
arm moves
    ↓
knife attached to arm moves
    ↓
knife happens to intersect player
    ↓
contact
```

There must NOT be:

```
left click
    ↓
DamagePlayer()
```

A player may also walk directly into an already-held melee weapon and cause contact without the attacker clicking.

## **No Artificial Melee Cooldown**

There is intentionally **no generic melee hit cooldown**.

Simulation is 60 Hz.

Therefore a melee weapon may potentially hit:

```
once per victim
per weapon
per simulation tick
```

or:

```
60 hits/second
```

This is intentional.

Do NOT "fix" this by adding:

```
100 ms cooldown
250 ms cooldown
500 ms cooldown
one hit per animation
one hit until weapon leaves body
one hit per click
```

unless a specific future weapon explicitly defines such behavior.

Extremely fast repeated melee contact is allowed and encouraged.

---

# **2\. Shared Physical Melee System**

Spy Knife must NOT become a one-off melee implementation.

The desired shared architecture is approximately:

```
                PHYSICAL MELEE
                      │
        ┌─────────────┴─────────────┐
        │                           │
 weapon/contact definition      weapon pose
        │                           │
        └─────────────┬─────────────┘
                      ▼
               CONTACT QUERY
                      │
                      ▼
              first body contact
                      │
                      ▼
             MELEE CONTACT EVENT
                      │
          ┌───────────┼───────────┐
          ▼           ▼           ▼
       DAMAGE      KNOCKBACK    EFFECTS
          │           │           │
          └───────────┼───────────┘
                      ▼
                  NETWORKING
                      │
                      ▼
             SERVER VALIDATION
```

The shared system should define concepts such as:

```
weapon instance
attacker
victim
simulation tick
weapon collision shape
contact position
contact body part
weapon velocity
relative contact velocity
attack direction
victim facing direction
attacker facing direction
damage rule
knockback rule
predicted/confirmed/rejected state
```

Do not create separate fundamental systems for:

```
SpyKnifeDamage()
SwordDamage()
ChainsawDamage()
PistolWhipDamage()
```

when they can all use the same contact pipeline with different weapon configuration.

Client, server, NPCs, players, replay, and tests should reuse the same underlying gameplay calculations wherever possible.

---

# **3\. Contact Detection & Per-Tick Hit Rules**

A Mode 1 melee hit begins with **physical overlap/contact**.

The damaging collision shape should correspond closely to the visible weapon.

For Spy Knife:

```
visible knife
≈
damaging knife collision
```

The player should be able to understand:

> "The knife physically touched that body."

## **One Damage Event Per Victim Per Tick**

A weapon may intersect several victim body parts during one simulation tick.

Example:

```
tick 123

knife intersects:
right arm
torso
left arm
```

This is **ONE damage event**, not three.

Rule:

```
maximum:
1 damage event
per weapon
per victim
per simulation tick
```

Record the **first valid body contact detected** for that tick.

Example:

```
tick: 123
weapon: spy_knife_18
victim: PlayerB
firstContactBodyPart: right_arm
```

On tick 124, if contact still exists:

```
another hit is allowed
```

Therefore:

```
tick 123 → HIT
tick 124 → HIT
tick 125 → HIT
tick 126 → HIT
...
```

is legal.

Do not require separation/re-entry between hits.

## **Body Parts**

The system should preserve which body part was contacted.

This is important for:

```
debugging
effects
backstab validation
future damage rules
future exact geometry
replay
network disagreement analysis
```

---

# **4\. Spy Knife Behavior, Animation & Aim Pose**

Spy Knife is the first reference implementation of Mode 1 physical melee.

When equipped:

```
Spy Knife
    ↓
physical object in right hand
    ↓
attached/welded to right-hand/arm pose
```

The existing aim-body system controls the character pose.

As of 09-06-2026 this relates to:

```
aimbody.json
```

Looking upward moves the arm/knife upward.

Looking downward moves the arm/knife downward.

The physical weapon follows the resulting pose.

## **Clicking**

Left click starts the slash animation.

Clicking again immediately restarts the animation.

Example:

```
tick 100: click → animation starts
tick 102: click → animation restarts
tick 103: click → animation restarts again
```

Do not wait for the previous animation to finish.

Animation timing is measured in **simulation/client ticks**, not rendered frames or milliseconds.

Eventually the attack animation should be able to involve:

```
arms
head
torso
legs
whole-body pose
```

rather than only moving an arm.

But:

> **Animation itself never owns damage.**

Animation merely moves the character and therefore the attached physical weapon.

## **Stationary Contact**

Clicking is not required.

If the player holds the knife still and another player walks into it:

```
victim enters knife collision
        ↓
physical contact
        ↓
valid melee hit
```

subject to normal server validation.

---

# **5\. Spy Knife Damage, Backstabs & Backstab Pose**

Spy Knife has two important contact outcomes.

## **Front / Normal Contact**

Normal contact deals a small amount of damage.

Initial desired behavior:

```
approximately 1 damage per valid contact
```

The exact damage belongs in the existing weapon configuration and must be hot reloadable through the appropriate weapons JSON.

The desired practical result is that a normal slash may produce several tiny damage events:

```
1
1
1
```

rather than one conventional large melee hit.

A typical front slash may contact for roughly three ticks, producing roughly three damage events, depending on actual physical contact.

Do NOT implement this as:

```
click = exactly 3 damage
```

The number of hits comes from actual per-tick physical contact.

## **Backstab**

Backstab:

```
damage = 999
knockback = none
```

The exact configured values belong in weapon configuration.

A valid backstab requires ALL applicable conditions to pass at the relevant historical tick.

Initial V1 conditions:

1. physical knife/body contact exists;  
2. attacker is in the valid region behind the victim based on victim facing direction;  
3. attacker facing direction is sufficiently toward the victim;  
4. the knife contacts an appropriate back body region/body part.

Conceptually:

```
physical contact
AND
victim facing-angle condition
AND
attacker facing-angle condition
AND
back-body contact
        ↓
BACKSTAB
```

Facing-angle thresholds must be configurable rather than buried as unexplained constants.

The server evaluates these conditions at the **historically relevant contact tick**, not using unrelated current poses after the packet arrives.

Future versions may use more exact knife geometry/contact-point/body-surface analysis.

For V1, facing directions \+ body-part contact are sufficient.

## **Backstab Readiness Pose**

When:

```
target within approximately 3 meters
AND
facing-angle conditions indicate a potential valid backstab
```

the Spy Knife arm should automatically raise into a recognizable backstab-ready pose similar in purpose to TF2 Spy's visual readiness feedback.

This is presentation/animation state, NOT authoritative proof that a backstab occurred.

The actual hit still requires physical contact and server validation.

Relevant presentation values should be JSON/config driven and hot reloadable, including appropriate:

```
range
angle thresholds
pose/animation parameters
timing
```

where consistent with the owning configuration architecture.

---

# **6\. Physical Knockback & Weapon Force**

Normal Spy Knife contact primarily exists to produce **physical-feeling knockback** with small damage.

Knockback direction should derive from the actual physical motion/contact of the knife rather than simply:

```
attacker position → victim position
```

The weapon's world-space contact velocity should be the primary physical input.

Conceptually:

```
knife world-space velocity at contact
                +
configured knockback behavior
                ↓
        knockback impulse
```

Examples:

```
knife moving upward into victim
→ victim receives upward impulse

knife moving forward into victim
→ victim moves backward

knife moving diagonally upward/right
→ corresponding directional impulse
```

The goal is for contact direction to behave intuitively like physical momentum.

Knockback should be a **single-tick external impulse**.

After that tick, normal movement systems naturally act on the resulting velocity:

```
gravity
friction
air movement
player input
collision
other impulses
```

Do not implement melee knockback as an arbitrary multi-frame scripted push.

## **Backstab Exception**

A Spy Knife backstab has:

```
999 damage
0 knockback
```

because it is intended as an immediate lethal stab rather than a launching attack.

## **Future Weapons**

The same shared knockback architecture can support:

```
large sword → huge directional impulse
club → heavy blunt impulse
chainsaw → repeated smaller physical impulses
pistol whip → weapon-motion impulse
tiny knife → smaller impulse
```

without inventing another knockback system.

---

# **7\. Client Prediction, Effects & Reconciliation**

Melee must feel immediate.

The attacking client does NOT wait for a server round trip before showing predicted contact.

On predicted contact:

```
knife intersects victim
        ↓
IMMEDIATELY:
damage number
hitmarker
hit effect
hit sound
victim reaction
predicted HP change where appropriate
predicted knockback
```

The attached networking specification already establishes this broader rule: clients predict immediate feedback while the server owns what ultimately becomes real. fileciteturn0file0L527-L560

Knockback should also be predicted immediately; the networking architecture explicitly treats immediate predicted knockback followed by correction as the target behavior. fileciteturn0file0L1092-L1119

## **Prediction Goal**

The target is:

> **Client prediction and server authoritative reality agree as close to 100% of the time as practical.**

The system should be engineered, instrumented, and tested toward extremely rare visible disagreement.

Physical latency cannot be removed.

The goal is to make correct prediction conceal its perceptual consequences wherever possible.

## **Server Disagreement**

Example:

```
client predicts:
tick 100 hit
tick 101 hit
tick 102 hit

server:
tick 100 CONFIRMED
tick 101 CONFIRMED
tick 102 REJECTED
```

The client must reconcile authoritative gameplay state.

For example:

```
predicted victim HP = 97
authoritative victim HP = 98
        ↓
restore/correct HP to 98
```

Movement/knockback must likewise reconcile to authoritative state.

The already-played predicted effects represent what the player actually experienced and should not be silently rewritten as though they never happened.

A rejected/incorrect prediction should produce the existing **server disagreement** presentation/diagnostic according to the appropriate effects/networking/debug specifications.

The networking specification similarly requires server authority over final health/damage while allowing predicted hitmarkers, damage numbers, reactions, and other immediate presentation. fileciteturn0file0L1061-L1089

---

# **8\. Batched Melee Networking & Server Validation**

Local melee contact detection continues at:

```
60 Hz
```

Network transmission does NOT need to occur once for every contact tick.

This follows the networking architecture's existing distinction:

> simulation rate ≠ transmission rate. fileciteturn0file0L184-L219

## **Initial Batch Rate**

Initial target:

```
melee simulation: 60 Hz
melee claim transmission: 10 Hz
```

Therefore approximately:

```
6 simulation ticks
→ one outgoing melee contact batch
```

The batching rate must be configurable/hot reloadable through the appropriate networking/weapon configuration.

Do not hardcode 10 Hz as an eternal protocol assumption.

Test other rates under real conditions.

## **Conceptual Batch**

Exact packet structure belongs to the networking architecture, not this weapon spec.

Conceptually a batch needs enough information to associate claimed contacts with their original simulation ticks:

```
MeleeContactBatch

attacker
weapon instance
batch ID
start tick
end tick

contacts:
    tick 100 → victim B → torso → contact...
    tick 101 → victim B → torso → contact...
    tick 102 → victim B → arm   → contact...
```

Do NOT create a completely separate Spy Knife networking architecture.

The networking specification requires generic requests/shared weapon paths rather than packet types invented for each weapon. fileciteturn0file0L425-L447

Therefore the exact implementation should integrate with/generalize the existing generic attack/melee networking owner.

## **Server Validation**

The server remains authoritative.

For each claimed contact, it maps the client tick onto the relevant authoritative historical tick and validates against historical state.

The networking specification already defines melee as a historical moving-volume problem: reconstruct attacker and target poses over relevant ticks, evaluate the melee volume, and confirm configured repeated contacts. fileciteturn0file0L1569-L1594

Conceptually:

```
receive batch
    ↓
deduplicate
    ↓
map client contact tick → server tick
    ↓
load relevant historical state
    ↓
reconstruct attacker pose
    ↓
reconstruct knife pose
    ↓
reconstruct victim body pose
    ↓
perform shared physical melee query
    ↓
validate contact
    ↓
validate front/backstab conditions
    ↓
calculate shared damage/knockback
    ↓
apply authoritative result
    ↓
confirm/reject
```

Do not simply trust:

```
client says:
"I hit PlayerB"
```

But also do not validate a delayed melee claim only against where everyone happens to be when the packet finally arrives.

---

# **9\. Players, NPCs, Debugging & Current Regression**

Players and NPCs use the **same melee damage architecture**.

There must NOT be:

```
player melee pipeline
NPC melee pipeline
```

for equivalent physical interactions.

Desired:

```
                MELEE CONTACT
                     ↓
             SHARED VALIDATION
                     ↓
          DAMAGE + KNOCKBACK
                ↙        ↘
             PLAYER      NPC
```

Differences between players and NPCs should exist only where the entities genuinely require different behavior.

## **Current Bad Behavior — 09-06-2026**

Observed:

```
equip Spy Knife
↓
physically hit NPC
↓
client shows damage number
client shows hitmarker/effects
client appears to detect contact correctly
↓
NPC authoritative health does NOT decrease
```

This means presentation/prediction appears to be occurring, but the authoritative result is not being accepted/applied.

Do NOT invent the root cause from this symptom.

Current status:

```
OBSERVED:
predicted Spy Knife hit effects occur against NPC,
but authoritative NPC damage does not occur.

ROOT CAUSE:
UNCONFIRMED.
```

Investigate:

```
contact generation
batch/request generation
transport
server receipt
tick mapping
historical melee validation
NPC historical collision state
shared damage calculation
authoritative NPC health application
confirmation event
```

Identify the exact failure before changing architecture.

Once demonstrated:

```
observed behavior
expected behavior
root cause
exact fix
regression test
proof
```

should be recorded according to the regression/debug specifications.

## **Required Melee Diagnostics**

For development/debugging, a contact should be traceable through:

```
contact ID / prediction ID
batch ID
attacker ID
victim ID
player/NPC entity type
weapon ID / instance
client contact tick
mapped server tick
current server tick
rewind amount
first contacted body part
contact position
knife position
knife velocity
attacker position/facing
victim position/facing
normal/backstab classification
predicted damage
authoritative damage
predicted knockback
authoritative knockback
accepted/rejected
rejection reason
```

Do not fabricate unavailable measurements.

---

# **10\. Regression Tests & Definition of Done**

The first implementation is **Mode 1 physical melee \+ Spy Knife**.

Mode 2 — animation-generated fighting-game hitboxes similar to Smash — is future work.

However, architecture should allow Mode 2 to eventually generate contacts into the same downstream pipeline:

```
MODE 1
physical weapon collider
        ↓
      CONTACT
        ↓
shared melee pipeline

MODE 2 — FUTURE
explicit animation hitbox
        ↓
      CONTACT
        ↓
same shared melee pipeline
```

## **Required Tests**

### **Physical Contact**

```
PASS:
knife physically touches victim
→ contact generated

PASS:
victim walks into stationary knife
→ contact generated

FAIL:
click with no physical contact
→ no damage
```

### **Per-Tick Behavior**

```
PASS:
knife touches 3 body parts on one tick
→ exactly 1 damage event

PASS:
record first contacted body part

PASS:
continuous contact for 5 ticks
→ up to 5 damage events

PASS:
continuous contact for 60 ticks
→ up to 60 damage events
```

### **NO COOLDOWN Regression**

Explicit permanent regression test:

```
continuous valid contact
for ticks 1–60

EXPECTED:
60 contacts may be accepted

FORBIDDEN:
hidden cooldown
one-hit-per-animation restriction
one-hit-per-click restriction
must-exit-before-rehit restriction
arbitrary throttle
```

**60 hits/second is intentional behavior.**

Networking optimization must NEVER change this gameplay rule.

### **Normal Spy Knife**

```
front contact
→ configured small damage

multiple contact ticks
→ repeated small damage

knife moving forward
→ victim receives corresponding physical knockback

knife moving upward
→ victim receives upward knockback
```

### **Backstab**

```
physical knife contact
AND valid victim facing
AND valid attacker facing
AND valid back body contact
→ 999 configured backstab damage
→ backstab effect/sound
→ no knockback
```

Fail backstab if any required condition fails.

### **Backstab Readiness**

```
within configured readiness range
+ appropriate facing relationship
→ raise knife/arm into backstab-ready pose
```

This does NOT itself grant damage.

### **Animation**

```
click
→ slash starts

click again before completion
→ slash immediately restarts

animation moves weapon
animation itself does NOT call damage
```

### **Prediction**

```
predicted contact
→ immediate effects
→ immediate predicted knockback
→ no server-round-trip wait
```

### **Rejection**

```
client predicts hit
server rejects
→ authoritative HP restored/corrected
→ movement reconciled
→ server-disagreement diagnostic/effect
```

### **Batching**

```
60 Hz contact simulation
≈ 10 Hz initial transmission
```

Verify batching does NOT reduce actual allowed hit frequency.

Six contacts occurring during one network batch remain six independently identifiable tick-specific contacts.

### **Player/NPC Equivalence**

The same physical melee query/damage/knockback path must pass against:

```
human player
NPC
```

The current NPC authoritative-damage regression must receive a demonstrated root cause and automated regression test.

## **Definition of Done**

A successful end-to-end Spy Knife test is:

```
launch MiMITA
↓
equip Spy Knife
↓
knife physically exists in right hand
↓
aim up/down moves physical knife through shared aim-body pose
↓
walk into victim without clicking
↓
physical contact can damage
↓
left click repeatedly
↓
animation restarts every click
↓
animation physically moves knife
↓
knife intersects victim for several ticks
↓
each tick may independently produce contact
↓
instant predicted damage numbers/effects/sounds
↓
instant predicted physical knockback
↓
contacts bundled for networking
↓
server maps each contact to historical tick
↓
shared melee query validates actual contact
↓
authoritative damage/knockback applied
↓
NPC and player use same pipeline
↓
client prediction confirms or reconciles
```

And:

```
approach victim from behind
↓
backstab-ready pose appears when conditions are appropriate
↓
physical knife intersects valid back region
↓
server verifies historical facing + contact
↓
999 configured damage
↓
backstab sound/effects
↓
0 knockback
```

## **Permanent Invariants**

> **Physical contact causes Mode 1 melee hits. Clicking does not.**

> **Animations move the weapon; animations do not own damage.**

> **One weapon may damage one victim once per simulation tick.**

> **60 hits/second is legal and intentional.**

> **Never add an arbitrary generic melee cooldown to solve networking or performance problems.**

> **Optimize transmission through batching, not gameplay through throttling.**

> **Knockback comes from the physical weapon/contact motion.**

> **Client prediction is immediate; server damage authority remains final.**

> **Server validation uses the historically relevant contact state rather than packet-arrival state.**

> **Players and NPCs use the same shared melee behavior.**

> **Spy Knife is a configuration/use of the shared melee system, not its own isolated architecture.**

> **Future swords, chainsaws, clubs, pistol whips, fists, and other physical melee weapons should reuse this system instead of creating new fundamental implementations.**

