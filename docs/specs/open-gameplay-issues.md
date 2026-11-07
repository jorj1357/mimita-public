2026 11 07 1716 jorj

big red outlines of enemies nto good

also npcs getting stuck in the ground enemies on funwordl3
ALSO
WE NEED TO MAEKIT SO 
SPAWNPOITNS IN THE GAME IS WHAT SPAWNS ARE USED
NOT THE BELNDER ONES 

2026 11 07 1710 jorj 
position corerciton issue when im in funworld3 map
payload
map editor
likeim getting caught on  nothing right now 
the big spehre 
idk

still having issues where 
60hz svrre its fine
but i am stuck and getting like
reconecint issue
fo

like i build up somethign
idk what tproblem is but it ugh ugh ugh whtvr bro 

2026 11 07 1428 jojr
issue in live game  reconnceingt isuse for no reason
ncps wont update idk hat the problem is 

2026 11 07 1411 jorj
map editor infinite world issue
it onl works with 10x10
not like 25x25 liek iw ant 

its lik eoffset bad
when i get far from the spawn point, it generates not like 
its like  i go 10 meters awa but it thinks im going 25 meters awa
25 meters = like 50 meters
50 =100
it not genrating wher i actually am  its like hardocdede fr me to be 10x10
needto make it work not liek ttha

also how do we put blocks on blocks
so taht i can put tunnels and walls etc for the blocks

and ned the texture rendingg and stuff to work, and npcs moving in the world meanas these generate chunks too
not just me or clients
it is all actors gnereate chunks 

also textures not showing for all blocks 
and im stuck 
in the chunks

like i put a block and it should have a lot of space but i cant walk thru it 
npcs cant walk on the geometry i think eiterh ? idk 

2026 11 07 1333 jorj

"how to amke it so native c++ doesnt have to be compiled
and all things are modules

would it be cool to ahve npc health/pos/target etc in the kernel as well?
givinga s much power to the module as possible so that we dont have to do
builds for
idk adding things like struct npc health, target, going to  npc struct health, target, pos,  like thats fine no issue

i agree thats good, safe gameplay tick is good bc not frame based i want to do it tck based ensure that is in spec 

can u exaplin more how
ignore parallel agent work
we should foucs just on the modules can u explain how we can 

we need to define the actual problems like in numbers and then how modules would work and how we can  solution the problems with numberrs that modules improve 

e.g.
npc behavior ; something that is kinda buried in .cpp
so u need to edit like 5 min,
wait to build, test, like 1-2 min
trial/error,  like 5-10 min? 

if we putin modules
then
edit like 5 min
skip build
trial/error at the same time as edit almost, 5-10 min

but the issue is trial/error is 5-10 min beacues u ahve to start at edit again 

if module works how i think it wokrs, then, 

given npc behavior problem
its in a module
its
problem, implement code chanegs to fix
start the .exe
test with events.jsonl for npc behavior
error found
edit code to fix error according to events.jsonl
that code change happens live, no build, no restarting the .exe
u just see the edit and see if the code change worksk live 

how do we do that, for all systems in the game, live editing, without breaking or making the code more complex or doing thigns that  agents or contriubutors wont understand

we shoul ddo spec writign first and iw ant to understand it directly first"

wi think this is best
we tried this before
if this is true , live edits
for antthing module wise
thatis awesome 
lets do that 

2026 11 07 1203 jorj todo explain
what id 
do we need to ahve server go to like 10 hz 5 hz 
when things are building?
why does tha thappen
how do w sto that

how do we amke it so liek
npcs update on their own thread
well idk
matbe just a solo mode
i wnana have th at
liek speedrun modes like
put checkpoints downinto the map and u ahve to do those as fast as u can 

2026 11 07 1042 jojr 

npcs behavior im watchingthe ai pla as teh npc
and the np

like
the npc will just follow the temamates over adn over
the issue here is 
its like
inefficient
like
idk hwo to explain like the npc will faill
follow an npc that is like 

it just follwos the one that is clsoest
needs to follow 
need to look for an enemy and hten fight with ur team
but need to move much faster and be more efficent with the movement 

NPCS STILL ARE FALLING THRU THE F GROUNDDDDDDD DWHATTTT WHATHERHWHAHTH

2026 11 056 1837
 jorj 

