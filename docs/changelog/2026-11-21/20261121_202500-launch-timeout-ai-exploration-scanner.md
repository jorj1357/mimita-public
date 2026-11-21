# Launch-timeout guard, client AI exploration, and event scanner

Date: 2026-11-21
Agent: melee-proof-3weapons

## Source changes

- `src/gui/gui-main.cpp`
  - Room-code wait is progress-aware: 60 s minimum, 45 s no-progress stall,
    300 s maximum.
  - `server.startup.stage` changes advance the progress timestamp.
  - Timeout/server-exit clears the server code and local fallback instead of
    writing `"Room creation timed out"` as a joinable code.
  - Direct local UDP join and the live server process are preserved.
- `src/actor/player-ai.cpp`
  - Removed automatic `npc_spawn_near` fallback spawning.
  - Added no-target exploration using map bounds and `NpcNavGraph` routes.
  - Added a hard 120-tick exploration plan rate limit.
  - Added no-target dash/down-dash and recovery-only freeze use.
  - Added `player.ai.explore` and exploration fields in AI state/action logs.
- `src/engine/engine-tick-net.cpp`
  - Exposes the current AI input frame to combat.
- `src/engine/engine-tick-combat.cpp`
  - Melee client input uses the AI target-gated primary instead of holding
    primary unconditionally.
- `src/network/multiplayer-projectiles.cpp`
  - Corrected `spawnExplosionFx` force-scale argument order.
- `devscripts/event-proof-scan.py`
  - New read-only scanner for room, melee, AI, performance, and error evidence
    across rotated client/server journals.
- `devscripts/dev-profiles/tdm-ai-exploration.json`
  - NPC-free TDM profile for bounded no-target AI tests.

## Build evidence

- Generation 427 published from source containing the GUI and melee-input
  changes.
- Generation 432 published and contains the final exploration rate limit.
- Build failures encountered and worked around:
  - stale packet registry codegen (`python devscripts/packet_registry_codegen.py --check` now passes);
  - unrelated compile blockers and long rebuild attempts recorded in
    `issue-a6890271d414`.

## Runtime evidence

- `logs/2026-11-21/20261121_192618` (generation 427, arena1 TDM):
  - mace claims 259, accepted 114;
  - `player.ai.fallback_spawn=0`;
  - local authoritative player kills 17;
  - no friendly-fire rejection stage in the sampled summary.
- `logs/2026-11-21/20261121_201749` (generation 432, arena1, NPCs deleted):
  - `player.ai.explore=11`, bounded cadence;
  - `player.ai.fallback_spawn=0`;
  - `target_id=0`;
  - dash 24, down-dash 17.
- Proof task `client-ai-exploration-rate-limited`:
  - proof `task-proof-851e73217692`, result `proven`;
  - attached to `issue-f8873319f12b`.
- `logs/2026-11-21/20261121_135248_01` documents the original room-code bug:
  literal `"Room creation timed out"` used as a room code, then
  `coordinator_begin_join` `room-not-found`.

## Not proven / remaining

- The GUI room-timeout behavior has not been exercised under controlled
  CPU/memory pressure or against a deliberately stuck server.
- Human/input-source tagging and an autonomous issue-predicate scanner are not
  implemented.
- Host CPU/memory “AI eyes”, low-power mode, multi-client release gate,
  all-weapon/all-actor matrix, and Caravan/vehicle work remain open.
- Melee feel/visual acceptance remains a separate `human_accepted` barrier.

## Related issues

- `issue-c8ac61cf07fc` — room creation timeout under load.
- `issue-2a5c5da53565` — dev-loop reliability under high CPU/memory.
- `issue-be39a4f5ee17` — AI-run server setup under resource pressure.
- `issue-f8873319f12b` — client AI controller.
- `issue-c8290acca248`, `issue-0539d967d696`, `issue-5534074d9309`,
  `issue-4392f56334ef` — logged follow-up program work.
- `issue-c4f2df8edeba` — cross-agent interference register: stale generated
  artifacts, overlapping edits/builds, ownership attribution, and long-term
  seamless collaboration. Task `cross-agent-interference-register` records the
  first bounded two-agent proof target.
