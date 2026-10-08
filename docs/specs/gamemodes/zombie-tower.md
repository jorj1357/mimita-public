### zomb tower v1 spec  2026 10 08 1303 jorj 

# MiMITA Zombie Tower — Full Specification v1

**Date:** 2026-10-08  
**Status:** DRAFT V1 / AUTHORITATIVE TARGET BEHAVIOR PENDING HUMAN EDIT  
**Mode ID:** `zombie_tower`  
**Primary purpose:** Large-scale cooperative PvE vertical survival mode and proving ground for generalized MiMITA monster, NPC, animation, spawning, map-entity, persistence, pickup, boss, replay, and creator systems.

---

# 1. End goal

Zombie Tower is a cooperative PvE mode where a very small group of players fights through an enormous number of monsters while climbing a large vertical tower.

The basic fantasy is:

```text
you are small

the tower is huge, halloween, old, retro(?), 2000s roblox 2012-2015, based on zombie tower from Nooooooo

there are far too many monsters

your movement is extremely powerful

your weapons are powerful

your teammates matter

you are barely surviving

and somehow you keep climbing
```

Moment-to-moment play should include:

```text
dash
run
air-strafe
wall-jump
rocket-jump
ragdoll-move
shoot
reload
find ammo
find health
find weapons
find teammates
revive teammates
escape swarms
fight bosses
discover secrets
climb
panic
recover
continue upward
```

The intended player reaction after a successful session is approximately:

```text
"That was so fun."
"I hate the flying monster theyre so hard."
"this is challengeing but i can see how i can improve"
"that was brutal but fair"

"I want to play that again."

"I want my friends to play that."

"I have ideas for monsters/maps/modes."

"I want to know how this was made."

"I want to change it."
```

Zombie Tower should not merely be a game mode.

It should demonstrate that MiMITA lets someone:

```text
experience a game
        ↓
inspect how it works
        ↓
trace the systems
        ↓
edit the systems
        ↓
make their own version
```

---

# 2. Inspiration

The high-level inspiration is the old Roblox-style Zombie Tower experience from approximately the early 2010s.
from Noooooooo 
https://www.roblox.com/games/23173663/Zombie-Tower here 
https://www.roblox.com/users/523394/profile/ this person
wow user 523394 theyre oldddd 

MiMITA Zombie Tower should preserve the strongest parts of that fantasy:

- upward progression;
- huge enemy counts;
- surviving with friends;
- frightening/strange monsters;
- simple readable goals;
- strong memorable atmosphere;
- replayability;
- accessible rules.
HUGE REPLAYABILITY
TAHT IS HUGE 

it shouldb e something u can 
mimita as a whole should be somewaht like tf2,
where, even if u are 100% alone not playing with anuone else, u can still ahve fun 
bc of NPC behvaior
as well as being able to edit it adn improve it and make things happen how u want 

But it should expand substantially through:

- much stronger movement;
- better performance (sorry Noooooo);
- much greater monster variety;
- bosses;
- advanced NPC navigation;
- pickups;
- persistent progression;
- checkpoints;
- player revival;
- reusable monster definitions;
- animation;
- future destructibility;
- future modular/infinite generation;
- future creator tooling.

---

# 3. Design principle: power versus pressure

The player should feel extremely powerful.

Zombie Tower MUST NOT achieve difficulty by taking control away from the player.
NO HITSTUN
no fov smaller
no poison effect
no  witch throwing a potion at u from 50 meters away that makes u have slowness II and then u die becaue u USED TO BE ABLe to dash but now u cant
that sucks
dont do that 

The player should retain:

- movement;
- aiming;
- weapon control;
- advanced movement techniques.

The mode should generally avoid:

```text
hit stun
movement lock
arbitrary slowdown
long forced animations
unavoidable instant damage
control removal
```

absoltely avoid long forced animations
and parry
like

mortem metallum in roblox i wou ok 2026 10 08 1308 jorj writing
i would pla that game
i woudl get cheats and i woud still be so mad 
bc 
of the 
parry system
like if u parry someone
u get like 3 full seconds
of just attackin ghtem
over adn over
liek its so 
parry sided
its so frustrating
and think about like real lief like
if you get parried 
and ur in a  knight armor whatever sword fighitng someone
if u slash and they parry 
its like they block ur sword with theirs
adn then can stab thru
but notice:
u are not frozen
for  feraking 3 seocnds
just sitting there waiting 
for them to hit you
its so  s 
Silly :) its so silly :D

The player may be tremendously mobile.

The monsters and environment create difficulty by placing the player under **multiple simultaneous pressures**.

Example:

```text
5 ground zombies approaching
+
3 flying enemies overhead
+
brute controlling a doorway
+
exploding enemy forcing movement
+
teammate downed behind player
+
low ammunition
+
boss still alive
+
lava nearby
```

The challenge is:

```text
What do I deal with first?
```

not:!!!!!!!!!!!!!!!!!!

```text
The game disabled my movement and killed me.
```

---

# 4. Difficulty philosophy

Zombie Tower should be very difficult.

A skilled player who understands all mechanics may still require approximately:

```text
~50 serious attempts
```

maybe even more 
like it should be like ultrakill brutal mode
or 
the hardest one
like 2x harder
i wish taht game was harder 

to complete a difficult/full version for the first time.

This number is a design aspiration, not a hard mathematical requirement.

Difficulty should emerge from:

- simultaneous pressures;
- monster composition;
- positioning;
- resource scarcity;
- team separation;
- mistakes under stress;
- movement errors;
- missed jumps;
- missed shots;
- target-priority errors;
- boss mechanics;
- environment hazards;
- panic;
- failure to revive teammates;
- poor coordination.

Difficulty MUST NOT primarily emerge from:

- invisible unavoidable attacks;
- one boring enemy repeatedly shooting the player;
- enemies having absurd damage without counterplay;
- fake difficulty through enormous health alone;
- perfect aimbot-style hitscan;
- hidden rules;
- unavoidable stun locking.

---

# 5. Readable failure

When a player dies, they should usually be capable of understanding why.

Examples:

```text
"I got separated."

"I ignored the wasps."

"I panicked and dodged into the wall instead of into safety."

"I backed into lava."

"I tried to revive at a bad time."

"I ran out of ammunition."

"I didn't dodge the brute slam."

"I moved too fast into the exploding enemy."

"I got greedy and stayed near the chainsaw boss."

"I missed the jump."
```

A death should preferably teach something.

---

# 6. Group fantasy

Default intended cooperative party size:

```text
4 actors
```

These may be:

```text
4 humans

3 humans + 1 NPC

2 humans + 2 NPCs

1 human + 3 NPCs

0 humans + NPC test configuration
```

The architecture MUST NOT permanently assume exactly four.

Larger experimental groups should remain possible.
like 999 players that would be so sick 
todo later i want to have like having 100,000 human players in 1 server at once that wouldb e so cool 

---

# 7. Group importance

The mode should strongly reward staying coordinated.!!!!!!!!!!!!!!!!!!!
make it obvious too like
cuz i had no clue
when i was a kid
like age 0 to age mabye 15
that if u arent near ur team = u are kinda at a disadvantage 
so stick close to them
with tips on screen 

Four actors moving and fighting together should generally perform much better than four actors scattered across the tower.

Team benefits include:

- combined firepower;
- revive access;
- shared awareness;
- protection while reloading;
- monster aggro distribution;
- easier resource recovery.

NOTHING LIKE 
No special group together
group within 20 meters of each other = sepcial stat boots
just make it liek a natural result of being clsoe together = strogner 

A player who separates from the group may survive through exceptional movement, but separation should be risky.

---

# 8. Large enemy scale

Target full-run monster count may be approximately:

```text
1000 monsters
```

across the entire tower.

This is not a requirement that 1000 are active simultaneously.

The mode should support:

- spawned waves;
- dormant zones;
- distance-triggered activation;
- encounter pools;
- bosses;
- dynamic counts.

The engine should ultimately support large battles where dozens or potentially hundreds of monsters are active.

---

# 9. One complete run

A complete run conceptually follows:

```text
shared lobby
        ↓
Zombie Tower selected
        ↓
30-second intermission
        ↓
players/NPC teammates prepared
        ↓
run begins
        ↓
tower section
        ↓
monster encounter
        ↓
pickup/resource decisions
        ↓
checkpoint
        ↓
more difficult sections
        ↓
boss
        ↓
checkpoint
        ↓
higher tower
        ↓
final boss
        ↓
tower completed
        ↓
persistent rewards
        ↓
return to lobby/intermission
```

---

# 10. Run duration

Target successful run duration:

```text
approximately 45–60 minutes
```

This may vary significantly according to:

- movement skill;
- team skill;
- secrets;
- deaths;
- checkpoints;
- difficulty;
- monster composition;
- route choice.

---

# 11. Tower physical scale

Approximate initial tower concept:

```text
20+ floors
```

with individual large areas potentially around:

```text
~100 m × 100 m
```

and substantial vertical separation.

Some floor sections may reach approximately:

```text
~50 m vertical scale
```

or otherwise provide enough room for MiMITA's fast movement.

The tower should not feel like a narrow hallway.

---

# 12. Nonlinear floor traversal

"Floor" does not necessarily mean:

```text
flat room
→ staircase
→ next flat room
```

Tower sections may require lateral traversal.

Example:

```text
forward section
        ↓
right section
        ↓
another right section
        ↓
incline
        ↓
gaps over lava
        ↓
upper section
```

Players may need to:

- move horizontally;
- move vertically;
- go up then down then up again;
- jump gaps;
- climb;
- ragdoll climb;
- rocket-jump;
- wall-jump;
- fight while traversing.

---

# 13. Movement freedom

Zombie Tower uses a fast MiMITA movement preset based on the normal/source-style movement.

Required capabilities may include:

```text
    dash
    down-dash
    freeze
    jump
    wall jump
    bunny hop
    air strafe
    ragdoll traversal
    rocket jump
    crouch
```

There is intentionally no low artificial speed limit.

---

# 14. No intended movement ceiling

The game should permit extremely high movement skill.

Example:

```text
ordinary player:
moves through intended route

advanced player:
wall-jumps
rocket-jumps
air-strafes
ragdolls
skips sections
```

The mode should not destroy advanced movement merely to force the intended route.

Instead:

```text
world
+
monsters
+
hazards
```

should remain interesting even to highly skilled movement players.

---

# 15. Movement skips are allowed

If a player can legitimately use MiMITA movement mechanics to skip part of a floor:

```text
ALLOW IT
```

unless it completely destroys the mode.

The tower should ideally tolerate:

- shortcuts;
- unusual jumps;
- route experimentation;
- speedrunning.

BUT IF U 
like
ok 
imagine u are so so goated at movement or the game gets so optimized
theres a glitch right now 2026 10 08 1315  
ragdoll mode 
on teh ground
then unragdoll
= like 500 m/s upwarrd momdntum
thats huge for this mode 
so 
like
combo with rocket launcher
u mihgt be able to skip like the entire tower
if u do that, it should be like
arena closer boss spawns for every floor that u skip
it shoudl float and hover
and go thru walls
and touching/intersecting = bigknockback and big damage
maybe like 66 damage per tick and scales with force adn stuff
it has 100,000 hp so u CAN kill it
but it should just mean like
just u have to go thru the mode naturally 
like each checkpoint kill all monsters etc 

---

# 16. Fast movement creates its own risk

Fast movement should also create danger.

Example:

```text
player moving 100 m/s
        ↓
enters monster activation region
        ↓
flies into air-pressure enemy / poison / hazard
        ↓
must react immediately
```

Some enemies should specifically punish blind speed. AS A NATRUAL RESULT OF HOW THEY ARE NOT LIEK SPECIAL PUNISHMENt

This should not mean they arbitrarily stop the player.

They create:

- prediction challenges;
- area denial;
- collision hazards;
- unexpected pressure.

---

# 17. Intermission

Initial run intermission target:

```text
30 seconds
```

The intermission may occur in a generalized shared lobby.
in the lobby u can get like better weapons or upgrades etc idk
liek 
idk whate ven u could od 
maybe spawn with 3 medkits or something
some kinda perk 

The lobby should eventually be reusable by multiple game modes.

---

# 18. Shared lobby philosophy

Waiting should not mean staring at a menu.!!!!!!!!!!!!!!!!!!!!!!!!!!

The generalized MiMITA lobby may allow:

- movement;
- physics experimentation;
- pickups;
- practice;
- interaction;
- conversation.

Zombie Tower should reuse this shared lobby architecture rather than inventing a special isolated waiting screen.

---

# 19. Run start

At the end of intermission:

```text
party prepared
        ↓
run initialized
        ↓
spawn at run start/checkpoint
        ↓
encounters activate
```

Exact countdown presentation remains configurable.

---

# 20. Party wipe

If every active team member becomes fully dead:

```text
PARTY WIPE
```

The game checks:

```text
remaining run attempts
highest checkpoint
```

If attempts remain:

```text
attempt += 1
        ↓
respawn party at highest checkpoint
        ↓
continue run
```

If no attempts remain:

```text
run ends
        ↓
return to intermission/lobby
```

---

# 21. Run attempts

Initial concept:

```text
3 attempts per run
```

Example:

