# Death presentation diagnostics

## Final state

Added bounded, structured diagnostics only. No ragdoll, death-state, HUD,
network snapshot, or gameplay behavior was changed.

## Source changes

- `src/network/multiplayer-interpolation.cpp`, `updateRenderedReplica` after
  the dead-state calculation: records `client.dead.presentation.state` when a
  dead replica still carries movement flags or one-shot presentation serials.
- `src/network/server-players.cpp`, `makePlayerEntity`: records
  `server.dead.snapshot.state` when a dead server player is still broadcasting
  movement flags or presentation serials. The diagnostic is placed after the
  serial fields are populated and is rate-limited to approximately once per
  second.
- `src/combat/death-system.cpp`, `DeathSystem::kill`: extends the existing
  `death.kill.enter` event with `last_damaged_by`, `killed_by_before`, and
  `killed_by_weapon` so raw local fallback attribution can be compared with
  authoritative attribution.
- `src/ragdoll/ragdoll-mode.h/.cpp`, `RagdollModeSystem::updateCorpses`: adds a
  per-corpse diagnostic flag and records
  `ragdoll.corpse.world_collision_escape` once when a corpse crosses Z=-500,
  including collision-mesh count, position, and velocity. It does not alter
  physics.

## Existing evidence

- `logs/10-08-2026/20261008_142151/events.jsonl` proves that remote and local
  death presentation reaches `ragdoll.corpse.spawned`,
  `ragdoll.corpse.render_submitted`, and `ragdoll.corpse.first_update`.
- The same journal shows mixed behavior: some corpses settle near Z=379,
  while others fall continuously to approximately Z=-2,800 before the normal
  20-second lifetime removal. This rules out a universal disabled-ragdoll
  explanation and identifies world-collision availability/query behavior as
  the first ragdoll divergence to inspect.
- Source tracing shows remote presentation continues through dash, jump,
  walking VFX/audio, and procedural animation after `player.dead` is set;
  server snapshots also pass client visual flags and serials through unchanged.
- `DeathSystem::kill` resolves `effectiveKiller` from `lastDamagedBy` for
  killfeed/replay but writes the raw `killer` argument to `victim.killedBy`.
  The HUD reads `victim.killedBy`, which explains a credible path to
  `you died to unknown`.
- The HUD death overlay is directly gated by `player.dead` and has no
  independent fade lifetime, so it can remain visible during spectator time.

## Validation

- `git diff --check`: passed.
- Canonical `python build_agent.py`: passed after one diagnostic-only compile
  correction; final build status `SUCCESS`, compiled 1 translation unit.
- `C:\mimita-v9\mimita.exe --versioninfo`: passed. Journal identity:
  `logs/10-08-2026/20261008_153426/events.jsonl`, run id `20261008_153426`.
- No live gameplay scenario was launched in this investigation, so the new
  diagnostic events are not yet runtime-proven in a fresh Juggernaut round.

## Fresh Juggernaut runtime evidence

- Dev-loop run: `20261008_193450`.
- Build: `1730`, executable
  `C:\mimita-v9\.dev\builds\1730\mimita.exe`, source-current and matched on
  server/client.
- Map/mode: `dust2cyberiav4`, Juggernaut launch mode 9.
- Journal: `C:\mimita-v9\logs\2026-10-08\20261008_193450\events.jsonl`.
- `client.dead.presentation.state` occurred 80 times. Example actor 100129
  remained at health 0 while carrying `state_flags=1`, dash serial 123, air
  jump serial 41, down-dash serial 105, and ground-jump serial 5.
- `server.dead.snapshot.state` occurred 49 times. Example actor 1 remained at
  health 0 while the server broadcast `state_flags=32` plus dash, jump,
  direction-change, and down-dash serials.
- `ragdoll.corpse.spawned` occurred 53 times and position samples showed
  corpses settling near the map surface. `ragdoll.corpse.world_collision_escape`
  occurred 0 times in this run, so the collision escape remains intermittent or
  map/location-dependent rather than the first failure in this scenario.
- One local authoritative death was observed as
  `client.local.death.applied` (health 79 -> 0); no local `death.kill.enter`
  event appeared in this network run, confirming that the local HUD attribution
  path needs a separate network kill-event trace before changing it.

## Human review still needed

The first fix boundary is now proven: sanitize dead presentation state at the
server snapshot owner and/or gate dead presentation at the client owner. Keep
corpse collision as a separate follow-up because this run did not reproduce an
escape. Killer attribution still needs the reliable kill-event/local-HUD trace
before implementation.

## Implemented fix and fresh runtime proof

- `src/network/server-players.cpp`, `makePlayerEntity`: dead player snapshots
  now clear state flags, weapon presentation state, and dash/jump/freeze
  serials. The existing diagnostic became `server.dead.snapshot.sanitized` and
  records before/after values.
- `src/network/server-npcs.cpp`, `makeNpcEntity`: applied the same contract to
  dead NPC snapshots. Added `server.dead.npc.snapshot.sanitized` with
  before/after values.