well
okbservartions
funworld3 counte r strike fun mode
tis fun
fun world fun strike
ok so waht do we need
better npc beahvior
that is not so grounded

npcs need to rocket jump
i need to author like specific moves like
npcs need to be doing dash and down dash and like jumping around and consider the environemtn and all their abilities 

also npcs kill me too fast with tehrevovler ALSO
HEALTHALL CMD DOESNT WORKk

2026 11 06 1826
jorj
ok the issue isss


2026 11 06 0828 jorj 
i want to amke world chunks repeating and like
make a new gamemode

i want to be able to make new gamemodes in teh mapeditor in th egame

as well as 
avaatr doesnt show like i will just hav the avatar tat  have disappear

camconfig head camera needs tobe smoothing
more smoothing
but ti good for bodycam mode

but i wnat to do infinit world chunking and connect them and get it to be smooth and loading infinitely even at 100,000 m/s 

also slowodnws


can u invesitgate how coem serve perrfmance is like  doing much worse lower hz and reconnect issue like   also fps is low like fps jitter  framte time suttering  it sas networking is like 1 ms 2 ms 3 ms 6 ms sometimes  and it is slow in the networking like i keep like slow slow slowin gdown then stopping or not being able to connect  phsics also was 6 ms 

12:18 AM
What I found
The server is repeatedly missing its 60 Hz tick budget in the current dev-loop run. Its journal records 308 ticks over 100 ms, with a worst tick of 642.7 ms. During the slow ticks, NPC simulation averaged 52.4 ms and contact/weapon work averaged 88.1 ms. Either can consume several 16.7 ms tick budgets by itself. The measurements are recorded in server.cpp (line 1163), with NPC costs reported by server-npcs.cpp (line 2556).

The client’s recent frame summaries also show jitter: across the last 60 one-second windows, median FPS was about 195, median p95 frame time was 8.0 ms, median p99 was 14.9 ms, and the worst individual frame reached 118.5 ms. Logged networking work on the client was at most 0.046 ms in those windows. So the low FPS and stuttering are not explained by the client’s measured networking time; the server’s long tick stalls are a strong candidate for the freezes and delayed updates you feel. The client frame summaries are emitted by perf.cpp (line 1111).

Reconnect problem
The loop currently lists server PID 23344 at 127.0.0.1:50165 and client PID 33920 as stale. The client journal shows it trying to reconnect to 127.0.0.1:1357, room RCEAVWZ, and reaching ReconnectFailed after ten attempts. The server journal records a reconnect token rejection with players=0. That points to the client retrying against a server session that no longer has its player, rather than merely suffering a few milliseconds of network processing delay. The reconnect health transition is handled in multiplayer-packets.cpp (line 1885), and token rejection is logged in server-packets.cpp (line 2016).

There’s also a run identity inconsistency to resolve: the client journal contains server records from PID 32956, while the run manifest lists PID 23344 as its server. That makes it harder to confidently attribute every client-side event to the server shown in the dashboard.

Assessment
Server tick rate: confirmed severe stalls in this run. NPC and contact/weapon work are the largest measured costs; the evidence doesn’t yet isolate the exact operation inside contact/weapon processing.
Client frame pacing: confirmed intermittent large spikes. The user-visible feel still needs direct review.
Reconnect: confirmed failure against an old address/room with a rejected token. The journal identity mismatch needs tracing before assigning the whole incident to one process.
“Physics 6 ms” / network 1–6 ms: I can’t map those displayed numbers to the journal fields with confidence. The client’s logged networking phase is much smaller; server tick costs are much larger. A screenshot or the exact overlay labels would help identify which timers you’re reading.
I made no code changes and did not restart or replace the running processes. The run evidence is in server-01/events.jsonl, client-01/events.jsonl, and run.json.

12:20 AM
can u see in live journal the issue like ti gets slower and slwoer until it stops
is there somethign that relates to that in repo or change logs

12:22 AM
Yes. I need to correct my previous read: I looked at the journal after the worst slowdown, when the client had already disconnected and its frame rate had recovered. The earlier live entries do show the client getting dramatically slower.

Between 05:16 and 05:18, the client’s one-second summaries report FPS falling to about 52–69, with average frame times reaching 42–53 ms. Individual frames reached 934 ms. During that same stretch, the measured Networking scope peaked at 580–690 ms. At 05:18:58, a 403 ms frame attributed 262 ms to that scope.