```text
attempt 1
reach checkpoint 4
party wipes

attempt 2
spawn checkpoint 4
reach checkpoint 7
party wipes

attempt 3
spawn checkpoint 7
party wipes

RUN LOST
```

Exact number should be configurable.

---

# 22. Checkpoints

Checkpoints preserve run progress.

A checkpoint should remember at minimum:

- checkpoint ID;
- tower progression;
- party respawn position;
- current run attempt;
- relevant run state.

Potentially preserve:

- collected run weapons;
- current inventory;
- run-specific resources.

Exact reset policy should be configurable.

---

# 23. Checkpoint frequency

Initial checkpoint count is not finalized.

Possible philosophies:

```text
few major checkpoints
```

versus:

```text
checkpoint each meaningful section/floor
```

Recommended v1 direction:

Use checkpoints frequently enough that failure is painful but not equivalent to replaying 45 minutes.

Boss encounters should ALWAYS have a checkpoint immediately before them.

---

# 24. Checkpoint notification

When checkpoint activated:

```text
CHECKPOINT REACHED
```

or equivalent minimal notification appears.

Do not necessarily reveal:

```text
Checkpoint 9 of 12
```

because the tower should preserve uncertainty about its total length.

---

# 25. Future Infinite Zombie Tower compatibility

Tower sections should eventually be modular.

Long-term:

```text
authored chunk
+
connector rules
+
difficulty metadata
+
monster pools
        ↓
procedural selection
        ↓
Infinite Zombie Tower
```

Zombie Tower v1 may be fully authored.

However, structures should avoid assuming:

```text
floor 17 always exists
```

when generalized section identifiers are possible.

---

# 26. Player starting loadout

Initial player starting weapons:

```text
Revolver
Shotgun
```

Additional weapons can be acquired through:

- pickups;
- persistent unlocks;
- gold purchases;
- rewards.

---

# 27. Weapon discovery

Weapons may physically appear in tower areas.

Possible interaction:

```text
walk over
```

or:

```text
look at item
+
press F
```

Interaction system should be generalized.
editor so u can go in the tower and put speicifc spots of weapon pickups like while ur goign thru the tower
we should do that
as well as  monster spawn locations etc 

---

# 28. Health pickups

Health pickups appear as readable/glowing tower objects or areas.

Interaction options:

```text
touch pickup
```

or:

```text
look + F
```

Health should restore according to pickup configuration.

---

# 29. Ammo pickups

Ammo pickups similarly restore:

- general ammunition;
- weapon-specific ammunition;

according to configuration.

---

# 30. Pickup primitive

A generalized pickup should be data-driven.

Conceptually:

```json
{
  "id": "health_large",
  "type": "pickup",
  "interaction": "touch_or_interact",
  "effect": {
    "health": 50
  },
  "respawn": false
}
```

Other pickup types can reuse the same primitive.

---

# 31. General pickup applications

General pickup system should support:

```text
health
ammo
weapon
currency
key
quest object
temporary buff
collectible
```

Zombie Tower should not need separate C++ pickup logic for each item.

---

# 32. Persistent gold

Monsters may award gold.

Gold should persist across sessions.

If a user:

```text
earns 10,000 gold
buys minigun
stops playing for 100 years
returns
```

the expected design goal is:

```text
minigun still unlocked
gold state still valid
```

subject to future persistence architecture.

---

# 33. Persistent XP

Monster kills and tower completion may award XP.

XP should persist similarly.

Potential uses:

- level display;
- unlock requirements;
- achievements;
- progression statistics.

XP must not become mandatory artificial grinding if it harms the game.

---

# 34. Persistent unlocks

Persistent rewards may include:

- weapons;
- cosmetics;
- achievements;
- titles;
- special items;
- avatar elements.

Completing meaningful challenges should unlock meaningful artifacts.

---

# 35. Cross-mode rewards

A major long-term principle:

```text
achievement in Zombie Tower
may unlock something usable elsewhere in MiMITA
```

Example:

```text
beat Zombie Tower
        ↓
unlock special cosmetic
        ↓
use cosmetic in Juggernaut / Duels / Sandbox
```

This creates a connected MiMITA world rather than isolated modes.

---

# 36. No aggressive monetization requirement

Zombie Tower progression should not require paid cosmetics or paid power.
nothing in mimita should require that
nothign shoudl be  made with a priority of money as #1
meaning
thres no  Get +9 walkspeed for 499 robux! New sale for tralalero tralala brainrot for only 799 robux! 
the prioirty is 
joy and fun and  excitement and happiness 
from this game 
i think ideally also  acting from a aplce with a expectation that this will not make money
im just doing this  bc i like to do it 
doing it for the love of the game in  slang terms
if u do things like that 
and the intention is  i just wnat this to exist bc its fun and it amkes me happy 
and u would be ok if it made 0 dollars ever 
then i think that is prett good
bc it wont make 0 dollars ever its ver ver unlikely
but money i beleive as of right now  2026 10 08 1321 jorj writing  its emergent from value
the same as  leaves emerge from branches emerge from trunk emerge from roots emerger from seeds

focus on the seed and strong roots e.g.
its fun consistnetnly its  makes me joy it makes me happy its strtong its consistent 
the leaves and branches will naturally emerge from a strong root/foundation
at least i believe right now

also i idk how to explain but like commercial advertisemnets its ughhh
selling u things
dont sell
show me something as if i am a friend and you want me to  have a better life as a result fo this thing u are showing me
e.g. ultrakill is a good  example
i havent seen 1 ultrakill ad
i saw jerma play it
i saw m friends talking abou tit
for years
then my other friend recommends me i t
and then now life has changed where  i would benefit bc 1. its fun and 2. helps me  see the lineage and movement philosoph of another game that is fun for people 
adn then i purchase it
if the goal was money then  ok cool
but the goal  i beleive was not having money at its #1 priroity
it seems that it was 
i like this thing and i want to share it with others 

Rewards can simply be rewards for playing.

Example:

```text
beat tower 10 times
→ special weapon

find secret
→ cosmetic

beat boss challenge
→ title
```

The existence of development effort does not automatically imply every reward must cost money.

---

# 37. Egg-hunt philosophy

Collectibles should evoke:

```text
"I want to find this because it is fun/memorable"
```

rather than:

```text
"I must log in today because a system threatens to take something away."
```

Future seasonal or permanent collectible hunts should be:

- exploratory;
- playful;
- discoverable;
- ideally replayable/version-preservable.

todo explain better  enshittification ? 

frmo wikipedia as of 2026 10 08 1324
Enshittification, also known as platform decay, is a process in which two-sided online products and services decline in quality over time. Initially, vendors create high-quality offerings to attract users. Over time, they degrade those offerings to better serve business customers. Ultimately, they degrade their services to both users and business customers to maximise short-term profits for shareholders.

as wella s this 

Here is how platforms die: first, they are good to their users; then they abuse their users to make things better for their business customers; finally, they abuse those business customers to claw back all the value for themselves. Then, they die. I call this enshittification, and it is a seemingly inevitable consequence arising from the combination of the ease of changing how a platform allocates value, combined with the nature of a "two-sided market", where a platform sits between buyers and sellers, hold each hostage to the other, raking off an ever-larger share of the value that passes between them.

so how do we not do that? 
some guesses/potential solutions
dont go public
mimita not public
no shareholders who want profits now
bc this not  gonna do profits now its gonna dao profits whenever  it feels like honestly
i can tell u  its gonna do 
fun right now
i can do fun right now 

also 
communicate with  users
if general sentiment is like mannn its so coproarate i sodn tlike how it changed etc
not good
cant do that 
give power to the users
wont even  be  a platform bc if u don tlike how its going then u can make ur own version that is how u want it to be 

---

# 38. Monster system goal

A monster should be primarily defined through data.

The same generalized actor systems should support:

```text
normal zombie
crawler
runner
brute
wasp
rolling spike ball
mimic
boss
```

without creating completely unrelated implementations.

---

# 39. Required monster definition

A monster definition should support at minimum:

```text
ID
display name
model/avatar
scale
body geometry
health
locomotion type
movement preset
navigation capabilities
behavior preset
animation set
sound set
attacks
drop table
special abilities
spawn properties
```

---

# 40. Optional monster properties

Possible optional fields:

```text
voice/personality
boss configuration
rare spawn chance
visual variations
pitch variation
color/material variation
loot overrides
special movement abilities
```

Damage resistances are not a v1 requirement.

---

# 41. Monsters should use generalized actor systems

Monster execution should reuse:

- actor health;
- actor movement;
- shared collision;
- weapon/attack systems;
- damage;
- death;
- ragdoll;
- replication;
- navigation;
- animations.

Do not create an unrelated:

```text
ZombieTowerMonsterPhysics
```

if generalized actor physics can solve it.

---

# 42. Initial body architecture

For v1, monsters may map onto the existing humanoid body convention:

```text
plrOrigin
head
torso
leftArm
rightArm
leftLeg
rightLeg
```

This is intentionally limited.

It allows v1 to ship without first implementing arbitrary skeleton topology.

---

# 43. Future generalized body definitions

Later MiMITA must support arbitrary body structures.

Examples:

```text
wasp with wings
six-armed boss
head-only enemy
spider
worm
many-legged monster
mechanical creature
```

Future actor body definitions should permit arbitrary named parts.

V1 does not require this.

---

# 44. Wasp v1 body workaround

A wasp may initially map major functional body parts onto humanoid slots.

Additional visual structures like wings may:

- attach as model geometry;
- be driven by animation;
- not necessarily require independent gameplay body parts in v1.

---

# 45. Monster scale

Monster scale may vary dramatically.

Examples:

```text
crawler
normal zombie
brute
giant brute
boss
```

The actor/avatar system must not assume all bodies share identical dimensions.

Collision/nav settings should derive from monster configuration.

---

# 46. Avatar Editor relationship

Monster authoring should reuse and improve the Avatar Editor where practical.

Useful capabilities include:

- body scale;
- limb positioning;
- model assignment;
- avatar preview;
- collision visualization;
- camera distance/FOV controls.

Monster requirements can act as forcing functions for general Avatar Editor improvements.

---

# 47. Monster locomotion categories

Primary locomotion families:

```text
GROUND
AIR
ROLLING
```

Future:

```text
TELEPORT
CLIMBING
SPECIAL
```

---

# 48. Ground locomotion

Ground locomotion includes:

```text
walking
running
crawling
```

These should primarily differ through:

- speed;
- body pose;
- movement preset;
- animation;
- clearance/collision.

They should reuse the same generalized ground navigation where possible.

---

# 49. Air locomotion

Air locomotion includes:

```text
flying
hovering
floating
```

These may share a generalized aerial navigation architecture.

They can differ through:

- preferred altitude;
- acceleration;
- vertical freedom;
- orbiting behavior;
- attack range.

---

# 50. Rolling locomotion

Rolling actors such as Griever require:

- physical rolling representation;
- goal direction;
- bounce/recovery;
- anti-corner-stuck behavior.

The actor should remain physically simulated.

Navigation provides target direction/path intent.

Physics determines actual rolling.

---

# 51. Teleport locomotion

Teleporting monsters are a future possibility.

Example:

```text
monster predicts player
→ disappears
→ reappears behind/near player
```

Teleporting should be readable and avoid unavoidable damage.

Not required for Zombie Tower v1.

---

# 52. Monster navigation target quality

A monster several floors below the player should ideally still be capable of finding a meaningful route upward when traversal permits.

Example:

```text
player AFK 5 floors above
normal monster below
        ↓
pathfind through tower
        ↓
eventually reach player
```

This is an aspirational navigation benchmark.

Flying monsters should especially be capable of pursuing players across significant tower distance.

---

# 53. Hiding should not trivialize the mode

Players should not generally be able to hide indefinitely and wait for danger to disappear.

If a player retreats and hides:

- monsters may continue searching;
- flying monsters may pursue;
- navigation may bring enemies to them;
- objectives/resources may force movement.

The mode rewards bravery and engagement.

---

# 54. Monster behavior does not require identical intelligence

Not every monster should behave like a Counter-Strike bot.

Examples:

```text
normal zombie:
simple relentless pursuit

mimic:
deceptive

wasp:
air pursuit

brute:
space control

boss:
complex attack selection
```

All may reuse generalized NPC primitives while having distinct behavior policies.

---

# 55. Reuse Counter-Strike NPC infrastructure

Zombie Tower should reuse useful generalized NPC capabilities developed for Counter-Strike:

- navigation;
- target selection;
- ActorIntent;
- movement execution;
- memory where useful;
- team/hostility rules;
- weapon use;
- shared physics.

Counter-Strike-specific tactical semantics should not be forced onto zombies.

---

# 56. Monster attacks as generalized weapons

Monster attacks should reuse generalized attack/weapon infrastructure where possible.

Examples:

```text
claw
bite
chainsaw
charge
acid spit
shockwave
explosion
```

should be modeled as generalized attacks rather than unique hardcoded functions.

---

# 57. Contact-based melee philosophy

Zombie Tower melee should support persistent physical damaging geometry.

Example:

```text
zombie claw attached to right arm
        ↓
claw physically intersects player
        ↓
damage/contact response
```

This differs from a fighting-game-only startup/active/recovery hitbox model.

---

# 58. Contact attack

A contact attack may define:

