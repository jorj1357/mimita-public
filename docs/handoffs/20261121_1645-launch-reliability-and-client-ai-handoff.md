# Handoff: launch reliability and client AI controller

Date: 2026-11-21
Agent: melee-proof-3weapons
Scope: first implementation slice from the agreed order: launch/resource
reliability, then client AI exploration/abilities, then the broader release gate.

## What is implemented (source only, not yet runtime-proven)

### 1. Room-creation timeout no longer becomes a fake join code
Files: `src/gui/gui-main.cpp`

- Replaced the fixed 60-second room-code deadline with a progress-aware policy:
  - minimum wait: 60 seconds;
  - if no new server startup stage for 45 seconds after the minimum, timeout;
  - absolute maximum: 300 seconds;
  - timestamp advances whenever `server.startup.stage` changes.
- On timeout, the GUI now:
  - clears `gServerLaunchSettings.serverCode`;
  - clears the local join fallback room code;
  - clears the displayed server code instead of writing
    `"Room creation timed out"`;
  - keeps the already-started local UDP join and the server process alive.
- On “server exited before creating a room”, it also clears the error string
  and marks the server stopped instead of leaving a bogus join code.
- The timeout event now records `stalled_for_ms`, min/stall/max limits, logger
  readiness, last startup stage, server journal, map, and mode.

Why: run `logs/2026-11-21/20261121_135248_01` shows the old GUI timeout setting
the room code to `"Room creation timed out"` and then the client attempting
`ice:Room creation timed out` and failing `coordinator_begin_join` with
`room-not-found`.

### 2. Client AI explores instead of walking into a wall
File: `src/actor/player-ai.cpp`

- Removed the automatic `npc_spawn_near 1.5 2` fallback. That fallback was
  adding same-team NPCs, causing friendly-fire melee claims, up to 57 NPCs,
  hundreds of MB of journals, and high CPU/memory in the long proof run.
- Added an exploration goal when there is no hostile:
  - picks the farthest valid ground point from map bounds plus center points;
  - uses `NpcNavGraph::findRoutes`;
  - replans on route exhaustion, goal arrival, or every 120 ticks;
  - falls back to local steering if no route/goal exists.
- Added `player.ai.explore` evidence with goal, distance, route points, and
  plan time.
- Extended `player.ai.state`/`player.ai.action` with `exploring` and
  `explore_goal`.
- No-target abilities:
  - dash on cooldown while moving on the ground;
  - down-dash while airborne and descending;
  - freeze only during progress recovery/stuck handling, not as a movement
    speed boost.
- Fixed stale-route handling when exploration cannot choose a goal.

### 3. AI melee uses the AI's target-gated primary intent
Files: `src/engine/engine-tick-net.cpp`, `src/engine/engine-tick-combat.cpp`

- `engine-tick-net.cpp` now stores the AI input frame for the current fixed
  tick and exposes `lastAiInputFrame()`.
- `engine-tick-combat.cpp` feeds `MimitaMelee::ClientInput` from that frame,
  rather than holding primary unconditionally for any active AI.
- This is what prevents an AI melee sweep from hitting nearby teammates while
  the controller is merely active.

### 4. Unrelated compile blockers fixed
File: `src/network/multiplayer-projectiles.cpp`

- Passed `forceScale = 1.0f` before the optional world-mark pointers in the
  `spawnExplosionFx` calls that had `nullptr` in the float argument.

### 5. New read-only evidence scanner
File: `devscripts/event-proof-scan.py`

- Scans `server-01/events*.jsonl` and `client-*/events*.jsonl`, including
  rotated journals.
- Reports room startup/timeouts, melee claims/accepted/rejected, physical
  damage and kills, AI exploration/targets/fallback/dash/down-dash/freeze, FPS,
  client frame p99, server tick p99, and top error events.
- Current limitation: it is a manual CLI, not yet an autonomous daemon that
  maps predicates to open issues or marks proof records.

### Issues created/updated
- `issue-c8290acca248`: automatic live multi-client release gate.
- `issue-0539d967d696`: autonomous Caravan/vehicle end-to-end proof.
- `issue-5534074d9309`: long-horizon VR/cross-platform/creator/distributed
  program.
- `issue-4392f56334ef`: review/sort the new backlog against `ROADMAP.md`.
- `issue-a6890271d414`: game-DLL build timeout/rebuild friction.
- Existing `issue-c8ac61cf07fc`: room creation timeout under load; this handoff
  adds the concrete “error string became room code” finding.
- Existing `issue-2a5c5da53565`: dev-loop reliability under high CPU/memory.
- Existing `issue-be39a4f5ee17`: AI-run server setup validation under resource
  pressure.

## Evidence collected so far

- `logs/2026-11-21/20261121_140624` (gen420 isolated TDM, AI melee):
  - scanner: mace claims 935, accepted 285; chainsaw claims 100, accepted 36;
    no swordsword.
  - local-player authoritative physical-contact kills: 54 (counted from
    `server.damage.applied` where `attacker_id=1`, `source=physical_contact`,
    `killed=true`).
  - before the fallback removal, this run recorded 24 `player.ai.fallback_spawn`
    events and up to 57 NPCs. The mace number is useful evidence, but the run
    is not the final proof for the new exploration/fallback changes.
- `logs/2026-11-21/20261121_135248_01`:
  - literal room code `"Room creation timed out"`;
  - `network.client.connect.requested` with that string;
  - `network.connect.stage` `room_lookup` -> `not_visible`;
  - `coordinator_begin_join` -> `room-not-found`.
- `devscripts/event-proof-scan.py` is syntax-checked and was run successfully
  against run `20261121_140624`.

