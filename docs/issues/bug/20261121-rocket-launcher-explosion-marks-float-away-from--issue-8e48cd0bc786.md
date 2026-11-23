# issue-8e48cd0bc786: Rocket launcher explosion marks float away from world surfaces

## Human report (verbatim, immutable)

> still rocket launcher like explosion amrks are loating in teh air, cant ahve them flaot ing the air , can ulook at the issues and make sure thre is a issue tacking this and make it so  u invesigtate and fin de videne like  get more events.jsonl evidence and stff and prove that its fixed  or brainstorm with otehr agents about what is  why this happenign anad how to fix


## Triage

### 2026-11-21T00:01:55.347Z — rocket-impact-investigation

Summary: Investigate and fix floating Rocket Launcher explosion marks with correlated runtime and visual proof.
Owner: rocket-impact-investigation
Priority: high

## Agent suggestion

### 2026-11-21T01:00:53.309Z — rocket-impact-investigation

Source and live-trace finding: src/network/server-projectiles.cpp::explodeProjectile already distinguishes world/player/entity/lifetime impactType, but ProjectileExplodeEventPacket was only sending position/normal and left reserved[0] unused. src/network/multiplayer-projectiles.cpp::mpProcessProjectileExplodeEventPacket and the owner-prediction path then called spawnExplosionFx with mark enabled and no world anchor, so generic explosion-point fallback can float marks on actor/open-air hits. Patch now carries a world-impact bit in reserved[0] and gates confirmed/predicted marks on it. Baseline isolated Rocket Duel run 20261121_000922 on game generation 416 showed 102 server projectile-created/shot and destructible-explosion events, but no impact_decal.trace/spawn/render_submitted or rocket.impact-mark.resolved events; hitfx_trace command delivered but client logs lacked the decal trace path. Build attempt generation 417 failed before source compile at packet_registry_codegen.py timeout; daemon then could not accept a new game-dll request. Source fix is unbuilt and runtime proof pending. Next: restore/reuse the single root dev-loop control path, request build, run pinned Rocket Duel and inspect client/server events for world-hit mark ID spawn/render plus actor/open-air suppression, then visually inspect marks at multiple camera angles.

## Agent discussion and suggestions

### 2026-11-21T03:46:45.012Z — build-wait-reviewer

Message: `msg-649f158c379a` (evidence)

Subject: Compile break in rocket impact change blocks DLL attempt d15759

Compile break blocks the latest DLL attempt from note dc0aed658650 (rocket world-impact classification; files src/network/server-projectiles.cpp, src/network/multiplayer-projectiles.cpp, src/network/schema/packet-combat.h). Attempt game-dll-20261121T022901426487Z-d15759 failed changed_source_preflight after 1240.7s: src/network/multiplayer-projectiles.cpp lines 1232, 1261, 1267 pass nullptr where spawnExplosionFx expects float forceScale (src/combat/explosion-fx.h:19). Attempt total_seconds=1914.5, success=false. Full evidence in .dev/build-timings/game-dll-20261121T022901426487Z-d15759.json. This is a compile blocker separate from the visual float verification for issue-8e48cd0bc786.

## Scheduled independent review

### 2026-11-21T19:57:08.306Z — build-wait-reviewer

A bounded independent review was queued as wake wake-cf31ac63cd7d after 164 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (inconclusive - fix compiled, runtime proof absent)

### 2026-11-21T20:11:37.512Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Bounded independent review by issue-review-sweeper (read-only; no source, build, run, fix, or closure claimed). Reviewer wake wake-cf31ac63cd7d.

## Observed

- Readiness is complete (`missing=[]`, 1 active task `rocket-impact-placement-proof`), but the issue is still `state=triaged` and no fix is proven.
- Task proof `task-proof-9d05ae9ca4ff` is internally contradictory and cites missing evidence: `status=runtime_proven`, `result=unproven`, `verified_utc=""`, `build_id=""`, `game_generation=0`, `run_id=20261121_000922`, `journal_path=logs/2026-11-21/20261121_000922/client-01/events.jsonl`. On disk `logs/2026-11-21/20261121_000922/` contains only `run.json`; `client-01/events.jsonl` does not exist. So no journal supports the claimed `impact_decal.render_submitted(decal_id)` predicate. This record must not be read as proof.
- The world-impact gating fix is present in the working tree (uncommitted): `src/network/server-projectiles.cpp:740` sets `packet.reserved[0]=1` only when `impactType=="world"`; `src/network/multiplayer-projectiles.cpp:1231-1232,1260-1261,1266-1267` and `:2418-2420` pass `spawnMark` only when `reserved[0]!=0` or `step.type==WorldImpact`. `src/combat/explosion-fx.h:15-22` confirms the trailing bool is `spawnMark`. The earlier compile break (passing `nullptr` for float `forceScale`) is gone in the current tree.
- The same gating is inside the successful generation-430 build snapshot `.dev/build-snapshots/game-dll-20261121T195311434653Z-0a1309/source/src/network/{server,multiplayer}-projectiles.cpp` at the same lines. `build/current.json` reports `generation=430`, `status=ok`, `dll=game-g430.dll`, `code_hash=96458...`; run `20261121_195955` pins `game_dll_sha256=96458...`, so the running client contains the gating source.
- The only gen-430 run `20261121_195955` is a `tdm-ai-exploration` / "tdm networking validation" run on `arena1` (launch_mode 41). Its server journal has zero projectile/rocket/explosion/impact events; the client journal has only `impact_decals.config_read` and `performance.client-projectile-window`. It does not exercise rockets and cannot prove the fix.
- Run `20261121_000922` did use launch mode 43 "rocket launcher duel" on generation 416 with 5 NPCs, but only `run.json` survives; both instance journals are gone.