```text
damage per contact/tick
cooldown per victim
force scaling
source body/weapon
knockback
```

we already have  this behavior like spyknife
we jut do that again 
use that same logic but for claws or zombie arms/zombie body in general 

It must prevent accidental absurd damage due solely to repeated solver contacts unless that behavior is intentionally configured.

---

# 59. Relative impact speed

Future melee/contact damage may consider relative impact velocity.

Example:

```text
zombie claw stationary
player hits claw at 100 m/s
```

This may produce stronger impact than a gentle overlap.

However:

**V1 should clamp this strongly.**

Extremely fast player movement must not produce inexplicable instant death from one tiny claw contact.

Suggested conceptual model:

```text
damage =
base damage
+
clamped impact contribution
```

rather than:

```text
damage = unlimited kinetic velocity damage
```

---

# 60. Attack types

Generic monster attack system should eventually support:

```text
contact melee
melee swing
bite
charge
hitscan
projectile
lobbed projectile
beam
explosion
shockwave
damage volume
poison
grab
knockback
pull
minion spawn
thrown physics object
teleport attack
```

V1 only needs a subset.

---

# 61. Attack phases

Monster attack definitions may optionally support:

```text
startup
active
recovery
```

measured in simulation ticks.

This is optional for basic Zombie Tower enemies.

Simple contact monsters may use continuously active physical weapons.

Bosses may benefit from explicit phases.

---

# 62. Telegraphing

Powerful attacks should normally be readable.

Telegraphing may include:

- animation;
- sound;
- flashing;
- movement;
- charging;
- body pose;
- world effect.

More dangerous attack:

```text
more readable warning
```

should generally hold.

---

# 63. Animation system goal

Monster animations should be authorable in Blender.

MiMITA imports/converts them into a data-driven runtime representation.

Named clips may include:

```text
idle
walk
run
crawl
fly
hover
attack_1
attack_2
hurt
spawn
death
boss_attack
```

---

# 64. Animation files

Animation sets should be stored outside behavior code.

Possible layout:

```text
config/animations/
    zombie_normal.json
    brute.json
    mutant_wasp.json
    chainsaw_boss.json
```

Exact path may follow existing MiMITA animation ownership.

Behavior presets reference animation IDs.

---

# 65. Tick-based animation

Gameplay-facing animation should correspond to the fixed simulation.

Example authored loop:

```text
tick 0
tick 15
tick 30
tick 45
tick 60 / loop
```

The engine smoothly interpolates between poses.

Animation should appear continuous and smooth even though gameplay uses fixed ticks.

---

# 66. Blender keyframes

Blender authors should not be forced to manually provide exactly four keyframes if standard Blender animation import can preserve intent.

The runtime may:

```text
import clip
→ sample/convert to MiMITA tick representation
```

The important invariant is that gameplay timing is tick-addressable.

---

# 67. Animation retrigger

Animations may be retriggered.

Example:

```text
attack starts tick 1
tick 37 brain requests same attack again
        ↓
attack clip restarts from beginning
```

Associated sound may also retrigger.

---

# 68. Animation cancellation

General MiMITA animation architecture should eventually support canceling one animation into another.

This is useful for:

- Super Smash Bros.-style modes;
- advanced NPC movement;
- responsive actors.

Zombie Tower v1 does not require complicated cancel windows for every monster.

---

# 69. Animation / attack attachment

Physical attack geometry may attach to animated parts.

Example:

```text
rightArm
    ↓
claw model/weapon
    ↓
attack animation moves arm
    ↓
physical claw moves through world
```

Collision/damage should correspond to what players visually see.

---

# 70. Texture pipeline requirement

Critical prerequisite:

```text
what is visible in Blender
≈
what appears in MiMITA
```

The import pipeline should preserve:

- UV mapping;
- materials;
- textures;
- model assignment.

---

# 71. Blender fidelity

If a monster is textured in Blender:

```text
export GLB
        ↓
MiMITA
```

should preserve intended appearance without requiring manual recreation of every material.

This is important for both monsters and maps.

---

# 72. Monster variation

Monsters should support inexpensive visual/audio variation.

Possible variation:

- texture variant;
- body scale;
- pitch;
- small color/material change;
- sound choice;
- animation timing;
- behavior tuning.

This helps large swarms avoid appearing cloned.

---

# 73. Monster sound sets

Each monster may have named sounds:

```text
spawn
idle
alert
attack
hurt
death
special
```

A sound set may contain multiple samples.

---

# 74. Sound variation

Monster sound may vary through:

- sample selection;
- pitch;
- timing;
- retrigger;
- volume.

Large crowds should not sound like one identical WAV starting simultaneously 40 times.

---

# 75. Sound retrigger behavior

MiMITA should support efficient sound retrigger.

For appropriate monster sounds:

```text
same sound source
→ restart/retrigger sample
```

rather than unbounded overlapping audio instances.

---

# 76. Spawn sound

NPC/monster spawn events should support a specific spawn sound.

This is a generalized actor feature.

---

# 77. Death sound ownership

Once an actor is dead:

```text
movement/footstep sound MUST cease
```

The existing issue where a dead actor's previous location continues emitting walking audio is a shared engine bug and should be fixed.

Zombie Tower depends heavily on dense audio and will make this bug much worse.

---

# 78. Initial monster roster

Potential full roster:

```text
Normal Zombie
Crawler
Runner
Brute
Stabber
Mutant Wasp
Griever
Mimic
Exploding Head
Hovering Angel
Minion Spawner
Giant Brute
Chainsaw Boss
future additional bosses
Final Boss
```

V1 does not require every idea to survive playtesting.

---

# 79. Required v1 monster variety

Zombie Tower v1 should prove at least:

```text
ground walker
fast ground attacker
heavy ground pressure
rolling/physics monster
flying monster
special/exploding monster
boss
```

Minimum useful candidate set:

```text
Normal Zombie
Brute
Griever
Mutant Wasp
Stabber or Runner
Exploding Head
Chainsaw Boss
```

A Hovering Angel and Giant Brute are strong additional candidates.

---

# 80. Normal Zombie

Role:

```text
baseline ground pressure
```

Properties:

- humanoid;
- simple pursuit;
- melee/contact attack;
- individually weak;
- dangerous in groups.

Design goal:

```text
100 normal zombies
```

should create large-scale swarm pressure without each one requiring sophisticated unique behavior.

---

# 81. Crawler

Inspiration:

low-to-ground horror creature such as Silent Hill-style crawler.

Role:

```text
low visibility / ankle pressure
```

Properties:

- low body;
- difficult to notice in a crowd;
- weaker;
- attacks player legs/lower body;
- may move relatively quickly.

Its role is not enormous damage.

Its role is:

```text
make the floor dangerous
```

---

# 82. Runner

Role:

```text
fast ground pursuit
```

Properties:

- similar to normal zombie;
- substantially faster;
- scary when closing distance;
- moderate health.

Counterplay:

- movement;
- target prioritization;
- shooting before it reaches player.

---

# 83. Stabber

Role:

```text
fast melee pressure
```

Design:

- fast movement;
- arm/blade-oriented attack;
- aggressively tries to reach stabbing distance.

The Stabber should visibly communicate:

```text
"Do not let this thing reach me."
```

---

# 84. Brute

Role:

```text
heavy displacement / space control
```

Properties:

- larger;
- taller;
- slower than standard zombie;
- high health;
- strong melee;
- high knockback.

Possible attacks:

```text
large punch
ground slam
```

---

# 85. Brute ground slam

Ground slam may create:

```text
shockwave
```

Actors hit may receive:

- vertical impulse;
- horizontal impulse;
- damage.

The slam affects both players and potentially monsters where configured.

Purpose:

```text
break safe clustering
```

and create chaos.

---

# 86. Giant Brute

Role:

```text
elite heavy ground pressure
```

Larger version of Brute with:

- more health;
- larger attack radius;
- stronger knockback;
- more readable/slower attacks.

It may function as a miniboss without requiring the full boss framework.

---

# 87. Mutant Wasp

Role:

```text
air pressure
```

Properties:

- flying;
- fast;
- aggressive;
- melee/stabbing style.

The Wasp prevents players from solving the mode entirely by:

```text
stay above ground enemies
```

---

# 88. Hovering Angel

Role:

```text
air support / ranged pressure
```

Possible behaviors:

- hover above battlefield;
- ranged projectile;
- debuff;
- area denial;
- support other monsters.

Exact behavior is not finalized.

---

# 89. Griever

Appearance:

```text
rolling ball with spikes
```

Role:

```text
physics disruption / rolling hazard
```

Behavior:

- selects target;
- rolls toward target;
- physical momentum;
- bounces;
- avoids being permanently stuck in corners.

Touching spikes causes damage.

---

# 90. Griever navigation

Navigation provides:

```text
desired route/target direction
```

Rolling physics provides:

```text
actual trajectory
```

When collision redirects it:

```text
reassess
→ continue toward target
```

Do not directly teleport or force position toward target.

---

# 91. Exploding Head

Role:

```text
mobile timed area denial
```

Behavior:

- approaches player;
- internal explosion timer selected from a range;
- timer is not directly shown;
- increasingly obvious warning near explosion.

Possible timer:

```text
5–15 seconds
```

or configurable equivalent.

---

# 92. Exploding Head telegraph

Player should not know exact remaining time.

Instead:

```text
slow warning
        ↓
faster warning
        ↓
rapid flashing/sound
        ↓
explosion
```

The player reads the creature rather than HUD seconds.

---

# 93. Mimic

Role:

```text
social deception
```

Mimics should be relatively uncommon.

They may initially be relatively weak.

When a Mimic kills a player:

```text
Mimic adopts victim avatar/appearance
```

The kill may intentionally avoid normal obvious killfeed disclosure.

---

# 94. Mimic communication fantasy

Potential scenario:

```text
Player A dies to Mimic

Mimic becomes Player A visually

Player B sees "Player A"

real Player A tells teammates:
"I got killed by a mimic."
```

This creates social horror.

Exact networking/nameplate rules require later specification.

---

# 95. Minion Spawner

Role:

```text
priority/support target
```

Behavior:

- large;
- slow;
- periodically creates smaller enemies;
- becomes increasingly dangerous if ignored.

This punishes:

```text
only shoot nearest enemy
```

and teaches target prioritization.

---

# 96. Enemy role taxonomy

Encounter designers should be able to classify monsters conceptually:

```text
ground_pressure
fast_pressure
air_pressure
displacement
area_denial
support
spawner
deception
explosive
elite
boss
```

This metadata may assist encounter generation.

---

# 97. Encounter composition

Difficulty should often come from composition.

Example:

```text
20 normal zombies
+
4 crawlers
+
3 wasps
+
1 brute
```

creates more interesting pressure than:

```text
50 identical rifle NPCs
```

---

# 98. Encounter director

Future generalized encounter logic may select monster groups by budget.

Example conceptual data:

```json
{
  "budget": 100,
  "roles": {
    "ground_pressure": 40,
    "air_pressure": 20,
    "displacement": 20,
    "special": 20
  }
}
```

V1 may use authored spawn zones instead.

---

# 99. Spawn volumes

Monster spawning is controlled through generalized spawn volumes/entities.

A spawn volume should define:

```text
ID
shape
position
size
activation radius
enemy pool
spawn count
max alive
spawn cooldown
one-shot/repeatable
difficulty
checkpoint requirement
boss flag
```

---

# 100. Spawn volume geometry

Initial supported volume:

```text
box
```

with an activation sphere/radius around the volume center.

Any legal point inside the box may be selected as a spawn position.

Future:

```text
sphere
polygonal volume
surface spawn
ceiling spawn
```

---

# 101. Distance activation

Core Zombie Tower behavior:

```text
player approaches within activation_distance
        ↓
spawn zone activates
        ↓
configured monsters spawn
```

This keeps the tower from simulating every monster from the beginning.

also if u go up higher and u dont kill all the monsters that spawned i think it shoudl be like an alert
but if u ahve to go searching for the monsteer bc its stuck that is lame
so 
should have like
ESP sometimes
if a monster hasnt been killed after lik 30 sec then someitmes a ESP visibility
or matbe a auto death?
or idk
just dont simulat someone whos alive some monster on floor 3 when ur all on floor 18 
just  like maybe kill or something 
or 
if u get 75% of the floor killed hten ur cool 

---

# 102. Spawn ownership

The authoritative server owns monster spawning.

Clients receive replicated spawn events/state.

Do not allow each client to independently generate different monsters.

---

# 103. Spawn placement validation

Before spawning:

- position must be valid;
- body must not intersect solid geometry;
- spawn should belong to intended volume;
- floor/air requirements must match locomotion.

---

# 104. Visible spawning

V1 may simply spawn enemies into existence visibly.

Later presentation may include:

- crawling from ground;
- dropping from ceiling;
- emerging from doors;
- materializing;
- flying into arena.

Presentation must not block basic spawning architecture.

---

# 105. Ceiling enemies

Future monster/spawn definitions may support:

```text
surface = ceiling
```

for spiders or other climbing creatures.

Not required in v1.

---

# 106. Spawn pool example

```json
{
  "id": "floor_04_main",
  "type": "monster_spawn_volume",
  "activation_radius": 20,
  "max_alive": 40,
  "count": 60,
  "pool": [
    {"monster": "normal_zombie", "weight": 60},
    {"monster": "runner", "weight": 20},
    {"monster": "crawler", "weight": 20}
  ]
}
```

