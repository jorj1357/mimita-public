# Hotness matrix

This document maps behavior to its code owner and the evidence required to call
it hot. It is an acceptance matrix, not a status snapshot. Current run evidence
belongs in `events.jsonl` and generated reports.

## Rules

- `game` = replaceable game DLL source. A change should activate in the same PID
  without an EXE rebuild.
- `resident` / `bootstrap` = cold host source. A change requires an EXE build and
  restart unless a new host capability specifically moves the policy.
- `module:*` = subsystem DLL source. It is not the final Cutover-1 owner; prefer
  folding it into the game generation.
- A runtime `path=dll` event proves the game path ran. It does not by itself
  prove that a specific edit was hot.
- A representative edit proof requires: owner check, game-DLL build, generation
  activation in the same PID, owner event, and human-visible result.
- The target column is the required end state. The current source-owner map can
  still contain policy in a resident file; that is an implementation gap, not a
  reason to weaken the target.

## Matrix

| Behavior | Primary owner | Cold exceptions | Live-fixable | Required evidence | Current proof |
|---|---|---|---|---|---|
| Melee claim, validation, damage, hit effects | `src/combat/melee-client.cpp`, `src/network/server-melee-actions.cpp`, `src/network/server-packet-handlers.cpp`, `src/network/server-contact.cpp`, `src/network/server-damage.cpp`, packet schema | none in the melee policy path; resident `server.cpp` pump only dispatches | Yes | `melee.contact.batch.received`, accepted/rejected reason, confirmed damage, one result per target/tick, `path=dll` | Runtime hit FX exists; a controlled live edit is still required |
| FPS measurement and simulation/render policy | `src/perf/perf-frame.cpp`, `src/perf/perf.cpp`, simulation/UI/render-policy game sources | resident renderer/swap/window/device; host polling and OS event pump | Mostly | `performance.fps`, per-frame budget sample, generation activation, measured `p50/p95/p99/p99.99`; build/activation latency recorded | `performance.fps` exists; per-frame budget is currently missed; no accepted fix |
| Room creation and room-code display | game networking packets/session/UI; `src/network/multiplayer-packets.cpp`, `src/gui`, room HUD | resident scheduler pump, OS socket/ICE primitive adapter only | Yes for room and session policy after the resident ICE/listener split; a raw primitive failure is cold | `network.room.registered` with code, connection state, room code HUD, public vs LOCAL ONLY | Public room registration and `Connected` observed; fresh live edit proof pending |
| Join room by code | game networking/ICE connection policy and packet handlers | libjuice agent/thread, OS sockets, `send`/`recv`, capability layout | Yes for join/ICE policy after the resident ICE/listener split; a raw primitive root is cold | `network.connect.stage` sequence, room code, `network.connection-state.changed`, final `Connected` or bounded failure | `Connecting -> Connected` observed; current runs also show reconnect failure |
| Reconnect/session health | `src/network/connection-health.cpp`, reconnect visuals/packets, multiplayer session policy | libjuice agent/thread, OS sockets, resident session storage mechanism | Yes for health/retry policy after the current session/ICE split; raw primitive root is cold | disconnect cause, attempt count, `Reconnecting`/`ReconnectFailed`/`Connected`, no initial-load mislabel, no swap-only reconnect | `ReconnectFailed` observed after 60 s; failure not accepted; root owner not yet isolated |
| NPC containment/stuck recovery | NPC and collision game code; `src/network/server-npcs.cpp`, physics movement files | resident server pump only | Yes | `npc.collision.stuck`, `npc.pose.reset`, `npc.pose.penetration`, final position; human visual check | Existing regression evidence open; human/runtime fix pending |
| Surface/corner snag | `src/physics/movement/actor-triangle-solver.cpp` and related collision game code | none in the solver path | Yes | speed-collapse/rollback/progress-stall before/after; no snag on connected walkable surfaces | Open regression; no accepted fix |
| Map editor gizmos | `src/mapeditor/*`, game editor services | window/input primitives only | Yes | gizmo hover/axis/drag events, map edit/save/reload, human visual check | Historical gizmo implementation; fresh visual proof pending |
| Outline config and rendering | game render policy + JSON config | resident renderer/device/swap only | Yes | room/gamemode config change, render policy activation, visual result | Config toggle exists; live render-edit proof pending |
| Input/action mapping | `src/input/*`, game bindings/actions | OS event pump and window focus | Yes | normalized input/action event, changed binding behavior, `path=dll` | Existing input evidence; representative edit proof pending |
| State migration | game migration + resident state storage | state-storage layout changes require host review | Code yes; ABI/storage no | `state.migration.prepare/commit/reject`, old/new size/version, retained state | `player_core` v1→v2→v3 and reject proven |
| ICE negotiation and listener | game ICE session/listener policy; peer and publication decisions | libjuice agent/callbacks/thread, OS socket/WSA, timers, capability layout | Yes after the current policy split; the primitive adapter stays cold | `network.client.ice.state`, connect stage, listener publication, peer admission, `network.hotswap.suspend/resume`, same connection after swap | Current policy is partly resident; split and connection-survives-swap proof pending |
| Server config/gamemode policy | `src/game/server-policy-module.cpp`, game server config/gamemode handlers | resident server pump/session containers | Policy yes; pump no | `server.policy.startup/world/configure`, gamemode event, rollback with same PID | Startup/world/configure observed; same-PID change/rollback pending |
| Safe unload/rollback | game teardown plus resident loader barrier | loader/barrier itself | Game resources yes; loader no | repeated activations, no post-unload callbacks/jobs, previous generation active on reject | Older gate passed; current-generation proof pending |

