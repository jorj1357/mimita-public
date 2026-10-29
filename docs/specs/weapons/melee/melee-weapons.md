2026 10 29 1115

deepseek v4.1 flash thnaks 

enabled — turns on client contact claims. Misses with no authoritative damage: ensure true. Set false only to fall back to the older server scan.

use_weapon_collision_hitbox — true uses the config/weaponcollisions.json box as the damage volume and sweeps it through the target. Thin/visual mismatches: use true or enlarge the box. Phantom hits: use false or shrink the box.

claim_tolerance_m — extra lag/pose allowance. Too many outside_claim_radius rejects: raise by 0.25 steps. Hits landing visibly beside the target: lower toward 0.25–0.5.

target_radius_m — lenient historical target body radius. Limb/edge hits missed: raise toward 1.1–1.3. Near-misses counting: lower toward 0.6–0.8.

max_reach_m — explicit server reach cap; 0 derives it from the box/blade size. Valid long swings rejected as out_of_reach: set/raise it. Remote hits accepted: lower toward visible reach.

claim_batch_ticks — how often claims are sent; detection is still 60 Hz. Damage feels delayed: lower toward 2–3. Packet volume: raise toward 6–12.

occlusion — rejects claims blocked by world geometry. Legitimate corner/limb hits rejected as world_occluded: investigate geometry first; false is the permissive fallback. Hits through walls: keep true.

damage_scale — multiplies authoritative damage after normal damage/action/impact calculations. Too little health loss: raise above 1.0. Too much: lower below 1.0.

force_scale — multiplies authoritative knockback. Barely moves: raise. Launches too far: lower. It does not change damage.

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

## **Additional user report — NPC and PvP coverage**

The existing NPC report above already describes client-side Spy Knife
presentation without authoritative NPC health loss. A newer user report says
the same mismatch is still encountered and that Spy Knife versus player (PvP)
damage is absent or nonfunctional. This report is not independently
reproduced here; keep NPC and player targets as separate scenarios while
tracing the same shared contact/authority pipeline.

### Source audit — client target selection and authoritative validation (2026-10-16)

Code review found two concrete client-side mismatches and two server-side
observability/geometry risks. These findings identify likely failure paths;
they do not prove which one caused a particular live report.

- In `src/combat/weapon-spyknife.cpp`, remote contact selection chooses the
  NPC replica map and falls back to remote players only when the NPC map is
  empty. When both target types are present, the current loop therefore never
  tests remote players. The client must evaluate both maps and preserve the
  target type for each contact.
- `applySpyKnifeRemoteHit()` calls `mpApplyPredictedDamage(..., true)` for the
  remote target. The boolean selects the NPC prediction store in
  `src/network/multiplayer-interpolation.cpp`; for a player target, prediction
  is therefore written to the wrong replica state. Pass the actual target
  type through the hit helper.
- On the server, `handleSpyKnifeHitClaim()` resolves an NPC ID before trying a
  player ID and does not use the packet's `targetIsNpc` bit to select the
  namespace. Verify whether player and NPC IDs can overlap before relying on
  this ordering; then make target kind explicit and validate it against the
  authoritative entity registry.
- Server hit validation checks the client OBB against a historical
  root-centered sphere, while client contact generation tests replicated
  physical-body parts. Long limbs or pose/rewind differences can create
  client-hit/server-reject disagreements. Align the authoritative historical
  body representation with the accepted melee contract; do not enlarge reach
  or radii without evidence.
- Missing historical NPC/player poses currently `continue` without a specific
  rejection record in the shown handler. Emit bounded categorized reasons so
  each candidate contact can be followed from client detection through server
  accept/reject and confirmation or correction.

Recommended implementation order after spec approval: (1) test NPC and player
maps independently and carry typed target identity end-to-end; (2) route
prediction to the correct replica store; (3) validate type/ID unambiguously;
(4) choose and align the authoritative historical hit geometry; (5) add
bounded contact-decision diagnostics. Validate with a real two-client server,
with at least one NPC present, plus NPC-only, player-only, rejected-hit,
rollback, and varied-avatar/body-pose cases. Keep PvP mode policy separate
from a contact validation failure. Runtime proof remains required before
calling any of these the reproduced root cause.

Required proof for both target types: client prediction/effects, contact
batch, server receipt and validation, authoritative damage/knockback result,
and target health/state after the result. Confirm player damage is permitted
by the active mode before calling a rejected hit a bug. A PvP test must use two
real clients on the authoritative server; NPC success does not prove PvP.

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

## Client contact claims — implementation bridge (2026-10-28)