Exact schema should align with MiMITA config conventions.

---

# 107. Downed state

When player health reaches:

```text
0
```

during Zombie Tower, the player does not immediately disappear.

They enter:

```text
DOWNED
```

state.

---

# 108. Downed duration

Initial target:

```text
30 seconds
```

If not revived before timer expires:

```text
fully dead
```

measured in ticks too
so
like
if 30 sec timer
then 
15 sec left = 50 hp
7.5 sec left = 25 hp
3.75 sec left (?) = 12.5 hp
etc

---

# 109. Downed movement preset

A downed player uses a dedicated role/movement preset.

Initial concept:

```text
ragdoll-focused movement
```

They are significantly less mobile than active players.

---

# 110. Monster behavior toward downed players

Default:

```text
monsters ignore downed players
```

This prevents monsters from endlessly attacking bodies and makes revival gameplay clearer.

Some special future monster may intentionally interact with downed players, but this must be explicit.

---

# 111. Revival

A living teammate may approach a downed player and:

```text
hold F
```

for approximately:

```text
5 seconds
```

to revive.
revive time  editabel too like can b e 5 sec  10 sec  100 sec 1 sec etc 
measuerd in ticks too 
so not just secodns 

WHILE REVIVING: HEALTH DOESNT DRAIN 
IT SHOUDLB E LIKE
MORE TIME HEALING = MORE HEALTH 
THEN WHEN U HAVE 100 HP THEN U ARE FULL REVIVED 
so

reviving someone who has 90hp = not even that much time to revive
reviving someone who has 10hp = a lot of time to revive 

---

# 112. Revival interruption

Revival stops if:

- reviver leaves range;
- interaction released;
- reviver becomes downed/dead;
- target expires;
- relevant state becomes invalid.

---

# 113. Revived health

Initial target:

```text
100 health
```

upon successful revival.

This is configurable.

---

# 114. Full team down

If all remaining active team members are down:

```text
party considered wiped
```

The game does not require waiting the full down timers.

Then run-attempt logic executes.
so if u have more attemtps left then use those, else, back to intermisison with that run's summary shown  like stats 

---

# 115. Boss definition

A boss is a specialized monster with:

- large health;
- explicit encounter identity;
- boss HUD;
- arena/progression lock;
- unique music/presentation;
- distinct attack/movement behavior.

---

# 116. Boss health bar

All clients in the encounter see:

```text
BOSS NAME
[======================]
```

with authoritative health.

Boss identity should be obvious.

---

# 117. No hidden health phase rule

Preferred boss philosophy:

```text
the visible health bar represents the actual fight
```

Do not frequently use:

```text
boss reaches 0
Trolled theres actuallt a  SECOND FULL HEALTH BAR
```

If boss has 20,000 HP:

```text
show 20,000 HP worth of bar from the beginning
```

Attack behavior may evolve with health if desired, but health should not deceptively reset.

---

# 118. Boss progression lock

Boss floor prevents further meaningful progress until boss dies.

Possible mechanisms:

```text
exit locked
objective blocked
next checkpoint unavailable
```

---

# 119. Boss checkpoint rule

Every major boss has a checkpoint immediately before the fight.

This prevents repeated long traversal before learning a difficult boss.

---

# 120. Boss music

Boss encounters may change:

- music;
- ambient layer;
- sound cues.

Music change should come from generalized music/event systems.

TODO WE NEED A MUSUCI MIDI PLAERS C:\mimita-v9\docs\specs\music-midi\music-midi.md  

this doc not super good right now  2026 10 08 1336 

---

# 121. Chainsaw Boss

Initial boss:

```text
Chainsaw Zombie
```

Approximate health:

```text
~5000 HP
```

subject to tuning.

---

# 122. Chainsaw Boss movement identity

The boss is:

```text
very fast
very high top speed
low friction
poor stopping ability
```

Example:

in the same amount of time, e.g. 1 second 
```text
player reaches ~100 m/s
boss may reach ~300 m/s
```

Values are conceptual and must be tuned.

---

# 123. Chainsaw Boss core attack

Primary behavior:

```text
charge toward player
+
chainsaw contact damage
```

The player survives through:

- sidestepping;
- predicting momentum;
- using geometry;
- maintaining distance.

---

# 124. Chainsaw Boss low-friction weakness

The boss's strength is also its weakness.

When committed to a direction:

```text
hard to stop
```

Player can exploit:

```text
sidestep
→ boss overshoots
```

The boss may bounce/redirect after hitting walls.

---

# 125. Chainsaw Boss air control

Boss may possess limited air strafing.

Therefore:

```text
barely sidestepping
```

may not always be sufficient.

The player must fully evade the attack trajectory.

---

# 126. Chainsaw Boss close-range emergency attack

If player remains excessively close:

```text
boss may use rare high-acceleration dash
```

This prevents trivial circle hugging.

Requirements:

- clearly telegraphed;
- cooldown;
- not constantly available.

---

# 127. Chainsaw Boss resource pressure

The fight should also pressure ammunition.

Players may eventually:

```text
run low/out of ammo
```

forcing:

- pickups;
- melee;
- riskier engagement.

This increases fear without removing control.

---

# 128. Additional bosses

Zombie Tower v1 should ideally contain:

```text
Chainsaw Boss
Boss 2
Boss 3
Final Boss
```

Exact Boss 2/3 designs remain open.

Boss encounters should be among the mode's strongest memorable moments.

some ideas: huge huge brute
like brute is big
huge brute is bigger
ultra brute is biggest
maybe early stage boss

then chainsaw boss
then the boss right before the final boss 
maybeeeeeeee 
idk ranged somehow 
crazy angel frmo infinite dungeon slayer roblox 

---

# 129. Final boss

The final boss should feel like an amalgamation of mechanics learned throughout the tower.

Potential capabilities:

- melee;
- ranged;
- air pressure;
- minion spawning;
- large physics attacks;
- throwing blocks;
- arm-mounted weapons;
- area denial.

Do not implement every possible mechanic merely for complexity.

The final boss should combine understandable learned ideas.

---

# 130. Final boss destructibility

Future version may use destructible-world mechanics heavily.

V1 does not require destructible world.

The final boss must not block v1 release waiting for destruction technology.

---

# 131. World hazard system

Tower hazards should use generalized map entities/volumes.

Examples:

```text
lava
poison
fire
falling floor
crusher
moving platform
breakable wall
explosive barrel
```

V1 only requires a subset.

---

# 132. Required v1 hazards

Minimum:

```text
lava
poison/damage area
```

Potential useful extra:

```text
explosive object
```

---

# 133. Damage volume

Generic map primitive:

```text
DamageVolume
```

Fields may include:

```text
shape
position
size
damage
damage interval ticks
damage type
team filtering
effects
```

---

# 134. Lava

Example:

```text
every 5 ticks inside lava
→ 5 damage
```

Exact values remain configurable.

Lava should be visually obvious.

---

# 135. Poison pool

Poison is the same generalized damage-volume concept with different:

- effect;
- damage;
- presentation;
- potentially status behavior.

Do not implement separate unrelated hazard pipelines.

---

# 136. Falling floors

Future map entity:

```text
falling_floor = true
```

An authored world object may have behavior metadata.

Not required for v1.

---

# 137. Moving platforms

Future generalized world primitive.

Not required for v1.

---

# 138. Breakable walls

Future destructible-world integration.

Not required for v1.

---

# 139. Map configuration

Every Zombie Tower map should have corresponding map configuration data.

Example:

```text
config/maps/zombie_tower.json
```

Data may include:

- spawn points;
- monster spawn volumes;
- checkpoints;
- damage volumes;
- boss triggers;
- pickups;
- objective metadata.

---

# 140. Static geometry versus gameplay metadata

Initial workflow:

```text
Blender:
static geometry
textures
UVs
models

MiMITA map JSON:
spawn points
checkpoints
triggers
hazards
pickups
monster zones
boss triggers
```

This reduces Blender round trips.

---

# 141. Map Editor long-term direction

Eventually MiMITA should edit both geometry and gameplay entities in-engine.

Zombie Tower v1 does NOT require full in-engine geometry modeling.

---

# 142. Entity Editor v0

Initial authoring can be command-driven.

Required concepts:

```text
spawnpoint_add
checkpoint_add
damage_volume_add
monster_zone_add
boss_trigger_add
pickup_add
```

Exact command naming should follow terminal conventions.

---

# 143. Look-to-place workflow

Preferred early workflow:

```text
enter editor/debug mode
look at desired location
execute command
        ↓
entity created at looked-at point
        ↓
map JSON updated
```

---

# 144. Hot reload

Changing map entity configuration should hot reload where safe.

Example:

```text
change monster zone radius
save JSON
        ↓
runtime updates
```

Invalid edits retain last valid configuration.

---

# 145. Future Entity Editor

Later:

```text
freecam
select entity
move
rotate
scale
edit properties
save
```

Mouse unlocking and gizmos can come later.

V1 can use terminal commands + JSON.

---

# 146. Blender round-trip metadata

Long-term, map object metadata should survive:

```text
MiMITA
↔
Blender
```

Potential through:

- GLTF extras/custom properties;
- map-side stable IDs;
- export metadata.

Example:

```text
object:
falling_floor = true
```

should not require losing identity during export/import.

---

# 147. Ambient audio

A silent tower feels unfinished.

Every area should support ambient sound.

Examples:

- rain;
- wind;
- electrical hum;
- machinery;
- distant monsters;
- environmental noise.

---

# 148. Music importance

Music is a major emotional component.

Zombie Tower should aim to create strong long-term memories associated with:

- specific areas;
- specific bosses;
- tower progression;
- victory.

---

# 149. Shared/default music

By default, players may hear the same intended soundtrack.

Users remain free to:

- mute music;
- use personal music.

Gameplay does not depend on music playback.

---

# 150. Future in-engine music authoring

C:\mimita-v9\docs\specs\music-midi\music-midi.md  this need expansion as of 2026 10 08 1339 todo  jorj 

Long-term possibility:

```text
MiMITA music/tracker/MIDI system
```

allowing songs to be authored from reusable instrument samples.

Potential benefits:

- tiny files;
- modifiable composition;
- procedural layering;
- inspectable music source;
- dynamic music.

This is NOT required for Zombie Tower v1.

Existing audio files may be used first.

---

# 151. MIDI possibility

Future soundtrack storage may include:

```text
.mid
```

plus soundfont/sample definitions.

This should be a separate audio/music architecture project.

Do not block Zombie Tower on it.

---

# 152. Dynamic music layers

Future:

```text
base ambience
+
combat layer
+
boss layer
+
critical-health layer
```

may be mixed procedurally.

Again: later.

---

# 153. HUD philosophy

Default HUD should remain relatively minimal.

Normal view may show:

```text
health
ammo
boss bar when applicable
temporary checkpoint message
```

---

# 154. No constant floor number

The HUD should not necessarily reveal:

```text
Floor 17 / 20
```

The tower should retain some mystery.
ALSO FOG
we cn do some fog 
reuse smoke grenade logic here 

---

# 155. Leaderboard / TAB

TAB may show:

- teammates;
- life/down state;
- kills;
- gold;
- XP;
- ping;
- possibly run progress.

Exact columns should remain JSON-configurable.

---

# 156. Boss HUD

Boss bar appears only while relevant boss encounter is active.

---

# 157. Gold/XP presentation

Gold and XP rewards should not disappear too quickly to understand.

Potential presentation:

```text
small persistent/queued notification
```

rather than extremely brief unreadable popup.

Full totals can live in:

- TAB;
- profile;
- progression UI.

---

# 158. Completion presentation

After final boss dies:

```text
ZOMBIE TOWER COMPLETE
```

or equivalent.

Show:

- completion;
- rewards;
- achievement;
- persistent unlock;
- run summary.

wait on that screen for  impact
then go back to intermission so u can do te whole thing again 

---

# 159. Save requirement

Zombie Tower v1 requires persistence for meaningful progression.

Must persist:

```text
gold
XP
tower completions
unlocks
achievements
```

---

# 160. Persistence architecture warning

The long-term MiMITA philosophy should not require a centralized service forever.

Therefore progression architecture should eventually support:

```text
portable local save
+
authoritative session events
+
optional account/cloud sync
```

A central database may exist as a convenience, not the only possible source of identity/progression forever.

this also relates tooooooo hmm 
C:\mimita-v9\docs\specs\distribution\distribution.md

this doc isnt really focused on just distribution 
it gets intolike idk 
what if 100 uears from now, no central mimita database, no mimita.fun website, no dedicated servers making room discovery hard, and no other players
then, someone finds mimita
how can that 1 person , with nothing but the .exe
find others who want to play and play with them?

thinkign ab tsolutions 
not so sure 
but i want it to work without central data, without centarlized anything 
data and anti cheat idk wha tto d oabout 
data saving for a lot of poeple
matbe some kinda blockcahin?
idk 

---

# 161. Website/account view

Current v1 target may include:

```text
website displays Zombie Tower completions
achievements
progression
```

This is an optional service layer.

The underlying gameplay/save format should not be designed so the mode becomes unusable if the website disappears.!!!!!!!!!!!!!!!!!!

