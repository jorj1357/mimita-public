# Fun CS defuse, plant/defuse progress readout, and NPC stall diagnosis

- Timestamp: 2026-11-04 06:10:00 EST (2026-11-04T11:10:00Z)
- Branch/commit: `20261011zt` / `53faec75`
- User report: holding F did not defuse in Fun Counter Strike; asked for a
  client-facing readout with plant/defuse seconds, preferably in the world for
  all players; also reported NPC freezes after death and after `healthall`.
- Pre-existing edits: preserved all dirty worktree files. Vehicle, config,
  docs, and physical-entity changes were already present and were not reverted
  or claimed.

## What changed

### Defuse eligibility (server authority)

- `src/network/server-gamemode.cpp`
  - Removed the designated-defuser gate from the human-player candidate loop.
    The gate remains for NPCs only.
  - The active human defuser is kept stable while they hold F, then the nearest
    eligible human is chosen, then the designated NPC.
  - Resets `teamBrainA.reportedDefuserId` whenever the bomb is not planted and
    at round assignment, so instant-respawn actors cannot permanently hold the
    designation across rounds.
  - Added bounded `objective.interaction-rejected` evidence with actor, team,
    distance, range, objective position, designated defuser, and reason.
  - Added seconds and distance fields to plant/defuse start and progress events.
  - Added `position` and `explosion_seconds` to `objective.planted`.

### Client HUD and world readout

- `src/engine/engine-tick-ui-overlays.cpp`
  - Prompts now show total seconds; active progress shows elapsed / total
    seconds plus percentage and tap count.
  - Added a world-anchored `objectiveWorldText` readout above the bomb showing
    `PLANTING x.xxs / y.yys`, `DEFUSING x.xxs / y.yys`, or the planted bomb
    countdown. It reads replicated progress so every client in the match sees it.
- `config/gui/gamemode-meta-gui.json`
  - Added `objectiveWorldText` to the `counterstrike` and `funcounterstrike`
    layouts.

### NPC freeze diagnostics and first performance fix

- `src/network/server-npcs.cpp`
  - Added `npc.tick-cost` with phase totals: NPC AI, navigator, recovery, pair
    collision, respawn, rebuild, trace.
  - Added `npc.containment-cost` for recovery/trace containment queries.
- `src/npc/npc.cpp`
  - Added bounded `npc.slow-update` with per-phase timings: sense, pick, gather,
    commit, navigator, path, physics, combat.
- `src/npc/npc-navigator.cpp`
  - `findRoutes` now asks for one route instead of `navGraphMaxRoutes` (3).
    The caller only ever consumed `routes.front()`, so two extra A* searches per
    replan were pure waste.
- `src/debug/structured-log.cpp` and `src/game/game-cli.cpp`
  - `--server` command lines now take precedence over an inherited
    `MIMITA_PROCESS_ROLE`, so locally hosted servers stop logging as `client`.
- `src/audio/audio.cpp`
  - Legacy world-sound attenuation is sampled every 10 s instead of per voice.
- `src/physics/movement/physics-collision-dispatch.cpp`
  - Trivial grounded limb bounces no longer emit 10 events/second.

### Specifications

- `docs/specs/gamemodes/funcounterstrike.md`: plant duration aligned to the
  shipped 5 s (was 1 s in the spec).
- `docs/specs/gamemodes/objective-hud.md`: requires elapsed/total seconds and
  the world-anchored progress readout; documents `objectiveWorldText`.

## Evidence

Separate claims:

- Source: changes above.
- Build: dev-loop canonical builds succeeded. Final build
  `2026-11-04T11:07:00.726575Z`.
- Defuse runtime proof (build `2026-11-04T10:35:33Z`, journal
  `logs/2026-11-04/20261104_104100/events.jsonl`):
  - `objective.defuse-start` with `operator=1`,
    `operator_kind="player"`, `designated_defuser=100002` (NPC).
  - `objective.defused` with `operator=1`.
  - `round.result` winner `Counter-Terrorists`.
- Rejection diagnostic proof (build `2026-11-04T11:07:00Z`):
  - `objective.interaction-rejected` with `reason="out_of_range"`,
    `actor=1`, `distance_m=23.70`, `range_m=6.0`, and the bomb position.
- Freeze-first-divergence proof:
  - `npc.tick-cost` showed `npc_ai_ms` was ~99% of the stall.
  - `npc.slow-update` showed `navigator_ms` was ~99% of that NPC phase.
  - After `maxRoutes=1`, slow NPC updates fell from ~122 per 70 s to 16 per
    75 s in comparable Fun CS runs.
- Human visual review: still required for the N-second world text placement,
  readability, and multiplayer visibility. Build success is not acceptance.

## Gold document

- Added `docs/gold/2026-11-04-events-jsonl-only-improvement-loop.md`.
- It records the preferred direction: one loop of idea/bug -> code -> compile/link ->
  live `mimita.exe` -> check the specified behavior in `events.jsonl`, repeating with
  better logging when the behavior is wrong.
