# NPC Waves respawn and visibility diagnostics

- Recorded: 2026-10-22T01:38:41Z (America/New_York: 2026-10-21 21:38:41 EDT)
- Branch: `20261011zt`
- HEAD before work: `9ec73c3ae724037da33d032d54843cbf1186f97a`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Finding

NPC Waves had `respawn_seconds: 0` while retaining 999 lives. `serverPlayerRespawnsEnabled()` allows respawn while wave lives remain, so this was a respawning mode with a zero timer. The shared delay helper accepted zero. Instant Space respawn also directly assigned zero.

The client applied the reliable spawn transform and generation but did not copy spawn health into `localServerHealth`. Local life reset depended on an explicit respawn request or observing a dead-to-alive health transition. If death and respawn occurred between snapshots, neither signal was guaranteed. The local player renderer skips a dead local player, and the damage handler can later clear `dead` when it applies a nonlethal hit. This source path matches the report, but the NPC Waves runtime symptom has not yet been reproduced against this build.

## Changes with exact before and after text

- `config/gamemodes/npc_waves.json:23`
  - Before: `"respawn_seconds": 0,`
  - After: `"respawn_seconds": 0.01,`
- `src/network/server-gamemode.cpp:255-269`, `serverConsumeNpcWaveLife` and `serverMatchRespawnSeconds`
  - Before: no StructuredLogger event for a lost wave life; helper returned configured zero unchanged.
  - After: emits `gamemode.npc_waves.life_lost` with wave, lives before/after, and respawn eligibility; clamps allowed respawn delays to at least `0.01f`.
- `src/network/server-damage.cpp:253-285`, lethal player damage
  - Before: assigned the respawn timer inline and death event omitted respawn eligibility and wave state.
  - After: records `respawn_allowed`, `npc_waves`, `wave`, and `wave_lives_remaining` in `server.death.transition`.
- `src/network/server-players.cpp:726-752`, `completeAuthoritativeSpawn`
  - Before: discarded the reliable queue result with `(void)qr;`.
  - After: emits `server.actor.spawn_published`, including reliable queue result, generation, epoch, health, dead/spawn state, avatar, and transform.
- `src/network/server-players.cpp:886-931`, `simulatePlayer`
  - Before: instant respawn set `p.respawnSeconds = 0.0f;`; no event recorded timer expiry and safe spawn selection.
  - After: instant respawn uses `serverMatchRespawnSeconds()` and expiry emits `server.actor.respawn_timer_expired` with timer, fixed-step duration, wave lives, and selected spawn.
- `src/network/multiplayer-tick.cpp:723-750`, `applyAuthoritativeSpawn`
  - Before: installed spawn transform and epoch but left `ctx.localServerHealth` unchanged; no spawn-packet event.
  - After: captures prior snapshot health, copies `spawn->health`, and emits `client.actor.spawn_packet_applied` with old/new generation and authoritative spawn state.
- `src/network/multiplayer-reconcile.cpp:100-120,260-289`, `mpReconcileLocalPlayer`
  - Before: new life required a pending request serial plus epoch transition, or a visible health transition.
  - After: higher authoritative spawn generation also identifies a new life; resets local death/presentation state and emits `client.local.spawn_reconciled` with detection path and dead before/after.
- `src/render/render-player.cpp:185-213,250-254`, `logPlayerRenderState` and `renderPlayerInternal`
  - Before: no StructuredLogger transition record tied together local/dead render gate and avatar/model readiness.
  - After: adds bounded actor-keyed, state-change-only `client.actor.render_state` events with gate, health, generation, avatar binding/loading, and mesh readiness. No prior code existed at this insertion point.

Pre-existing edits in `src/render/render-player.cpp` for trails/tint and other unrelated working-tree changes were preserved.

## Verbatim core edits

- `config/gamemodes/npc_waves.json:23`
  - Before: `"respawn_seconds": 0,`
  - After: `"respawn_seconds": 0.01,`
- `src/network/server-gamemode.cpp:265-269`
  - Before: `return d.respawnSeconds >= 0.0f ? d.respawnSeconds : 0.01f;`
  - After: `return std::max(0.01f, d.respawnSeconds >= 0.0f ? d.respawnSeconds : 0.01f);`
  - Added event: `"gamemode.npc_waves.life_lost"`; fields include `{"lives_before", livesBefore}` and `{"lives_after", d.waveLivesRemaining}`.