again relates to C:\mimita-v9\docs\specs\distribution\distribution.md
i think  
IDK :3

---

# 162. Completion count

Profile should be able to represent:

```text
Zombie Tower completions = N
```

This enables rewards such as:

```text
1 completion
10 completions
100 completions
```

---

# 163. Completion rewards

Examples:

```text
first completion
→ achievement + cosmetic/weapon

10 completions
→ special weapon

100 completions
→ rare cosmetic/title
```

Exact rewards are content decisions.

---

# 164. Special weapons

A powerful reward may resemble the classic idea of an unlockable overpowered weapon after substantial achievement.

This is acceptable in PvE progression.

In multiplayer competitive modes, eligibility may differ.

---

# 165. Admin commands

Development/testing commands must be permission-gated.

The fact that an admin can spawn monsters or give items MUST NOT count as legitimate completion.

---

# 166. Admin permission concept

Possible levels:

```text
admin 0
admin 1
admin 2
```

Exact semantics are not finalized.

Potential model:

```text
player
moderator/admin
host/root admin
```

This requires separate permissions specification.

---

# 167. Cheat/progression boundary

If a run uses progression-invalidating admin/cheat commands:

```text
persistent competitive/completion rewards may be disabled
```

The engine should explicitly know whether a run is:

```text
progression-valid
```

rather than attempting impossible generic anti-cheat logic.

---

# 168. Host authority warning

A self-hosted authoritative server can fundamentally alter its own game.

Therefore absolute cheat prevention is impossible for fully self-hosted sessions.

Possible progression trust categories:

```text
official verified
trusted community
local/unverified
sandbox
```

Persistent reward systems can choose which categories count.

This should be designed separately from Zombie Tower's core gameplay.
so zombie tower can use the design naturally instead of making it speicifc for zombie tower 

---

# 169. NPC teammates

When humans are unavailable, allied NPCs may fill party slots.

Default solo experience may be:

```text
1 human + 3 allied NPCs
```

They should:

- fight;
- follow;
- revive;
- navigate;
- use weapons;
- avoid becoming useless baggage.

AND BE SUPER GOOD
LIKE
NPCS CANT  JUST SUCK 
THEY SHOUDL BE GOOOODDDD 
AND HAVE VOICE LINES TOO 

---

# 170. Allied NPC revive behavior

Allied NPCs should eventually recognize:

```text
teammate downed
```

and evaluate whether revival is safe/useful.

They should not blindly revive while surrounded by lethal monsters every time.

---

# 171. Monster hostility

Monsters target active players/allied actors according to hostility rules.

Downed actors are ignored by default.

todo i think this relates to 
C:\mimita-v9\docs\architecture\player-npc-systems\npc-movement.md?

idk theres like 9 files there right now bro 
just that whole folder need cleaning and uninfiication 

---

# 172. Friendly fire

Zombie Tower friendly-fire policy is configurable.

Initial recommendation:

```text
player-to-player friendly fire = off or reduced
```

because the mode already creates extreme visual chaos.

This is a `NEEDS_SPEC_DECISION`.

friednly fire is off in v1 but toggleable so u can test if its cool or not 

---

# 173. Monster-on-monster collision

Monsters may physically collide.
AND
ACTOR VS ACTOR
ACTOR VS MONSTER
MONSTER VS MONSTER
ETC
ALL SHOUDLL COLLIDE 
BC RIGHT NOW NPCS DONT COLLDIE WITH EACH OTHER AND PLAYERS DONT COLLIDE WITH EACH OTHER EITHER UGH

Crowds should:

- flow;
- push;
- queue;
- avoid catastrophic deadlocks.

RVO2/DetourCrowd or generalized local avoidance may assist, but actor physics remains final authority.

---

# 174. Monster-on-monster damage

Default monsters do not intentionally attack one another unless:

- attack has area damage;
- friendly monster damage is configured.

This remains encounter-dependent.

---

# 175. Navigation requirements

Zombie Tower is a major stress test for NPC navigation.

Ground monsters must handle:

- ramps;
- stairs;
- multi-floor routes;
- long paths;
- corners;
- gaps where traversable;
- doors/passages;
- obstacle avoidance;
- large map scale.

---

# 176. Recast/Detour relationship

Global navigation may use the architecture defined in `npc-movement.md`:
C:\mimita-v9\docs\architecture\player-npc-systems\npc-movement.md

```text
world collision triangles
        ↓
Recast navmesh
        ↓
Detour corridor
        ↓
MiMITA ActorIntent
        ↓
shared movement/physics
```

Zombie Tower should not create a separate navigation implementation.

---

# 177. Air navigation requirement

Flying monsters require a generalized aerial navigation solution.

They should eventually navigate:

```text
through/around tower geometry
between floors
toward player
```

without merely moving directly through walls.

Exact representation is a separate navigation sub-spec.

---

# 178. Rolling navigation requirement

Rolling monsters use ground path intent but physically roll.

The navigation system should not fake the rolling by translating them directly.

---

# 179. Monster pursuit benchmark

A useful runtime benchmark:

```text
player located several floors away
monster has legal traversable route
        ↓
monster eventually reaches player
```

without:

- wall grinding;
- endless oscillation;
- repeated path reset.

---

# 180. Boss navigation

Boss navigation may have specialized movement capabilities while still using shared navigation/movement primitives.

Chainsaw Boss might use:

```text
high-speed pursuit
momentum-aware interception
```

rather than ordinary zombie steering.

---

# 181. Encounter activation versus long-distance pursuit

Not every monster should exist from run start.

Spawn zones keep inactive areas cheap.

Once spawned, monsters may continue pursuing according to their behavior.

---

# 182. Monster despawning

`NEEDS_SPEC_DECISION`

Possible reasons:

- player moved extremely far away;
- checkpoint permanently invalidates old area;
- monster unreachable;
- performance management.

Avoid obvious pop-out during active encounters.

if u skip too many monsters then a  arena closer should spawn
arena closer = like 100,000 hp
moves as fast as a plaeyr
goes thru walls
a big like 20 meter radius sphere
like almost totally silent except for a repeating hum taht u can hear from far away
and then it should be  touch it = damage, each tick its intersecting u = 66 damage minimum,higher force = more damage 
it can be killed but its jsut annoying
and should be clear that, ok, not killing all monsters/a acceptable amount of monsterrs = a arena closer spawns = i die = more likely to restart , which i dotn wnat to do , therefore kill all mosnters 

BUT THTA SUCSK THO
CUZ IT SHOULD BE INHERENT LIKE inherently fun
man i wish there was more moinsters i could kill
arena closer feels liek a hack fix for now
we should make it so  npcs dont get caught and u never ever ever have to go searching for monsetrs
tahts what i wnana avoid
like just  going thru the game and then missing 1 then u are dead bc u missed 1 becasue u cant even see where it is bc its off the map
that sucks
dont  do that 


---

# 183. Boss spawning

Boss encounter trigger may be:

```text
player enters boss trigger volume
```

Then:

```text
checkpoint confirmed
boss spawns
arena/progression locks
boss HUD appears
music changes
```

---

# 184. Monster drop tables

Each monster type may define chance-based drops.

Example:

```json
{
  "drops": [
    {"item": "ammo_small", "chance": 0.10},
    {"item": "health_small", "chance": 0.05}
  ]
}
```

Drop probabilities remain tuneable.

ALSO TODO RELATES TO C:\mimita-v9\docs\specs\monetization\monetization.md
random drops 

---

# 185. Gold rewards

Monster definitions may specify:

```text
base gold reward
```

Bosses award substantially more.

---

# 186. XP rewards

Likewise:

```text
base XP reward
```

The server grants authoritative progression rewards.

---

# 187. Loot fairness

Avoid making necessary resources depend exclusively on extremely low random drop probabilities.

Tower/map pickups can provide guaranteed recovery opportunities.

---

# 188. Secrets

Tower should contain secrets.

Potential:

- hidden rooms;
- alternate routes;
- secret weapons;
- collectibles;
- Easter eggs.

Movement skill may reveal shortcuts/secrets.

---

# 189. Secrets and nostalgia

The mode should create discoverable moments players remember.

It should be possible years later to:

```text
load exact old version
        ↓
experience old tower
```

as closely as technically possible.

---

# 190. Version preservation

MiMITA should preserve old game states through Git/content versioning where possible.

A user should eventually be able to identify:

```text
Zombie Tower version/commit from date X
```

and recreate it.

This is broader platform architecture but highly aligned with Zombie Tower's purpose.

---

# 191. Replay requirement

A full Zombie Tower run should be recordable as a replay.

C:\mimita-v9\docs\specs\replay\replay.md here todo not  done as of 2026 10 08 1355 jorj todo  expand 

Replay should preserve:

- players;
- monsters;
- attacks;
- projectiles;
- deaths;
- ragdolls;
- bosses;
- pickups;
- important state.

---

# 192. Replay correctness

Current known issues such as broken/missing ragdolls in replay output should be treated as shared replay bugs.

Zombie Tower's high actor count makes replay correctness especially valuable.

---

# 193. Replay export

Long-term replay export should support headless/offscreen rendering rather than consuming the whole interactive client display.

This belongs in the replay specification, not core Zombie Tower implementation.

---

# 194. Runtime observability

Zombie Tower must be diagnosable through:

```text
logs/<date>/<run>/events.jsonl
```

using the shared `StructuredLogger`.

---

# 195. Spawn diagnostics

Useful events:

```text
monster.spawn-zone-activated
monster.spawned
monster.spawn-failed
monster.despawned
```

Fields:

```text
tick
monster id
monster type
spawn zone
position
reason
```

---

# 196. Monster diagnostics

Useful records:

```text
monster.target-changed
monster.behavior-changed
monster.attack-started
monster.damage
monster.died
monster.drop
```

---

# 197. Checkpoint diagnostics

Events:

```text
zombie_tower.checkpoint-reached
zombie_tower.party-wipe
zombie_tower.attempt-started
zombie_tower.run-failed
zombie_tower.completed
```

---

# 198. Boss diagnostics

Events:

```text
boss.spawned
boss.attack-selected
boss.damage
boss.died
```

Do not log every tick by default.

---

# 199. Pickup diagnostics

Events:

```text
pickup.spawned
pickup.collected
pickup.rejected
```

Include:

- actor;
- item;
- previous/new state.

---

# 200. Persistence diagnostics

Progression should log:

```text
progression.gold-awarded
progression.xp-awarded
progression.unlock-awarded
progression.save
```

while avoiding sensitive credential information.

---

# 201. Runtime scenario tests

Zombie Tower needs real runtime scenario tests, not only synthetic unit tests.

---

# 202. Scenario: normal zombie pursuit

Setup:

```text
one normal zombie
one player
multi-room environment
```

Expected:

- finds path;
- reaches player;
- attacks;
- no wall grinding.

---

# 203. Scenario: multi-floor pursuit

Setup:

```text
monster 5 floors below player
legal path exists
```

Expected:

```text
monster eventually reaches upper area
```

---

# 204. Scenario: air pursuit

Flying monster:

```text
player changes elevation
```

Expected:

- navigation around world;
- no wall clipping;
- sustained pursuit.

---

# 205. Scenario: swarm

Setup:

```text
100 normal zombies
```

Measure:

- server tick health;
- navigation;
- crowd movement;
- combat;
- replication.

---

# 206. Scenario: mixed encounter

Example:

```text
30 normal
5 runners
3 wasps
1 brute
```

Purpose:

Test actual intended pressure composition.

---

# 207. Scenario: revive

```text
player A downed
player B holds F 5 sec
```

Expected:

- revive progress;
- restoration;
- correct replication.

---

# 208. Scenario: full wipe

```text
all party members down/dead
```

Expected:

- wipe detected once;
- attempt decremented;
- checkpoint restored or run ends.

---

# 209. Scenario: checkpoint

```text
party reaches checkpoint
party wipes later
```

Expected:

```text
restart at latest checkpoint
```

with intended preserved run state.

---

# 210. Scenario: lava

Actor enters lava.

Expected:

```text
damage at configured fixed-tick interval
```

leaving volume stops damage.

---

# 211. Scenario: pickup

Player approaches health/ammo/weapon pickup.

Expected:

- interaction works;
- server validates;
- item applies once;
- state replicates.

---

# 212. Scenario: Chainsaw Boss

Test:

- boss spawns;
- HUD;
- high-speed charge;
- overshoot;
- chainsaw contact;
- death;
- progression unlock.

---

# 213. Performance scale tests

Test Zombie Tower with:

```text
1
10
30
100
250
500
1000
```

active/dormant monsters where meaningful.

Measure:

- server tick time;
- AI time;
- pathfinding;
- physics;
- replication;
- client rendering.

Do not claim a supported scale merely because spawn command succeeds.

---

# 214. V1 map editing commands

Initial required command families:

```text
spawnpoint
checkpoint
monster_zone
damage_volume
boss_trigger
pickup
```

Each should support at minimum:

```text
add
remove
list
print
save
```

where appropriate.

---

# 215. Map entity stable IDs

Every authored entity should have stable ID.

Example:

```text
checkpoint_04
floor_07_west_spawn
lava_bridge_02
chainsaw_trigger
```

Do not depend solely on vector index.

