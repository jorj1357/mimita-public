# ICE heartbeat self-heal + hot NPC movement ownership (P1)

Date: 2026-09-17 00:15 EDT (UTC 2026-09-17T04:15:00Z)
Branch: worktree, uncommitted
Result: `PASS (cold EXE build + 6 movement/selftests)` — runtime/human
acceptance pending (no live game run this session)

## Task

1. Record the ICE join failure as an OPEN regression, establishing that the
   coordinator / `server.js` is not the cause (unchanged for days).
2. Make the server resilient to the room expiring and stop starving the
   coordinator heartbeat.
3. P1 of the server-gameplay hot program: give hot code full ownership of
   server NPC movement so NPC movement/routing is hot-reloadable.

## What changed

### 1. Regression record (new)
- `docs/regressions/2026-09-17-ice-host-poll-not-fetched-after-first-join.md`:
  symptom, server+coordinator evidence, synthetic-replay proof, git + mtime
  proof `coordinator-server/server.js` unchanged since 2026-09-01. Status OPEN.

### 2. ICE resilience (`src/network/server-ice.cpp`)
- Heartbeat watchdog: every 10 s `coordinatorIceLookup(serverCode)`; when the
  room is reachable but gone (`reachable && !exists`) the server re-registers
  via `coordinatorIceHost(sessionId, agent->localSdp(), metadata)` using the
  existing agent, updating `serverCode`/`joinToken`/`hostedRoomSession`/
  `setServerCoordinatorState`. Null-agent guarded.

### 3. Heartbeat starvation (`src/network/server.cpp`)
- `tickIceCoordinator` now runs **before** the simulation step in both the
  dedicated and listen loops (rate-limited to 500 ms internally), so a long or
  starved tick cannot let the room expire between heartbeats.

### 4. Log spam (`src/network/multiplayer-tick.cpp`)
- `[NET TICK] sock=INVALID_SOCKET` now logs on state change or every 2 s
  instead of every tick.

### 5. Hot NPC movement ownership (P1)
- `src/hot-reload/game-api.h`: `GAME_CAP_NPC_MOVE` (`actor.move.npc`) +
  `GameNpcMoveFn(host, entity, tick, dt) -> handled`.
- `src/hot-reload/modules/actor-movement-system.cpp`:
  - `kSimulateServerNpcs = true`; `simulateOneActor` now returns handled and
    filters two disjoint sets (remote-network players vs `GAME_CONTROL_SERVER_NPC`).
  - New `actor.move.npc` capability provider calls the same pipeline inline and
    returns 1 when it owns the actor. (Replaces the earlier disabled-only,
    post-movement claim design so the moved result is visible to the same tick.)
  - `physics.move` is now actually provided so hot movement collides.
- `src/live-code/live-behavior.cpp`: kernel `physics.move` primitive
  (`capPhysicsMove`); headless path prefers the hot `physics.capsuleSolve`, then
  the kernel headless resolve. Registered in the kernel capability table.
- `src/npc/npc.cpp`: after `applyLiveActorBehavior` writes intent (now
  unconditional), the NPC kernel calls `actor.move.npc` inline; on handled it
  projects generic Transform/Velocity/RuntimeState onto the typed body and
  skips `physicsMainUpdate`. No provider / handled=0 falls back to the cold
  kernel. `MovementRuntimeState.grounded` is seeded from the typed body so the
  solver starts from the same contact state.

## Evidence

- Cold build: `build_agent.py` -> `mimita-20260917T000812.exe` SUCCESS; copied to
  `mimita.exe`. All source edits precede the artifact mtime.
- Hot DLL: `build/mimita-game.dll` rebuilt after the hot edits.
- Selftests PASS: `--movement-selftest`, `--movement-algorithm-selftest`,
  `--movement-parity-selftest`, `--air-movement-parity-selftest`,
  `--server-spatial-authority-selftest`, `--entity-slice-selftest`.
- NOT yet verified: live NPC locomotion parity, ICE recovery under a real
  join, coordinator self-heal against a live room. These require a live run.

## P2 — damage/attack/melee routing (hot)

Findings: the damage *decision* seam already existed and is hot —
`GAME_EVENT_DAMAGE_POLICY` (`DamagePolicyV1`) dispatched from
`serverResolveDamagePolicy` to `rocket-behavior.cpp`, plus the `damage.apply`
capability (hot -> `serverApplyEntityDamage`). Hitscan/melee/physical-contact/
splash all already route through `serverResolveDamagePolicy`; hot tools
(melee/hitscan/rocket/grenade/banana) already route through `combat-policy.cpp`
and `damage.apply`. The only hardcoded cold decision was the safety cap.

### Changes
- `src/hot-reload/game-api.h`: `DamagePolicyV1.reserved` -> `outDamageLimit`
  (same size, no ABI change).
