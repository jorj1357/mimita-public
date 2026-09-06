9 6 2026
End goal
I join mimita game
I join someones server
Theure running bomb tag
And at the top i see
“(username) has the bomb!!!! 13.26”
That 13.26 number is the timer
Bomb
Its just a big sphere a plauer holds, 
Like godball, but the weapon pose is with the right arm up and forward, not just at the side like it is for the godball right now 
Like 0.5 meter radius cuz players default height is 1.8m 
Attached to the right hand like godball 
Bomb looks like
A 10,10,10 rgb sphere in the game, 1.0 alpha = 100% opaque 
Every 30 ticks, it blinks from being 10,10,10 color, to 255,0,0 color
So
Tick 1 - 30 = 10,10,10 color
Tick 31 - 60 = 255,0,0 color
Tick 61 - 90 = 10,10,10 color
On adn on and on
When it is passed
Turns grey, to like 80,80,80 color, and plays passing sound
Its idle when its grey, its idle for 60 ticks, so show to others that 
Bomb timer
Above the actual bomb itself, not above the plauer,
It should countd own super quick, as well as having the on screen timer countdown
Both UI’s just read from the server’s tick of what server thinks the actual amount of ticks left is 
Hot reloadalbe as well !!!!
Bomb sounds like
Holding it = "C:\mimita-priv-v8\assets\sound\weapon\bomb\bombtick1.wav"
1 sec pass = 1 tick sound
60 ticks = 1 sec, so bomb tick sound = once everu 60 ticks
Passing it
It plaus this sound
Passing sound effect plays here:  "C:\mimita-priv-v8\assets\sound\weapon\bomb\bombpass1.wav"
Then for the entire 60 ticks its inactive, it has this sound playing, then it stops when its not inactive anmore 
"C:\mimita-priv-v8\assets\sound\weapon\bomb\bombinactive1.wav" 
Bomb explodes
Sound:
"C:\mimita-priv-v8\assets\sound\weapon\bomb\explosion2.wav"
Im holding it 
I instantly die, and the bomb is instantly passed to a new random player
Entirely random, and no repeatts 
Todo define like a super good random function bc i notice random in the repo somedimtes doesnt actually mean random? Like random map switcher , like maybe dates 8 31 2026 to 9 4 2026, working on that, random map picker picked always the most recent one in alphabetical order, not good 
Im not holding it
Gui
HOT RELOADABLE look at the full gui spec here https://docs.google.com/document/d/1uTxOkciwKHtSzBm4rKxxMqMshNGttK9TOk8ju3uBu-E/edit?tab=t.el6s4djyhvo1 
Im not holding it 
(username) has the bomb!!! x.xx until it explodes!!!”
x.xx is seconds, like 5.82, 11.85, 2.04,  0.01, etc 
Im holding it
You has the bomb!!!! x.xx until it explodes!!!”
15 sec timer, server sided timer
Pass from plr A to plr B = 1 sec idle state, cant pass backand forth super fast, this limits it
A SINGLE timer, so passing between players DOES NOT reset the timer
Bad:
Plr A passes to plr B at tick 100, bomb has 8.15 sec left
Secs are represented to plauers, but the server thinks in ticks. So its like 8.15 * 60 ticks left before exploding
Plr B has the bomb now, but it ahs 15 secs left now, bc it was passed
Good
Plr A passes to plr B at tick 100, bomb has 8 sec left
Plr B has the bomb now, and it has 8.15 sec left because thats how much time it had before it was passed 
Again server thinks in ticks but clients see seconds counting down super quick 
Networking
I WANT TO TOTALLY avoid the issue where i acn get tagged by someone who is like 5 meters away
Thats not good
So
Server authoritative
Attacker advantage, lag compensation
If u get tagged from far away, it should show l ike
The meters away u were tagged from
And log that in the server log, according to debug spec ehre https://docs.google.com/document/d/1uTxOkciwKHtSzBm4rKxxMqMshNGttK9TOk8ju3uBu-E/edit?tab=t.uuzhfzjvuuv5 
The distancein meterrs the attacker plr origin was from victim plr oriign, and how far the bomb was from the victim, and what bod part was touched and how far away it was on the victims’s screeen, attacker’s screen, and to the server how far away they were 
So we know what is doing better vs not better  with actual numbers and not just  describitng it 