---

# 216. Map config hot reload safety

If JSON becomes malformed:

```text
reject new version
retain last valid version
log error
```

Never replace live valid map state with invalid data.

---

# 217. Monster config hot reload

Monster configuration should hot reload where safe.

Good candidates:

- health;
- speed;
- behavior tuning;
- sounds;
- animation references;
- drops.

Structural model/skeleton changes may require respawn.

---

# 218. Spawn individual monster command

Development command:

```text
monster_spawn <id>
```

or equivalent.

This allows each monster to be tested individually.

---

# 219. Monster list command

Development command:

```text
monster_list
```

shows registered definitions.

Potentially reuse generalized:

```text
actor_list
```

if architecture makes more sense.

---

# 220. Boss spawn testing

Development should support:

```text
boss_spawn chainsaw
```

without playing the entire tower.

Testing a boss must be fast.

---

# 221. V1 monster development loop

For each monster:

```text
author/import model
        ↓
texture
        ↓
animation
        ↓
configure movement/nav
        ↓
configure attack
        ↓
configure sounds
        ↓
spawn command
        ↓
fight it alone
        ↓
fight it with allied NPCs
        ↓
fight it in mixed encounter
        ↓
keep/tune/remove
```

---

# 222. No wasted monster experiment

If a monster is not fun:

```text
it does not have to remain in Zombie Tower
```

The model/behavior experiment may still be useful elsewhere.

Monster creation is experimentation.

---

# 223. Fun acceptance

The most important test is not:

```text
all selftests pass
```

It is:

```text
Is fighting this monster fun?
```

Then:

```text
Is fighting 20 of it fun?
```

Then:

```text
Is fighting it mixed with other monsters fun?
```

Then:

```text
Is it fun with humans?
```

---

# 224. Human playtest hierarchy

Test configurations:

```text
solo
solo + 3 allied NPCs
4 allied NPCs
large allied NPC group
multiple real humans
```

Human multiplayer is the final important validation.

---

# 225. Shared engine bugs blocking Zombie Tower

Before declaring Zombie Tower release-ready, resolve major shared issues including:

- periodic networking hitch/stutter;
- avatar replication;
- hybrid/AimBody network smoothness;
- dead actors emitting footsteps;
- melee authority mismatch;
- replay ragdoll correctness;
- replay export usability.

Zombie Tower does not require every engine imperfection fixed, but systemic truth-breaking bugs should not be ignored.

---

# 226. Networking periodic hitch

Known symptom as of 2026 10 08 1357 obserevr b jorj  like matbe 2 das ago 
or 3 das idk 

```text
snapshot age ~15 ms
        ↓
periodically ~250 ms
        ↓
returns
```

possibly around a one-second/60-tick pattern.

This should be diagnosed evidence-first in `events.jsonl`.

Do not guess that Zombie Tower caused it.

---

# 227. NPC hybrid/AimBody

Monster/NPC visual actors should eventually use the same hybrid aiming/body architecture where appropriate.

Example ranged humanoid:

```text
looks upward
→ arms/weapon aim upward
```

This is shared actor visual architecture.

---

# 228. Melee authority issue

Known issue:

```text
client sees hit feedback
but authoritative target health may not change
```

This must be diagnosed/fixed before monster melee relies heavily on the system.

Monster attacks should reuse the corrected generalized melee/damage owner.

---

# 229. Data-driven weapon direction

Long term, weapons/attacks should be highly data-driven.

Adding a monster attack should ideally not require:

```text
new bespoke C++ function for each monster
```

Zombie Tower should push this architecture forward where necessary.

---

# 230. But do not block v1 on perfect architecture

Development philosophy:

```text
make observable behavior work
        ↓
understand it
        ↓
generalize
        ↓
remove duplication
```

A prototype attack may begin messily if needed.

But once behavior is proven, migrate it to reusable primitives before multiplying it across 50 monsters.

---

# 231. V1 required feature set

Zombie Tower v1 MUST contain:

```text
one complete authored tower
45–60 minute target run
fast MiMITA movement
checkpoints
limited run attempts
downed/revive
health pickups
ammo pickups
weapon pickups
gold/XP
persistent completion
multiple monster locomotion types
at least ~5–7 distinct enemy behaviors
at least one major boss
preferably several bosses
final boss
lava/damage volumes
spawn volumes
NPC teammate support
real textures
animations
sounds
music/ambience
replay compatibility
```

---

# 232. V1 explicit nonrequirements

V1 does NOT require:

```text
destructible world
moving platforms
breakable walls
full geometry map editor
procedural infinite tower
arbitrary skeleton topology
complex doors
fully generated music
vehicles
portals
every proposed monster
perfect distributed persistence
fully decentralized discovery
```

These can come later.

---

# 233. Strongly recommended V1 scope

A practical first playable vertical slice should contain:

```text
3–5 major tower sections
at least 1 checkpoint
Normal Zombie
Brute
Mutant Wasp
Griever
Runner/Stabber
one hazard
health/ammo pickup
down/revive
Chainsaw Boss
```

Then expand toward full tower.

---

# 234. Full V1 acceptance test

Zombie Tower v1 is REAL when a player can:

1. select Zombie Tower;
2. enter the intermission/lobby;
3. start alone with NPC teammates or with human teammates;
4. use fast unrestricted MiMITA movement;
5. fight through the authored tower;
6. activate real distance-based monster zones;
7. encounter at least five meaningfully distinct enemy types;
8. encounter both ground and air pressure;
9. see correct monster textures;
10. see smooth monster animations;
11. hear appropriate monster/environment sounds;
12. pick up health;
13. pick up ammunition;
14. acquire weapons;
15. become downed;
16. revive a teammate;
17. reach checkpoints;
18. wipe and resume from a checkpoint while attempts remain;
19. encounter multiple boss/miniboss situations;
20. fight at least one fully implemented major boss;
21. survive world hazards;
22. reach the final boss;
23. defeat the final boss;
24. receive a clear completion state;
25. receive persistent gold/XP;
26. receive a persistent completion reward/achievement;
27. quit;
28. return later;
29. still possess saved progression;
30. record/replay the run without fundamental state corruption.

---

# 235. Human acceptance boundary

Automation cannot prove:

```text
scary
fun
panic-inducing
memorable
good music
good enemy mixture
satisfying movement
```

Human playtesting is mandatory.

A gold Zombie Tower build requires someone to actually play it.

---

# 236. Runtime acceptance boundary

A successful selftest is not evidence that the mode works.

Required layers:

```text
BUILD
        ↓
component tests
        ↓
runtime scenarios
        ↓
events.jsonl evidence
        ↓
human playtest
```

Report each separately.

---

# 237. Architectural prohibitions

Do NOT create:

```text
ZombieTowerPlayerPhysics
ZombieTowerDamageSystem
ZombieTowerNetworking
ZombieTowerNPCMovement
ZombieTowerSpecialSpawnImplementationForEveryMonster
```

when generalized owners should handle the behavior.

---

# 238. Reusable primitive rule

Every significant Zombie Tower feature should ask:

```text
Could another mode use this?
```

Examples:

```text
spawn volumes
→ RPG / Survival / Payload events

damage volumes
→ lava / poison / traps / fire

pickups
→ every mode

down/revive
→ PvE / team modes

boss system
→ RPG / Infinite Dungeon

monster definitions
→ Infinite Dungeon / open-world RPG

checkpoints
→ campaigns / movement maps

animation sets
→ all actors
```

---

# 239. Infinite Dungeon Slayer relationship

Zombie Tower monster assets and systems should be reusable directly by Infinite Dungeon Slayer.

Shared:

- monsters;
- attacks;
- animation;
- pickups;
- bosses;
- XP/gold;
- navigation;
- encounter roles.

Infinite Dungeon adds:

```text
procedural world/chunks
endless progression
scaling
loot
```

rather than rebuilding monsters.

---

# 240. RPG relationship

Zombie Tower progression systems should also provide foundations for a future RPG:

- persistent gold;
- XP;
- inventory;
- monster definitions;
- open-world monster spawns;
- quests later;
- bosses;
- pickups;
- saved progression.

---

# 241. Editor relationship

Zombie Tower is a forcing function for:

```text
Entity Editor
Monster Editor
Animation Editor
Map gameplay metadata editor
```

Do not build full editors before understanding the data they need to manipulate.

First:

```text
working data format
```

then:

```text
editor around that format
```

---

# 242. Monster Editor target

Long-term Monster Editor should expose:

```text
model
body
scale
health
movement
navigation
behavior
animations
sounds
attacks
drops
special abilities
```

with live preview.

---

# 243. Animation Editor target

Long-term:

- timeline;
- simulation ticks;
- keyframes;
- clip names;
- interpolation;
- playback;
- attack attachment preview;
- export JSON.

Blender remains supported as external authoring tool.

---

# 244. Boss Editor target

Long-term boss editor may compose:

```text
base monster
+
health
+
attack set
+
decision rules
+
music
+
arena trigger
+
boss HUD
```

rather than requiring bespoke boss C++.

---

# 245. Initial implementation sequence

Recommended dependency-aware sequence:

```text
1. Fix shared melee authority issue
2. Verify texture/UV Blender → MiMITA fidelity
3. Verify/import animation clips
4. Define MonsterPreset schema
5. Implement generalized contact monster attack
6. Implement monster_spawn command
7. Build Normal Zombie
8. Build Brute
9. Build Mutant Wasp / air navigation baseline
10. Build Griever rolling locomotion
11. Build Runner/Stabber
12. Implement pickups
13. Implement map spawn volumes
14. Implement checkpoints/run attempts
15. Implement down/revive
16. Implement damage volumes/lava
17. Build Chainsaw Boss
18. Implement boss HUD/encounter rule
19. Author first playable tower slice
20. Playtest heavily
21. Expand monster pool
22. Expand tower
23. Add additional bosses
24. Implement progression persistence
25. Final boss
26. Full-run acceptance
```

---

# 246. Parallel shared-engine fixes

These may happen alongside the main sequence:

```text
network stutter
avatar replication
NPC hybrid movement
dead footsteps
replay ragdolls
headless replay export
```

Prioritize them when they materially interfere with Zombie Tower testing.

---

# 247. Do not do first

Do NOT immediately divert into:

```text
vehicles
Caravan
full destructible world
portals
infinite procedural planets
complete Lua platform
full in-engine Blender replacement
```

before Zombie Tower's first vertical slice is playable.

---

# 248. First milestone

The first milestone is NOT:

```text
all Zombie Tower systems architecturally perfect
```

It is:

```text
I can load a small tower
with three teammates
and fight several genuinely different monsters
and it is fun.
```

---

# 249. Second milestone

```text
I can play from start through Chainsaw Boss,
hit checkpoints,
use pickups,
get downed/revived,
and restart correctly.
```

---

# 250. Third milestone

```text
I can complete the whole tower,
fight the final boss,
receive a persistent reward,
and want to play it again.
```

---

# 251. Ultimate design test

The mode has succeeded when the player does not think:

```text
"This is a demonstration of NPC technology."
```

They think:

```text
"OH GOD THERE ARE 40 ZOMBIES,
THE WASP IS STILL HERE,
LAUREN IS DOWN,
I HAVE 3 SHOTS LEFT,
THE BRUTE JUST THREW ME INTO THE AIR,
WHERE IS THE HEALTH PACK,
RUN."
```

and afterward:

```text
"Again."
```

---

# 252. Permanent guiding principle

Zombie Tower should combine:

```text
extreme player freedom
+
extreme situational pressure
+
readable enemies
+
meaningful teamwork
+
strong atmosphere
+
persistent memories
```

The player should feel powerful.

The world should still feel dangerous.

The monsters should not defeat the player by taking control away.

They defeat the player by giving the player **too many meaningful things to think about at once**.

And every system built for Zombie Tower should make the rest of MiMITA stronger.

### rant below

2026 10 06 1603 jorj todo - explain zombie tower

but

its basically like

the roblox zombie tower game like old frmo 2012 ish
but
better

more movement, maybe destrcitble world
much more monsters like 
walking crawling running  
flying  hovering
mimic movement
teleport?
bossifghts etc
etc etc etc

it should be a lot of  fighitng a lot of monsters with ur friends and survivng to get all the wa up to the top of zombie tower and beat it 

but htis mode should be difficult, a pro full locked ni should prob take like 50 attempts to beat it?
NOT because its ridiculous and like
overpowered damage
itslike
u knowwhat to do 
but u panic
and u make a bad decision
tahts what u do 
it should not be like  a bunch of  simple boring npcs that have revovler and u just get beamed to death instnatl

it shouldb e like
ground pressure enemies
AND air pressure
AND mimics
AND like group debuffs maybe?
AND looking for health pickups or ammo pickups or weapons or etc ?
AND boss fighitng same time 
etc etc
big varierty

ALSO THIS RELATES TO  infinite dungeon slauer so that 
we can  reuse the monsters for that  mode
and other modes as well
and npc beahvior should be general so it can eb applied to counterstrike, bomb tag, zombiet tower, survive zombies, infintie duengeon slauer, minecraft sttle mode, payload, class based tf2, super smash bros melee mode etc all of htat 

2026 10 06 1631 also this jorj 

