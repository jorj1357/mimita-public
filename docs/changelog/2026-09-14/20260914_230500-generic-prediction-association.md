# Generic predicted -> authoritative entity association

Date: 2026-09-14 23:05 EST (UTC 2026-09-15T03:05:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW`

## Scope

Identity + lifecycle only. No prediction/interpolation math, networking
redesign, server-combat, or gameplay.60 changes.

## 1. Prediction correlation audit

- `fireSerial` == client attack `requestId`; `provisionalProjectileId` =
  `0x80000000 | requestId`; authoritative `uint32 projectileId` assigned by the
  server; provisional EntityId = `ClientPredicted/Projectile/provisionalId`;
  authoritative EntityId = the replicated entity (server packed id).
- Prediction key available on the client: `requestId`/`fireSerial`. The
  authoritative entity currently carries no prediction key, so association
  requires a small generic link (see below).

## 2. Generic prediction association mechanism

New `src/ecs/prediction-registry.{h,cpp}`: `PredictionRegistry` maps
`predictionKey -> PredictionAssociation { provisional, authoritative, status,
tick }`. `registerProvisional`, `associate`, `canonical`, `provisionalOf`,
`authoritativeOf`, `statusOf`, `onEntityDestroyed`, `pruneOlderThan`. Retiring a
provisional is automatic on association. No `RocketPredictionState`.

## 3. Local prediction entity

`PresentationEntities::ensurePredicted(key, projectileId, pos, vel, mesh, tex,
scale, tick)` creates ONE provisional generic entity (`ClientPredicted` +
`Projectile`) with Transform, Velocity, PresentationState, and a generic
`PredictionLink`, and registers it with the registry.

## 4. Authoritative arrival / handoff

`PresentationEntities::associateByLink()` enumerates `PredictionLink` components;
the authoritative entity carrying the same key is passed to
`PredictionRegistry::associate`, which makes it canonical and retires the
provisional. Runs once per render tick.

## 5. Arrival-order safety

Registry selftests cover: prediction-first, authority-first (late provisional
retires), destroyed authority (no dangling / no resurrection), stale prediction
prune, duplicate association (newest authoritative wins). Generation reuse is
handled by `alive()` checks against the full EntityId.

## 6. PresentationEntities fate

Thin projection/prediction helper. It no longer owns a normal-projectile
identity namespace (remote uses the replicated EntityId; predicted uses the
registry).

## 7. networkProjectiles

Remains the prediction/interpolation/network mechanism; no entity/presentation
ownership.

## 8. Exact-one-entity / exact-one-draw tests

`--hot-combat-selftest` asserts: provisional canonical while predicted; one
canonical entity after handoff; provisional retires on association; authority
-first late provisional retires; destroyed authority does not dangle; stale
prediction retires; client link associates authority and retires provisional.

## 9. Non-projectile genericity

The registry is exercised with `EntityDomain::WorldObject` entities, proving no
projectile-specific association API.

## 10. Not done / documented integration hook

For real multiplayer predicted projectiles, the **server** must put
`PredictionLink.predictionKey` on the authoritative entity. Concretely:
`ToolUsePolicyV1` should carry a generic `predictionKey` (set from the attack
request id in the projectile/fire-intent dispatch) and the hot projectile tools
should write `PredictionLink` on the created entity. This is a small generic
data addition (no `GameplayContextV1` field), owned by the server-combat agent's
area and documented rather than implemented here. Until it lands, client
association fires only for authoritative entities that carry a link, and the
local shooter may transiently draw both provisional and authoritative.

## Evidence

- `python build_agent.py` -> `Status: SUCCESS`.
- `build_game_dll.py` -> `build/mimita-game.dll`.
- `--hot-combat-selftest` -> PASS (all association checks). Full suite PASS.

## Classification

- SELFTEST PROVEN: generic `PredictionRegistry` semantics (all arrival orders,
  non-projectile); client `ensurePredicted` + `associateByLink`.
- COMPILED INTEGRATION: render-tick association call; predicted branch uses the
  registry; `PredictionLink` schema registration.
- LIVE MULTIPLAYER PROVEN: none.
- LIVE VISUAL PROVEN: none.
- HUMAN VERIFICATION NEEDED: real predicted->authoritative projectile handoff
  once the server writes `PredictionLink`; join-in-progress; visual no-duplicate.

## Files changed

`src/ecs/prediction-registry.{h,cpp}` (new), `src/hot-reload/hot-prediction.h`
(new), `src/hot-reload/hot-modules.json`,
`src/hot-reload/modules/presentation/debug-presentation.cpp`,
`src/render/presentation-entities.{h,cpp}`,
`src/network/multiplayer-projectiles.cpp`, `src/engine/engine-tick-render.cpp`,
`src/network/hot-combat-selftest.cpp`, audit + next-steps, this changelog.

## Next cold owner selected

Add the documented `predictionKey` transport + `PredictionLink` write on the
server side (smallest generic integration), then GLB loading through
`PresentationResourceProvider`, then the hot HUD widget tree.