The spike details narrow down what “Networking” means here: much of that time was spent in remote-entity updates starting audio. One spike records Audio::StartSound taking about 262 ms inside Network::RemoteEntities. The scope covers more than packet transport, so the overlay’s networking number does not prove the network connection itself took that long. The relevant paths are engine-tick-net.cpp (line 71), multiplayer-tick.cpp (line 2986), and audio.cpp (line 450). This makes remote-entity audio startup the clearest client-side lead in the live evidence.

The server has a separate, serious problem in the same run: it logged 423 fixed-tick stalls over 100 ms between 05:15 and 05:22, with a worst stall of 642.7 ms. NPC simulation and contact/weapon processing repeatedly take far longer than the 16.7 ms budget for a 60 Hz tick. The stalls are frequent, though the records don’t show a steadily worsening server trend; the server was still writing events at 05:22. The client entered ReconnectFailed at 05:19:11, and the dev-loop currently reports it as stale.

This closely matches existing unresolved records. The server tick budget regression says later NPC and weapon-contact stalls remained unresolved. The Nov. 5 investigation changelog records the same gap after an earlier fix: map setup and snapshot-starvation handling were addressed, but later tick overruns remained open. For the failed connection, the frozen-client regression describes a related stale-client/reconnect pattern.

So the journal supports two connected leads: remote-entity audio work contributes to the client’s severe frame stalls, while server tick overruns delay authoritative updates and the client ultimately loses its session. It does not yet prove the exact server-side cause of the reconnect failure. The current run’s server and client journals are here and here.

12:24 AM
can u proove the serverside cause 

12:27 AM
Yes—the live server journal proves the stalls are happening inside the server’s simulation work, even when no clients are connected. It narrows the cause to NPC simulation and the melee/physical-contact phase. It does not yet identify the exact function inside that contact phase.

In the current run, the server recorded 456 ticks taking over 100 ms between 05:15 and 05:28. The median was 147 ms, and the worst took 643 ms. The server is still alive and writing events; the journal shows it is falling far behind its 16.7 ms tick budget, rather than proving the server process itself has stopped.

The phase timings make the cause more specific:

NPC phase: over 16.7 ms in 417 of those ticks; median 51 ms, maximum 448 ms.
Contact/weapon phase: over 16.7 ms in 425 ticks; median 88 ms, maximum 496 ms.
Both phases exceeded 50 ms in 160 ticks.
454 of the 456 stalls report zero connected players, while the server is simulating 19 NPCs.
These are timed in the server tick itself. The NPC measurement wraps simulateSharedNpcs; the contact/weapon measurement wraps melee actions, physical-contact weapons, and world-impact processing in server.cpp (line 1038). That means the stall is not explained by waiting for a remote client. NPC work and contact/weapon processing are consuming the server’s tick time.

The contact phase still groups several functions together, so the journal cannot tell me whether melee actions, NPC weapon contacts, or world-impact processing accounts for its largest spikes. The source shows NPCs continue through animated physical-contact processing even with no human players connected: server-physical-contact.cpp (line 533) and server-physical-contact-actors.cpp (line 439). That makes NPC contact work a strong candidate, but the current logs don’t time those inner functions separately.

This matches the unresolved portion of the server tick budget record: the earlier fix removed a first-tick map-build stall, while later NPC and weapon-contact overruns remained open. The Nov. 5 changelog also says those later overruns were unresolved.

One evidence limit: the running server uses build 2026-11-06T04:43:34Z; the dev loop reports a newer build published at 05:24:59Z, which is not the running server. So these results prove the behavior of the currently running executable, not whether the newer build changes it.

The journals are server-01/events.jsonl and run.json. The next step to prove the exact inner cause is timing those existing contact-phase functions separately in a dev-loop build and comparing their measurements in that build’s live journal.

12:29 AM
yes do that

12:32 AM
I added separate timers for melee actions, physical-contact weapons, and world-impact flushing to the server’s existing stall event in server.cpp (line 1057). The patch passed git diff --check, and I recorded it in the changelog.

I couldn’t build and run it: the dev-loop journal shows a later stop request at 05:44:39 UTC, and its watcher honored that shutdown at 05:46:18 UTC. I left the server process untouched. The new timings still need a fresh dev-loop build and runtime journal before I can identify which contact phase is responsible.