Zombie tower
Models: 
plrOrigin
head
leftArm
rightArm
torso 
leftLeg
rightLeg
Textures for the models like zombies
Sounds for the models 
Monsters so far:
Idk 
Boomer
Stabber
Crawler
Mutant wasp
Griever
Mimic
Brute
Normal zombie
Todo zombies
Chainsaw zombie
Exploding head
Hovering angel zombie?
Big fat slow minion spawner zombie 
Even bigger brute
Final boss should be a combo of all these things + do DESTRUCTIBLE WORLD maybe… later… idk… 
Animations 
How to not make it so we need blender pre authored animations?
Crawling, walking, hovering, flying, wings flapping, etc? 


so we need like animation editor, monster editor, maybe map editor? not sure 

also music 
i have a huge huge hgue  nostalgia feeling whei  i hear the zombie tower music
i want to replicate that and give that to others
AND 
NOT ONLY 
GIVE THAT 
BUT
U CAN FIGURE OUT HOW IT WAS MADE
u can trace
directly 
the code paths
that make npcs kill u 
that makes u fly around
u can edit it  
u can make ur own thing
etc
and ur like 

imagine being 7 uears old and learning computers can even do this
and plauing this mode
and hten learning omfg i can just do that 
i can just  code tha t
i want to  do that for poeple 1wa

voice to text 2026 10 08 1223