- `src/network/server-damage.cpp:253-254,277-280`
  - Before:
    `target.respawnSeconds = serverPlayerRespawnsEnabled(target.id)`
    `    ? serverMatchRespawnSeconds() : -1.0f;`
  - After:
    `const bool respawnAllowed = serverPlayerRespawnsEnabled(target.id);`
    `target.respawnSeconds = respawnAllowed ? serverMatchRespawnSeconds() : -1.0f;`
    Added death fields: `{"respawn_allowed", respawnAllowed}`, `{"npc_waves", serverGamemodeState().npcWaves}`, `{"wave", serverGamemodeState().waveNumber}`, and `{"wave_lives_remaining", serverGamemodeState().waveLivesRemaining}`.
- `src/network/server-players.cpp:729-752,886-887,919-930`
  - Before: `(void)qr;` and `p.respawnSeconds = 0.0f;`
  - After: map reliable result to `queued`, `connection_unavailable`, or `backlog_saturated`, then emit `"server.actor.spawn_published"`; instant request sets `p.respawnSeconds = serverMatchRespawnSeconds();`; timer expiry emits `"server.actor.respawn_timer_expired"`.
- `src/network/multiplayer-tick.cpp:731,738-750`
  - Before: after `ctx.localServerEpoch = spawn->transformEpoch;`, health was not copied and no packet-applied event existed.
  - After: `ctx.localServerHealth = spawn->health;` and event `"client.actor.spawn_packet_applied"` records prior snapshot health, spawn health, generation, epoch, transform, and reliable event ID.
- `src/network/multiplayer-reconcile.cpp:106-115,260-289`
  - Before: new life required `ctx.pendingRespawnSerial != 0 && epochAdvanced` and a positive server health value.
  - After: positive server health plus `spawnGenerationAdvanced` also detects a new life; reset includes `player.dead = false;`, `player.netPredictedDead = false;`, `player.networkDeathPresented = false;`, and `player.spawnGeneration = ctx.lastKnownSpawnGeneration;`; event `"client.local.spawn_reconciled"` records detection and dead before/after.
- `src/render/render-player.cpp:185-213,250-254`
  - Before: no helper or event at this insertion point.
  - After: bounded state-change helper emits the Rendering-category `"client.actor.render_state"` with `{"dead", player.dead}`, `{"render_gate", renderGate}`, `{"avatar_load_pending", avatarLoadPending}`, `{"avatar_bound", player.avatarInstance != nullptr}`, and `{"model_loaded", player.modelLoaded}`.
## Evidence

- Specification and code trace: read `docs/specs/networking/networking.md`, `docs/specs/gamemodes/gamemodes.md`, `docs/architecture/ecs-entity-etc/ecs.md`, and `docs/architecture/json-configuration/json-configuration.md`.
- Logging and runtime workflow: read `docs/specs/debug-logging/debug-logging.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/skills/spec-behavior-review-v1.md`, and `docs/skills/logging-checker-v1.md`.
- `git diff --check`: passed.
- NPC Waves JSON parse: passed.
- Canonical dev loop build: `python devscripts/dev-loop.py --request build --wait-for-build`; accepted and succeeded as build 2749.
- Published artifact: `.dev/builds/2750/mimita.exe`; SHA-256 `8A74B8F4C6C4CE7609EC5313EDB3417793C24489C9BCD6881F7A830C9DC97F01`. It matches the root `mimita.exe` hash.
- Exact artifact `--versioninfo`: passed; run `20261021_213841`, journal `logs/10-21-2026/20261021_213841/events.jsonl`.
- The existing dev-loop runtime was a Duel session. It was left running. No NPC Waves gameplay reproduction, visible-behavior check, or human acceptance was performed. On the next NPC Waves run, inspect the server death/life/timer/spawn events and correlate them with client spawn packet, reconciliation, and render-state events.
- No regression record was added because the reported NPC Waves behavior was not reproduced in a live run during this session.

## Focused review

- `docs/skills/spec-behavior-review-v1.md`: `PASS_WITH_HUMAN_REVIEW`; respawn eligibility stays server-owned and local dead-state reset is keyed to authoritative spawn generation.
- `docs/skills/logging-checker-v1.md`: `PASS`; events are categorized and state-change/bounded, with server queue outcome and client render readiness.

## Pre-existing working tree

Unrelated user changes were present before implementation in configuration, visuals/rendering, player state, and the untracked `docs/changelog/2026-10-22/` directory. They were preserved. Additional unrelated configuration changes visible after the dev-loop run were also left untouched. No commits were created.