May I restart the existing dev loop to finish that build and runtime check? Starting it again would override the recorded stop request.

12:48 AM
yes 

12:48 AM
Yes—this confirms a server-side tick performance problem in the FCS run.

The instrumented server recorded five tick stalls between 05:56:25 and 05:58:09 UTC. They lasted 101–608 ms. In the worst stall, physical-contact processing took 415 ms; across the five stalls its median was 280 ms. Melee actions and impact flushing were each under 0.5 ms, so they don’t explain the spikes. NPC processing also took up to 213 ms and may contribute.

This shows tickServerPhysicalContactWeapons() was the dominant measured phase in these stalls. It does not yet prove that this phase causes the progressively worsening slowdown or reconnect failures: the run only captured five stalls, and the dev loop’s 45-second check never saw FCS reach its “active” state, though the client did connect.

The evidence and limits are recorded in the changelog. The instrumented build ran through the dev loop, and the final diff check passed.

12:59 AM





2026 11 05 1719 jojr todo also 
animations must be presendted betewen clients better
like
npcs need animatins
plauers need animatins
remote players

client A connects to server A and client B is thre too

with cleint B's existing info that is sent frmo server A , i get what state/what aniamtion client B should be in  and my client handles it servr doenst do the edits and presetnatnon 

2026 11 06 1640

jorj 


we also need json checker for gui bc somethings dont update  some fields in gui things dont eidt in game beahvior 

also need inventor defined better 

# Open Gameplay Issues

## Purpose and audience

This document is the short working index for current gameplay reports, verification work, and ideas that should not be lost.

**Audience:** the player and contributors triaging gameplay and product work.

**Does not:** replace feature-owner specifications, prove a fix, or turn an exploratory idea into a committed implementation requirement. The linked owner documents define behavior; runtime evidence and human review remain separate.

## Status and evidence

- **Active:** a current report or agreed work remains open.
- **Needs recheck:** the user reports that something may be fixed, but it has not been confirmed here.
- **Resolved (user report):** the user says the issue is fixed or no longer wanted; this is not independent runtime evidence.
- **Deferred:** retained for later prioritization.
- **Idea:** a dated design direction, not a delivery commitment.

Priorities: **P0** is the current top project goal; **P1** is high-impact active work; **P2** is follow-up or recheck; **P3** is deferred. Build, runtime, and human-acceptance evidence are distinct.

## Active

| ID | Priority | Issue | Owner / next step |
|---|---:|---|---|
| OGI-001 | P0 | Complete Zombie Tower as a playable mode | [Zombie Tower](gamemodes/zombie-tower/zombie-tower.md); track full-run acceptance there. Floor-count choice remains open in that spec. |
| OGI-002 | P0 | NPC navigation still fails on multi-floor and complex routes | [NPC movement](../architecture/player-npc-systems/npc-movement.md); reproduce Hospital pursuit and validate traversal in Zombie Tower. |
| OGI-004 | P1 | Physical impacts can snag instead of producing the desired bounce | [Movement](movement/movement.md), [collision architecture](../architecture/collision/collision.md); retain the reported Dust2 Cyberia snag and replay evidence. |
| OGI-007 | P1 | Map Editor crash and unreliable bomb-site dragging | [Entity Editor](entity-editor/entity-editor.md); viewport picking omitted bomb sites and now has a source fix pending live drag confirmation. The separate crash still needs reproduction and a first-divergence trace. The user says the editor remains a separate issue. |
| OGI-009 | P1 | Interaction range and prompt behavior are inconsistent; bomb defuse feels too hard to reach | [Counter-Strike](gamemodes/counterstrike.md), [Fun Counter Strike](gamemodes/funcounterstrike.md); consolidate interaction eligibility/range and one reusable popup, then validate plant, defuse, pickup, and interruption. Exact revised range is not decided. |
| OGI-010 | P1 | Scenario recording and replay are not reliably usable | [Scenario recording](dev-loop/scenario-input-recording.md); investigate incomplete/broken recordings, missing metadata, replay checkpoints, and reported performance impact. |
| OGI-011 | P1 | JSON field inventory and stale cross-file references remain incomplete | [JSON configuration](../architecture/json-configuration/json-configuration.md); align runtime loading and validation across configuration families. |
| OGI-012 | P1 | Automatic validation runner should finish from expected scenario events | [Quality gates](dev-loop/quality-gates.md); status retained as previously recorded per user request. |
| OGI-022 | P1 | Freeze/slowdown is followed by reconnect, disconnect, or a failed return to the same server | [Networking](networking/networking.md), [performance](performance/performance.md); capture the exact server/client state and first divergence in a fresh report. User says leaving and rejoining can recover, but the slowdown/freeze still occurs. |
| OGI-023 | P1 | Rejoining can leave team identity and friendly/enemy outlines wrong | [Counter-Strike](gamemodes/counterstrike.md), [gamemodes](gamemodes/gamemodes.md); apply the server-assigned mode team before client outline/hostility presentation. Whether team choice persists across disconnect remains unspecified. |
| OGI-024 | P2 | Server creation should be easier to find and work for local play without coordinator access | [Networking](networking/networking.md); keep local hosting usable when offline and clearly state that the room is local-only and not discoverable to remote players. |