- `src/network/server-damage-policy.cpp`: fills `outDamageLimit` from
  `serverAuthoritativeDamageLimit()` and honors the hot override when handled.
- `src/hot-reload/modules/rocket-behavior.cpp`: hot `kDamageLimit` override
  (0 = unlimited) so the authoritative clamp is editable live.

### Evidence
- Cold build `mimita-20260917T001856.exe` SUCCESS; hot DLL rebuilt after edits.
- PASS: `--hot-combat-selftest` (damage.apply resolved / real authoritative
  damage / NPC + non-actor victims), `--gameplay-boundary-selftest`,
  `--live-code-selftest` (hot damage policy dispatch), `--hot-authoritative-selftest`
  (dev-branch damage limit unlimited), `--npc-entity-selftest`.

## P3 — projectile/explosion routing (already hot)

Findings (no code change needed):
- State: `HotProjectileStateV1` / `HOT_PROJECTILE_COMPONENT`
  (`src/hot-reload/hot-projectile.h`).
- Spawn: hot tools (`tools/rocket-tool.cpp`, `tools/grenade-tool.cpp`,
  `banana-launcher.cpp`) create the entity and write the component.
- Integration, world/actor contact, lifetime, explosion and splash:
  `tools/hot-projectiles.cpp` (`explode()`/`applyDamageTo` via `damage.apply`).
- Execution: system `hot.projectile-sim` in domain `projectiles.60`, run on the
  server via `runtime.runRegisteredDomains` (`server.cpp:903`).
- Cold `tickServerProjectiles` (`server-projectiles.cpp:1743`) and
  `handleGenericProjectileAttack` are orphaned (only `tests/` call them).
- Evidence: `--gameplay-boundary-selftest` counts canonical `HotProjectileState`
  entities and PASS; `--hot-combat-selftest` PASS (canonical rocket/grenade).

## P4 — gamemode/lifecycle routing (hot decision + NPC respawn bridge)

Findings: gamemode selection/scoring/win/lifecycle/phase ownership/round-spawn
are already hot (`gamemodes/{counterstrike,tdm,ffa,objective,hot-test}.cpp` via
the mode registry + `actor.killed` / `match.evaluate` / `match.lifecycle` +
`MatchPhaseOwnership`/`ObjectiveOwnership`). Player respawn already dispatches
the hot lifecycle policy (`server-players.cpp:741`). The gap was cold NPC
respawn placement/loadout.

### Changes
- `src/network/server-npcs.cpp` `respawnServerNpc`: dispatches
  `GAME_EVENT_ACTOR_LIFECYCLE_POLICY` with the NPC's chosen spawn/yaw and actor
  entity, then honors the hot `position`/`yaw` override. The existing hot
  `lifecycle-policy.cpp` handler also arms spawn protection on the NPC entity.
  Unhandled / no hot module keeps the cold spawn point unchanged.

### Evidence
- Cold build `mimita-20260917T002507.exe` SUCCESS; copied to `mimita.exe`.
- PASS: `--gamemode-hot-selftest`, `--match-policy-selftest`,
  `--counterstrike-selftest`, `--objective-generic-selftest`,
  `--dynamic-lifecycle-selftest`.

## P5 — death/ragdoll routing (hot decision + hot corpse cap)

Findings: death presentation (`presentCorpse`) and respawn policy
(`allowLocalRespawn`) are already hot (`rocket-behavior.cpp`), ragdoll solve
(`ragdoll-solve.cpp`) and presentation (`ragdoll-present.cpp`) are hot
capabilities, and corpse lifetime/impulse/fade/blood are already
config-driven and hot-reloadable. The only hardcoded corpse value was the count.

### Changes
- `src/ragdoll/ragdoll-mode-config.h`: added `maxCorpses = 12`.
- `src/ragdoll/ragdoll-mode-config.cpp`: parses `corpse.max_corpses`
  (clamped >= 1) from `config/ragdoll.json`.
- `src/ragdoll/ragdoll-mode.cpp`: `kMaxCorpses` constant replaced by
  `cfg.maxCorpses`, so the corpse bound is hot-reloadable with the rest of the
  corpse tuning.

### Evidence
- Cold build `mimita-20260917T002827.exe` SUCCESS; copied to `mimita.exe`.
- PASS: `--ragdoll-slice-selftest`, `--hot-combat-selftest`,
  `--entity-slice-selftest`.

## Notes / risk

- Enabling hot NPC movement means NPCs run the hot Source pipeline (ground/air/
  dash/jump/gravity/capsule) instead of the cold `physicsMainUpdate` specials.
  Expect gait differences; validate NPC pursuit/patrol live before shipping.
- P2–P5 (damage/attack, projectiles, gamemode, death/ragdoll) remain.
