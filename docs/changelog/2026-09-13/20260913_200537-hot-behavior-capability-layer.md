# ABI v4 generic capability layer + hot behavior seams; bugs 1-5 wired (source complete; one cold bootstrap required)

- EST timestamp: 2026-09-13 20:05:37 EDT (UTC 2026-09-14T00:05:37Z); updated 2026-09-13 20:35 EDT
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `SOURCE_COMPLETE / BOOTSTRAP_PENDING` (cold build refused: `mimita.exe` running); all five reported bugs have kernel seams and hot behavior handlers

## Goal

Move the bug-relevant systems behind the hot boundary so their behavior is
editable while `MiMITA.exe` stays running. This is the bootstrap that installs
the generic component-capability layer and the behavior seams; once installed,
future edits to these systems are hot gameplay behavior, not cold kernel code.

## Architecture

```
kernel (cold)                                  hot gameplay module (DLL)
  fill POD payload (base policy)
  -> LiveBehavior::dispatchPayload(GAME_EVENT_*)
       GameplayContextV1 { readComponent, writeComponent,
                           findEntities, queryWorldRay, log, emitEvent }
  <- behavior edits payload (handled + out fields)
  kernel applies payload
```

One generic dispatch shape (`LiveBehavior::dispatchPayload`) is reused by every
seam, so a new behavior for an existing event needs no new EXE call site.

## What was implemented

### ABI v4 (cold, once) — `src/hot-reload/game-api.h`
- `MIMITA_GAME_API_VERSION` 3 -> 4.
- `GameComponentType` ids + POD projections for Transform, Velocity, Health,
  MovementIntent, AimIntent, FireIntent, Projectile, Collider, Body, Ragdoll
  Limb/Joint/Root/Grab, and BehaviorBindings.
- New events: `GAME_EVENT_RAGDOLL_BIND`, `_MOVEMENT_RECONCILE`,
  `_MOVEMENT_VALIDATION`, `_PROJECTILE_PRESENT`, `_ACTOR_DEATH`,
  `_ACTOR_RESPAWN`, `_CONNECTION_STATE`.
- Payloads: `RagdollBindPartV1`, `MovementReconcileV1`,
  `MovementValidationV1`, `ProjectilePresentV1`, `ActorDeathV1`,
  `ActorRespawnV1`, `ConnectionStateV1`.
- `GameplayContextV1` now carries typed capabilities: `readComponent`,
  `writeComponent`, `findEntities`, `queryWorldRay`, `log` (plus existing
  `emitEvent`).

### Capability bridge (cold, once) — `src/live-code/live-behavior.h/.cpp`
- `dispatchPayload(typeId, payload, size, tick, ...)` generic dispatcher.
- `setDispatchWorld(const void*)` for the ray capability.
- Capability implementations over `EntityRegistry` / `World`
  (`capReadComponent`, `capWriteComponent`, `capFindEntities`,
  `capQueryWorldRay`, `capLog`).

### Seams wired in the kernel (cold, once)
- **Ragdoll bind (bug 1)** — `src/ragdoll/ragdoll-body.cpp`: `buildBody` now
  derives the node->body frame from the full linear part (`mat3`) instead of a
  quaternion, so mirrored avatar nodes (negative determinant, e.g.
  `abusivegirlheadless4` rightLeg `scale [-1,1,1]`) place their capsule on the
  correct side. It also dispatches `GAME_EVENT_RAGDOLL_BIND` so the frame is
  live-tunable.
- **Local reconcile (bug 2)** — `src/network/multiplayer-reconcile.cpp`:
  dispatches `GAME_EVENT_MOVEMENT_RECONCILE`; while `player.ragdollModeActive`
  the authoritative snap is suppressed (kernel default even with no behavior),
  breaking the self-locking loop where the client reported a stale position.
- **Death / respawn (bug 4)** — `src/combat/death-system.cpp`: corpse
  presentation dispatches `GAME_EVENT_ACTOR_DEATH`; the respawn block dispatches
  `GAME_EVENT_ACTOR_RESPAWN` and never locally respawns a server-authoritative
  life. This removes the once-per-tick kill -> respawn -> reconcile loop.
- **Connection reset (bug 5)** — `src/network/multiplayer-packets.cpp`:
  `teardownPreviousSession` now calls `mpIceConnectCancel()`, making it the
  single reset owner for the connect job on every teardown path.

### Hot behavior (hot) — `src/hot-reload/modules/rocket-behavior.cpp`
- Handles all new event types (bind, reconcile, validation, present, death,
  respawn, connection). Defaults are identity/safe; the death/respawn branch
  encodes the authoritative-life rule.

### Manifest — `src/hot-reload/hot-modules.json`
- Added the newly cold-modified sources to the `cold` list.

## Evidence

- Source: all changed TUs pass `g++ -fsyntax-only` (live-behavior, ragdoll-body,
  death-system, multiplayer-packets, multiplayer-reconcile, live-gameplay,
  live-modules, live-presentation, server-damage-policy, hot-reload-system).
- Hot DLL: `python build_game_dll.py --generation 9001/9002/9003` -> SUCCESS
  (validates the v4 header and the hot behavior module).
- Cold build: NOT RUN. `mimita.exe` is running (pids 15180, 19528);
  `build_agent.py` refuses by design. Runtime and human acceptance are pending
  the intentional cold bootstrap.

## Round 2 — remaining bugs wired (same session)

- **Bug 3 (invisible rocket)** — `src/combat/weapon-rocket-launcher.cpp`:
  `WeaponRocketLauncher::render` now emits `GAME_EVENT_PROJECTILE_PRESENT` per
  rocket. `src/combat/weapon-system.cpp`: in-flight rockets render regardless of
  the equipped weapon (rocket def resolved by id when needed).
  `src/npc/npc-combat.{h,cpp}`: new `renderNpcProjectiles`, called from
  `src/engine/engine-tick-render.cpp`, so `gNpcRocketState` rockets are visible.
  `src/network/multiplayer-projectiles.cpp`: network projectiles use the same
  seam.
- **Bug 2 (server side)** — `src/network/server.h`: `ServerPlayer::ragdollActive`
  /`ragdollLastSeenMs`. `src/network/server-packets.cpp`: PACKET_RAGDOLL_STATE
  marks ragdoll authority; `handleInputPacket` accepts the client root instead of
  geometrically correcting it and emits `GAME_EVENT_MOVEMENT_VALIDATION`.
- **Bug 5 hot status** — `src/network/multiplayer-tick.cpp`: the main-thread ICE
  poll emits `GAME_EVENT_CONNECTION_STATE` so the visible status text is
  hot-editable (transport stays kernel).
- **Bug 4 dedup** — `src/network/multiplayer-tick.cpp`: PACKET_CORPSE_SPAWN is
  gated by `mpAcceptReliableEventOnce` using `deathEventId`.

All seven modified cold TUs pass `g++ -fsyntax-only`; the hot DLL builds
(`generation 9004`).

## Still to do

1. One cold bootstrap build with `mimita.exe` closed (`python build_agent.py`),
   then live proof and human acceptance.
2. Optional follow-ups: extend `IceConnectStatus` with phase/attempt so the hot
   connection policy sees real retry numbers; add a `NET_STATE_RAGDOLL` input
   flag if the snapshot-derived authority ever proves too coarse.
