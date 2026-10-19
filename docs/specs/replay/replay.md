2026 10 19 1800 JORJ TODO - 

REPLAY
I
IKE EDITOR
RPLA ECITOR
ED
REPLAY EDITOR
STRONG LIEK AFTER EFFECTS
QUICK LIKE  NOTHING
LIKE GET AFTER EFFECTS LEVEL VISUALS ON LIKE 240FPS ONA BAD DEVICE BC I SWEAR ITS POSSIBLE IDK WHY VISUAL THINGS ARE SO OSLOWWWWWW 
LIEK EDITOR SOFTWEARE WE CAN DO IT BETTERRRRRR 
WE CNA DO LIKE COOL FRAG MOVIES
AND STUFF
AND SHADERS Hmmmmmm
ne
OMG
POST FX PRESETS AND LOAD THEM OMG OMG OMGOMGMOGOM GOMGMO GOM GOMGMO OGM OMGMOG OMGO OM G
FOR BODYCAM I WANAN DO BODYCAM MODE omgeorindof

2026 10 08 1259 jorj todo explain - 

repla means

record the whole match u just played
save to a readable format liek .json

read it in the game, with a in game replay editor WHIHC EXISTS RIGHT NOW BTW 

u also should be able to start/stop recording in the game
so that 
idk
the commands idk the commands but 

start/stop
and u can save it like
can be  30 sec, 1 sec
can be 30 min
30 hours
30 da ys
we shoudl be able to make it handle antthing like any size 

but u should be able to make frag movies like skrudge
with sounds and smoothing
and 
motion blur
FOV changes
u can set where the camera is

ALSO
TAS STUFF
RECORD THE ENTIRE ALL UR INPUTS AND POSITIONS
AND U CAN EDIT EACH SINGULAR TICK
AND MKAE 
SUPER ULTRA OVERPOWERED FIGHT SCENES LIKE HITTING THE CRAZIEST MOST  UNLIKLIEST MOVEMENT PATH ok 

but u sh yea
like coooooool frag movies
mw2 quickscope montages
or trickshot motnages

right now P = save last 15 sec of footage and export automaticallt
thats good but has some issues like ragdoll not good, sounds dotn retrigger, as of 2026 10 08 1302 etc other bugs too

but the big idea is
replay record any length not just 15 sec
replay edit like mkae cool movies like source filmmaker u shoudl bea ble to do it in here as well
replay export
combo multiple replays togethr into 1 replay editor rpoject file

todo figure out commands exact commands and what they do
todo figure out  data saving how u can roll like 50 replay separate files into 1 bigger movie file with a lot of replays in it etc 

todo add the google doc  replay editor spec cuz i wrote a bunch about it 

## Export execution and progress

Replay export should be available in headless operation as well as from the
interactive replay editor, using the same export path and result reporting.
The interactive game should show export progress at least once per 60 source
simulation ticks, plus a clear completion or failure result; headless runs
should expose equivalent progress and final status through the existing
diagnostic/output path. Progress reporting must not require rendering every
exported frame in a visible window. This is a desired behavior; supported
headless export and current export reliability remain to be verified.

Ragdoll state and sound retrigger behavior are separate replay fidelity
requirements. An export completing does not establish that either has been
captured and replayed correctly.