MiMITA Bomb Tag Gamemode Specification
Date: 09-06-2026
Status: Desired behavior / implementation specification

1. End Goal & Core Game Loop
Bomb Tag is a fast, infinite MiMITA gamemode built around physically passing one bomb between players.
The desired experience:
I launch MiMITA.
↓
I join someone's server.
↓
The server is running Bomb Tag.
↓
One player has the bomb.
↓
I see:

"PlayerA has the bomb!!!! 13.26 until it explodes!!!"

↓
PlayerA physically tags PlayerB with the bomb.
↓
PlayerB instantly becomes the bomb holder.
↓
Bomb becomes inactive for 60 ticks.
↓
The SAME explosion timer continues counting down.
↓
PlayerB can pass again after the 60-tick inactive period.
↓
Timer reaches 0.
↓
Current holder instantly explodes/dies.
↓
They instantly respawn.
↓
Another player instantly gets the bomb.
↓
New 15-second timer immediately begins.
↓
Continue forever.
Speed is a major design priority.
There should be very little waiting.
For V1:
Infinite gameplay.
No overall match time limit.
No rounds.
No permanent elimination.
Instant respawning.
No weapons other than the bomb.
Bomb cannot be voluntarily unequipped.
Persistent kills/deaths/XP/gold continue to work according to their existing specifications.
Future versions may add:
rounds
win conditions
elimination
last-player-standing
time limits
score limits
Those are explicitly not required for V1.

2. Bomb Appearance, Pose, Audio & Hot Reload
The bomb is a physical sphere attached to the holder's right hand.
Default dimensions:
Player height = approximately 1.8 m
Bomb radius = approximately 0.5 m
The bomb should behave similarly to the existing Godball attachment system where useful, but use a different holding pose.
Holding Pose
Desired right arm pose:
right arm:
up
+
forward
Do NOT simply reuse the current Godball arm-at-side pose.
The bomb should visibly look like something the player is presenting/trying to tag another player with.
Active Bomb Appearance
Default active color:
RGB = 10, 10, 10
Alpha = 1.0
Every 30 simulation ticks, alternate between:
10, 10, 10
and:
255, 0, 0
Example:
Ticks 1–30:
10, 10, 10

Ticks 31–60:
255, 0, 0

Ticks 61–90:
10, 10, 10

Ticks 91–120:
255, 0, 0
Continue indefinitely while active.
This behavior must be tick-based, not frame-based.
Inactive Bomb Appearance
After a successful pass, the bomb becomes grey:
RGB = 80, 80, 80
for:
60 ticks
This communicates:
The bomb belongs to this player, but cannot currently be passed.
After 60 ticks, it becomes active again.
Tick Sound
While active and held, play:
C:\mimita-priv-v8\assets\sound\weapon\bomb\bombtick1.wav
Default interval:
once every 60 ticks
At 60 Hz:
60 ticks = 1 second
Pass Sound
On successful pass:
C:\mimita-priv-v8\assets\sound\weapon\bomb\bombpass1.wav
Inactive Sound
During the entire 60-tick inactive period:
C:\mimita-priv-v8\assets\sound\weapon\bomb\bombinactive1.wav
The inactive sound stops when the bomb becomes active again.
Explosion Sound
On explosion:
C:\mimita-priv-v8\assets\sound\weapon\bomb\explosion2.wav
Appearance, UI placement, UI sizing, timing, colors, audio parameters, and other appropriate presentation values should be hot reloadable wherever practical.
Do not hardcode presentation values unnecessarily.

3. Server-Sided Bomb Timer
There is exactly one authoritative bomb timer.
The server owns it.
Default:
TICK_RATE = 60

BOMB_TIMER_SECONDS = 15

BOMB_TIMER_TICKS = 15 × 60
                 = 900 ticks