## Inference / uncertainty

- Source reading indicates the network path now suppresses decals for actor/open-air explosions and still requests one for a validated world hit at the packet position/normal. That is consistent with "no floating", but it is unproven at runtime and unobserved visually.
- Because the proof record has no surviving journal and no build/generation identity, it provides no evidence either way.

## Contrary evidence / scope risks

- `src/pobjects/persistent-physics.cpp:403` `doExplosion` calls `spawnExplosionFx(pos, obj.weaponId, obj.ownerName)` with default `spawnMark=true` and default +Z normal; it is used by the grenade launcher. Out of the rocket path, but it is a sibling unconditional mark path that can still float on an in-air explosion and is not covered by this fix.
- Offline/SP `src/combat/weapon-rocket-launcher.cpp:160-167` still finds the nearest world triangle for actor impacts and places a mark, while the network path suppresses actor marks. That will not float, but the two modes now differ; acceptance should state which is intended.
- Suppressing actor marks rather than anchoring them is explicitly allowed by the handoff (suppress when no suitable surface; never attach to actor body), but it is not the same as the acceptance sentence "every explosion mark sits flush"; human review should confirm the suppression outcome is acceptable.

## Next discriminating check (cold-handoff)

1. Produce a pinned rocket_duel run on a build containing the gating (generation >= 430), e.g. launch mode 43 / a rocket profile, map funworld3, `hitfx_trace 1`, with at least one local world hit, one actor hit, and one open-air/lifetime explosion.
2. In that exact run's client journal confirm: a world hit yields `rocket.impact-mark.resolved` + `impact_decal.spawn` + `impact_decal.render_submitted` with matching `decal_id`/position; actor-hit and open-air yield `mark_applied=false` and no decal ID/render submission.
3. Capture multi-angle visuals of the world-hit mark, and record run_id, game_generation, DLL sha256, and event line numbers.
4. Replace/repair `task-proof-9d05ae9ca4ff` only with that pinned, non-zero-generation journal. Do not mark behavior done from compilation or from the current contradictory record.

## Scheduled independent review

### 2026-11-22T20:21:35.304Z — cpp-latency-manifest-cache

A bounded independent review was queued as wake wake-604df6e18ff9 after 326 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (inconclusive - gate committed and compiled; no hitfx-traced runtime proof)

### 2026-11-22T20:33:53.870Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Bounded independent review by issue-review-sweeper (read-only; no source, build, run, fix, or closure claimed). Reviewer wake wake-604df6e18ff9.

## Observed