## Needs recheck

| ID | Priority | Issue | Current user update / next step |
|---|---:|---|---|
| OGI-003 | P2 | Actors may sink, pin, or enter closed geometry under pressure | User thinks NPC collision may be fixed. Recheck the reported `atdm`, `tdmtest3`, and `dust2cyberiav4` cases; include player ground failure. |
| OGI-005 | P2 | NPC mesh may disappear while the NPC remains active | User is unsure whether this still occurs and says NPCs seem fine. Confirm a current Fun Counter Strike run before closing. |
| OGI-014 | P2 | NPCs may stop attacking, fail to acquire targets, or ignore live profile edits | User thinks NPC combat may be fine. Recheck in a confirmed TDM run. |
| OGI-015 | P2 | Authored spawn points may not be used by every player/NPC spawn path | User thinks NPC spawn behavior may be fixed. Verify initial join, respawn, NPC spawn, team filtering, and remote edits. |
| OGI-025 | P2 | Fire and Deathball hit damage may not use normal damage presentation | An older note says this may be missing; the user was unsure whether Deathball showed damage. Recheck current impact feedback before deciding whether this remains a defect. [Weapons](weapons/weapons.md), [visuals](visuals/visuals.md). |
| OGI-026 | P2 | Player avatar identity/appearance and saved avatar state may not agree across clients | The 2026-11-04 note says avatars are not replicating/saving reliably. Confirm whether the same selected actor/avatar is visible to self and remote clients and whether saved selection returns after rejoin. [Networking](networking/networking.md), [avatar editor](avatar/avatar-editor.md), [data saving](data-saving/data-saving.md). |
| OGI-027 | P2 | Some female-presenting avatars may not use the expected animations | The 2026-11-03 note says these avatars do not animate like the Jason avatar. Recheck current supported avatar sets against the shared animation contract. [Animations](animations/animations.md). |

## Resolved or no longer wanted

These statuses reflect the user's 2026-11-05 update. They do not claim independent runtime verification.

| ID | Status | Issue | Evidence / owner |
|---|---|---|---|
| OGI-006 | Resolved (user report) | Unexpected Normal Zombie at the player's Fun Counter Strike spawn | User says Fun Counter Strike spawns are fine. [Fun Counter Strike](gamemodes/funcounterstrike.md) |
| OGI-008 | Resolved (user report) | Bomb-site visibility when Entity Visibility is enabled | User says bomb-site visibility is fine. [Entity Editor](entity-editor/entity-editor.md) |
| OGI-013 | Resolved (user report) | Split the dev-loop entry point into focused modules | User says this is done; `devscripts/dev_loop/` exists. [Quality gates](dev-loop/quality-gates.md) |
| OGI-016 | Resolved (user report) | VS Jason preset correction and visual/feel check | User says this is done; no new human review claim is made here. [VS Jason changelog](../changelog/2026-11-01/20261101_155817-vs-jason-role-preset.md) |
| OGI-020 | No longer wanted | Make Fun Counter Strike NPCs easier | User says to remove this request. [Fun Counter Strike](gamemodes/funcounterstrike.md) |
| OGI-021 | Resolved (user report) | Weapon-set override concerns | User says the behavior is fine and to leave it alone. [Gamemodes](gamemodes/gamemodes.md) |

