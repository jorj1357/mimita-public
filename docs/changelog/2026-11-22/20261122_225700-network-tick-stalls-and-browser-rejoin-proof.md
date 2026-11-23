# Network tick stalls and browser rejoin proof

## Changes

- Fixed the game-module bridge fixed-tick flag so `insideFixedTick()` reflects the server loop's `beginFixedTick`/`endFixedTick` calls. This lets Recast navigation defer a costly full mesh build from the server's fixed-tick path.
- Changed NPC avatar identity discovery to use a background catalog refresh and a cached result. Added bounded constructor phase timing and a one-second retry for empty NPC identities so the initial filesystem scan no longer blocks bulk NPC adoption.
- Fixed queued agent mouse clicks to advance from the owning interactive UI pass as well as the simulation frame. A paused menu can now release the synthetic click without waiting for an unpaused simulation tick.
- Reported the runtime evidence and remaining startup/NPC latency findings to the networking, GUI-control, FCS NPC, startup, and server-performance issue discussions. All behavior issues remain open.

## Build evidence

- The root dev-loop published game-DLL generation 541 from attempt `game-dll-20261122T224214407127Z-4d2a9b`, source revision `33367364f9da22ccbb0e3f56c185d19226c7eb255f66bbb6bc96ad079f4195ad`.
- No separate compiler or dev-loop was started. The isolated test run pinned generation 541 and EXE build `2026-11-22T21:32:48.195707Z:e24aaf16cbfb97461c3d51e25f7f1424c56865155889c6be816a7bc9b9b44e82`.

## Runtime evidence

- Headless Fun Counter Strike run `20261122_224628` used `dust2cyberiav4`, server PID 5444, and client PID 9832. UI actions were issued only through that run's `dev-loop --run-id ... --client client-01 --command` path; no computer-use automation was used.
- `leaveButton`, `confirmYes`, and `serverbrowser_join_7XPE5ZA` each produced an accepted `ui.button_input` event with `clicked=true`, without a separate manual mouse-up command. The browser click sent a join request at 22:48:47.880Z; the server received it at 22:48:48.016Z and emitted `network.server.join.accepted` at 22:48:48.065Z; the client transitioned to Connected at 22:48:48.082Z.
- The client applied 131 snapshots after the rejoin. Maximum interarrival was 1.387 seconds; none reached the 2.5-second stale-peer threshold, and no stale-peer event appeared in the capture.
- The server registered room `7XPE5ZA` in 590.98ms. However, registration started only after `world_load` completed. That stage took 24,563.24ms (`src/network/server.cpp:890-909`); the under-one-second room registration does not meet the user's under-one-second start-to-join target.
- A separate fixed-tick stall reached 709.387ms at tick 426, with 692.652ms in NPC work (`src/network/server.cpp:1634`). Later attribution recorded a 94.005ms shotgun combat call for NPC 100006 (`src/npc/npc.cpp:4055`, `src/npc/npc-combat.cpp:303,619`, `src/combat/weapon-fire-hit.cpp:206`) and a 59.847ms physics / 56.884ms animation update for NPC 100018 (`src/npc/npc.cpp:3514-3518`). These explain server-side delay contributors but do not yet identify the expensive internal operation in each subphase.
- Previous pinned runtime evidence in generation 539 measured Recast query time at no more than 14.75ms after the fixed-tick bridge correction, versus the prior 5.724-second mesh setup; it also showed NPC adoption falling from about 10.1 seconds to about 306ms after async avatar discovery.

## Limits and next step

This run proves a successful community-browser rejoin and sustained snapshots for one local FCS client. It does not prove all reconnect scenarios or operation under arbitrary resource pressure. The network issue remains open: the 24.56-second world-load stage gates room registration, and NPC simulation still produces server fixed-tick stalls. Trace the world-load subphases and expensive NPC combat/physics/animation owners before claiming the under-one-second startup and stable-snapshot goals are met.
