### top issue as of 2026 11 09 1041 jorj

2026 11 08 2100
repo-local workspace + module migration continuation recorded
- canonical workspace rule: docs/architecture/live-development/self-contained-workspace.md
- canonical continuation plan: docs/architecture/live-development/module-migration-plan.md
- goal: behavior lives in modules; cold builds only for kernel/device/transport/packet-schema/loader/public-ABI changes; effectively never for behavior work.

2026 11 08 1212 jorj

2026 11 09 1027 jorj  

issues

1. build time took 18 min
- this cannot happn
- build times since  i think its best to move as much as we can to hot reloadable modules
- make build times as close to 0 sec as possible

if we fix build time issues, hto reloadable is much ebtter

2. things aren't hot reloadable
- want to fix a bug like reconnecting

if we make things hot reloadable, for these next bugs, we can see the effects of our changes live, with no cold .exe kill and restart required 

3. reconnecting for no reason
- like i was fine
- and then i have to reconect
- then i reconnec thten i disconnect adn need to reconnect again
- how come  gpt 6 luna on medium cant see the  events.jsonl logs like 
- like in the exact file rigth next to it 
- whatver

4. Synchronizing map... 984.13s
- like 
- why am i stuck on ttihs
- how am i stuck on this
- what even do we need to  do this for 

### other issues 

2026 11 11 1637 jorj 
space on disk
not good
not good  its just
idk
mimita opening inthe backrgound
for noreason
eating space
need o get rid of this 
itsbeen issue for like a week i think 

2026 11 11 1323

"Current Baseline
Server first tick 1916.778 ms (npc_ms 1215.419); 17 >100 ms tick stalls observed.
Client startup still shows a 1593.75 ms State stall; async map-load benefit is not proven in this main host.
Pre-existing noise: npc-perception host capability 6 unavailable; missing avatar animation JSON files.
42-NPC clean fixture, 240 FPS target, true server conformance, and Agent 2 packet splits are still pending.
Next
Create the 21v21 Payload fixture and start Agent 1 Phase 0: true server tick conformance, client NPC/snapshot diagnostics, and resolve the startup State stall before touching NPC AI performance."

from deepseek v41 flash 1017

2026 11 11 0904 jorj

todo logs folder needs to have 1gb limit total
as well as replays folder
get rid of the oldest?
i think?
or combo into like somethign smaller
idk

but i have to like re delete things over and over bc things int he repo are just taking up so much space for no reasn 

2026 11 10 1008 jorj 

ok just problems in general
60hz making the hhole game suck
we need to like
idk
i think we need to rethink the 
npc behavior?
becuse 
like an actual human or client 
at a lower hz like
u get 30hz of their snapshots
still theyr emoving fast, but u jsut see 1/2 the snapshots
still, smae speed
if the server itself is like 30hz, then u literally the game is at 1/2 the speed
need to be like
not liek that

also cooldown issue
i shoto and shoot and shoot and etc
and then its 
cooldown issue

payload needs an actual map, and map authoring in the game i think we need better for that
infintie world we might be able to  leave alone?
its more exploratory nad i want to do more fighitng

how can we make it fun to fight people i think its like paload and maps and stuff 

we need to also have like
sizing in mind
becasue its gonna be like 
ur super fast so u need like a lot of 
map things
between u and epopel
buttttttt

idk waht the prblem 

its so 
its slow
its slow thats the problem

game needs ot be faster
no more freaking
grneades
all in m face bro 

so we nede to make it ez to edit rpesentation live 
but

what to focus on
editors 
andm aking the

make the game fun first
then
make vidoe editor/repla editor
to m ake edits
i want to do that that is huge  edits

like tdm mode is fun 
its just me shooting people
we can do this
its prob also because its dust 2 map
funworld3 is not balanced for like
its not reallt like a map map
it need slike
walls and sizing 
10x10 meter chunks
i think it might be the map el o el 
thats crazy 

but 
infinite world, vr
4d dimeinsions 
infinite exploration
rooms bigger on the inside
portals
that stuff is huge and epic 