## Deferred

| ID | Priority | Idea | Owner |
|---|---:|---|---|
| OGI-017 | P3 | General melee improvements | [Melee weapons](weapons/melee/melee-weapons.md), [weapons](weapons/weapons.md) |
| OGI-018 | P3 | Focused behavior specifications for Godball and HAFS | [Weapons](weapons/weapons.md) |
| OGI-019 | P3 | Easier ways to equip weapons beyond slot 10 | [Weapons](weapons/weapons.md) |

## Dated idea index

These summaries preserve the product meaning and capture dates of the informal notes. They are ideas unless a linked owner specification says otherwise; personal storytelling and transcription noise are omitted.

| Topic and dates | Preserved direction | Owner documents |
|---|---|---|
| Avatars and replication — 2026-11-03 to 2026-11-04 | Show the selected actor consistently to self and other clients; make avatar replication/saving dependable; give all applicable avatars, including female-presenting avatars, their intended animations. Independent client accounts would help test distinct saved data. | [Networking](networking/networking.md), [animations](animations/animations.md), [avatar editor](avatar/avatar-editor.md), [data saving](data-saving/data-saving.md) |
| Controls, interactions, and inventory — 2026-11-03 to 2026-11-04 | Separate gameplay, chat, editor, and other input contexts; support cross-platform remapping and a shared action surface usable from the game and developer tools. Use one shared interaction path and consistent popup. An `inventory_open` action should open an editable inventory GUI; selecting an owned item should use the normal equip path. Python-based GUI presentation was floated as an option, but no implementation language was selected. | [Counter-Strike](gamemodes/counterstrike.md), [gamemodes](gamemodes/gamemodes.md), [GUI](gui/guiv2.md), [accessibility](../architecture/accessibility/accessibility.md), [weapons](weapons/weapons.md), [JSON configuration](../architecture/json-configuration/json-configuration.md) |
| Editors and creation tools — 2026-11-01 to 2026-11-04 | Make map/entity authoring easier inside the game, including live rotation tuning and a Blender-to-game workflow without repeated direct JSON edits; connect map, video, and future sound creation workflows. | [Entity Editor](entity-editor/entity-editor.md), [Video Editor](creator-tools/video-editor.md), [Music](music/music.md), [project vision](../architecture/vision-mentality/vision.md) |
| Performance and diagnostics — 2026-11-01 to 2026-11-05 | Keep authoritative simulation at 60 Hz and rendering responsive; expose server Hz in F3 as the concrete overlay request. Older notes cite a 240 FPS target, but the archive below records that number as not a current goal. Use detailed, event-linked scenario evidence to find the first bad tick while preserving runtime performance, including earlier concerns about low server tick rate under high NPC counts. Explore camera-near hard/soft culling as an optional presentation technique, without choosing distances yet. The notes also ask for shorter edit-to-feedback cycles and a later development/stable executable distinction, without weakening required validation. | [Performance](performance/performance.md), [Networking](networking/networking.md), [debug logging](debug-logging/debug-logging.md), [scenario recording](dev-loop/scenario-input-recording.md) |
| Weapons and effects — 2026-11-03 to 2026-11-04 | Explore more visible, distinct grenade presentation and stronger bounce/throw distance; continue darkbang experiments; use a large initial blood mark with smaller outward marks and more visible body gore. Exact tuning and presentation remain open. | [Weapons](weapons/weapons.md), [effects](effects/effects.md), [blood and viscera](visuals/blood-bulletholes-viscera.md) |
| Persistence, networking, and sharing — 2026-11-04 to 2026-11-05 | Explore durable RPG progress across sessions/devices and storage that can serve player-created games; the local-versus-centralized/distributed model remains undecided. Make local hosting understandable and support optional sharing. | [Data saving](data-saving/data-saving.md), [Networking](networking/networking.md), [project vision](../architecture/vision-mentality/vision.md) |
| Future modes and worlds — 2026-11-03 to 2026-11-04 | Explore Retrograd/Juggernaut play, infinite generated and non-Euclidean worlds, VR, and social play across devices. One Juggernaut idea is that NPC opponents rush the Juggernaut and make that target easy to recognize. The same design notes question how to make the Juggernaut visually distinct and readable. [Juggernaut](gamemodes/juggernaut.md). Prioritize reusable features over adding content for its own sake. These are not current delivery commitments. | [Gamemodes](gamemodes/gamemodes.md), [RPG](gamemodes/rpg.md), [procedural worlds](procedural-infinite-world/procedural-infinite-world.md), [project vision](../architecture/vision-mentality/vision.md) |