- Readiness is complete (`missing=[]`, 1 active task `rocket-impact-placement-proof`); issue remains `in_review` and no fix is proven.
- Task proof `task-proof-9d05ae9ca4ff` is still self-contradictory and cites evidence that does not exist: `status=runtime_proven`, `result=unproven`, `verified_utc=''`, `build_id=''`, `game_generation=0`, `run_id=20261121_000922`, `journal_path=logs/2026-11-21/20261121_000922/client-01/events.jsonl`. On disk `logs/2026-11-21/20261121_000922/` contains only `run.json` (4727 B); there is no `client-01/` directory. The claimed predicate `impact_decal.render_submitted(decal_id)` is unsupported. This record must not be read as proof. Root cause is tooling: `proof_add()` defaults `status='runtime_proven'` at creation (devscripts/agent_coord.py:1430-1451) while `result` stays unproven until optional `proof_verify` (1453-1496).
- The world-impact gate is committed now, not just working-tree: `git blame` attributes `src/network/server-projectiles.cpp:740 packet.reserved[0] = std::strcmp(impactType,'world')==0?1:0;` and `src/network/multiplayer-projectiles.cpp:2416-2420 hasWorldImpactAnchor` to `d89884f6` (2026-11-21 16:47:59 -0500). The rocket source files are clean against HEAD. The prior review's 'uncommitted' note is outdated.
- The gate is present in the current build: `build/current.json` is `generation=513`, `status=ok`, `dll=build\game\generation-513\game-g513.dll` (built 2026-11-22T19:49Z from `src/network/server.cpp`). Snapshot `.dev/build-snapshots/game-dll-20261122T194850321333Z-5c98ea/source` contains server:740 and multiplayer:1232/1261/1267 (`event->reserved[0] != 0`) and :2416-2420.
- Source mapping is unambiguous (`server-projectiles.cpp:1955-2001`): `LifetimeExpired -> 'lifetime'`, `PlayerImpact -> 'player'`, `WorldImpact -> 'world'`, `EntityImpact -> 'entity'`; only `'world'` sets reserved[0]=1, so actor/entity/lifetime explosions suppress the mark and a validated world impact requests one.
- New candidate runtime evidence exists but is not traced: run `20261122_174937` (gen 507, `game-g507.dll` sha256 `fa0e3f08...`, mode 44 juggernaut horde, dust2cyberiav4) is a real rocket scenario. Server journal: 389 `npc.projectile-created` with `weapon=rocket_launcher`; 328 `PROJECTILE_POLICY_EVENT` for rocket_launcher = 202 `stepType=2` (WorldImpact), 65 `stepType=4` (PlayerImpact), 61 `stepType=7` (LifetimeExpired); 271 `world.destructible-explosion`. Client journal: 130 `explosion.shockwave.spawn` and 8 `explosion.shockwave.draw` with `correlation_id=rocket_launcher`.
- However that run never enabled hitfx tracing: `gHitFxTraceEnabled` defaults false (`src/effects/hit-effects.cpp:34`), and `impact_decal.spawn` / `impact_decal.render_submitted` are emitted only when `gHitFxTraceEnabled` (`src/effects/effect-part.cpp:440-465`, `src/effects/effect-part-render.cpp:838-844`). Run 174937 has 0 `impact_decal.spawn` and 0 `impact_decal.render_submitted` in both journals; its only command was `team_pick 2`. Its decal silence is expected and proves nothing either way.
- Gen 507's DLL/snapshot are already pruned (`build/game` keeps 510-515; no 507 snapshot), but gen 507's build time (2026-11-22T08:00:31Z) is after the gate commit d89884f6 (2026-11-21T21:47Z), so containment is inferred from commit order, not re-inspected.

## Inference / uncertainty

- The committed source now suppresses decals for player/entity/lifetime rocket explosions and requests one for world impacts at the packet position/normal. That is consistent with 'no floating' but is unproven at runtime.
- Run 174937 is the strongest available base for the next check: it already produces both WorldImpact and PlayerImpact rocket steps on a post-gate build. A re-run with `hitfx_trace 1` would turn the same steps into mark events without needing a special duel setup.
- It is uncertain whether NPC-fired rocket explosions exercise the same client mark path as the local player's predicted rocket (`multiplayer-projectiles.cpp:2416`); the user report concerns the local rocket. Acceptance should state whether NPC-rocket marks are in scope.

## Contrary evidence / scope risks

- `src/pobjects/persistent-physics.cpp:403 doExplosion` still calls `spawnExplosionFx(pos, obj.weaponId, obj.ownerName)` with default `spawnMark=true` and default +Z normal; this sibling persistent/grenade explosion path can still create an in-air mark and is not covered by the rocket gate.
- Offline `src/combat/weapon-rocket-launcher.cpp:158-171` still finds the nearest world triangle for actor impacts and applies a mark, while the network path suppresses actor marks. That will not float, but the modes now differ; acceptance should say which is intended.
- Suppressing actor marks is allowed by the handoff ('suppress when no suitable surface'), but it is not literally 'every mark sits flush'; human review should confirm suppression is acceptable.

## Next discriminating check (cold-handoff)

1. Pin one isolated run on a post-gate generation (>=507; current 513) that produces all three cases, and enable `hitfx_trace 1` in the client command queue. Cheapest: repeat a mode-44 juggernaut-horde run like 174937 (NPC rockets already give WorldImpact/PlayerImpact/LifetimeExpired), or use a rocket launcher duel (mode 43) with the local player firing on funworld3. Keep `impact_decals.config_read` showing enabled.
2. In that exact run's client journal require: WorldImpact -> `impact_decal.spawn` (kind `explosion_mark`) plus `impact_decal.render_submitted` with the same `decal_id`; PlayerImpact/EntityImpact/LifetimeExpired -> no `explosion_mark` decal id and no `render_submitted`. Verify position/normal are finite and the spawn normal is not the +Z default for world hits.
3. Capture multi-angle visuals of the world-hit mark; record run_id, game_generation, DLL sha256, and event line numbers.
4. Replace/repair `task-proof-9d05ae9ca4ff` only with that pinned, non-zero-generation, hitfx-traced journal; otherwise leave it unproven. Do not close on compilation, shockwave FX, or the current contradictory record.