2026 11 10 0922 jojr 
"(request.mode == WEAPON_CHOICE_FORCE) {
    if (valid(request.forceIndex))
        choose(request.forceIndex, WEAPON_CHOICE_FORCE);
        choose(request.forceIndex, WEAPON_CHOICE_REASON_FORCE);
    else
        keep(WEAPON_CHOICE_NO_CANDIDATE);
} else if (request.mode == WEAPON_CHOICE_SCORED) {
    const float currentScore = valid(request.currentIndex)
$ $old='2026-11-10T13:38:43.371704Z'; python devscripts/agent-lane.py run --launch-mode 1 --profile npc-navigation hot-session-20261109 2>&1 | Select-Object -Last 5

build_id=2026-11-10T13:38:43.371704Z:236bc468dd026441ae8c562e85e98c5f8ed7afed38ba2db5d8e7a70ba8363983 executable=C:\mimita-v9\.dev\agent-lanes\hot-session-20261109\.dev\builds\2026-11-10T13-38-43.371704Z\mimita.exe
build_source_revision=75436e453e04055c3aeb76dc0bd3d0a6cced3dfc dirty=True changed_during_build=False source_sha256=39ae81da4c3d8a36de3e680d7a3a752a28c96d78c027a5c4ff7ef49ce74456c3
run_id=20261110_135719 journal=C:\mimita-v9\.dev\agent-lanes\hot-session-20261109\logs\2026-11-10\20261110_135719\dev-loop\events.jsonl
instance=server-01 role=server pid=18940 journal=C:\mimita-v9\.dev\agent-lanes\hot-session-20261109\logs\2026-11-10\20261110_135719\server-01\events.jsonl
instance=client-01 role=client pid=1636 journal=C:\mimita-v9\.dev\agent-lanes\hot-session-20261109\logs\2026-11-10\20261110_135719\client-01\events.jsonl
Build queued. Wait/poll for new build. The previous failed build compiled some; now only the API header changed (fix) and module source; the header change may trigger full rebuild again. The failed build likely compiled many files before hitting the enum error in some TUs; the next build needs to recompile all TUs that include the header. It may take a while. We need to wait and check for errors.

Use a shorter poll command (avoid harness kill): sleep 300s then status/build log. Or poll 20 times. Let's wait 5 min then check. The build may take 4-10 min. We'll use Start-Sleep 300; status; tail. If no build, wait more.

Also, note the run 135719 is active old build; after build, relaunch mode 57. The module source changed; after EXE build and launch, module hot-build gen 12 (weapon choice) should trigger. The stale lock? We removed; module build currently gen 11, no lock. Good.

Let's wait 300s and check.

$ Start-Sleep -Seconds 300; python devscripts/agent-lane.py status hot-session-20261109 2>&1 | Select-Object -First 8; Write-Output '=== compilers ==='; (Get-Process -Name 'cc1plus','g++' -ErrorAction SilentlyContinue | Measure-Object).Count; Write-Output '=== new build dirs ==='; Get-ChildItem 'C:\mimita-v9\.dev\agent-lanes\hot-session-20261109\.dev\builds' -Directory | Sort-Object Name -Descending | Select-Object -First 2 Name; Write-Output '=== build log tail ==='; Get-Content 'C:\mimita-v9\.dev\agent-lanes\hot-session-20261109\.dev\build-output.log' -Tail 15

lane=hot-session-20261109 branch=codex/agent-hot-session-20261109 revision=75436e453e04055c3aeb76dc0bd3d0a6cced3dfc dirty=true source_sha256=7ec55fadc730dcc82daa4974319e614941b5c968ebacd6bb3019ded7830ad293
worktree=C:\mimita-v9\.dev\agent-lanes\hot-session-20261109 status=dirty
build_id=2026-11-10T13:38:43.371704Z:236bc468dd026441ae8c562e85e98c5f8ed7afed38ba2db5d8e7a70ba8363983 executable=C:\mimita-v9\.dev\agent-lanes\hot-session-20261109\.dev\builds\2026-11-10T13-38-43.371704Z\mimita.exe
build_source_revision=75436e453e04055c3aeb76dc0bd3d0a6cced3dfc dirty=True changed_during_build=False source_sha256=39ae81da4c3d8a36de3e680d7a3a752a28c96d78c027a5c4ff7ef49ce74456c3
run_id=20261110_141432 journal=C:\mimita-v9\.dev\agent-lanes\hot-session-20261109\logs\2026-11-10\20261110_141432\dev-loop\events.jsonl
instance=server-01 role=server pid=32548 journal=C:\mimita-v9\.dev\agent-lanes\hot-session-20261109\logs\2026-11-10\20261110_141432\server-01\events.jsonl
instance=client-01 role=client pid=4732 journal=C:\mimita-v9\.dev\agent-lanes\hot-session-20261109\logs\2026-11-10\20261110_141432\client-01\events.jsonl
=== compilers ===
5
=== new build dirs ===

Name
----
2026-11-10T13-38-43.371704Z
2026-11-10T13-28-20.308251Z
=== build log tail ===
  417 |     WEAPON_CHOICE_FORCE = 8,
      |                           ^
src/modules/api/npc-behavior-api.h:399:5: note: previous declaration 'mimita_npc_behavior::WeaponChoiceMode mimita_npc_behavior::WEAPON_CHOICE_FORCE'
  399 |     WEAPON_CHOICE_FORCE = 3,
      |     ^~~~~~~~~~~~~~~~~~~

In file included from src/hot-reload/module-host.h:20,
                 from src\procedural\infinite-world-runtime.cpp:24:
src/modules/api/npc-behavior-api.h:417:27: error: 'WEAPON_CHOICE_FORCE' conflicts with a previous declaration
  417 |     WEAPON_CHOICE_FORCE = 8,
      |                           ^
src/modules/api/npc-behavior-api.h:399:5: note: previous declaration 'mimita_npc_behavior::WeaponChoiceMode mimita_npc_behavior::WEAPON_CHOICE_FORCE'
  399 |     WEAPON_CHOICE_FORCE = 3,
      |     ^~~~~~~~~~~~~~~~~~~


The build log still shows the old error at line 417; the new build started at 14:14:32 (run) and compilers=5; the log is from the previous failed build, not yet overwritten. The source hash changed to 7ec...; build queued/compiling. Wait for completion. The old log tail is stale; check timestamp/last write. It may still be building. We need to wait more.

The run 141432 launched old build. The build is in progress. Wait 5 min and check. The source change (enum fix) invalidated many objects; full rebuild may take ~4-10 min. Let's wait 300s.

We also need to ensure the module build gen 12 after launch. We'll see.

Let's wait.

$ Start-Sleep -Seconds 300; python devscripts/agent-lane.py status hot-session-20261109 2>&1 | Select-Object -First 8; Write-Output '=== compilers ==='; (Get-Process -Name 'cc1plus','g++' -ErrorAction SilentlyContinue | Measure-Object).Count; Write-Output '=== build log tail ==='; Get-Content 'C:\mimita-v9\.dev\agent-lanes\hot-session-20261109\.dev\build-output.log' -Tail 20; Write-Output '=== builds ==='; Get-ChildItem 'C:\mimita-v9\.dev\agent-lanes\hot-session-20261109\.dev\builds' -Directory | Sort-Object Name -Descending | Select-Object -First 2 Name"

these things 
need to  
no happenign 
annmore
all hneeds to be module
nothing heren eesa  rebuild 
needs rebuild never ever 

2026 11 09 2343 moevment sourcejorj movemt soure not hcanging 
the actual behavior
i disable dash and stuff but it doenst disable it for me 

2026 11 09 1600 glb map udates map editor a whole block is just missing bro funworld3 and pos corerction servre errors , as well as  map editor it onl shows the boxes for like, idk what it is but its like arranged in a differnet way than it actuallt is on the funworld3 actual map, so not sure what to do or how to fix 

2026 11 09 1504 jorj  - everu time a agnet makes a new lane , the agent mkaes a new dev loop gui show up, not good 

2026 11 09 1029 jojr  todo make it so dev loop launchgin that just 1 single gui, make th gui like its own thing not part of dev-loop 


2026 11 09 1001 jorj 

"I checked the current module catalog and source ownership audit. **For this issue, the only hot-reloadable networking source is** [`network-policy-module.cpp`](/C:/mimita-v9/src/network/network-policy-module.cpp), registered as `network-policy` in [`modules.json`](/C:/mimita-v9/config/modules/modules.json). The audit passes with 9 module sources and 579 EXE sources.

That module can change some snapshot and reconnect *policy* live. It does **not** currently own the map-ready handshake or the decision that actually changes the client to `Reconnecting`. Changes to the EXE-owned C++ files below won’t take effect in the already-running game; the dev loop can build a new EXE, but that EXE must then be launched.

### Cold files directly involved

These are the files most likely to matter for the fixes we discussed:

| File | Why it may need to change |
|---|---|
| [`src/engine/engine-tick-state.cpp`](/C:/mimita-v9/src/engine/engine-tick-state.cpp:112) | Loads the initial default map and later loads the requested join map. This is the main owner for avoiding the unnecessary Dust2 load. |
| [`src/engine/engine-tick-net.cpp`](/C:/mimita-v9/src/engine/engine-tick-net.cpp:749) | Checks that the local world matches the server’s required map, then sends `CLIENT_MAP_READY`. Needed only if changing that gate or its reporting. |
| [`src/network/multiplayer-packets.cpp`](/C:/mimita-v9/src/network/multiplayer-packets.cpp:2042) | Builds the “synchronizing map” status text and owns connection-health checks, the snapshot-timeout event, reconnect startup, and client-side ICE setup. |
| [`src/network/connection-health.cpp`](/C:/mimita-v9/src/network/connection-health.cpp:15) | Makes the actual connection-state decision. In this run, snapshot starvation caused the transition to `Reconnecting`. This is the key candidate for moving the decision behind the hot policy module. |
| [`src/network/multiplayer-tick.cpp`](/C:/mimita-v9/src/network/multiplayer-tick.cpp:1362) | Handles join/welcome and reconnect packets, map-ready flag resets, incoming snapshots, and recovery state. |
| [`src/network/multiplayer-context.h`](/C:/mimita-v9/src/network/multiplayer-context.h:615) | Holds the connection and map-readiness state those files share. Only needed if we add or change tracked state. |

### Cold files needed only if the journals point there

- **Server stops sending or mishandles readiness/spawn:** [`src/network/server-packets.cpp`](/C:/mimita-v9/src/network/server-packets.cpp:2781) builds and sends snapshots and handles map-ready/spawn packets; [`src/network/server.cpp`](/C:/mimita-v9/src/network/server.cpp) owns the server loop.
- **ICE signaling or transport stalls:** [`src/network/server-ice.cpp`](/C:/mimita-v9/src/network/server-ice.cpp), [`src/network/coordinator-client.cpp`](/C:/mimita-v9/src/network/coordinator-client.cpp), [`src/network/coordinator-client.h`](/C:/mimita-v9/src/network/coordinator-client.h), [`src/network/ice/ice-agent.cpp`](/C:/mimita-v9/src/network/ice/ice-agent.cpp), and [`src/network/ice/ice-agent.h`](/C:/mimita-v9/src/network/ice/ice-agent.h). Transport interface headers—[`ice-transport.h`](/C:/mimita-v9/src/network/ice-transport.h), [`game-transport.h`](/C:/mimita-v9/src/network/game-transport.h), and [`udp-transport.h`](/C:/mimita-v9/src/network/udp-transport.h)—are only candidates if the fault is in that interface.
- **Snapshot chunks are sent but fail to assemble:** [`src/network/snapshot-chunks.cpp`](/C:/mimita-v9/src/network/snapshot-chunks.cpp) and [`src/network/snapshot-chunks.h`](/C:/mimita-v9/src/network/snapshot-chunks.h).
- **Wire format itself is wrong:** [`src/network/packets.h`](/C:/mimita-v9/src/network/packets.h) and possibly [`src/network/packets.cpp`](/C:/mimita-v9/src/network/packets.cpp). The current evidence doesn’t point to a packet-format problem.

### Not EXE rebuilds

[`devscripts/dev-launch-modes.json`](/C:/mimita-v9/devscripts/dev-launch-modes.json:522) can pass `funworld3` to mode 56’s client on the next launch; that’s launch configuration, not a hot module or an EXE source file. [`config/networkingconfig.json`](/C:/mimita-v9/config/networkingconfig.json) is also separate: networking settings are hot-reloaded, but changing the timeout there would tune the threshold, not explain why server traffic stopped.

One correction to my previous message: `main-systems.cpp` contains another `dust2cyberiav4` declaration, but it has no references. It isn’t part of the active map-loading path and shouldn’t need an edit for this fix."

all these files we need to mkae into hot reloadable m odules
and packets.h too big
need to make that  not be included evurwhere


# Open Gameplay Issues

Last updated: 2026-11-09

This is the short, current index of reported problems and follow-up work. The
first five entries are the current focus. Status describes the evidence
available; it does not imply a fix unless stated.

## Purpose and audience

**Audience:** the player and contributors triaging gameplay, networking, and
build-feedback problems.

**Does not:** replace feature-owner specifications, prove a fix, or turn an
exploratory idea into a committed implementation requirement. Linked owner
documents define behavior. Build, runtime, and human acceptance evidence stay
separate.

## Status and evidence

- **Active:** a current report or agreed verification work remains open.
- **Needs recheck:** the user reports improvement or uncertainty, but the
  behavior has not been confirmed in a current run.
- **Resolved (user report):** the user says it is fixed; this is not independent
  runtime evidence.
- **Deferred:** retained for later prioritization.
- **Idea:** a dated design direction, not a delivery commitment.

Priorities: **P0** is the current top focus; **P1** is high-impact active work;
**P2** is follow-up or recheck; **P3** is deferred. A code change or successful
build alone does not prove that runtime behavior works.

Items not revised in this pass keep their last recorded status; this pass did not
retest them or make new runtime-acceptance claims.

## Active

| ID | Priority | Issue | Current status and next step |
|---|---:|---|---|
| OGI-028 | P0 | Reconnects or disconnects even when the connection appears fine | User reports that reconnect starts after 2.5 seconds without authoritative snapshot traffic. Verify the trigger and first missing step in a fresh client/server run with matching run, process, endpoint, room, and journal identities; determine whether the snapshot stopped or the timeout policy is wrong. The 2026-11-08 reconnect change is recorded as compiled but not runtime-verified. [Networking](networking/networking.md), [reconnect regression](../regressions/networking/frozen-client-dead-REG-2026-11-04.md), [ICE reconnect change](../changelog/2026-11-08/20261108_1724-ice-transport-reconnect-rebuild.md). |
| OGI-029 | P0 | Client loops between “Synchronizing map” and reconnecting or stays on “Synchronizing map” | The reconnect accept now carries the authoritative map id and the client preserves its previous required map when an older peer omits it; reconnect re-arms the map-ready handshake. The server-phase budget thresholds are hot policy. Needs a fresh identity-matched mode-56 run to prove `network.map-sync.state`, `network.snapshot.timeout`, and recovery in `events.jsonl`. [Networking](networking/networking.md), [map snapshot evidence](../changelog/2026-11-08/20261108_055845-live-map-geometry-authority-remote-snapshot.md). |
| OGI-030 | P0 | Make networking and NPC behavior hot-reloadable and verify the recent modularization changes | The target is edit behavior and see the fix live without an EXE build. `network-policy` now also owns server phase-budget policy; map-sync/reconnect remain its hot state machines. `npc-perception` now owns FOV/range/LOS gating, memory, belief, and prediction through a bounded world-query capability. Transport, packet schema, map/loader, world/actor storage, and the fixed tick remain cold mechanisms. Prove activation and behavior in a fresh server/client journal; do not count unrelated pre-existing gate failures as new regressions. |
| OGI-031 | P1 | Reduce build wait and keep routine agent work inside the repository | Work notes report a 1,070.877-second (~17.8-minute) cold build and a 1.157-second no-op; I could not locate the corresponding machine-readable timing record. Capture a comparable attempt with phase and per-file timings, then target the measured bottleneck. Lane worktrees now live under `.dev/agent-lanes/`, scheduler diagnostics under `.dev/build-scheduler/`, temporary files under `.dev/tmp/`, and the shared cache under `.dev/ccache/`. `devscripts/path_containment.py` rejects escaping paths; an outside path remains a documented human emergency only. Do not impose a speculative “10 symbols per header” rule before measuring header fan-out. [Build-time observability](dev-loop/build-time-observability.md), [agent build authority](dev-loop/agent-lanes-build-authority.md), [build operations](../operations/build-and-exe/build-and-exe.md), [cold-build debt](../regressions/cold-build-required-REG.md). |
| OGI-032 | P2 | Dev-loop helper windows can appear repeatedly and interrupt work | The 2026-11-08 report describes command windows stealing focus; the visible-window regression remains only partially verified. Reproduce current dashboard/helper behavior and confirm one intended UI with no foreground console churn. [Visible game-window regression](../regressions/dev-loop/visible-game-window-REG-2026-11-08.md), [dev-loop](dev-loop/dev-loop.md). |
| OGI-033 | P1 | Agent lanes open duplicate dashboard windows and status is hard to read | The main checkout owns one coordinator dashboard; ensuring or running a lane must not open a window. The dashboard lists the main checkout and registered lanes with build phase/elapsed time and runtime counts, plus one shared `Updated N seconds ago` indicator. Show discoverable MiMITA processes in a readable, scrollable table with lane, role, PID, state, and connection target when known. Keep build/run controls routed to the owning checkout. Validate multi-lane and multi-client/server visibility plus stale/unavailable lane handling. [Dashboard contract](dev-loop/dashboard.md), [multi-instance topology](dev-loop/multi-instance.md), [agent build authority](dev-loop/agent-lanes-build-authority.md). |

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
| OGI-022 | P1 | Freeze/slowdown is followed by reconnect, disconnect, or a failed return to the same server | Separate from OGI-028, which tracks reconnects without an obvious preceding freeze. [Networking](networking/networking.md), [performance](performance/performance.md); capture the exact server/client state and first divergence in a fresh report. User says leaving and rejoining can recover, but the slowdown/freeze still occurs. |
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
