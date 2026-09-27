# Real predicted -> authoritative handoff: generic prediction key transport

Date: 2026-09-14 23:45 EST (UTC 2026-09-15T03:45:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW`

## Scope

Smallest server-side integration to carry a generic prediction correlation key
through the existing action/tool path. No server combat, gameplay.60, tool
ownership, projectile simulation, match, or prediction math changes.

## 1. Real predictionKey transport path

Client attack request `requestId` (`fireSerial`) -> `handleAttackRequest`
`ToolUsePolicyV1.predictionKey` -> `LiveBehavior::dispatchToolUse` -> hot tool
behavior -> `entity.create` -> `PredictionLink { predictionKey }` on the
authoritative entity -> existing generic dynamic-component replication ->
client `associateByLink` -> `PredictionRegistry` -> provisional retires.

## 2. Server action payload change

`ToolUsePolicyV1` gained an append-only generic `predictionKey` (uint64). No
`GameplayContextV1` field; no weapon-specific field.

## 3. Population from the real request

`server-attack.cpp`:
- projectile request path (preliminary generic tool fact and the projectile
  branch): `use.predictionKey = req->requestId;`
- held-fire paths (generic runtime tool and registered weapon): `use.predictionKey
  = held.intentId;`
Non-predicted actions leave it 0. The server does not invent a new identity.

## 4. Authoritative PredictionLink write

`tools/rocket-tool.cpp` and `tools/grenade-tool.cpp` write the generic
`PredictionLink` dynamic component on the created entity when
`predictionKey != 0`. A generic non-projectile behavior `hot.predicted-test`
(`modules/presentation/debug-presentation.cpp`) creates an arbitrary entity and
writes the same link, proving no projectile assumptions.

## 5. Replication path

`PredictionLink` travels as an ordinary replicated dynamic component
(`GAME_NET_ALL`) in the existing `PACKET_DYNAMIC_COMPONENT` envelope. No new
packet.

## 6. Provisional -> authority handoff proof

`--hot-combat-selftest` dispatches the rocket tool with `predictionKey=12345` and
asserts the new projectile entity carries `PredictionLink{12345}`. A separate
client-side test (previous round) proves `associateByLink` makes the
authoritative entity canonical and retires the provisional.

## 7. Exact-one-visual proof

Registry-level: only one canonical entity exists after association (provisional
destroyed). Full chain assertion of "one mesh submission throughout" across a
real two-client frame is NOT run.

## 8. Join-in-progress

A client that never predicted receives `PredictionLink` harmlessly; the
authoritative entity presents via `projectReplicatedProjectiles()` without any
registry entry.

## 9. Non-projectile proof

`hot.predicted-test` tool behavior creates a non-projectile entity that carries
`PredictionLink` and no `HotProjectileState`.

## 10. Redundant correlation classified (not removed)

- `PresentationEntities::ensure()` (old per-projectileId bridge) is superseded by
  `ensurePredicted` for predicted projectiles; left in place to avoid touching
  live interpolation.
- `MultiplayerContext::predictedProjectileIds` is write-only; classified
  redundant.
- `networkProjectiles` `fireSerial`/`requestId` reconciliation remains the
  interpolation/prediction mechanism (not redundant).

## 11. GLB / HUD

Not reached this pass (server integration only).

## Evidence

- `python build_agent.py` -> `Status: SUCCESS`.
- `build_game_dll.py` -> `build/mimita-game.dll`.
- `--hot-combat-selftest` -> PASS incl. "authoritative projectile carries the
  prediction key" and "non-projectile predicted entity carries PredictionLink".
- Full suite PASS.

## Classification

- SELFTEST PROVEN: PredictionRegistry association semantics; authoritative
  projectile carries the prediction key; non-projectile predicted entity carries
  the link; client link association retires the provisional.
- COMPILED INTEGRATION: predictionKey population in server-attack (request and
  held paths); PredictionLink write in the hot tools; generic replication carries
  it.
- LIVE MULTIPLAYER PROVEN: none.
- LIVE VISUAL PROVEN: none.
- HUMAN VERIFICATION NEEDED: two clients, prediction key from a real client
  request, no duplicate visual, clean retirement, join-in-progress.

## Files changed

`src/hot-reload/game-api.h`,
`src/hot-reload/modules/tools/rocket-tool.cpp`,
`src/hot-reload/modules/tools/grenade-tool.cpp`,
`src/hot-reload/modules/presentation/debug-presentation.cpp`,
`src/network/server-attack.cpp`, `src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.

## Next cold owner selected

GLB mesh loading through the existing `PresentationResourceProvider` (no separate
subsystem), then the hot HUD widget tree.
