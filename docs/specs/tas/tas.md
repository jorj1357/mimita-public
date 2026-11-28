2026 11 02 0939 jojr todo eplxina

tas = tool assisted speedrun

captian falcon ssbm tas is so fun to watch on uoutube
ok but what we doing it here for
welllllll

one its super cool

but two its bc im doing debug things rihgt now and i realized

if we have like a super strng specific scenario that covers all the issues we talking about like

that would be awesome

like 
idk how itd be  but itd be like

it would be console commands to start off of coruse
but

tas_*

the end goal iw ant is
press R, reset back to pos X as sooon as i press it
strafe, no not quite rihgt
press R, doit again
not quite right
do it again
etc mmm not quite right

slow down the game to 0.5x speed
means i get 2x the time to do the exact thing
its played back at 60hz, but the inputs cn be done at like 
10 hz
1hz


like geometry dash speedhack

so then, u have sooooo much time
and smooth interpolation betewen each tick 

THIS ALSO RELATES TO 
becaues 
i was just thinking like
this game is like
kinda fast
if it was a little slower i can see how it would be 
idk
more thinking required ? or something? 

or it can just make it so u can do harder things
but idk
ilike the fast speed
and thank goodness, i dont have to compeitively be the #1 top ranked player i can juts make the funcitons and do waht i would like to with them 
ok

but

thsi is good for debug because 
if i can 

imagine the scenario is only 60 seconds
so thatu dont need more than 
WAIT BUT WHY CANT WE JUST PLAY it back quicker ?
the scenario

i figure we can 
if its like 300 sec scenario
just pla it back much much quicker
to test colisions and stuff
ur checking the events.jsonl of whts happenign anwyays
i tihnk?
idk

i do tihnk we should log only to 1 events.jsonl

idk this need smore explantion

## Relationship to human-recorded runtime regressions

Collision debugging needs a human-recorded failure to replay through the real
fixed-tick movement and collision owners, with the resulting evidence written
to the canonical `events.jsonl` journals. Human bookmarks are restore-after
checkpoints: replay and analyze the segment through the bookmark, then restore
the complete recorded gameplay state and continue. This collision-specific
checkpoint contract is required for reliable repeat runs; it is distinct from
the broader TAS save-state feature below.

General TAS controls may later help inspect or recreate a difficult captured
state: reset, pause, single-step, and change wall-clock playback speed while
preserving the fixed 60 Hz simulation tick. General-purpose save states remain
a broader optional capability. Do not treat a saved visual position or a
transform-follow replay as a restored physics state.

## Tick-exact gameplay authoring direction — 2026-11-28

> “freestlye and tas mode, advance per tick and put exact input values per tick
> get exact what u want to have happen happen
> replaying them determinisittally
> should be 100% deterministic
> save a tas file + run it on any fps performance etc”

Interpretation: TAS is both a debugging tool and a way to author precise performances. The user can switch into TAS mode during play, pause at the current simulation tick, inspect that tick's applied inputs and state including velocity, then play, pause, reset, or advance exactly one simulation tick at a time. TAS input records store the exact normalized or quantized control values accepted by the gameplay input owner for every simulation tick, plus the content, configuration, seed, and simulation identity needed to interpret them.

Rendering frame rate and wall-clock playback speed MUST NOT change which simulation inputs occur on which ticks. A TAS file MUST identify incompatible simulation/content versions and report the first tick and state component that diverges. The target is exact deterministic reproduction across supported frame rates and, eventually, supported machines. Cross-machine bit-exactness is not claimed until replay evidence proves it across the declared support matrix. TAS events and checkpoints use the existing canonical runtime `events.jsonl`; TAS MUST NOT create a competing gameplay journal.

Rocket League-style vehicle work is a concrete initial scenario: record and replay a human or bot's per-tick `VehicleInput`, inspect car/ball state at a chosen tick, and compare state hashes and contact/goal events. Importing an external RLBot/SSL bot is an option to investigate, not a dependency or compatibility promise. A locally authored freestyle bot remains possible and must use the same vehicle input surface as a human.