The server thinks in ticks.
Clients display seconds.
Conceptually:
remainingSeconds =
remainingTicks / 60.0
Example client display:
13.26
8.15
5.82
2.04
0.01
The clients do NOT independently determine when the bomb explodes.
Both:
top-screen timer
bomb-world timer
display the server-authoritative bomb timer.
Clients may visually interpolate/count between authoritative updates so the display looks smooth, but the server's bomb tick is the truth.
Passing Does NOT Reset Timer
This is a non-negotiable rule.
BAD:
Player A:
8.15 seconds remaining

A passes to B.

Player B:
15.00 seconds remaining
GOOD:
Player A:
8.15 seconds remaining

A passes to B.

Player B:
8.15 seconds remaining
Passing changes:
bomb holder
inactive state
It does NOT change:
explosion deadline
Inactive State Does NOT Pause Timer
Example:
30 ticks remain
↓
A passes to B
↓
B enters 60-tick inactive state
↓
30 ticks pass
↓
bomb explodes
B dies even though the bomb was still inactive.
Inactive means:
cannot pass
It does NOT mean:
timer paused

4. Bomb Passing & Physical Contact
Passing works like melee.
It requires physical overlap/contact between the attacker's bomb hit volume and a valid body part of another player.
Conceptually:
Bomb collision shape
        +
Victim body collision shape
        ↓
valid overlap/contact
        ↓
server validates
        ↓
bomb transfers
The bomb collision representation may use the appropriate existing collision primitive:
sphere
capsule
box
other supported physical hit volume
The visible bomb and collision representation should correspond closely enough that players understand why contact occurred.
Successful Pass
Player A has bomb.
A's bomb makes valid physical contact with Player B.
Server confirms the contact.
Immediately:
A.hasBomb = false
B.hasBomb = true
The bomb is now attached to B.
B immediately enters:
bombInactive = true
for:
60 ticks
During those 60 ticks:
B visibly possesses the bomb.
Bomb is grey.
Inactive sound plays.
B cannot pass.
Explosion timer continues.
Bomb can still explode.
After 60 ticks:
bombInactive = false
and B may pass it.
No Rapid Back-and-Forth
The 60-tick inactive state prevents:
A → B → A → B → A
multiple times almost instantly.
It is the intentional pass-rate limiter.
Do not add another arbitrary pass cooldown unless the specification is intentionally changed.

5. Bomb Ownership, Unequipping, Disconnects & Respawning
Bomb ownership is server authoritative.
Conceptually, a player may have state such as:
hasBomb = true
The exact implementation can differ, but the behavior must remain the same.
Bomb Cannot Be Unequipped
If:
hasBomb = true
the player cannot voluntarily unequip the bomb.
Example:
Client:
"I want to unequip bomb."

Server:
"You currently have the authoritative bomb."

Result:
UNEQUIP REJECTED
The client cannot bypass Bomb Tag rules by changing weapon state locally.
There are no other usable weapons in Bomb Tag V1.
Holder Disconnects
If the bomb holder leaves/disconnects:
holder disconnects
↓
server detects it
↓
new eligible player selected immediately
↓
bomb transferred
↓
game continues
The chat system should display that the player left and communicate why bomb ownership changed.
Conceptually:
PlayerA left the server.
Bomb passed to PlayerC.
Join/leave messages should use the existing/shared chat system and its specification rather than creating a Bomb-Tag-specific chat implementation.
New Players
A player joining an active Bomb Tag server is added to the eligible random-selection pool.
They can participate immediately according to normal spawn/join rules.
Explosion Death
When timer reaches zero:
current holder
↓
instantly dies
↓
bomb explosion/audio/effects
↓
player instantly respawns
↓
new bomb holder selected
↓
new 900-tick timer starts immediately
Minimize dead time between cycles.

