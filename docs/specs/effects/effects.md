9 6 2026

- End goal  
  - DOCUMENT ALL THIS IN CHANGELOGS, AND CHECK REGRSSIONS, AND DONT DO THINGS THAT ARE REGRESSIONS   
    - Log regression once u figure this out so we know exactly what made it do bad behavior and what fixed it and how to not do that again   
  - Effects exist in the world,   
    - and are called with the same exact paths for in game as they are for replay,   
    - so exporting the replay shows exactly what u saw in the game  
    -  from a tick to tick perspective   
  - Writing this because currently  
    - There is a backface culling issue 9 6 2026 1114 idk why need to investigate  
      - Needs to be fixde, then written in regressions  why it failed  
      - I see the backsides of  effects, but the front side is culled and i cant see the front  
      - Need it to be flippewd   
    - No effects show in exported replays   
      - No damage numbrs  
      - No getting shot/hit effects  
      - No server disagree effects/text from those effects  
      - No dash, landing, walking, moving, world impact spark effects   
    - unconfirmed/other  
      - Chat might not show? Not sure . whole chat needs to show in the replay, and show the typing indicator in the chat at top left , and also a typing indicator above the users head   
      - Link to chat spec [journal v38 MiMITA plan v6](https://docs.google.com/document/d/1uTxOkciwKHtSzBm4rKxxMqMshNGttK9TOk8ju3uBu-E/edit?tab=t.w7dhicb7w67a)   
- Effects for everything   
  - Walking  
  - Jumping  
  - Air jump  
  - Dash  
  - Hitting the ground  
  - Shooting someone   
    - Damage numbers  
    - A elongated sphere that is like along the direction u hit them from  
  - Firing a gun  
    - White sphere that exists for 1 single tick  
      - Hot reloadable  
    - Actual dynamic lighting that exists super quick  
      - HOT RELOADABLE AS WELL\!\!\!\!  
    - Tracer   
      - Defined here hot reloadable, for now defined here 9 6 2026 1115 C:\\mimita-priv-v8\\config\\weapon-tracers.json  
    - Shooting sound  
      - Current: plays a new sound every time  
      - Desired:   
        - One sound, that just goes from   
        - Imagine   
        - Fired at tick 12, tick 15, and tick 16  
        - Old: mkes a new sound that plays entirely each tick, so u hear overlapping  
        - New:  
        - One single sound, that   
        - Tick 12: start playing at 0 sec  
        - Tick 12-14: playing,  
        - Tick 15: go back to 0 sec, and plau again  
        - Tick 16: go back to 0 sec , and play again  
        - Tick 16and on: if no firing again, just let it finish its sound  
  - Firing rocket launcher/missile  
    - Same as above, but smoke/stylized low poly smoke for it , and rocket launcher projectile, and explosion is a big red spehre that exists for like 3 ticks matbe, so it shows super qcik the hitbox of the rocket launcher   
  - Melee  
    - Reuse the stretched spheres and damage numbrs that show rigt now, for each hit  
    - Melee also should be limited only by the tick rate, u can hit 1 time per tick with no cooldown. So u can hit 60 times in 1 second, that should be allowed and legal and encouraged, so u can see the client sided hit effects, super fast  
      - But server doesnt receive all this at once, it receives batched, so it doesn thave to work sooo ohard  
      - And other clients just predict the damage i think? Idk needs more work, and better networking spec doc [journal v38 MiMITA plan v6](https://docs.google.com/document/d/1uTxOkciwKHtSzBm4rKxxMqMshNGttK9TOk8ju3uBu-E/edit?tab=t.o3bdxu8eurs)  here  
  - Hitting the world  
    - Collisions from like , future physical objects \= debris happens  
    - Projectiles hit the world \= debris and effects, scaled with how fast it hit the surface and how direct the angle is it hit it from   
    - Plr bod parts should also have these hit effects as well from hitting the world, like little shockwaves or dust plumes or sparks should show   
- Replays  
  - Effects seem to not show in replays, why?  
    - 9 6 2026 1116 something to do with bakcface culling and stuff we did ?   
    - Check git diff but honestly not sure 

# **MiMITA Effects, Replay Determinism & Regression Specification**

**Date:** 09-06-2026  
**Status:** Desired behavior \+ current issues \+ regression requirements

---

# **1\. End Goal**

MiMITA effects must have **one implementation path**.

There must NOT be separate implementations such as:

```
LiveGunEffect
ReplayGunEffect
LiveExplosion
ReplayExplosion
```

Instead:

```
Gameplay / Client Experience
        ↓
Effect Event
        ↓
Shared Effects System
        ↓
Visible Effect
```

A replay records the event:

```
Replay
    ↓
Same Effect Event
    ↓
Same Shared Effects System
    ↓
Same Visible Effect
```

The fundamental rule is:

> **Same event \+ same tick \+ same seed \+ same configuration \= same visuals.**

The replay should reproduce what that specific player's client actually experienced, **not reconstruct a separate approximation of it**.

For V1, replays represent the recorded player's POV.

A future replay mode may reconstruct the server-authoritative version of events, but that is separate and out of scope here.

MiMITA does not currently need a full ECS or complicated new architecture just to accomplish this. Implement the smallest event system necessary to satisfy this behavior.

---

# **2\. Shared Event Architecture**

Effects should be generated from events rather than recording every individual visual object into the replay.

Example:

```
tick = 1042
event = PlayerHit
attacker = PlayerA
victim = PlayerB
damage = 35
position = X,Y,Z
direction = X,Y,Z
seed = 92841
```

That event goes through the shared effects system:

```
PlayerHit Event
      ↓
Effects System
      ├── damage number
      ├── stretched hit sphere
      ├── particles
      ├── sound
      └── other configured visuals
```

The replay records the event and enough information to reproduce it.

It should NOT need to record:

```
particle 1 moved here
particle 2 moved here
particle 3 moved here
...
```

Instead:

```
event + tick + seed + configuration
```

should reproduce the effect.

Random effects should eventually use deterministic seeds.

MiMITA does not currently have to already possess a global seed system. Add the smallest deterministic mechanism needed for effects.

Presentation systems should remain logically separate:

```
Effects
Audio
Chat
UI
Typing indicators
```

But all of them should follow the same replay principle:

> **Record the event/state needed to reproduce what the player experienced, then replay it through the same normal implementation.**

Do not create separate replay-only versions of these systems.

---

# **3\. Effect Definitions**

The effects system should support at least these gameplay events.

## **Movement**

```
Walking
Jumping
Air jump
Dash
Landing / hitting ground
```

## **Player Hit**

When a player is hit:

```
damage number
stretched / elongated sphere in hit direction
configured hit effects
```

These should use the existing stretched-sphere/damage-number behavior where appropriate rather than unnecessarily creating another system.

## **Gun Fire**

Firing a gun should support:

```
1-tick white sphere / muzzle effect
dynamic muzzle lighting
tracer
shooting sound
```

The muzzle effect and dynamic lighting must be hot reloadable.

Tracer configuration currently exists at:

```
C:\mimita-priv-v8\config\weapon-tracers.json
```

## **Gun Sound Behavior**

Do NOT create overlapping copies of the same firing sound every time a weapon fires.

Desired example:

```
Fire tick 12:
sound begins at 0 seconds

Ticks 13-14:
same sound continues

Fire tick 15:
same sound restarts from 0 seconds

Fire tick 16:
same sound restarts from 0 seconds

No more firing:
sound continues normally until finished
```

Conceptually:

```
one weapon firing sound source
        ↓
new shot
        ↓
restart that source at t=0
```

## **Rocket / Missile**

Rocket firing uses the normal shared weapon effects where applicable, plus:

```
rocket projectile
stylized low-poly smoke trail
rocket-specific effects
explosion
```

The explosion should initially show a large red sphere representing the explosion/hit volume for a very short tick-based duration, approximately 3 ticks unless later tuned.

## **Melee**

Reuse:

```
stretched hit sphere
damage number
hit effects
```

Melee may produce one legitimate client-side hit per simulation tick.

At 60 Hz:

```
maximum temporal resolution = 60 hits/second
```

Do not add an arbitrary visual cooldown merely to reduce the frequency.

Networking/batching for extremely rapid melee events belongs in the networking specification rather than being independently invented here.

## **World Impacts**

World collisions should eventually generate effects based on information such as:

```
impact velocity
impact direction
surface normal
impact angle
object/material
```

This applies to:

```
projectiles hitting world
physical objects hitting world
player body parts hitting world
future debris/destruction
```

Possible effects include:

```
debris
dust
sparks
shockwaves
impact particles
```

---

# **4\. Replay Desired Behavior**

The exported replay should reproduce the recorded player's experience **tick by tick**.

If the player saw an effect during live gameplay at tick `T`, replaying tick `T` should produce that same effect through the same shared system.

This includes:

```
movement effects
jump effects
air-jump effects
dash effects
landing effects

weapon firing effects
muzzle effects
dynamic lights
tracers
weapon sounds

damage numbers
player-hit effects
world-hit effects

rocket projectiles
rocket smoke
explosions

melee effects

server-disagree effects/text

chat
typing indicators
```

Chat must reproduce the entire chat history/timeline relevant to the replay.

Typing state should also reproduce:

```
typing indicator in top-left chat UI
typing indicator above player's head
```

Chat behavior is defined separately in the MiMITA chat specification and should be reused rather than reimplemented specifically for replay.

## **Predicted Client Events**

The replay represents **what the recorded client experienced**.

Example:

```
Tick 500:
client predicts rocket hit
→ hit effect appears

Tick 506:
server disagrees
→ disagreement/correction effect appears
```

Replay should reproduce:

```
Tick 500:
predicted hit effect

Tick 506:
server-disagree effect
```

Do NOT silently rewrite this into the server's final version of reality.

A future server-perspective replay mode can do that separately.

---

# **5\. Current Known Problems — 09-06-2026**

These are **observations**, not automatically confirmed root causes.

## **Effect Face/Culling Problem**

Observed at approximately 09-06-2026 11:14:

```
OBSERVED:

Some effects appear to have their visible orientation reversed.

The backside can be seen.
The expected front-facing side appears culled/invisible.
The desired behavior appears to require the opposite orientation.
```

Current hypothesis:

```
HYPOTHESIS — UNCONFIRMED

Possible causes include:

- face winding
- normals
- backface culling state
- transform handedness
- effect geometry generation
- renderer state
- another rendering change
```

Do NOT write:

```
ROOT CAUSE:
backface culling
```

until investigation proves that.

The root cause is currently:

```
ROOT CAUSE:
UNKNOWN / UNCONFIRMED
```

Investigation should include relevant recent Git diffs and comparison against the last known-good version.

Once the exact cause is demonstrated:

1. Fix it.  
2. Determine the responsible change/code path.  
3. Document the root cause.  
4. Document why the fix works.  
5. Add an automated regression test.  
6. Record the last-known-good and first-known-bad commits where possible.

## **Effects Missing From Exported Replays**

Currently observed missing or potentially missing:

```
damage numbers
getting-shot/player-hit effects
server-disagree effects/text
dash effects
landing effects
walking/movement effects
world-impact sparks/effects
other gameplay effects
```

Chat replay behavior is currently unconfirmed and must be tested.

Do not assume the culling problem caused the replay problem.

Treat these as separate hypotheses until evidence demonstrates a shared cause.

---

# **6\. Determinism & Tick Rules**

Effect timing must be based on simulation/replay ticks rather than rendered frames.

At the current 60 Hz simulation rate:

```
1 second = 60 ticks
```

Example:

```
effect begins = tick 500
effect lifetime = 3 ticks

visible:
500
501
502

finished:
503
```

A machine rendering at:

```
30 FPS
60 FPS
144 FPS
300 FPS
```

should still observe the same underlying effect timeline.

Rendering frequency may differ.

Simulation/replay state must not.

## **Deterministic Randomness**

Randomized visual effects should receive a deterministic seed.

Conceptually:

```
EffectEvent {
    tick
    type
    position
    direction
    seed
    ...
}
```

Therefore:

```
same event
+ same tick
+ same seed
+ same configuration
=
same generated effect
```

Do not build a giant ECS or engine rewrite solely to accomplish this.

Start with the smallest deterministic event mechanism that works.

---

# **7\. Automatic Effect Tests**

Every important effect should have a small deterministic test/demo scenario.

Conceptually:

```
tests/effects/
    walking
    jump
    air-jump
    dash
    landing

    gun-fire
    player-hit
    world-hit

    rocket-fire
    rocket-trail
    rocket-explosion

    melee-hit
    server-disagree

    chat
    typing-indicator
```

This structure is conceptual. Exact repository paths should follow the existing repository architecture rather than creating unnecessary organization.

Each effect should be tested at three levels.

## **Level 1 — Event Test**

Prove that the correct event happened.

Example:

```
Fire weapon at tick 100

EXPECTED:
GunFire event exists at tick 100.
```

## **Level 2 — State Test**

Prove the effect system created the expected effect state.

Example:

```
GunFire event at tick 100

EXPECTED:
muzzle effect exists
tracer exists
sound restart occurs
dynamic light exists
```

Then replay the same event:

```
EXPECTED:
same effect types
same tick
same position
same direction
same seed
same lifetime
same relevant configuration
```

## **Level 3 — Visual Test**

Render a tiny deterministic known-good scene.

Compare the result against a known-good visual reference or another robust visual assertion.

This is particularly important for bugs such as:

```
front face invisible
back face visible
incorrect winding
wrong effect orientation
missing effect
incorrect color
incorrect position
```

Do not rely exclusively on screenshot comparison.

Event and state tests should catch structural problems even when minor rendering differences make image comparison noisy.

---

# **8\. Regression Logging**

Every regression that can reasonably be understood should become permanent project knowledge.

A regression entry should contain:

```
REGRESSION ID

DATE/TIME

OBSERVED BAD BEHAVIOR

EXPECTED BEHAVIOR

LAST KNOWN GOOD COMMIT

FIRST KNOWN BAD COMMIT

ROOT CAUSE

RESPONSIBLE FILES / CODE PATH

FIX

AUTOMATED TEST ADDED

HOW THE TEST WOULD HAVE CAUGHT IT

PROOF THE FIX WORKS
```

If information is unknown, explicitly write:

```
UNKNOWN
UNCONFIRMED
NOT YET ISOLATED
```

Never invent the answer merely to complete the regression entry.

## **Regression Lifecycle**

```
Bug observed
    ↓
Record observation
    ↓
Reproduce
    ↓
Investigate
    ↓
Find demonstrated root cause
    ↓
Fix
    ↓
Add regression test
    ↓
Verify live behavior
    ↓
Verify replay behavior
    ↓
Document cause + fix
    ↓
Regression permanently protected
```

A hypothesis is not a root cause.

For example:

```
BAD:

ROOT CAUSE:
Backface culling.

GOOD:

OBSERVED:
Front-facing portion invisible while backside visible.

HYPOTHESIS:
Possibly culling/winding/normals.

ROOT CAUSE:
UNCONFIRMED.
```

Once demonstrated, replace `UNCONFIRMED` with the actual cause and evidence.

The goal is to log **as many meaningful regressions as reasonably possible**, especially recurring failures or failures in shared systems.

---

# **9\. No-Regression Change Rule**

A code change is not successful merely because the feature being edited now works.

Before changing shared effects/replay/rendering code:

```
run relevant regression tests
        ↓
establish known-good baseline
        ↓
make change
        ↓
run tests again
```

If:

```
editing gun effect
        ↓
gun effect improves
        ↓
world-hit effect breaks
```

then the gun-effect change is **NOT ACCEPTABLE**.

Do not merge/accept it merely because the requested gun behavior works.

Instead:

```
reject / undo candidate change
        ↓
investigate coupling
        ↓
determine why unrelated system broke
        ↓
simplify/fix architecture or specification
        ↓
implement again
        ↓
all tests pass
```

The desired invariant is:

```
OLD PASSING BEHAVIOR
+
NEW DESIRED BEHAVIOR
=
NEW PASSING VERSION
```

Not:

```
fix A
break B
fix B
break C
fix C
break A
```

If changing one effect repeatedly breaks unrelated effects, treat that as evidence of excessive coupling or an incorrect abstraction.

Fix the underlying architecture rather than normalizing the regression cycle.

An intentional behavior change is allowed only when the specification itself intentionally changes and the associated tests are deliberately updated.

---

# **10\. Definition of Done**

This system is complete when one deterministic gameplay test can produce a sequence such as:

```
Tick 100: walk
Tick 120: jump
Tick 140: air jump
Tick 160: dash
Tick 180: land

Tick 200: fire gun
Tick 201: hit player
Tick 202: damage number

Tick 240: projectile hits world
Tick 241: world impact effect

Tick 300: fire rocket
Tick 301+: rocket + smoke
Tick 340: explosion

Tick 400: melee hit

Tick 500: predicted hit
Tick 506: server disagreement

Tick 600: player starts typing
Tick 620: chat message sent
```

The live game produces the expected experience.

Then export/replay the exact scenario.

The replay must use the same event paths and shared systems and reproduce the player's experience at the corresponding ticks.

For deterministic effects:

```
same event
same tick
same seed
same configuration
        ↓
same visuals
```

All three test layers must pass:

```
EVENT TEST      PASS
STATE TEST      PASS
VISUAL TEST     PASS
```

Relevant existing regression tests must also pass:

```
PREVIOUS EFFECTS    PASS
REPLAY              PASS
RENDERING           PASS
CHAT/INDICATORS     PASS
NEW BEHAVIOR        PASS
```

The current face/culling issue must be:

```
reproduced
investigated
root cause demonstrated
fixed
regression logged
automated test added
verified live
verified in replay
```

The currently missing replay effects must be restored through the **shared effect/event path**, not by creating replay-specific copies.

The long-term rule is:

> **Live gameplay and replay do not have separate effects. They are two consumers of the same recorded events and the same presentation systems.**

And the regression rule is:

> **Every understood failure should make MiMITA harder to break the same way again.**

The desired development loop is therefore:

```
MAKE SOMETHING WORK
        ↓
PROVE IT WORKS
        ↓
AUTOMATE THE PROOF
        ↓
LOG WHAT WAS LEARNED
        ↓
NEVER SILENTLY BREAK THAT BEHAVIOR AGAIN
```

