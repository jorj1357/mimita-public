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

## Scheduled independent review

### 2026-11-23T21:07:29.464Z — resource-efficiency-goal

A bounded independent review was queued as wake wake-eb9c69c1a3e0 after 453 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (inconclusive - gen-571 rocket terminals exist; hitfx tracing never enabled)

### 2026-11-23T21:20:24.190Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Bounded independent review by issue-review-sweeper (read-only; no source, build, run, fix, or closure claimed). Reviewer wake wake-eb9c69c1a3e0.

## Observed facts

- Readiness is complete (`missing=[]`); issue state is `in_review`, 1 active task `rocket-impact-placement-proof`. No fix is proven.
- Task proof `task-proof-9d05ae9ca4ff` remains self-contradictory: `status=runtime_proven`, `result=unproven`, `verified_utc="`, `game_generation=0`, `build_id="`, journal `logs/2026-11-21/20261121_000922/client-01/events.jsonl`. Test-Path for that journal is False (only `run.json` survives). It is not proof.
- The world-only gate is committed and built. git blame attributes `src/network/server-projectiles.cpp:740` (`packet.reserved[0] = std::strcmp(impactType,world)==0?1:0`) and `src/network/multiplayer-projectiles.cpp:1231-1232` to `d89884f6` (2026-11-21). Both files are clean against HEAD. `build/game/current.json` is `generation=571`, `status=ok`, `dll=build/game/generation-571/game-g571.dll`, `code_hash=11c1f7b7...`. Snapshot `.dev/build-snapshots/game-dll-20261123T200924195125Z-7401c3/source` contains server:740, multiplayer:1232/1261/1267, and `hasWorldImpactAnchor` at 2416-2420. The gate is in the current build.
- New strongest base run: `20261123_205333` (profile networking-validation, mode 45 fun counter strike, map dust2cyberiav4, 5 NPCs; `game_generation=571`, `game_dll_sha256=11c1f7b7...`, EXE sha `2e24d312...`). Server journal has 115 rocket_launcher `PROJECTILE_POLICY_EVENT`s: 57 `explode-world` stepType=2 (WorldImpact), 22 `explode-player` stepType=4 (PlayerImpact), 2 `explode-lifetime` stepType=7 (lifetime explode), plus 34 `continue-event` stepType=7. Client journal has 81 `explosion.shockwave.spawn` (all `weapon=rocket_launcher`) and 47 `explosion.shockwave.draw`. 57+22+2 = 81 exactly, so every server rocket terminal explosion reached client FX.
- Decals are enabled in that run: one `impact_decals.config_read` (config/impact_decals.json, fingerprint 5200:fb6ab44cb75865aa); `config/impact_decals.json` has `enabled: true` and `explosionMarks.enabled=true`.
- Despite that, run 205333 has zero `impact_decal.spawn` and zero `impact_decal.render_submitted`. Across all 271 `events*.jsonl` files under `logs/` (rg --no-ignore), there are zero `impact_decal.spawn`, zero `impact_decal.render_submitted`, zero `explosion_mark`, and zero `hitfx_trace` lines. No captured run has ever enabled hitfx tracing.
- The silence is explained by the tracing toggle, not by absent marks: `gHitFxTraceEnabled` defaults false (`src/effects/hit-effects.cpp:34`); `impact_decal.spawn` is emitted only under it (`src/effects/effect-part.cpp:440-465`, from `spawnExplosionMark` at :392-421); `impact_decal.render_submitted` likewise (`src/effects/effect-part-render.cpp:838-844`). The command exists at `src/effects/hitfx-commands.cpp:194-197` (`hitfx_trace [0|1]`).

## Inference / uncertainty

- Source/build containment is solid for generation 571, and run 205333 supplies real WorldImpact + PlayerImpact + lifetime rocket terminals on that generation. This is a better proof base than the previously cited gen-507 run.
- Run 205333 is a connected client observing NPC rockets. It exercises the client terminal path (`multiplayer-projectiles.cpp:2416-2420`), the same gating users see for replicated explosions; the local player's own predicted rocket is not exercised, so the user-facing local-rocket case remains unproven.
- Absence of mark events does not show the marks are wrong or absent; it shows the proof instrumentation was off.