- It explicitly demotes work outside that loop (agent gate as an everyday completion
  ritual, scenario-first responses, parallel debug files, unrelated matrices) and
  prioritizes speed, the first divergence, and runtime evidence.

## Verification limits

Per the user's instruction, no agent gate, no scenario files, and no
`--objective-selftest` were run. Proof is the live dev-loop executable and its
`events.jsonl` journals. The remaining NPC plan spikes are reduced but not
eliminated; the next candidate is sharing graph routes across NPCs with the same
goal instead of planning per actor.

## Follow-up: healthall 500, respawn, and multi-instance presentation

### NPC/fixed-tick follow-up

- Skipped the custom global route graph when Recast is the authoritative backend.
- Skipped deep containment for below-map NPCs until their scheduled respawn.
- Accelerated pointInsideSolidId through the collision tree and made
  worldPointInsideSolid test only containing solid AABBs.
- Bounded NPC safe-spawn candidate search to 12 candidates / 4 ms in recovery.
- Clamped actor-pair collision sweeps to 8 m so teleports/respawns cannot create
  tens of thousands of broadphase cells.
- Added 
pc.pair-cost, 
pc.tick-cost, 
pc.slow-update, and
  
etwork.snapshot.timeout evidence.

Live result (build 2026-11-04T13:56:08Z, run
logs/2026-11-04/20261104_135620): 0 fixed-tick stalls in the last
~2.5 minutes of play, 8 clean player respawns, pair-cost=0,
slow-update=0, containment=1. Remaining stalls were match-start only.

### Multi-instance presentation

Specs:
- Added docs/specs/dev-loop/multi-instance.md.
- Rewrote docs/specs/debug-logging/canonical-jsonl.md for one run folder with
  one journal per executable.
- Added the multi-instance section to docs/specs/dev-loop/dev-loop.md.
- Routed the new documents from docs/ROUTER.md.

Implementation:
- devscripts/dev_loop/instances.py: run layout and atomic un.json manifest.
- dev-loop.py: per-instance journals, MIMITA_INSTANCE_ID, MIMITA_LAUNCH_ID,
  manifest writes, instances[] status, per-instance state derivation,
  periodic state save, and journal readers that scan all instance journals.
- Native/terminal dashboard rows show instance, role, state, PID, snapshot age,
  and journal path.
- StructuredLogger now includes instance_id and launch_id; MIMITA_RUN_ID`n  wins over the instance folder name.
- Client snapshot starvation now emits 
etwork.snapshot.timeout and enters the
  reconnect state instead of replaying the last frame silently.

Live proof (run logs/2026-11-04/20261104_142829):
- un.json lists server-01 and client-01 with separate journals.
- logger.started reports un_id=20261104_142829 for both.
- stop-server produced server-01=stopped, client-01=frozen,
  snapshot_age_ms≈21591, plus 
etwork.snapshot.timeout and 4
  
etwork.session.health reconnect transitions in the client journal.
- New N-server/N-client start/stop controls remain the next Phase 3 step.

- Added read-only dev-loop.py --list-instances [--json] for the new per-instance rows.

## Follow-up: multi-client dashboard controls and testing docs

- Added native/terminal dashboard process rows with counts, PID, state, uptime,
  snapshot age, and journal path.
- Added dashboard buttons: Start server, Start client, Stop selected, Restart selected.
- Added CLI: --start-server, --start-client, --stop-instance, --restart-instance,
  --client, --name, --badconn-preset.
- Additional clients get their own instance id, journal, command queue, badconn preset,
  and process record. Commands route by --client.
- Added docs/workflows/multi-client-dev-testing.md and
  docs/specs/networking/bad-connection-simulation.md, routed from ROUTER.
Live evidence (run logs/2026-11-04/20261104_145751):
- server-01 + client-01 + client-02 all listed with separate journals.
- Both clients reported `remote_players=1` and `remote_npcs=9` with
  `snapshots_missed=0`, proving they saw each other on the same server.
- --stop-instance client-02 removed only client-02; server/client-01 stayed.
- --restart-instance client-02 changed its PID and kept the same id/preset;
  the restart also produced reconnect churn on client-01, recorded as a follow-up
  networking investigation rather than claimed fixed.

Still pending after this pass:
- Explicit `server.player.registered/unregistered/stale` events (roster count is
  currently derived from active client instances).
- Run-level quota across all instance journals (per-instance rotation exists).
- New badconn impairment fields: duplicate packets, periodic latency spikes,
  bandwidth cap, and presets 11-16. The spec is written; existing presets 1-10 work.
- Human visual review of the new dashboard process rows and buttons.
- Restarting a client while another client is connected caused reconnect churn on
  client-01 in run `20261104_145751`; this is a follow-up networking investigation.