6. Random Holder Selection
Random bomb selection must avoid immediate/repeated selection patterns.
Use a shuffle-bag style system.
Example with:
A
B
C
D
Generate a randomized ordering:
C
A
D
B
Then holder selection proceeds:
C → A → D → B
Every eligible player is selected once before the bag is reshuffled.
After exhausting it:
reshuffle all eligible players
and begin another randomized sequence.
This provides:
random ordering
+
no repeats until everyone has been selected
Player Joins
New eligible players are incorporated into selection without breaking the no-repeat guarantee for players already selected in the current cycle.
Implementation may choose the simplest correct method.
Player Leaves
Disconnected/ineligible players are removed from pending selection.
If the departing player currently owns the bomb, immediately select another eligible player.
Randomness Regression
MiMITA has previously shown suspicious random-selection behavior in other systems, including map selection apparently favoring a particular ordering.
Do not assume:
"we called random()"
means the resulting selection behavior is correct.
Test the actual selection algorithm.
At minimum verify:
no alphabetical-order dependency
no insertion-order dependency
no immediate holder repetition when alternatives exist
every eligible player selected before reshuffle
many cycles produce varied ordering
If an existing randomness bug is identified and its root cause demonstrated, document it in the regression system.

7. GUI & World-Space Timer
Bomb Tag has two primary timer displays.
Both read from the same server-authoritative timer.
Top-Screen GUI
When another player has the bomb:
PlayerA has the bomb!!!! 13.26 until it explodes!!!
When the local player has it:
You have the bomb!!!! 13.26 until it explodes!!!
Use grammatically correct "You have", even if earlier notes say "You has".
The countdown should visually update quickly/smoothly.
Example:
13.26
13.25
13.24
13.23
...
0.03
0.02
0.01
The visual display may update more frequently than network packets arrive by deriving the visible countdown from the latest authoritative server timing information.
It must not become a separate authoritative timer.
World-Space Bomb Timer
The bomb itself has a timer above it.
Important:
Timer is above the bomb, not simply above the player's head.
Example:
      8.15
       ●
      /|
Both UI timers represent the same bomb deadline.
They must not drift into independently maintained countdowns.
Hot Reload
Bomb Tag GUI should follow the existing MiMITA GUI specification.
Appropriate properties should be hot reloadable, including:
position
offset
font
font size
color
scale
visibility
formatting
world-space offset
Do not create a separate incompatible GUI architecture for Bomb Tag.

8. Networking, Lag Compensation & Pass Validation
Priority order:
1. Server authority
2. Responsive attacker experience
3. Lag compensation
4. Prevent obviously impossible-looking tags
5. Instrument everything so tuning uses measurements
A client saying:
"I touched PlayerB."
is not sufficient by itself.
Instead:
Client reports pass/contact attempt
        ↓
Server identifies relevant simulation tick
        ↓
Server examines/rewinds historical state
        ↓
Server checks bomb vs victim-body contact
        ↓
Server applies sanity limits
        ↓
valid?
    ↙       ↘
  yes       no
   ↓         ↓
transfer    reject
Lag Compensation
If A legitimately contacts B on A's screen, but B has moved by the time the packet reaches the server, the server may rewind relevant historical state.
The server should determine whether contact was valid at the appropriate historical moment.
This is NOT:
attacker said hit
=
automatic hit
It is:
attacker reports contact
+
server reconstructs/rewinds relevant state
+
server independently verifies sufficiently valid contact
=
accepted pass
Physical Contact Still Matters
Ordinary valid passes should correspond to bomb/body overlap.
Lag compensation is not permission to transfer a bomb across empty space.
Maximum Sanity Distance
Initial hot-reloadable maximum:
MAX_BOMB_PASS_SANITY_DISTANCE_METERS = 3.0
This is a hard rejection/sanity bound, not the normal hit radius.
It does NOT mean:
players within 3 meters can automatically tag each other
Actual physical/rewound contact is still required.
Rather:
if relevant discrepancy/distance exceeds configured hard limit
→ reject pass
The exact definition and best threshold should be refined using measured bad-connection tests.
The 3.0 m value is an initial tunable value, not a permanently assumed ideal.
Test with:
normal connection
100 ms
300 ms
500 ms
jitter
packet loss
reordering
temporary blackout
Then tune from evidence.

9. Pass Visualization, Debugging & Regression Tests
Every successful bomb pass should have a visible informational effect.
Draw a beam between:
old bomb position
        ↓
new bomb position
Display relevant distance information.
This resembles the existing server-disagree/debug visual language but is not itself a server-disagree event.
It is a Bomb Tag pass-information effect.
Pass Debug Data
For every pass, record enough information to understand exactly why it was accepted.
At minimum:
pass ID
server tick
reported/client tick