## Contrary evidence / scope risks

- `src/pobjects/persistent-physics.cpp:403 doExplosion` still calls `spawnExplosionFx(pos, weaponId, ownerName)` with default `spawnMark=true` and default +Z normal; the grenade/persistent explosion path (358 grenade_launcher policy events in 205333) is not covered by this gate.
- Offline `src/combat/weapon-rocket-launcher.cpp` still places a mark from the nearest world triangle for actor impacts, while the network path suppresses actor marks; modes differ.
- Acceptance says every explosion mark sits flush; the current fix suppresses actor/open-air marks rather than anchoring them. Human review should confirm suppression is acceptable.

## Next discriminating check (cold-handoff)

1. Re-run the already-proven rocket scenario with tracing on: one mode-45 networking-validation run like 205333 (or the mode-43 rocket duel) at generation >=571, with `hitfx_trace 1` in the client agent-command queue. No new setup is needed; 205333 already produced 57 world / 22 actor / 2 lifetime rocket terminals.
2. In that exact pinned client journal require: each WorldImpact (expected about 57) yields `impact_decal.spawn` with `kind=explosion_mark` and `impact_decal.render_submitted` with the same `decal_id`; PlayerImpact/LifetimeExpired yield no explosion-mark decal id and no render submission. Check finite position/normal and that world-hit spawn normals are not the +Z default.
3. For the user's local-rocket case, have the local player fire at a wall, at an actor, and into open air, and repeat the same event checks.
4. Capture multi-angle visuals of the world-hit mark. Record run_id, game_generation 571, DLL sha256 11c1f7b7..., and event line numbers.
5. Repair `task-proof-9d05ae9ca4ff` only from that pinned, non-zero-generation, hitfx-traced journal. Do not close on compilation, shockwave FX, or the current contradictory record.

## Agent discussion and suggestions

### 2026-11-23T21:20:52.760Z — issue-review-sweeper

Message: `msg-b2c8db342b1e` (message)

Subject: Agree on one hitfx-traced gen-571 rocket run for issue-8e48cd0bc786

Independent review id 301 appended to issue-8e48cd0bc786. New evidence: run 20261123_205333 on generation 571 already gives 57 rocket WorldImpact, 22 PlayerImpact and 2 lifetime terminals, with 81 matching client rocket shockwaves, but zero impact_decal.spawn/render_submitted because hitfx_trace was never enabled (0 of 271 stored journals). Cheapest next check: repeat a mode-45 networking-validation run like 205333 with hitfx_trace 1 in the client agent-command queue, then require explosion_mark spawn+render for WorldImpact and none for PlayerImpact/Lifetime. Please review and agree on this single check; do not repair task-proof-9d05ae9ca4ff until that pinned journal exists.

## Scheduled independent review

### 2026-11-24T23:15:48.272Z — root-mode3-latency

A bounded independent review was queued as wake wake-f370c84b5d4f after 658 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (inconclusive - gate present in gen 627; no trace-enabled runtime evidence and prior base journals pruned)

### 2026-11-25T00:30:19.305Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review by issue-review-sweeper (read-only; no source edit, build, run, lease, closure, or fix claimed). Reviewer wake wake-f370c84b5d4f, 2026-11-24T23:15Z. Prior reviews: issue_reviews id 57, 211, 301; prior text is preserved (append-only).

## Observed facts

