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

## Matrix

| Behavior | Primary owner | Cold exceptions | Live-fixable | Required evidence | Current proof |
|---|---|---|---|---|---|
| Melee claim, validation, damage, hit effects | `src/combat/melee-client.cpp`, `src/network/server-melee-actions.cpp`, `src/network/server-packet-handlers.cpp`, `src/network/server-contact.cpp`, `src/network/server-damage.cpp`, packet schema | none in the melee policy path; resident `server.cpp` pump only dispatches | Yes | `melee.contact.batch.received`, accepted/rejected reason, confirmed damage, one result per target/tick, `path=dll` | Runtime hit FX exists; a controlled live edit is still required |
| FPS measurement and simulation/render policy | `src/perf/perf-frame.cpp`, `src/perf/perf.cpp`, simulation/UI/render-policy game sources | resident renderer/swap/window/device; host polling and OS event pump | Mostly | `performance.fps`, frame budget/resource window, generation activation, measured `avg/p50/p95/p99`; build/activation latency recorded | `performance.fps` exists; budget is currently missed; no accepted fix |
| Room creation and room-code display | game networking packets/session/UI; `src/network/multiplayer-packets.cpp`, `src/gui`, room HUD | resident `server.cpp` pump, ICE/socket primitives, coordinator transport boundary | Mostly; if root is in resident ICE/socket, no | `network.room.registered` with code, connection state, room code HUD, public vs LOCAL ONLY | Public room registration and `Connected` observed; fresh live edit proof pending |
| Join room by code | game networking/ICE connection policy and packet handlers | resident ICE agent/server, sockets, `send`/`recv`, coordinator transport | Mostly; cold if ICE primitive root | `network.connect.stage` sequence, room code, `network.connection-state.changed`, final `Connected` or bounded failure | `Connecting -> Connected` observed; current runs also show reconnect failure |
| Reconnect/session health | `src/network/connection-health.cpp`, reconnect visuals/packets, multiplayer session policy | resident ICE/socket/session storage where applicable | Mostly; cold if resident transport root | disconnect cause, attempt count, `Reconnecting`/`ReconnectFailed`/`Connected`, no initial-load mislabel | `ReconnectFailed` observed after 60 s; failure not accepted; root owner not yet isolated |
| NPC containment/stuck recovery | NPC and collision game code; `src/network/server-npcs.cpp`, physics movement files | resident server pump only | Yes | `npc.collision.stuck`, `npc.pose.reset`, `npc.pose.penetration`, final position; human visual check | Existing regression evidence open; human/runtime fix pending |
| Surface/corner snag | `src/physics/movement/actor-triangle-solver.cpp` and related collision game code | none in the solver path | Yes | speed-collapse/rollback/progress-stall before/after; no snag on connected walkable surfaces | Open regression; no accepted fix |
| Map editor gizmos | `src/mapeditor/*`, game editor services | window/input primitives only | Yes | gizmo hover/axis/drag events, map edit/save/reload, human visual check | Historical gizmo implementation; fresh visual proof pending |
| Outline config and rendering | game render policy + JSON config | resident renderer/device/swap only | Yes | room/gamemode config change, render policy activation, visual result | Config toggle exists; live render-edit proof pending |
| Input/action mapping | `src/input/*`, game bindings/actions | OS event pump and window focus | Yes | normalized input/action event, changed binding behavior, `path=dll` | Existing input evidence; representative edit proof pending |
| State migration | game migration + resident state storage | state-storage layout changes require host review | Code yes; ABI/storage no | `state.migration.prepare/commit/reject`, old/new size/version, retained state | `player_core` v1→v2→v3 and reject proven |
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

Changes in these owners require an EXE rebuild/restart:

- `src/main.cpp`, process bootstrap, launcher;
- `src/network/server.cpp` server pump/session containers;
- `src/network/ice/ice-agent.cpp`, `ice-server.cpp`, `server-ice.cpp`;
- `src/network/net_common.cpp`, raw socket/transport primitives;
- `src/renderer/renderer.cpp`, window/GL device/swap;
- `src/host/abi/**` ABI headers;
- resident loader/barrier/state storage.

Changing `send`, `recv`, `socket`, libjuice, or ICE primitives is cold. Fixing
the game-side "Connecting → Connected" policy, room-code display, packet
handling, or session decisions is live when the exact owner is game-owned.

## Related documents

- [Hot reload specification](hotreload.md)
- [Networking specification](../networking/networking.md)
- [Runtime scenario validation](../../workflows/runtime-scenario-validation.md)
- [Cold-change guard](../../operations/devscripts/cold-change-guard.md)
