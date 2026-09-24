# Network hot tick, damage, projectile, and connection policy

Date (UTC): 2026-09-24T03:15:13Z
Status: implemented; cold build and selftests verified; runtime/human acceptance pending

## Scope

Convert `src/network/server.cpp`, `server-damage.cpp`, `server-projectiles.cpp`,
and `multiplayer-packets.cpp` into stable cold-kernel adapters whose gameplay
policy lives behind existing generic hot capabilities. Per the task instruction,
nothing was deleted: remaining dead/legacy branches were marked LEGACY in place.

This narrowed the task to the real gaps: `GameLogEventV1`, `GameDamageResolveV1`,
and the `GAME_CAP_CONNECTION_TRANSITION` id already existed, and damage
accept/reject, splash falloff, and connection health/cadence were already hot.

## Changes

### Workstream 1 — POD envelopes and generic capabilities
- `src/hot-reload/game-api.h` (append-only): `GAME_CAP_SERVER_TICK` +
  `GameServerTickV1`/`GameServerTickFn`; `GameConnectionActionV1`,
  `GameJoinStageV1`, `GameConnectionTransitionV1`/`GameConnectionTransitionFn`;
  `GAME_CAP_PROJECTILE_CANCEL` + `GameProjectileCancelV1`; and appended
  `projectile-impact.v2` fields on `ProjectileImpactPolicyV1` (tick, collision
  result, victim list, returned damage/knockback, self-damage, cancel, result).
- `src/hot-reload/hot-damage-application.h`: `GameDamageOutcomeV1`,
  `net.damage-application.v2` outcome/suicide/score fields, and
  `GameActorDeathResultV1`; the shared fallback now sets suicide explicitly.
- `src/hot-reload/hot-projectile-splash.h`: added `GameSplashFalloffV1::mode`
  (0 = legacy gaussian, 1 = canonical linear power curve) so one editable policy
  serves both the cold fallback and the live hot projectile path.
- New hot headers: `hot-server-tick.h`, `hot-connection-transition.h`,
  `hot-projectile-cancel.h`. New hot modules: `modules/server-tick-policy.cpp`,
  `modules/projectile-cancel-policy.cpp`; `modules/connection-policy.cpp` also
  registers `connection.transition`.
- New cold bridge `src/live-code/net-hot-log.h` routes lifecycle diagnostics
  through `log.event` with a `LiveEventJournal` fallback.

### Workstream 2 — damage/death/respawn
- `src/network/server-damage.cpp`: fills the v2 identity/outcome fields, applies
  `outSuicide`/`outScoreEligible`, propagates them into `ServerDamageResult`
  (`src/network/server.h`), and in `queueServerDamageConfirmedEvent` skips
  `serverGamemodeRecordKill` for a suicide, emitting `actor.killed` with
  killer == victim. Emits generic `actor.damage` and `actor.respawn_requested`
  facts through the existing event system.

### Workstream 3 — projectile
- `src/network/server-projectiles.cpp`: `cancelDeadNpcProjectiles` now resolves
  `net.projectile-cancel` (shared fallback preserves behavior); legacy branches
  marked LEGACY in place. `modules/tools/hot-projectiles.cpp` now resolves
  `net.projectile-splash` for explosion falloff (mode 1 preserves the exact
  canonical curve) instead of an inline formula.

### Workstream 4 — join/reconnect
- `src/network/multiplayer-packets.cpp`: added `resolveConnectionTransition`
  (POD facts in, state/stage/retry/label out) and wired it into
  `mpTickReconnect` and `mpUpdateConnectionHealth`; connection lifecycle records
  route through `log.event`. `MultiplayerContext` gained a `joinStage` fact.

### Workstream 5 — server tick
- `src/network/server.cpp`: added one shared `resolveServerTickPolicy` used by
  both `runServer` and `simulateOneServerTick`. The hot policy gates snapshot
  cadence, gameplay/post-movement domains, catch-up cap, and shutdown. The
  listen body gained the gameplay domain, shot resets, `tickHeldFireIntents`,
  and `LiveIdentity::setSimulationTick` for parity with the dedicated body.
  Server-start/tick/shutdown/thread diagnostics route through `log.event`.

### Workstream 6 — legacy, no deletions
- `src/hot-reload/hot-modules.json`: new hot headers listed; the four files'
  legacy `why` text updated to "cold mechanism, hot policy". No file deleted.

### Workstream 7 — docs
- `docs/architecture/live-development/hot-cold-audit.md`: appended the
  2026-09-23 update classifying the four files and listing live-proof debt.
- `docs/regressions/2026-09-20/cold-build-required-REG.md`: appended
  `Cold-build occurrence 3`.

## Evidence

Source changes (source evidence): files listed above; new files
`src/hot-reload/hot-server-tick.h`, `hot-connection-transition.h`,
`hot-projectile-cancel.h`, `modules/server-tick-policy.cpp`,
`modules/projectile-cancel-policy.cpp`, `src/live-code/net-hot-log.h`.

Build (build evidence): `python build_agent.py` -> `Status: SUCCESS`,
`mimita-20260923T231639.exe` (an earlier `mimita-20260923T231343.exe` built the
same tree before a small per-tick generation-lookup removal in `server.cpp`).

Automated tests (test evidence):

```text
--live-code-selftest         PASS (incl. new server-tick, connection-transition,
                                   projectile-cancel, and damage suicide checks)
--server-journal-selftest    PASS
--capability-selftest        PASS
--gamemode-hot-selftest      PASS
--hot-authoritative-selftest PASS
--match-policy-selftest      PASS
--hot-combat-selftest        only the 25 known pre-existing animation/phase2 FAILs
```

Runtime / human acceptance: pending.

## Pre-existing changes preserved

`config/accounts/default.json`, `config/analytics.json`, and
`config/audio/music-settings.json` were modified before this session and are
untouched by this change. `docs/changelog/2026-09-23/20260923_222300-network-jsonl-death-join-diagnostics.md`
was an untracked pre-existing file and is preserved.

## Known gaps / human review needed

- No live edit, rocket self-kill, falloff edit, or join-retry edit was observed.
- The two tick bodies share the policy path but are not a single function; the
  listen-body parity additions change listen behavior and need review.
- `GameDamageResolveV1` keeps its existing hot→cold consequence meaning; the
  consolidated damage/death envelope is `GameDamageApplicationV1` v2 +
  `GameActorDeathResultV1`, as agreed.
- `ProjectileImpactPolicyV1` was extended append-only rather than adding a second
  projectile-impact type, as agreed.