- Readiness complete: missing=[], ready=true, 1 active task rocket-impact-placement-proof, issue state in_review. No fix is proven.
- Proof record task-proof-9d05ae9ca4ff is unchanged and still self-contradictory: status=runtime_proven, result=unproven, verified_utc="", build_id="", game_generation=0, run_id=20261121_000922, journal_path=logs/2026-11-21/20261121_000922/client-01/events.jsonl. Test-Path on logs/2026-11-21 is False; the cited journal does not exist. It is not evidence.
- Current build at review time: build/game/current.json generation=627, status=ok, dll=build\game\generation-627\game-g627.dll, code_hash=cb87962e328830ebd689745d5ba558c502945ad728afc50329d53ad30af9026a, revision=7092860..., generated 2026-11-24T22:33:49Z; ready.json published 22:33:56Z. No generation-628 DLL exists.
- The world-only gate is present in the generation-627 snapshot .dev/build-snapshots/game-dll-20261124T230934953855Z-9ace92/source: server-projectiles.cpp:741 `packet.reserved[0] = std::strcmp(impactType, "world") == 0 ? 1 : 0;`; multiplayer-projectiles.cpp:1270/1299/1305 pass `event->reserved[0] != 0`, and hasWorldImpactAnchor at 2480-2484.
- Mark tracing is still gated: effect-part.cpp:447 emits impact_decal.spawn; effect-part-render.cpp:906 emits impact_decal.render_submitted; hit-effects.cpp:34 `bool gHitFxTraceEnabled = false;` with uses at 174/239.
- No surviving journal has ever enabled tracing. `rg --no-ignore` for `impact_decal.spawn`, `impact_decal.render_submitted`, or `hitfx_trace` across logs/ returns zero matches.
- Log retention has removed the prior bases: only the date root logs/2026-11-24 remains. logs/2026-11-21, logs/2026-11-23, and run logs/2026-11-23/20261123_205333/client-01/events.jsonl (the strongest base from review 301) are all absent. No journal from reviews 57/211/301 can be re-inspected.
- The surviving 2026-11-24 runs are not rocket-launcher impact runs: the newest are vehicle_arena (Rocket League) physics (rl-physics-*, ball-proof, settle-proof/solver-proof) and Bloodtest. In them `rocket_launcher` appears only in actor loadouts/presentation (for example rl-physics-1 server loadout lists it); the only projectile-policy events found in 20261124_150432 are `weapon=projectile_rifle`, not rocket launcher.
- config/impact_decals.json still has enabled:true and explosionMarks.enabled:true.
- The working tree has an uncommitted change to src/network/server-projectiles.cpp (3 added diagnostic fields: configured_speed_mps, definition_speed_mps, definition_id at ~line 1596+). It does not modify the world/actor gate.

## Inference / uncertainty

- Source containment is solid for generation 627: the network path suppresses marks for non-world terminal impacts and requests one for a world impact. That is consistent with no floating mark, but remains unproven at runtime.
- Because no trace-enabled journal exists and all older bases are pruned, there is currently no runtime evidence for or against the fix. Absence of decal events is explained by the default-off tracer, not by correct or missing marks.
- The local player's own predicted-rocket case (the user's report) is still not covered by any existing run; the 205333-class base was NPC/replicated rockets.

## Contrary evidence / scope risks

- src/pobjects/persistent-physics.cpp:403 doExplosion still calls spawnExplosionFx(pos, weaponId, ownerName) with default spawnMark=true and default +Z normal; the grenade/persistent path is outside this gate and can still create an in-air mark.
- Offline src/combat/weapon-rocket-launcher.cpp still derives a mark from the nearest world triangle for actor impacts, while the network path suppresses actor marks; the modes differ. Acceptance should state whether suppression is intended.

## Next discriminating check (cold handoff)

1. Fresh pinned run at generation >=627 with `hitfx_trace 1` in the client command queue (mode 45 networking-validation like the pruned 205333, or mode 43 rocket duel). Because prior journals are gone, this must be produced new.
2. In that exact client journal require: each WorldImpact (stepType=2) yields impact_decal.spawn (kind=explosion_mark) plus impact_decal.render_submitted with the same decal_id; PlayerImpact/EntityImpact/LifetimeExpired yield no explosion-mark decal id and no render submission; finite position/normal and world-hit normal not the +Z default.
3. For the user's case, have the local player fire at a wall, at an actor, and into open air and repeat those checks.
4. Capture multi-angle visuals. Record run_id, game_generation 627, DLL sha256, and event line numbers. Then repair task-proof-9d05ae9ca4ff only from that pinned, non-zero-generation, trace-enabled journal; otherwise leave it unproven.
5. Preserve the pinned journal (or a copy) against log rotation so the next reviewer can re-inspect it.

## Scheduled independent review

### 2026-11-26T00:58:51.192Z — build-wait-reviewer