- `src/network/multiplayer-interpolation.cpp`, `updateRenderedReplica`: dead
  replicas no longer run movement VFX/audio or procedural movement animation;
  stale serial cursors are advanced and one-shot flags are cleared so delayed
  packets cannot replay effects after death.

Fresh runtime:

- Build 1737, matched server/client executable:
  `C:\mimita-v9\.dev\builds\1737\mimita.exe`.
- Juggernaut run id: `20261008_194153`.
- Journal: `C:\mimita-v9\logs\2026-10-08\20261008_194153\events.jsonl`.
- `server.dead.snapshot.sanitized`: 8.
- `server.dead.npc.snapshot.sanitized`: 17.
- `server.dead.snapshot.state`: 0.
- `client.dead.presentation.state`: 0.
- `ragdoll.corpse.spawned`: 9.
- `ragdoll.corpse.world_collision_escape`: 0.
- `client.local.death.applied`: 1.

This proves the server now clears stale state for both players and NPCs, and
the client observed no dead actor carrying the old live-presentation state in
the fresh run. Human visual confirmation of silent/frozen dead bodies remains
separate from the journal proof.

## Pose-handoff diagnostics and visible runtime proof

Implemented bounded, owner-level diagnostics in `src/ragdoll/ragdoll-mode.cpp`
and `src/ragdoll/ragdoll-mode.h`:

- `ragdoll.corpse.pose_step_trace` records each corpse's physical part positions
  and orientations before and after its first fixed-tick physics step, including
  moved-part count and maximum translation/rotation.
- `ragdoll.corpse.pose_render_trace` records the same corpse pose when it is
  first submitted to the renderer.
- These diagnostics do not change ragdoll behavior and are emitted once per
  corpse.

Build evidence:

- `python build_agent.py` completed successfully, compiling 14 units and
  returning code 0.
- Fresh published executable: `C:\mimita-v9\.dev\builds\1746\mimita.exe`.
- `--versioninfo` identified that executable and recorded its independent
  journal at `C:\mimita-v9\logs\2026-10-08\20261008_195125\events.jsonl`.

Runtime evidence:

- Visible Juggernaut run: build `1746`, run id `20261008_195110`.
- Server and client were ready on the same build; the client was left running
  for manual play/reproduction.
- Journal:
  `C:\mimita-v9\logs\2026-10-08\20261008_195110\events.jsonl`.
- After the first 30 seconds: `pose_step_trace=10`,
  `pose_render_trace=10`, `ragdoll.corpse.spawned=10`,
  `ragdoll.corpse.world_collision_escape=0`, and
  `client.dead.presentation.state=0`.
- A representative pose-step event reported six of six physical parts moved,
  with `max_translation=0.7971063` and
  `max_rotation_degrees=4.9988737`. The corresponding render trace contained
  all six physical parts with matching positions/orientations for renderer
  submission.

Interpretation: this run proves the corpse enters physics, changes pose, and
reaches the renderer. It does not yet prove the final visual appearance in the
user's exact spectator camera is satisfactory; the visible executable remains
running for that human check. The requested legacy "died to unknown" issue was
intentionally not changed.

## Spectator corpse update fix and heartbeat diagnostics

The prior journal identified the spectator-specific cause: the render loop kept
drawing corpses, but `engine-tick-replay.cpp` skipped `simulateTick` whenever
forced spectator freecam was active. Corpse physics lived inside that skipped
tick, so a corpse could render forever at its first pose while active play
continued to update normally.

Implemented:

- `src/engine/engine-tick-replay.cpp`: spectator freecam now runs only the
  corpse ragdoll fixed-tick update while normal gameplay simulation remains
  disabled.
- `src/ragdoll/ragdoll-mode.cpp`: added bounded
  `ragdoll.corpse.heartbeat` events with spectator-mode context, update count,
  life tick, render submission count, blood-effect count, position, velocity,
  and fastest-part speed.
- `src/ragdoll/ragdoll-mode.h`: added per-corpse counters for updates, render
  submissions, and blood effects.
- Increased the bounded corpse pool from 12 to 64 so a Juggernaut round with
  up to 18 actors does not evict fresh corpses merely because a full round is
  being watched from spectator mode.

Build/runtime evidence:

- Canonical `python build_agent.py`: success, return code 0, 8 translation
  units compiled.
- Visible Juggernaut run: build `1751`, server/client matched, client ready.
- Executable: `C:\mimita-v9\.dev\builds\1751\mimita.exe`.
- Run journal:
  `C:\mimita-v9\logs\2026-10-08\20261008_201953\events.jsonl`.
- `--versioninfo` journal:
  `C:\mimita-v9\logs\2026-10-08\20261008_162007\events.jsonl`.
- After approximately 35 seconds: 15 corpse spawns, 91 heartbeat events, zero
  corpse evictions, 15 pose-step traces, 15 pose-render traces, and zero
  stale `client.dead.presentation.state` events.
- Heartbeats show `life_ticks` increasing from 60 to 240, render submissions
  increasing, and blood-effect counts increasing, confirming physics and blood
  continue during ordinary play.

The visible run remains open for the real spectator transition. A heartbeat
with `spectator_mode=true` after the player dies is the final runtime proof
that the freecam-specific path is active; no synthetic death was injected.