SwordSword, mace, and chainsaw now use a lenient client-contact claim path in
addition to the existing authoritative server contact scan.

- The client resolves the same rendered weapon attachment it draws, tests the
  swept blade capsule against remote player/NPC replicas at the fixed 60 Hz
  tick, and queues one contact per target per tick.
- Claims carry target identity/kind, contact tick and ID, blade segment and
  radius, contact point/normal, and impact speed/directness. They are batched
  through the existing physical-contact batch transport at the configured
  contact claim cadence, initially 6 ticks.
- The server validates attacker life/equipment/generation, target life and
  friendliness, claimed tick, blade bounds, historical target pose, reach, and
  clear world occlusion. It accepts when the claimed blade is within the
  target collision volume plus blade radius plus configurable
  `contactClaimTolerance` (default 0.75 m).
- The server recalculates damage/knockback from the weapon/action configuration
  and applies it through the shared damage owner. Client damage numbers remain
  prediction.
- The existing server contact scan remains a fallback. Accepted claims are
  deduplicated per target/tick so the fallback cannot apply the same contact
  twice.
- A rejected claim does not grant damage; the client receives the normal
  disagreement/reconciliation presentation. No generic melee hit cooldown is
  added. Contact frequency remains one per victim per weapon per simulation
  tick.

### Per-weapon melee contact configuration

Every `config/weapons/<id>/config.json` carries a top-level
`melee_contact` object. The weapon masterlist
`config/weapons/masterlist/weapon-masterlist.json` owns the field definition
and default value. Use:

```text
python devscripts/weapon_json_sync.py --sync
python devscripts/weapon_json_sync.py --reorder
python devscripts/weapon_json_sync.py --check
```

Defaults:

```json
"melee_contact": {
  "enabled": false,
  "use_weapon_collision_hitbox": false,
  "claim_tolerance_m": 0.75,
  "target_radius_m": 0.9,
  "max_reach_m": 0.0,
  "claim_batch_ticks": 6,
  "occlusion": true,
  "damage_scale": 1.0,
  "force_scale": 1.0
}
```

Current enabled weapons:
- SwordSword: enabled, weapon-collision box, tolerance `0.75`, target radius
  `0.9`.
- Mace: enabled, weapon-collision box, tolerance `1.0`, target radius `1.1`.
- Chainsaw: enabled, weapon-collision box, tolerance `0.9`, target radius
  `1.0`.

#### What the setting changes in the pipeline

For an accepted client claim the server computes approximately:

```text
force01 = clamp(impact_speed / contactMaxForceSpeed, 0, 1) * impact_directness
damage  = mix(contactMinDamage, contactMaxDamage, force01)
        * action_damage_multiplier
        * rev_multiplier
        * melee_contact.damage_scale
knockback = knockback_direction
          * mix(contactMinKnockback, contactMaxKnockback, force01)
          * melee_contact.force_scale
```

The claim is accepted only after the server:

1. checks attacker life, spawn generation, equipment, and move-set revision;
2. rewinds the target to the claimed tick;
3. checks the claimed capsule/box against the target volume using
   `target_radius_m + claim_tolerance_m`;
4. checks reach and optional world occlusion; and
5. applies damage through the shared authoritative damage owner.

`damage_scale` and `force_scale` are **after** all normal weapon/action/impact
calculations. They tune this claim path without changing the base weapon
damage or knockback fields.

#### Field reference and tuning