1. momen to moment its a lot o dashing, running around, shooting monsters, going up floors, wheres m teammates, i need to revive them , is that health/ammo pickup i should get that , wtf is that monster  thats scary, yes its a lot It's a lot of barely surviving. Yeah. Finding secrets. Fighting bosses? Yep. Huge swarm. It's a lot of. Like little person in like a big world, like there's just you and like a couple other people that are here that have to handle like 1000 actual monsters. Like not even counting bosses and stuff or like random, like maybe it'll spawn more or less or something. Somebody afterward saying that the mode worked, it should be like. That was so fun. I have some ideas on how to make it better. But that was so fun and I'm gonna I want to do that again tomorrow and I want to do that with my friends and I want to make different modes for it. And I was it was really scary. I really like the environment. I really like the music. I like the guns and the. Monsters, I think it all meshed together really well. Basically it's like it was really, really fun and they wanna do it again. Number two yeah, it's pretty much what you said. It's like lobby the lobby I think should have like it should be it's own place that other game modes can use too. So like all the game modes can share like one lobby, but like when you're dead or. Then you can be in that lobby. But it'll just be like. Like just one like map file and then it'll have like functions in it or something like stuff that'll make it so you're not just waiting for whatever game mode to be over. It's like you can walk around, you can like pick stuff up, you can do physics experiments. You should probably, it should probably be like not. Like maybe we could do it so. Like every the first person to die has the spectator server and so spectator or non essential server stuff is like in it's own server process and then it can also send information although I don't know if that's like. Useful or good, I don't know, but that would be cool if you can have multiple. Every single person has their own server process, but like there's one that's the host and so the host server process is like the final authority but like if everyone needs to like. Everyone can simulate that like their own version of something I don't know. But. Is there a countdown? Yeah, like 32nd intermission. There is like there, there's bath floors. There's a lot of climbing upward. Yeah. If everybody dies, that means it's over. You just gotta. You're gonna start again from the intermission, and it's a 32nd intermission. Like 30 seconds. Number 3A successful run I feel like should be about like. Maybe like 45 minutes to an hour because the tower it's like 50 meters for one floor and there was like. Like at least 20 floors and like every floor is like 100 by 100 square. And some of them are like a lot like some of them, you can't go straight up. You have to go like through through a section of it. Like there's this one part where you can't go up, but you can't, you have to go to the right. And it's like a. It's like 3 cubes forward and then three cubes to the right and then three cubes to the right again, and then like a little upward incline and then there should be like gaps between them. So like you have to get over the lava and stuff. So it'll be and like there should be monsters through that whole section as well. It's like. There should be like 1000 monsters I think overall, and maybe like I don't know whether to do like minimal checkpoints like 3 or like a checkpoint for every floor, I don't really know. Floors eventually should be modular enough so that infinite zombie tower can happen 'cause that would be awesome. #4 it's difficult because. They're dying, but yeah, they're dying because they're panicking. They're dying because, like, what do you do when there's five walking? OK, they're easy and funny you, but you know that there's something behind you. And then there's like 3 flying monsters and then there's also a boss that you haven't been able to kill yet. And there's not, there is no like movement limit limitations. There's no hit stunned, there's no hitlock. There's no like you you feel if you get smaller, there's like you always have full control over your movement and like attacking the world like you should be. Overpowered like a like compared to other games but. But it should be. It should not be just because you're getting spammed with like one attack over and over. It should be, yeah, getting surrounded. You missed a jump that the team member it got lost in like separated from the rest of you guys. And so if you're all together as one unit, then you'll do a lot better. But if you're just one by one like you handling all those monsters all together is gonna not be super. Doable. but it should not be like you're just getting spammed he spent by 1 monster #4 it's difficult because oh wait no we were just doing #4 so #5 yeah it should be the normal source preset you have you can dash down dash freeze wall jump ragdoll rocket jump Bunny hop Bunny hopping Crouch air strafing there's no intended speed limit you should be able to bypass a lot of the monsters because just like you can go way faster than the monsters can deal with but you also should have like it's like since you're going fast like of course you can travel much further but because monsters spawn based on if you're close to like their spawn point then it should be like there's some flying monsters that will punish like super fast speeds 'cause you'll just dash right into it and then it'll like hit you or do a lot of damage or something or like a poison damage or like an area denial like if you're in this area then you're taking damage like a like lava or a fire grenade from counter strike there's no intended speed limit like you can do all the tricks I think it's just up to me to make it challenging enough that like someone who knows all the tricks is still having a difficult time like passing the game number six yeah you spawn I think it should be like you spawn with the standard like you should spawn with the the revolver and the shotgun but you should also be able to find weapons during the run like you should be able to find goal you should get currency for killing more monsters and more monsters currency means you can get better weapons but we need to figure out data saving there 'cause like I don't think it really works even and yeah you should keep weapons between checkpoints between day once you unlock a weapon and you buy it like you should just have that link to your account until like indefinitely like you buy like you kill enough monsters to get like a mini gun and it cost like 10,000 gold and then don't play for 100 years and then you log back in day one year 101 and you still have the mini gun it should be like that you should still have all your gold all your levels all your everything health packs and ammo packs and weapon pickups those should be like glowing areas of of the tower and you should be able to walk near them and then pick them up or look at them and press F or hold F and then you pick them up umm later we can do like unlock the mouse and then click on it and you pick it up but for now we should just do like look at it and press F and you pick it up or walking over touching it physically picks it up #7 every monster should be able yeah it should be pretty much all of that like literally all of that probably not damage resistances everything should be able to be attacked with like any weapon but yeah pretty much all of that like mandatory is model avatar scale body part geometry health locomotion type navigation preset movement preset navigation capability behavior preset and then animation sounds and attacks I think can be like editable live I don't I don't think we need damage resistances knock back is the same thing as attack spawn rules is controlled by like the spawning thing like you should be able to walk around and put a spawn point wherever you're standing or wherever you're pointing drops should also be controlled like per behavior preset like per monster monster type so like monster should have like a a chance to drop like whatever some things and then special abilities I think should be just weapons or attacks so we need to make like a general monster attack thing or like we need to reuse like what already exists we need to reuse like the the spy knife thing which also we need to fix because sometimes it'll work sometimes it just won't work like me as the client I'll be seeing all the results I'll be getting all the hit feedback but the person I'm attacking just doesn't take damage like their their health bar doesn't change so we need to reuse we need to fix that logic and then reuse it for monster attacks that are melee 'cause they're just gonna like slash their arm forward they're gonna have like claws or something and then if you touch the claws then you're taking damage like it should just be I like that much more than like it's like a specific hit window if it's we're doing a hit window then it'll be like super smash brothers game mode but we're doing zombie tower so it should be like Roblox how if you just walk into something it fires a touch event like don't actually do that in the code but like if you touch it then you're taking damage or you're doing whatever is bound to that touch event for #8 #8 how should monster bodies work? Yeah, they should map onto that. Player origin with head, torso, arms and legs. But yeah, that's a good question. Like a wasp has like wings and a crawler, a crawler is just like a humanoid, so it'll have the legs. Same with the brute, same with the Angel. Head only, enemy can probably just have like. Every other body part is like so small or invisible I don't know A boss with six arms. I think we should, yeah, we should do generalized body definitions afterward so that you can have like you can just have whatever amount of parts you want, But like if you just call one part like the legs or the arms or something, and then it'll use the animations for that leg or arm. But yeah, just map it onto humanoid body parts for now. #9 Yeah, I want it to be stuff like that. It should be named and like attacks and stuff. And it should all be each of them should be like in their own animations folder and like dot, Jason and so like the monster behavior preset should be able to call like for the for like a normal zombie, like the attack animation should be like attack_1 and that animation. Like in a folder, maybe in the same behavior profiles folder, but it probably would be better to put it in somewhere else. But we need to decide. But it should have like all the poses like per tick like 0153045. For all the poses that it's gonna do. A clip should contain that should be start up, start up, active and recovery game play windows that should be enabled enablable or disable able in that config. But for now I think it should not have any startup or active or recovery. It should just be always active or like as soon as you want the animation to happen it does it. Animation should absolutely be able to cancel into other animations, but that's like for. That's like other things. Like Super Smash Brothers mode, but like for now it should just be like, play the animation and then play the animation. It's like if you're play at tick one, play the animation and then at tick 37 play the same animation again, that means you go right back to tick one of the same animation and then you do like the sounds again And the sounds should be retrigger, so like you can spam the same animation over and over. And it'll be like the same sound repeating over and over. I like that a lot, That sound thing. Interpolation should just be smooth. Like just no. No jittering, no chopping. Like I should be able to smoothly track the target across the screen everywhere. #10 is pretty much exactly what you said. I think we should reuse the player, the player weapons and like already how NPC's can use weapons and stuff but we need to make it so. Yeah, it should be. Melee like Maybe like a sphere or like a hit box around like a specific? Like you can author like a claw weapon in Blender and that is attached to the zombie's right arm. And so every time it does the animation, it slashes that arm forward. And there's no like active or inactive windows. But like if you just touch the claw at all based on how fast you like how fast it hit you that can relate here too, like. For for like faster movement or faster players, like if you're moving super fast but they're not moving fast at all, that should still count as like a huge force 'cause if the zombie is just sitting still, but you're if you hit their claws going like 100 meters per second, even if like the base damage is like 10. The force of that alone should be like 50 at least. 50 damage at least, and then if you're like intersecting it for more than. Like for every tick you're intersecting the claw and taking that means you're taking damage. So like you might be able to, you might instantly die if you're going too fast, but that's that might be too punishing or messed up. That's like too fast of a death. But yeah, for number 10, pretty much everything that you said. There's no it should be enable. It should be disabled by default but enabling later if we want for like boss fights like start out like a wind up for an attack from a boss cause bosses have big attacks. But it should be like. It should be like if you're doing like wind up things, it should just be an animation. Like your wind up animation is active for whatever how many ticks and then the attacking animation is active and then the recovery animation it just switches between them like per tick. We can even do like pop up ticks counter like it should just be a number that's like going down. It should start at like 10. If it if the tick wind up is like 10 then it should count down super quick. But #10 pretty much everything you said #11. Normal zombies just like. Like you just just put like 100 of these and it should be overwhelming. A crawler is like the Silent Hill 3 crawler on like I think the hospital level. It should like bite your ankles and be kind of hard to see or attack but like weaker. The runners should be like a faster normal zombie and like be kind of scary. The brute should be taller and like a little slower than the normal zombie but like. Huge damage, huge knockback. It should be able to like slam the ground and like do a shockwave and send all of the people or monsters that it hits. It should send them all up in the air and it should be able to do like a huge punch attack. The Saber is like the runner, but like with its arms are literally like. So it should like try to be stabbing you as much as possible. The mutant wasp should be the same as like the stabber, but umm. It should be flying, so it should be like even scarier than the stabber and the griever is like a ball with spikes on it. So we need to do like a rolling, a rolling locomotion mode where it's like a physical object but like it's target is like It has to move according to like how like swarm behavior moves. So it'll like pick. It'll pick a target, but it'll because it's a ball with spikes on it, it's just going to roll over there. So we need to have make sure that it bounces so that it doesn't get stuck in like corners. And it also needs to like roll towards the and then if you touch the spikes, it should do damage. The mimic is just someone who like copies. It should be kind of rare but like they should have also be kind of weak. But if they kill a player then they just like inherit their avatar. It doesn't show up in the kill feed. You don't get any notification or like anything. Unless you guys are playing together, like if you put a chat in the game, like I just got killed by a mimic, there's a mimic of me. That's like one of the only ways you would be able to know that there's a minute. And then chainsaw zombie, that should be a boss. That boss is gonna have like 2000 health. It should run super fast and have super low friction. So it like if you just move out of the way then it'll like go flying past you. But it should be a chainsaw zombie and so do a lot of damage with a chainsaw. Exploding head should be like the runner, but it tries to get close to you and then once it's close enough to you and the timer it should have a random timer like ticking time bomb and you shouldn't be able to know how long it has until it explodes. But you should be able to tell like with sounds and like like it could be 15 seconds or five seconds, but no matter what it is, it's like before it's about to explode, it starts flashing super quick and makes like a about to explode sounds. So it's like based on you don't get to know the exact time it'll take to explode. But you can see like visibly that it'll it's going to do it. But the things that need to exist are the normal zombie. Umm. The Brute, The Griever, the mutant Wasp. A hovering Angel. And giant brew as well as bosses. Giant Brute. So #12. Crawler. Yep, I pretty much like how you said it I think, yeah, I think we could do deliberate enemy roles because we can just add like a role preset in the roles dot Jason. Yeah, we should do it like that for how you said for number 12. Zombies should. I feel like we should pretty much read for number 13. We should reuse the counter strike NPCS but like. If there's one monster 5 floors below and it's like a normal monster and you're just AFK, it should be able to pathfind its way up through the floors that you might have rocket jumped or done like crazy movement tech. It should be able to find a path to get there. And it should be able to like attack you. It should be that good like it should know where they are and it should have like a path finding way to get there. And the world also needs to be traversable by a zombie 'cause like, if it's not, then then you can't really do that. But I don't know, maybe it's maybe not a normal zombie, but like a flying monster. Definitely a flying monster should be able to. Pathfind through the whole tower and it'll it should be able to find you if you're like hiding somewhere. Yeah, you can't. You shouldn't be able to hide. If you're getting scared and you hide, then you should have like a big punishment because you have to be brave and like be scared and fight them anyway. #14 pretty much, yeah. It should be like a spawn volume is like a specific, maybe like a box like. You should be able to define the size of it like in this space of the world like this. Any valid point in this space is a valid spot for a monster to spawn. It should have specific enemy pools. It should have an activation distance, absolutely. Like a radius, like a sphere around the center of the box. Enemy pool? Like what kinds of enemies? Yep. Count. Yes. Maximum alive. Yeah, pretty much everything that you said. Enemies should visibly spawn. Like they should just appear out of nowhere. I want to do like they should, they should have, they should crawl up from the ground. But we can like do we can add that I think later, but for now just spawn them in and then instantly they can. Do attacks. We could also do spawning from ceilings. I think that's cool. We need to do like a spider monster I think, something that'll crawl on the ceiling. #15 Health reaches 0. You are down for 30 seconds. If you get healed within 30 seconds, then your. Then you're back to. I think. Probably 100 health, yeah, you just back to 100 health. And then when you're down, your your role, your movement preset is the down preset. So it's like you're only, I think it should be like you're only in ragdoll mode or something. Like ragdoll mode isn't like it's not horrible but it's definitely not as fast as normal source movement. If everyone is down, that means the game is over and you gotta do go back to intermission. If the total party is wiped, you'd go back to intermission. And yeah, yeah, actually, if you hit a checkpoint, then yeah, you should go back to the highest checkpoint you hit. Absolutely. If everybody's dead, then just go back there and then just count it as like the second attempt. Actually, no, I feel like it should be like each run should have like a specific amount of attempts, like if everybody dies, but you get to checkpoint. Then like that was attempt one, now you're on attempt 2, but you spawn at checkpoint 4 with all your weapons and all the things that you already got. And then if you get the tech .7, but then you die, everybody's dead again, Then check. I mean attempt three, you spawn at 6.7 and then you just keep going. And then if you die at like checkpoint 10, that was your third attempt. So that means like the whole game is over. Then you just go back to the intermission. #16 I really don't know how to make it a reusable thing. The health bar, it should probably, yeah, I should have like a on everybody. Every client should get like a boss fight health bar. Everyone should see the name of it and like a bit make it super obvious where it is. It should spawn like on specific floors and phases. I don't know, definitely music but I don't. I don't really like how you can kill a boss once and then it has a second hidden face. I like when it's just like you just kill it and the health bar is gone and then it's dead. I don't like how there's like second hidden phases. Just make it have all the help all at once. Just put like 20,000 HP on it. Arena Lock. Yeah, you have to kill the boss before you can go to the next stage. Music changes, I don't know, probably that would require. I feel like we need to do like an in game MIDI player or in game Music Maker so that we don't have to just keep putting like. Actual music files in there over and over. Because then you can just compose. Like specifically you can do ambience for the Zombie tower. You can make specific songs for Zombie. I think I wanna do that. That would be awesome. Yeah, you get a checkpoint before you the before every boss, the chainsaw boss, I'm watching it happen. I'm watching the chainsaw guy. He has like 5000 health. Every time I shoot him, it only does like a like. 10/20/50 damage 'cause I have my revolver and my shotgun, the most damage I can do is with my rocket launcher and that took forever to get. And so he just he if I'm moving, if I can move like I can get up to like 100 meters per second in about like like a like one second. But this guy can get up to like 300 meters per second in like the same time, but his friction is so slow. So he like he just charges at you and then you sidestep a little bit, but then he bounces off the wall and goes like almost directly towards you. And he also can air straight a little. So like if you're not, if you're not totally out of the way, then he can still like straight and control himself in the air and like still attack you with a chainsaw. It's like kind of difficult to kind of difficult to. Avoid. You should learn that he can air strafe. He pretty much only uses a chainsaw. You should dodge because he's super low friction so he's just gonna every time he charges he's just gonna go flying that way. So you can just use his like low momentum. Also maybe his acceleration should be super slow. So if you just go in circles around him, you can just But then I think if you're too close you should have like a special like jump scare attack where he gets like super quick for no reason or like he can dash towards you with the chainsaw. But can't do that very often. You should only be able to do that if you're like, way too close to him. Umm panic I don't really exploit is like yeah you should probably find movement bugs with him and panic should be pretty much like the whole thing. Like he moves so fast. I only have 100 health but this guy was 5000 health. I have limited ammo. I have like, eventually I might run out of ammo and I need to use, I need to fight him with my melee which doesn't have ammo, but I need to be physically right there next to him to do damage. That should be scary too. 17 lava damage, absolutely falling floors. Maybe. But we can, if we're going to do that spawn point thing where you just go through the world and you set like, OK, this spot is a is a spawn point. Well, you should also be able to do like I feel like we should be able to do. Map editor, like the Jason should be able to edit like. You should be able to look at a specific block of it, click it, and then set it to be like a falling floor like this. This object is a falling floor and you should be able to see that in the Jason for the map. And then you should be able to export and look at it in blender. And then that specific object that you put should have like an attribute. I don't know how to do attributes in blender but like. In Roblox, it would be like this block has an attribute of like falling floor is true, or like it's tagged as a falling floor. So we need to do that so we can go back and forth between Blender and the engine. But once we do that, then yeah, we could do all those the falling floors, crushing crushers, moving platforms. I don't know if we have moving platforms right now. Definitely later breakable walls maybe? I don't think we should do that yet either. I think we need to get version 1 to just be like static somewhat and then version 2 should absolutely have doors and breakable walls and stuff. And poison pools is just the same as lava explosive objects. Maybe like barrels, the physics debris that would be cool but I don't know destructive will cover I would like to do but I don't know all that stuff I would like to do but I feel like for V1 the necessities would just be the lava damage and. Poison pools and maybe explosive objects because of the exploding guy. Like it's just gonna be a lot of like just running around and shooting the monsters. For #18 Yeah, it should be like commit. It should be all of those commands that you just said. You look at a point and you place it there. Yes, you draw, don't draw boxes visually. we should do it. We should save to exactly. We should hot reload while playing and save it to that map. The config dragging handles or doing like GUI stuff. I feel like we can do that stuff later. We should do more like. Do a command in the game and it'll spawn a thing and go into the maps config file and then you can edit the position of it. But I also know like that kind of sucks 'cause then you gotta go back and forth and you're editing the actual numbers. So that would be cool to have like an editor mode. We should do like entity editor mode. Where it's like you go into free Cam and you don't have a body, but like wherever you're pointing, it's like that's the thing that you're selecting. You should all be, you should be able to unlock your mouse too. So you can just click on specific objects with the cursor and then it'll select that. But I feel like we don't need to do that yet. We can just, I think we'd be able to get away with just. Console commands and editing the Jason for specific values. But for number 18 it should pretty much be exactly like that. Like spawn point, add checkpoint, add damage volume, add monster zone, boss trigger, pick up, especially pick up 'cause then you can do like weapon pickups or. Health pickups are just general item pickups. #19 When the tower sounds like ambient, like nothing like when a when nothing is going on, it should just be like ambient. Like if you're outside and it's like raining a little, it should be raining and it should be dark. When a wave begins, it should be like. It's just so like checkpoint reach or like checkpoint #3 reached or like. But it shouldn't be super like if you shouldn't really be able to know how long this tower is, you should be kind of like in the dark or blind about it. When 40 monsters are nearby it should be like you just hear a bunch of sounds you should hear like the music should pretty much be the same like the whole time or like. I don't really know how to do them because everybody should be listening to the same music, but you can also pick like to listen to your own music if you'd like or just mute the music. But by default, everyone gets the same song to play and then they can all hear the same song. But we should that's that's another thing if we do it, if we do in the engine like a song maker, like you can just have a bunch of dot MIDI files. In some folder and then just play it. And then you can even layer stuff on like if you just, all you need to do is put sound clips in there and make a sound font and then you just reuse that over and over and over. And then you can make specific songs for Zombie Tower. Any specific songs for specific sections? Like every time something happens it's like you play a specific sound effect. monsters get like voice chat, like they get voice lines and like monster yelling and screaming and dying and stuff. Nonverbal sounds pretty much. The HUD that should exist HUD is Yep, health ammo, not the floor number. Yes, teammates that should be existing if you press tab and look at the leaderboard boss bar if there is a boss active right now. Checkpoint notification, Yeah, I think it should be checkpoint notification XP and Gold, I don't know, maybe that should be in the leaderboard as well, like. Check leaderboard and you can see, OK, I have this much gold and this much XP and other people have this much gold and this much XP. Like in a leaderboard, an objective, I feel like that should also be in the leaderboard like always. Like, yeah. And then with no leaderboard, it should just be health ammo. And like boss bar and checkpoint notification like just kind of just that. And for #20? The acceptance test is Pretty much what you said. You can start the mode, you join it and you can have team, you can have MPCS on your team if you're not playing with anybody, like you can have like a total of four of you. Umm, you can reach the last you can finish the whole tower. You the VE version 1 is real means you finish the whole tab. You kill the final boss and you get like a reward and it saves your account and you can check the website and you can see how many times you beat the tower and you have like achievements and stuff. And then you can just go do. But we need to do like if you're doing if you like. Some kind of anti GI. Don't really know how you could even do an anti cheat other than like. Flying or teleporting. If you're the. If you're the. If you're the host. Or maybe those commands can't work. Maybe we should gate a lot of those commands behind like. Like a cheats command. Or like a game or like in Minecraft, you just go into creative and then you can do all that stuff. Or operator. Or like host we should just call it admin. I think we should call it admin like you get admin commands. And then admin 10, admin 1 means all those commands are you can admin a player and then admin one or zero like 1 means you can anyone who's an admin can use those commands. Admin 0 means anyone who's an admin can't use those commands. And then like maybe admin 2 can be like. Only the host can use the admin commands, but that needs more work to figure that out. But yeah, encounter at least 5 distinct monsters. Yep, it should be walking, flying. Exploding. N. Maybe 2 walking so The Walking would be like. The normal zombie and maybe the stabbing zombie or the brute. And then the griever, which is that rolling ball, it should be like a mutant wasp. That should be huge. That was flying monsters. Anymore flying monsters, really. And we need bosses. The chainsaw boss is one final boss is like, should be an amalgamation of all the things that you just fought. But we need, I think we need like the like two more bosses like the chainsaw boss and then two more 'cause that's like not enough. I feel like the bosses are like the hugest, coolest part. And yeah, you should get a save completion reward. Yeah, you should be the tower. And it says you be the zombie tower. And then you get like a bunch of gold and rewards and you get like an achievement and you should get like for your next time you wanna do that. If you're logged in as whoever you're logged in right now, that means like you have. You beat, you beat the game like you beat the tower. And so that means you get like a special, you should get like a special like weapon or like a cosmetic or something. Something that persists like outside of that game, that too. That's something huge. I really, really feel I wish Roblox would do this. Like they started doing it kind of like if you go into one game and then you do the quest in that game, you will get like. You get, you get cosmetics that you can use in other games, but they're so like difficult to do. So I feel like we should we can do that different here. And what can be postponed is a lot of the moving like moving platforms like super like like doors or like infinite generation of it or we don't need a bunch of different monsters I feel like I feel like what we should do is we should make a bunch of monsters and then play test all of them and then whichever ones are good like through play tests and keep those and then every single there's no wasted effort 'cause you already made that model and you design the whole monster but if it's just not fun that's cool like you just learn something you can use probably for a different mode but like it should just make a bunch of monsters and then put them in the game and then test it like is it fun is it fun just alone is it fun with NPCS is it fun with 50 NPCS on my team is it fun with just me and three other NPCS so 4 total the huge thing is is it fun with like humans doesn't even work with other humans stuff like that and yeah and replay the run you should be able to save a replay but yeah this is something else too we need to do like a better replay editor like a source movie maker like we don't have that right now but that's like a different document 