A bounded independent review was queued as wake wake-5a57be69dc90 after 754 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (inconclusive - gen-627 rocket bases survive on disk; hitfx tracing never enabled; local actor-mark heuristic unexercised)

### 2026-11-26T01:14:34.317Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review by issue-review-sweeper (read-only; no source edit, build, run, lease, closure, or fix claimed). Reviewer wake wake-5a57be69dc90, 2026-11-26T01:13Z UTC (2026-11-25 20:13 EST). Prior reviews ids 57, 211, 301, 404 are preserved; this is append-only.

## Observed facts

- Readiness is complete (`missing=[]`, ready=true, 1 active task `rocket-impact-placement-proof`); issue state is `in_review`. No fix is proven.
- Proof record `task-proof-9d05ae9ca4ff` is unchanged and still self-contradictory: `status=runtime_proven`, `result=unproven`, `verified_utc=""`, `build_id=""`, `game_generation=0`, `run_id=20261121_000922`, `journal_path=logs/2026-11-21/20261121_000922/client-01/events.jsonl`. `Test-Path logs/2026-11-21` is False, so the cited journal does not exist. This record is not evidence.
- Current build: `build/game/current.json` `generation=662`, `status=ok`, `dll=build\game\generation-662\game-g662.dll`, `code_hash=24d73b811b3d4a8a3bb5e36ded1034730932592456aaf5fc0565c199521182c4`, `revision=e4b902e4...`, generated 2026-11-26T00:59:11Z; `ready.json` published 00:59:13Z. Snapshot `.dev/build-snapshots/game-dll-20261126T005556909723Z-fa8154/source` contains `server-projectiles.cpp:741 packet.reserved[0] = std::strcmp(impactType, "world") == 0 ? 1 : 0;` and `multiplayer-projectiles.cpp:1303/1332/1338 ... event->reserved[0] != 0`. The world-only gate is present in the current generation.
- New strongest surviving rocket bases (both generation 627, mode `fun counter strike`; artifact build_id `2026-11-25T01:21:28.987918Z:5dda08799cefdf4caad640a34a806c92e4a03c3886acdd4aa943be8c558eeb8b`, mapped by `.dev/build-timings/game-dll-20261125T012133976103Z-b7c31e.json` to generation 627):
  - Run `20261125_175329`: 680 `npc.projectile-created` rocket_launcher; terminal `PROJECTILE_POLICY_EVENT` reasons `explode-world` 578, `explode-player` 117, `explode-lifetime` 6 (plus 147 `continue-event`). Client journal has 71 rocket `explosion.shockwave.spawn` and 2 `impact_decals.config_read`; 0 `impact_decal.spawn`, 0 `impact_decal.render_submitted`, 0 hitfx lines.
  - Run `20261125_184820`: 2379 rocket_launcher created; `explode-world` 2003, `explode-player` 400, `explode-lifetime` 33 (plus 949 continue). Client journal has 348 rocket shockwaves and 2 config reads; 0 decal spawn/render, 0 hitfx lines.
  - Combined: 2,581 world, 517 player, 39 lifetime rocket terminals on a post-gate generation-627 build reached client FX, yet produced no decal events. These journals survive on disk, unlike the pruned 205333 base from review 301.