| Field | What it does | If hits feel too weak / not registering | If hits feel too strong / phantom |
|---|---|---|---|
| `enabled` | Turns on client contact claims for this weapon. With `false`, only the older server animated-contact fallback can damage. | Set `true` if the client visibly hits but authoritative health does not change. Verify the move set and weapon identity first. | Set `false` only as a diagnostic fallback; this makes hit detection slower and less aligned with the client. |
| `use_weapon_collision_hitbox` | `true` uses the `config/weaponcollisions.json` box as the damage volume and sweeps it through target body parts. `false` uses the animated blade capsule. | Set `true` when the visual weapon is much larger than the thin blade capsule, especially SwordSword/mace. Enlarge the box in `weaponcollisions.json` if the visual weapon still exceeds it. | Set `false` for a tighter shape. If true causes phantom hits, shrink the box in `weaponcollisions.json`; do not compensate with a large negative tolerance. |
| `claim_tolerance_m` | Extra radius added to the target body radius when validating the claimed contact. This absorbs network lag, one-frame pose differences, and animation mismatch. | Increase in steps of `0.25` (`0.75 → 1.0 → 1.25`). Use more for fast weapons, high ping, or large swings. | Decrease toward `0.25–0.5`. If hits land visibly beside the target, tolerance or target radius is too large. |
| `target_radius_m` | Treats the historical target as a body sphere of this radius for the lenient claim check. It is deliberately larger than the true limb geometry. | Increase toward `1.1–1.3` if edge/limb hits are rejected with `outside_claim_radius`. | Decrease toward `0.6–0.8` if near-misses count. Keep it close to the actual actor capsule; do not use it to fake reach. |
| `max_reach_m` | Explicit maximum attacker-to-target distance for a claim. `0` derives a value from the claimed shape size + target radius + tolerance + 2 m. | If valid long-reach swings are rejected as `out_of_reach`, set an explicit value slightly above the visible reach (for example sword `8`, chainsaw `7`, mace `14`). | Lower it toward the visible reach. Never set it below the actual blade/box length or legitimate overhead swings will be rejected. |
| `claim_batch_ticks` | Sends queued contacts every N fixed ticks. Detection still runs every 60 Hz tick; only transmission is batched. `1` = every tick, `6` = 10 Hz. | Lower to `2–3` when damage feels delayed behind the visual hit. | Raise toward `6–12` if packet volume is a concern; higher values increase perceived latency. Never set below `1`. |
| `occlusion` | When `true`, rejects a claim if world geometry is clearly between the attacker position at the contact tick and the target center. | Set `false` only if legitimate corner/limb/through-gap hits are being rejected as `world_occluded`. | Keep `true` to prevent hits through walls. If it causes false rejects, fix the contact point/geometry rather than disabling it globally. |
| `damage_scale` | Multiplies authoritative damage after base range, action multiplier, rev multiplier, and impact force. | Increase above `1.0` (`1.1–1.5`) if accepted contacts do too little health damage. Check `server.damage.applied` first. | Decrease below `1.0` (`0.5–0.9`) if accepted contacts damage too much. This does not change hit registration. |
| `force_scale` | Multiplies authoritative knockback impulse. | Increase above `1.0` if hits connect but barely move the target. | Decrease below `1.0` if targets launch too far. This does not change damage. |

#### Quick tuning recipes

**"I see the sword/mace pass through but health does not change."**

1. Confirm `enabled: true`.
2. Set `use_weapon_collision_hitbox: true`.
3. Inspect the journal for `melee.contact.claim.rejected`:
   - `outside_claim_radius`: raise `claim_tolerance_m` and/or
     `target_radius_m`.
   - `out_of_reach`: set or raise `max_reach_m`.
   - `world_occluded`: check the contact geometry; lower/disable `occlusion`
     only if the rejection is a false wall hit.
   - `invalid_contact_bounds`: the `weaponcollisions.json` box is too large or
     malformed; resize it.
4. If the client emits no `melee.client.contact_claim`, the client did not
   detect contact. Check the `weaponcollisions.json` box dimensions, weapon
   model pose, and that the target replica is present/aligned.

**"It hits too easily / hits nothing visible."**

1. Lower `damage_scale` and `force_scale` first if only the numbers are too
   strong.
2. If phantom contacts are the problem, lower `claim_tolerance_m` and
   `target_radius_m`, then set `use_weapon_collision_hitbox: false` or shrink
   the box in `weaponcollisions.json`.
3. Keep `occlusion: true` unless it is producing demonstrated false rejects.

**"Damage is too weak even though contact is accepted."**

1. Check `damage` in `melee.contact.claim.accepted` and
   `server.damage.applied`.
2. Raise `damage_scale`.
3. If the damage is still low, inspect the base/action contact damage range
   (`contactMinDamage`, `contactMaxDamage`, action `contact_min_damage`,
   `contact_max_damage`) and impact speed/directness.

**"Target barely moves."**

1. Raise `force_scale`.
2. If still weak, inspect `contactMinKnockback` / `contactMaxKnockback` and the
   action knockback overrides.
3. Do not change hit registration settings to compensate for weak knockback.

**"Damage arrives late."**

1. Lower `claim_batch_ticks` toward `2–3`.
2. Do not add a cooldown or change contact geometry.

#### Important invariants

- `melee_contact` settings tune the claim/validation path; they do not add a
  generic melee damage cooldown.
- Contact detection remains one possible damage event per weapon per victim per
  60 Hz tick.
- The server still owns final health, death, friendly-fire, spawn protection,
  and knockback application.
- Client damage numbers are prediction; if they disagree with accepted server
  damage, check `damage_scale`, base/action contact values, and move-set
  revision/hash agreement.
- The masterlist is the source of truth for field presence. Edit per-weapon
  values, then run `--sync`, `--reorder`, and `--check`.