## Representative live-edit proof format

For each behavior, record:

```text
behavior:
owner_file:
changed_source_owner: game | resident | module | bootstrap
cold_change_guard_result:
game_generation:
activation_pid:
build_seconds:
activation_seconds:
first_visible_result_ms:
owner_event:
journal_path:
human_review:
```

Melee, FPS, room joining, and reconnect are the first four required proofs.

## Cold-only work

Changes in these mechanisms require an EXE rebuild/restart:

- `src/main.cpp`, process bootstrap, launcher;
- `src/network/server.cpp` scheduler/pump and session containers after policy is
  split out; this file currently also contains policy debt;
- the primitive adapter portions of `src/network/server-ice.cpp`,
  `src/network/ice/ice-agent.cpp`, and `src/network/ice/ice-server.cpp`; the
  current files mix policy and primitive code and must be split;
- `src/network/net_common.cpp` WSA/socket helpers; pure parsing/formatting
  helpers should be shared or hot;
- `src/renderer/renderer.cpp`, window/GL device/swap;
- `src/host/abi/**` ABI headers;
- resident loader/barrier/state storage.

Changing `send`, `recv`, `socket`, libjuice, or ICE primitives is cold. Fixing
ICE negotiation/listener policy, the game-side "Connecting → Connected" policy,
room-code display, packet handling, peer lifecycle, or session decisions is live
when the exact owner is game-owned.

A resident source listed here is not evidence that the whole file is correctly
cold. Policy found in a primitive file is a Cutover-1 gap; split it instead of
accepting the file-level owner as final.

`ice-server.cpp` and `udp-echo.cpp` are diagnostic tools, not production
networking policy. Their cold/tool status is not a Cutover-1 gap and must not be
used to keep production join, reconnect, ICE negotiation/listener, room,
snapshot, or session policy resident. If a tool path becomes normal gameplay,
it requires a new owner and hotness proof.

## Related documents

- [Hot reload specification](hotreload.md)
- [Networking specification](../networking/networking.md)
- [Runtime scenario validation](../../workflows/runtime-scenario-validation.md)
- [Cold-change guard](../../operations/devscripts/cold-change-guard.md)