- `rg --no-ignore` across `logs/` still returns zero `impact_decal.spawn`, zero `impact_decal.render_submitted`, zero `explosion_mark`, and zero `hitfx_trace` lines. Tracing defaults off: `src/effects/hit-effects.cpp:34 bool gHitFxTraceEnabled = false;`; spawn emit at `src/effects/effect-part.cpp:440`; render emit at `src/effects/effect-part-render.cpp:900`; command `hitfx_trace [0|1]` at `src/effects/hitfx-commands.cpp:194`.
- First surviving `rocket.impact-mark.resolved` event exists: `logs/2026-11-24/20261124_205956/client-01/events-0005.jsonl:8138` (build `2026-11-24T19:27:02.742Z`, map `dust2cyberiav4`, mode `[2 CLIENTS] Vehicle Arena - 1v1`, local user Guest1859). Fields: `impact_surface=world_triangle`, `mark_surface=static_world_triangle`, `mark_applied=true`, `world_triangle_index=9140`, `nearest_world_surface_distance_m=-1.0`, `impact_position == mark_position == [183.832,-298.669,735.635]`, `weapon_mark_texture=explosionmarkv1.png`. This is the only such event in all logs. It is a world-hit resolver data point, not a spawn/render proof; it also corrects review 404's statement that no surviving 2026-11-24 run had rocket mark evidence. The `-1.0` distance is the unset sentinel for the direct world-triangle path (`src/combat/weapon-rocket-launcher.cpp:156,160,182`).
- `config/impact_decals.json` still has `enabled: true` and `explosionMarks.enabled: true`; both new runs loaded it (fingerprint `6989:b53d80c391645c31`).
- No rocket impact run exists on generations 628-662. The newest gen-662 runs (`20261126_010040` and neighbours) are `Vehicle Arena - Rocket League Free Play` on `arena1` with zero rocket-launcher projectile-policy events.

## Inference / uncertainty

- The world-only gate is contained in generation 662. Suppression of replicated player/lifetime marks and one mark for a world hit is consistent with expected behavior, but remains runtime-unproven because tracing has never been on.
- The new gen-627 counter-strike runs are the best available proof base: they survive on disk and already produce thousands of world terminals. A single re-run with `hitfx_trace 1` would turn those same terminals into mark spawn/render events without new scenario setup.
- The single mark event shows the local path can anchor a direct world hit exactly at the impact point. It says nothing about actor-hit or open-air, which are the float-prone cases.

## Contrary evidence / scope risks

- The offline/local actor-impact path in `src/combat/weapon-rocket-launcher.cpp:157-167` still calls `findNearestWorldSurface` and anchors the mark to the nearest static world triangle within 8 m for actor impacts. A midair actor hit near a ceiling/wall can therefore place the mark on that nearest surface instead of suppressing it, which is the reported floating symptom. The network path suppresses actor marks; the local/SP path does not. The one observed mark is `impact_surface=world_triangle`, so this actor path is unexercised.
- `src/pobjects/persistent-physics.cpp:403 doExplosion` (grenade/persistent explosions) still defaults `spawnMark=true` with a +Z normal; it is outside the rocket gate and can still float on an in-air explosion.
- Acceptance says every mark sits flush; the implemented network behavior suppresses actor/open-air marks rather than anchoring them. Human review should confirm suppression is acceptable.
- The `rocket.impact-mark.resolved` event is emitted only from the local weapon path; replicated/NPC marks are observable only through `hitfx_trace`. A world-terminal journal with tracing off cannot distinguish correct suppression from a skipped or broken mark path.

## Next discriminating check (cold handoff)

1. Re-run a surviving gen-627 base (`20261125_175329` or `20261125_184820`, `fun counter strike`) or a newer generation >= 662 rocket profile with `hitfx_trace 1` in the client agent-command queue; prefer a current-generation run and keep `impact_decals.config_read` showing enabled.
2. In that exact client journal require: each `explode-world` (stepType=2) yields `impact_decal.spawn` (kind `explosion_mark`) plus `impact_decal.render_submitted` with the same `decal_id`; `explode-player`/`explode-lifetime` yield no explosion-mark decal id and no render submission; spawn position/normal are finite and the world-hit normal is not the +Z default.
3. Separately exercise the local/offline actor case: fire a rocket at an actor in open air and require `rocket.impact-mark.resolved` with `impact_surface=actor` and either `mark_applied=false`/`mark_surface=none_within_8m`, or a `nearest_world_surface_distance_m` consistent with a truly adjacent surface and a mark visibly flush. A mark anchored to a surface >0 m away from a midair actor is the floating defect.
4. Capture multi-angle visuals for the world-hit mark and the actor case; record run_id, game_generation, DLL sha256, and event line numbers.
5. Repair `task-proof-9d05ae9ca4ff` only from that pinned, non-zero-generation, trace-enabled journal; otherwise leave it unproven. Preserve a journal copy against log rotation.

Verdict: inconclusive - gen-627 rocket bases survive on disk; hitfx tracing still never enabled; local actor-mark heuristic unexercised.
