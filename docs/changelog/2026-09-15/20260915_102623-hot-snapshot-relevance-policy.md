# Hot snapshot/relevance policy over generic spatial state

- EST timestamp: 2026-09-15 10:26:23 EDT (UTC 2026-09-15T14:26:23Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + `--relevance-policy-selftest` 9/9 +
  full suite 23/23)

## 1. Spatial authority audit (server)
| Field | Authoritative owner | Mirrors / projections | Writers | Readers | Network path |
|---|---|---|---|---|---|
| `ServerPlayer.pos/yaw` | typed simulation | generic `TransformComponent` | `movementStateFromServerPlayer`/`applyMovementStateToServerPlayer`, `resolveWorldCollision`, `beginAuthoritativeTransform`, `authoritativeServerPlayerRespawn`, knockback | snapshot (`makePlayerEntity`), rewind (`pushPositionHistory`), gameplay | `SnapshotEntity` -> `CompactEntityData` -> `PACKET_SNAPSHOT` |
| `ServerPlayer.vel` | typed simulation | generic `VelocityComponent` | movement step, `beginAuthoritativeTransform`, impulses | snapshot, rewind | snapshot velocity |
| `Npc.body.pos/vel` | typed `Npc` | generic `TransformComponent`/`VelocityComponent`, `ServerNpc.pos/vel` | `npc.cpp` simulation, `server-npcs.cpp` mirror | `server-npcs.cpp` snapshot, AI | snapshot via `ServerNpc` |
| `ServerNpc.pos/vel` | typed mirror | generic components | `resetGamemodeActorsAtMapSpawn`, `server-npcs.cpp` | `makeNpcEntity` | snapshot |
| projectile pos/vel | typed `ServerProjectile` | generic `Transform`/`Velocity` (entity slice) | projectile sim | `ProjectileComponent` replication + presentation | dynamic component replication |
| generic world-object Transform | generic `TransformComponent` | — | editor/generic code | editor/render/hot | generic entity replication |
| spawn/reset | `serverSpawnOrResetActor` | typed fields updated | hot `actor.spawn` | movement reads typed | snapshot |
| snapshot encoding | `makePlayerEntity`/`makeNpcEntity` (typed) | reads generic Transform for relevance | — | clients | `PACKET_SNAPSHOT` |
| rewind/history | `pushPositionHistory` (typed broadcast pos) | — | pushPositionHistory | hit rewind | local only |

**Conclusion:** typed `ServerPlayer.pos/vel`, `Npc.body.pos/vel`, and
`ServerNpc.pos/vel` are still the **authoritative simulation** state; generic
`Transform`/`Velocity` are written from them each tick. Flipping the movement
integrator to generic-first is a deep change to the prediction/parity/rewind
pipeline and is **not** done in this pass (see limits).

## 9. Snapshot candidate state reads generic Transform
- `buildAndSendSnapshot` now derives each relevance candidate's position from the
  generic authoritative `TransformComponent` (with a projection-safe fallback to
  the cold snapshot position). So the relevance decision operates on generic
  state, not typed coordinates.

## 10-16. Hot snapshot/relevance policy boundary
- New `network/relevance.h`: `GameRelevanceQueryV1` (viewer entity + position,
  bounded candidates with position + flags) and `GAME_EVENT_NET_RELEVANCE`.
- New hot `net-relevance.cpp` policy: always-relevant (`ReplicationPolicy` bit0)
  -> high tier; near (<= 30) -> high tier (every tick); far -> low tier at a
  reduced cadence (1/6). Works for any entity with generic Transform state.
- `buildAndSendSnapshot` asks the policy per viewer and selects/orders entities
  (deterministic: tier then stable entity id). If no policy handles the query it
  keeps the original broadcast path. Transport/framing/socket/MTU are unchanged
  (cold mechanism); only selection/priority/cadence are hot.
- No `PlayerReplicationPolicy`/`NpcReplicationPolicy`/entity-type switch.

## 13. First real relevance rule + 14. spatial query
- Rule: near actors/projectiles every tick; far entities lower cadence;
  always-relevant match/objective state regardless of distance. Viewer and
  candidates use generic Transform state.

## 15. Runtime unknown entity
- Proven in the self-test: a runtime entity created after startup is classified
  through generic `ReplicationPolicy` metadata with no conceptual type/enum.

## 18. Tests
`--relevance-policy-selftest` PASS 9/9: policy handles the query; near/far/always
included; near+always high tier, far low tier with cadence; deterministic for
identical input; `ReplicationPolicy` schema registered; runtime-unknown entity
classified via generic metadata; destroyed entity not a candidate.

## 19. Live hot edit
Not run. Not claimed.

## Status labels
- SELFTEST PROVEN: generic `net.relevance` boundary; near/far/always tiers;
  deterministic selection; runtime-unknown entity classification; generic
  `ReplicationPolicy` metadata; destroyed entities excluded.
- COMPILED INTEGRATION: `buildAndSendSnapshot` per-viewer selection through the
  hot policy (fallback preserves the previous broadcast); candidates use generic
  Transform.
- LIVE MULTIPLAYER PROVEN: no.
- HUMAN VERIFICATION NEEDED: in-game snapshot behavior under the new per-viewer
  cadence (far-entity low-frequency updates), and two-client parity.

## Honest limits
- **Generic Transform/Velocity are NOT yet the authoritative movement state.**
  Player/NPC movement still integrates typed `p.pos/vel` (and `Npc.body`) first
  and projects to generic each tick. Making generic authoritative requires
  flipping `movementStateFromServerPlayer`/`applyMovementStateToServerPlayer`
  and `resolveWorldCollision` to generic-first, which touches prediction parity,
  reconciliation, and rewind; that is the intended next pass, not this one.
- `typed ServerPlayer/ServerNpc` snapshot fields remain the client-facing
  representation (interpolation unchanged).
- The per-viewer relevance cadence is a real behavior change in snapshot
  membership/ordering and needs live multiplayer verification; the redundancy
  single-chunk resend is skipped while the policy is active.
- Rewind/history still uses typed broadcast positions (documented bridge).

## Files changed
`src/network/relevance.h` (new), `src/hot-reload/modules/net-relevance.cpp` (new),
`src/network/relevance-policy-selftest.{h,cpp}` (new),
`src/network/server-packets.cpp` (per-viewer relevance selection + generic
Transform candidates), `src/game/game-cli.cpp`, `src/hot-reload/hot-modules.json`;
docs + this changelog.

## Next (auto-selected)
Generic Transform/Velocity as the authoritative movement state (flip the
player/NPC movement integrator to generic-first, typed fields as projections),
then interpolation policy, prediction/reconciliation, lag compensation/rewind,
and typed snapshot schema removal.