No runtime proof exists yet for the new exploration/fallback removal, the GUI
timeout fix, or the target-gated melee input.

## Current build/runtime state (as of handoff)

- Latest observed HEAD: `d651e133` (“much better melee hit registrtion and mace
  has tint ogmgg”); this checkout has uncommitted source changes below.
- Current source changes, uncommitted:
  - `src/actor/player-ai.cpp`
  - `src/engine/engine-tick-combat.cpp`
  - `src/engine/engine-tick-net.cpp`
  - `src/gui/gui-main.cpp`
  - `src/network/multiplayer-projectiles.cpp`
  - `devscripts/event-proof-scan.py` (new)
- Latest published game DLL observed: generation 420, but it does not contain
  these changes.
- Build attempt `game-dll-20261121T162506366821Z-0079b9`, generation 426,
  revision `c5bd2494d1df578026e012e384d9c50ab73f42e619296e2d047ad50247b931a5`,
  was active when this handoff was written. A build-wait command was aborted by
  the user, not by the build; check the root dev-loop status.
- Active shared run: `logs/2026-11-21/20261121_144350` (Payload/funworld3),
  server PID 7556 alive, no client. Treat it as user-owned; do not stop it
  unless the task requires it.
- The earlier daemon became unresponsive and was restarted via the approved
  control path: old daemon 13660, new daemon 30588, watchdog 22088. The user
  server was preserved.

## Left for the next agent

### Must do before claiming this slice
1. Wait for the root dev-loop to publish a build containing the files above.
   Do not invoke a compiler, `build.py`, or a second dev-loop.
2. Verify the build compiles the exploration/GUI changes. If it fails at a
   stale packet codegen stage, regenerate only the normal packet ledger through
   `devscripts/packet_registry_codegen.py` and retry through the dev loop.
3. Start a pinned isolated TDM run on the new generation:
   - spawn with no close hostile and verify `player.ai.explore` > 0;
   - verify `player.ai.fallback_spawn` = 0;
   - verify dash/down-dash events and bounded freeze use;
   - place/wait for a hostile behind cover and verify target acquisition,
     route, and attack;
   - verify melee claims do not target teammates and the target-gated melee
     input produces accepted damage.
4. Run `python devscripts/event-proof-scan.py <run-dir>` and save the output
   with the run identity.
5. Test the GUI room timeout fix under load:
   - launch a Payload local server while CPU/memory is intentionally loaded;
   - verify `network.room-code.wait.timed-out` can be delayed by startup
     progress and never emits a literal error room code;
   - verify local direct gameplay still starts independently;
   - test server-exit-before-room-code too.
6. Attach evidence to `issue-c8ac61cf07fc`, `issue-2a5c5da53565`, and
   `issue-be39a4f5ee17`; do not close them without runtime evidence.

### Still not implemented from the agreed plan
- Human-session/input-source attribution (`human`, `ai`, `scenario`) in events,
  which is required for automatic human-play proof.
- Autonomous scanner daemon that tails journals, matches open-issue proof
  predicates, appends evidence, groups duplicate signatures, and separates
  `runtime_proven` from `human_accepted`.
- Host resource monitor/AI eyes beyond the existing `performance.*` events and
  `devscripts/perf-watch.py`: system CPU, memory load, disk, per-process usage,
  low-power mode, and dashboard/status surfacing.
- Multi-client/human-vs-human release gate, all-weapon/all-actor matrix, bad
  ping/performance injection, and Caravan/vehicle program.
- Tests for `event-proof-scan.py` and a live scanner integration test.

### Constraints
- Preserve the user-owned run `20261121_144350` and unrelated processes.
- Do not revert other agents’ unrelated edits. The shared checkout is dirty.
- Melee feel/visual acceptance remains `human_accepted`; it is not a substitute
  for runtime evidence, and multiple humans are the final confidence barrier.

## Post-handoff progress (same agent, later activation)

Generation 427 was published and verified against the new source.

### TDM target-gated melee
Run `20261121_192618` (arena1, generation 427):
- `player.ai.fallback_spawn = 0`
- mace claims 259, accepted 114
- no friendly-fire rejection stage in the sampled summary
- local authoritative player kills 17
- target acquisition observed (`target_id=200004`, distance ~10 m)

### No-target exploration
Run `20261121_201749` (arena1, generation 432, NPCs removed with
`npc_delete_all`):
- `player.ai.explore = 11` (bounded to roughly one plan per 120 ticks)
- `player.ai.fallback_spawn = 0`
- `target_id = 0`
- dash = 24, down-dash = 17
- no NPC spam, no per-tick exploration log flood

The exploration planner needed a function-local 120-tick rate limit because
the earlier `gRouteTargetId`/`gExplorePlanTick` guard was still being reset or
bypassed. Generation 432 is the verified build for this behavior.

Proof record:
- task `client-ai-exploration-rate-limited`
- proof `task-proof-851e73217692`, result `proven`
- attached to `issue-f8873319f12b`

### Build state after the slice
- Latest published game DLL observed: generation 432.
- Latest build attempt containing this slice: generation 432, published.
- `src/gui/gui-main.cpp` timeout fix compiled in generation 427+, but it has
  not yet been exercised under controlled CPU/memory pressure.
- `issue-c8ac61cf07fc` now records the source fix and the original
  literal-error-room-code evidence; it remains open.

### Reduced remaining work
1. Runtime-test the room-timeout fix under load and against a deliberately
   stuck server.
2. Add human/input-source attribution and make `event-proof-scan.py`
   autonomous for issue predicates.
3. Add host resource monitoring and low-power behavior for the user’s
   CPU/memory-pressure complaint.
4. Run the two-client human-vs-human/all-weapon release gate and the larger
   Caravan/vehicle program.