An incomplete 2026-11-03 fragment says Rocket Launcher explosion decals should not “...”; the rest of the request is missing, so no behavior is inferred. Clarify before adding it to the weapon/effects contract.

The original notes also mentioned live avatar/role policy, NPC behavior, grenades, blood, and performance repeatedly. Their current requirements remain in the owner specs above; this index replaces the repeated raw transcript rather than duplicating those contracts.

## Archive — not current work

“Archived” means not currently tracked as active work; it does not assert that
the behavior was fixed. Reopen an item with a current report or evidence.

| Archived item | Decision / evidence | Owner or evidence |
|---|---|---|
| Destructible world in TDM | Disabled by design because inconsistent floors/slopes were not fun; revisit for a mode built around destruction | [Decision changelog](../changelog/2026-10-31/20261031_152055-destructible-world-disabled-default.md), [destructible-world spec](destructible-world/destructible-world.md) |
| 240 FPS for 60 seconds and older frame-pacing reports | The target was not proved, but the user says play felt fine; the number is not a current goal | [Performance spec](performance/performance.md) |
| Older Juggernaut lag, startup, choppiness, and F3-pause reports | Archive unless they recur; the new 2026-11-05 freeze/reconnect report is tracked separately as OGI-022 | [Performance spec](performance/performance.md), [dev loop](dev-loop/dev-loop.md), [networking spec](networking/networking.md) |
| Camera settings overwritten in TDM | Archive unless it happens again | [JSON configuration](../architecture/json-configuration/json-configuration.md), [TDM spec](gamemodes/teamdeathmatch.md) |
| Large client/server position disagreement and ordinary movement rubber-banding | Archive unless it happens again; the cited large correction followed an explicit teleport into blocking geometry | [Movement spec](movement/movement.md), [networking spec](networking/networking.md) |
| Shot/cooldown reliability, shotgun pellet direction, Spy Knife health mismatch, and PvP report | Archive unless any symptom happens again | [Weapons](weapons/weapons.md), [melee](weapons/melee/melee-weapons.md) |
| Bullet-hole marks and broad enemy/damage presentation feedback | User says archive both for now | [Visuals](visuals/visuals.md), [effects](effects/effects.md) |
| Older Duel/Juggernaut avatar appearance, aim feel, head movement, full-screen shadow, and small visual-presentation reports | Archived as older visual/feel reports; reopen with a current example | [Visuals](visuals/visuals.md), [graphics presentation](graphics/graphics-presentation.md), [fullscreen/shadow evidence](../gold/20261024_191700-fullscreen-shadow-small-fix.md) |
| Ragdoll limb tunneling and TDM NPC-versus-NPC death ragdoll | Archive unless it happens again | [Ragdoll](ragdoll-retrograd/ragdoll-retrograd.md), [NPC behavior](npc-behavior/npc-behavior.md) |
| Repeated mode-start/reconnect reliability | Archive unless it happens again; prior fixes have runtime evidence but the full matrix was incomplete | [Networking reliability changelog](../changelog/2026-10-28/20261028_125325-networking-reliability.md) |
| Broad Payload/Zombie Tower map-authoring blocker | Archive with the positive report that the Map Editor made Payload easier; current crash/drag defects stay active above | [Editor feedback](../gold/20261027_020542-map-editor-payload-positive-feedback.md), [Entity Editor](entity-editor/entity-editor.md) |
| Dust 2 ramp “freeze” recording | The recording included freeze input and did not establish a grounded collision stall; the broader current snag report remains active above | [Collision evidence intake](../changelog/2026-10-31/20261031_140500-collision-stepup-and-scenario-evidence.md) |
| Old “presentation first” priority | Superseded by the 2026-11-01 decision that Zombie Tower is the top project priority | [Graphics presentation](graphics/graphics-presentation.md) |

## Reopen rule

Reopen an archived report when a new user observation, scenario, or journal
shows the behavior again. Link the new evidence and compare it with the owner
spec before deciding whether the requirement is missing, the implementation
violates it, or the original report was not reproduced.