attacker username / ID
victim username / ID

old holder
new holder

attacker origin
victim origin
bomb position

body part contacted

attacker-view player distance
victim-view player distance
server-current player distance
server-rewound player distance

attacker-view bomb-to-body distance
victim-view bomb-to-body distance
server-current bomb-to-body distance
server-rewound bomb-to-body distance

rewind ticks
rewind milliseconds

pass accepted/rejected
reason

configured maximum sanity distance
Where a measurement cannot legitimately be known, log that explicitly rather than inventing it.
Debug logging should integrate with the existing MiMITA debug/logging specification.
Example Debug Visualization
BOMB PASS

A → B

Attacker view:       0.08 m
Victim view:         1.82 m
Server current:      1.51 m
Server rewound:      0.09 m

Bomb → body:         0.02 m
Body part:           right forearm

Rewind:              9 ticks / 150 ms
Result:              ACCEPTED
This allows networking decisions to be based on numbers rather than:
"that felt far"
Required Regression Tests
At minimum:
PASS:
real contact transfers bomb

FAIL:
no contact does not transfer

PASS:
valid rewound contact can transfer

FAIL:
fabricated client hit cannot force transfer

FAIL:
pass beyond hard sanity limit rejected

FAIL:
inactive holder cannot pass

PASS:
inactive bomb still explodes

PASS:
passing does not reset timer

PASS:
holder cannot unequip bomb

PASS:
holder disconnect transfers bomb

PASS:
new player joins selection pool

PASS:
shuffle bag selects everyone before reshuffle

PASS:
bomb remains server authoritative
For networking regressions, save deterministic scenarios where practical.
Once a bad networking behavior is understood:
observe
↓
reproduce
↓
measure
↓
identify root cause
↓
fix
↓
add regression
↓
never silently reintroduce it

10. Definition of Done
Bomb Tag V1 is complete when this exact type of session works:
A, B, C, D join Bomb Tag.
↓
Server creates randomized holder order.
↓
A receives bomb.
↓
900-tick timer begins.
↓
Top GUI shows A has bomb.
↓
World timer appears above bomb.
↓
Bomb alternates dark/red every 30 ticks.
↓
Bomb tick sound plays every 60 ticks.
↓
A physically contacts B with bomb.
↓
Server validates contact.
↓
Pass effect shows measured transfer.
↓
B immediately owns bomb.
↓
Bomb becomes grey.
↓
Pass sound plays.
↓
Inactive sound begins.
↓
B cannot pass for 60 ticks.
↓
ORIGINAL TIMER CONTINUES.
↓
60 ticks finish.
↓
Bomb becomes active.
↓
B can pass.
↓
Timer reaches zero while C owns bomb.
↓
Explosion sound/effect occurs.
↓
C instantly dies.
↓
C instantly respawns.
↓
Next eligible randomized player gets bomb.
↓
New 900-tick timer immediately begins.
↓
Game continues indefinitely.
The following are non-negotiable regressions:
Passing must NEVER reset the active timer.

Inactive state must NEVER pause the active timer.

Client must NEVER independently choose bomb ownership.

Bomb holder must NEVER successfully unequip the bomb.

A client claim alone must NEVER force a pass.

Lag compensation must NEVER mean arbitrary distance tagging.

A valid pass must require server-validated physical/rewound contact.

The hard sanity-distance limit must remain hot reloadable.

Bomb holder disconnect must NEVER permanently break the mode.

Random selection must NEVER depend on alphabetical ordering.

Players must NEVER repeatedly be selected while other eligible
players remain unused in the current shuffle bag.

Bomb Tag must NEVER introduce other usable weapons in V1.
Before accepting changes to Bomb Tag:
run existing Bomb Tag regressions
↓
make change
↓
run them again
↓
new desired behavior passes
↓
old desired behavior still passes
If:
fixing bomb audio
↓
breaks bomb timer
the audio change is not considered successful.
Reject/undo the candidate change, investigate the coupling, and implement it without the unrelated regression.
The core invariant is:
One server-authoritative bomb, one continuous tick-based timer, physical server-validated passing, immediate gameplay, measurable lag compensation, and no silent regressions.

