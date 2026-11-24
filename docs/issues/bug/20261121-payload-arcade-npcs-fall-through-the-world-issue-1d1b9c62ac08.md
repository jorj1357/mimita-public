# issue-1d1b9c62ac08: Payload Arcade NPCs fall through the world

## Human report (verbatim, immutable)

> also, npcs just fall thru the ground in arcade mode for payload, no one is colldiing witht he world, all npcs just fall thru the ground


## Triage

### 2026-11-21T14:56:17.364Z — payload-arcade-collision-fix

Summary: Trace Payload Arcade NPC fall samples to world collision geometry and repair the first missing contact stage; existing run is available for read-only observation.
Owner: payload-arcade-collision-fix
Priority: high

## Scheduled independent review

### 2026-11-21T20:58:17.086Z — cpp-live-generation-plan

A bounded independent review was queued as wake wake-c1e3af39b434 after 171 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (unverified)

### 2026-11-21T21:09:36.546Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review (issue-review-sweeper, wake-c1e3af39b434, 2026-11-21). Read-only: no source edit, build, run, close, or fix claim.

OBSERVED FACTS
- Readiness: ready=true, score 8/8, missing=[], state=triaged, one open task payload-arcade-npc-grounding (owner payload-arcade-collision-fix).
- Proof: task proof id 41 result=unproven (expected method runtime_proven); build_id, run_id, journal_path, verified_utc all empty. Companion proof 40 (payload-cart-arcade-contact) is also unproven. No accepted runtime proof exists.
- Build: generation 432 (published 2026-11-21T20:17:09.591Z, build/game/generation-432/game-g432.dll, code hash b01e47a7...) was compiled from a snapshot whose src/network/server-npcs.cpp contains resolveSafeStandingSpawn(world, rawSpawn, 0.60f, 2.0f, 128, 25.0), and whose sweep-slide.cpp hash 6b5fd3... equals the current working tree and proof 41 source hashes. So the candidate change is compiled: build evidence yes, behavior evidence no.
- Linked journal: logs/2026-11-21/20261121_144350/server-01/events.jsonl exists (run.json still pins build 2026-11-21T02:26:50..., build_match=false). Surviving window 20:23:24.057Z-20:29:59.807Z has 6,147 npc.collision.fall-sample events, 6,105 with world_contact=false; last is NPC-1005 tick 559247 with candidate_count=0, sweep_hits=0, world_contact=false.
- Generation signals disagree inside that one run: 329 game.hotness.summary events (20:23:24.840Z-20:29:59.183Z) all report generation=432 and server_path="dll" (HostRuntime active whole-game generation), while 375 npc.phase-window events all report module_generation=420 (ModuleHost.activeGeneration("npc-behavior")), and run.json says build_match=false.
- Other runs: 20261121_192618 pinned gen 427 with --npcs 5, but only run.json survives (journals cleaned). Runs 20261121_201749 through 195955 are TDM with --no-npcs. No inspectable fresh-NPC journal on generation >= 427.

INFERENCE / UNCERTAINTY
- If game.hotness.summary.generation is authoritative, the 20:23-20:29 fall samples were emitted by generation-432 code and the candidate change did not stop sustained non-contact fall samples. That fits the diff: server-npcs only widens spawn validation (24 -> 128 candidates, same 25 ms cap) and sweep-slide adds physical-entity (cart) sweeps and entityId handling; neither changes the world-triangle gather that produced candidate_count=0. This suggests the first divergence is world-candidate gathering/broadphase coverage at movement time, not spawn validation.
- If module_generation=420 is authoritative for the NPC collision path, the same samples are pre-fix and say nothing about 432. Current artifacts cannot resolve this; the ambiguity itself blocks any proof verdict.

CONTRARY EVIDENCE
- The gen-420 tick-1 sample and the surviving journal reproduce the user report, but they do not demonstrate the fix failing on fresh spawns. The surviving process is 6h+ old; its ~19 NPCs likely spawned before generation 432, and the spawn change only runs at spawn/respawn. "Still falling" therefore does not cleanly disprove the spawn change.
- No journal proves the fix either: there is no fresh-NPC generation-432 run containing npc.collision-world.summary plus world_contact=true/grounded_this_tick=true.

NEXT DISCRIMINATING CHECK (pinned)
1. Via the root dev-loop start one isolated Payload/FunWorld3 Arcade run pinned to generation 432 with a small fresh NPC count; record run id, EXE hash, DLL generation/hash, journal path.
2. In that exact journal require generation identity: latest game.hotness.summary.generation == 432 with server_path=dll, and NPC spawn events (npc.spawn or safe-spawn) timestamped after run start, so the spawn path actually executed.
3. Classify npc.collision.fall-sample: pass only if fresh-spawn NPCs show world_contact=true/grounded_this_tick=true with no sustained descending series; fail if candidate_count stays 0 and world_contact=false. Also capture npc.collision-world.summary (npc_triangles/headless_triangles/solid_ids_aligned) and the exact NPC positions vs collisionMesh bounds.
4. If fresh spawns still fail, record the first divergence as world-candidate gathering for the root capsule (gatherGLBTriangles/broadphase), not spawn validation, and attach the evidence here.

CONFIDENCE
- Facts (readiness, proof, hashes, counts): high. Fix effectiveness: unproven/low. Effective generation for the fall samples: medium, because the three identity signals disagree; resolve step 2 before accepting any proof or failure claim.

## Scheduled independent review

### 2026-11-22T21:26:55.797Z — build-wait-reviewer

A bounded independent review was queued as wake wake-48e347846cf2 after 333 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Scheduled independent review

### 2026-11-22T21:51:15.940Z — mimita-process-multiplex-followup

A bounded independent review was queued as wake wake-8600b57cb056 after 338 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (unverified)

### 2026-11-22T22:02:49.644Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review (issue-review-sweeper, wake-8600b57cb056, 2026-11-22). Read-only: no source edit, build, run, close, or fix claim. Prior review text is preserved.

OBSERVED FACTS
- Readiness: ready=true, score 8/8, missing=[], state=in_review; one open task payload-arcade-npc-grounding (owner payload-arcade-collision-fix). Proof 41 latest_proof result=unproven; build_id/run_id/journal_path/verified_utc are empty. No accepted runtime proof.
- The issue linked evidence journal logs/2026-11-21/20261121_144350/server-01/events.jsonl no longer exists. logs/2026-11-21 is gone; the only retained dated root is logs/2026-11-22 (8 runs), while 11-20-2026/11-21-2026/11-22-2026 are empty. The gen-420 fixture can no longer be re-inspected; only the copied fields in issue evidence survive.
- Current published gen 526 (build/game/generation-526/game-g526.dll, code hash 75ff257e..., published 2026-11-22T21:28:06Z) was built from .dev/build-snapshots/game-dll-20261122T212248194307Z-487aa2/source. Its physics-collision-glb-main.cpp sha256 0abedb32... and physics-collision-glb-sweep-slide.cpp 6b5fd3... are identical to the current working tree and to proof 41/30 recorded collision hashes. Its server-npcs.cpp 8f37f955... differs from the worktree (72e1d221...) only by an uncommitted FCS retaliation-targeting hunk and includes the spawn instrumentation plus resolveSafeStandingSpawn(world, rawSpawn, 0.60f, 2.0f, 128, 25.0). So the candidate collision code is compiled and live in gen 526. Proof 41 is nonetheless stale: its recorded server-npcs.cpp hash 2c0d3e... no longer matches source.
- New pinned fresh-NPC journal logs/2026-11-22/20261122_214309 (parallel_agent_test, gen 526, exe sha e24aaf16..., mode juggernaut horde, map dust2cyberiav4, --npcs 5, waves up to ~72 actor ids). npc.spawn.resolved fires at t~1.5s (safe_spawn_resolve_ms 0.11-0.17, solid_bounds=485). npc.collision-world.summary: npc_triangles=headless_triangles=5465, removed=0, union=0, solid_ids_aligned=true, unknown_solid_triangles=1534.
- In that exact run npc.collision.fall-sample persists t=3.37s to ~793s: 5,535 + 7,466 + 7,504 = 20,505 events; 13,471 (65.7%) have candidate_count=0; 18,428 (89.9%) have world_contact=false (2,077 true). Every sample has grounded_this_tick=false by construction: src/physics/movement/physics-collision-glb-main.cpp:615 emits only when !groundedThisFrame && p.vel.z < -5.0f, rate-limited to 30 ticks per actor.
- Fresh pinned run logs/2026-11-22/20261122_215639 (gen 530, fun counter strike, dust2cyberiav4) also contains 3,115 fall-sample events, same behavior.
- Per-actor trajectories do NOT show sustained unbounded descent. Example NPC-200000: z=2347.08 at t=15.47, 2354.63 at t=410, 2352.63 at t=409/771, 2349.22 at t=793. Across 72 actors the largest first-to-last z drop is ~-13.4 m (NPC-200064); all actors persist and positions oscillate around ~2347. So the diagnostic fires during ordinary descending/ledge motion, not a continuous fall out of the world.
- No retained journal is Payload/FunWorld3 Arcade. Every retained 2026-11-22 run is map=dust2cyberiav4 with mode juggernaut horde, fun counter strike, or ssbm server flag. The reported reproduction has not been run on any retained generation.

INFERENCE / UNCERTAINTY
- fall-sample cannot serve as a positive grounding proof: it only exists when NOT grounded, so an acceptance phrased as world_contact=true and grounded_this_tick=true is unsatisfiable from this event alone. A proof needs a positive grounded/contact event or position-stability evidence.
- candidate_count=0 with world_contact=false shows the world-triangle query sometimes returns no candidate near a descending NPC capsule on gen 526, consistent with (not proof of) the previously hypothesized gatherGLBTriangles/broadphase gap. The oscillating trajectories and the missing Payload/FunWorld3 run leave the reported bug unproven.
- The first divergence from the original report cannot be re-confirmed because the linked journal was pruned.

CONTRARY EVIDENCE
- The gen-420 fixture is gone, so the still-falling claim cannot be re-verified from disk.
- On gen 526 no actor shows sustained descent; max drift -13 m with recovery, so this run does not reproduce the user report.
- Some fall-samples have candidate_count up to 8 and world_contact=true, so candidate gathering is not uniformly empty.

NEXT DISCRIMINATING CHECK (pinned)
1. Via the root dev-loop start one isolated Payload/FunWorld3 Arcade run pinned to the current published generation (526, or the newer republished gen), small fresh NPC count; record run id, EXE sha256, DLL generation+sha256, journal path.
2. In that exact journal require: latest game.hotness.summary.generation == pinned gen with server_path=dll; npc.spawn.resolved after run start; npc.collision-world.summary; and a positive grounded/contact signal (existing grounded event or an explicit contact sample), because fall-sample alone cannot show grounding.
3. Pure unit/component self-tests are not substitutes. Fail if fresh-spawn NPC positions descend monotonically below the authored floor z with candidate_count staying 0 and world_contact=false across consecutive samples for the same actor. Pass only if positions stabilize at the floor with grounded/contact true and no sustained negative-z series.
4. If fresh Payload NPCs still fail, record the first divergence as gatherGLBTriangles/broadphase coverage for the NPC capsule (candidate_count=0) at physics-collision-glb-main.cpp:615-645, not spawn validation (safe spawns landed at z~2360 on the tested map). Capture unknown_solid_triangles=1534 and solid_bounds for the failing map.

CONFIDENCE
- Facts (readiness, proof, hashes, counts, retention): high. Reported bug status: unproven. Candidate-gathering hypothesis: medium-low. Effective generation for the old samples: unverifiable, because the linked journal was deleted.

## Scheduled independent review

### 2026-11-23T22:20:26.442Z — resource-profiler

A bounded independent review was queued as wake wake-bd5be8c47840 after 462 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (unverified)

### 2026-11-23T23:51:14.216Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review (issue-review-sweeper, wake-bd5be8c47840, 2026-11-23). Read-only: no source edit, build, run, close, or fix claim. Prior review text is preserved.

OBSERVED FACTS
- Readiness: ready=true, score 8/8, missing=[], state=in_review; one open task payload-arcade-npc-grounding (owner payload-arcade-collision-fix). Proof 41 latest result=unproven; build_id/run_id/journal_path/verified_utc are empty. Its recorded src/network/server-npcs.cpp hash 2c0d3e... no longer matches the worktree (031beb66...), so the proof record is also stale on inputs. No accepted runtime proof.
- Current published generation 573: build/game/generation-573/game-g573.dll, code hash a10e5f32402bfdbd0f6572f0121da0a67e4eb1d8db7e6209520f4cd10dcdfdc6, published 2026-11-23T22:23:53.590Z. Working-tree physics-collision-glb-main.cpp sha256 0abedb32... and physics-collision-glb-sweep-slide.cpp 6b5fd3... are identical to the prior review's values; sweep-slide matches proof 41's recorded hash. The candidate collision path is therefore still the one under test.
- New pinned fresh-NPC run logs/2026-11-23/20261123_223229 (parallel_agent_test; gen 573; exe sha 2e24d312...; server map dust2cyberiav4; --npcs 5; args --mode/--gamemode funcounterstrike; ~10.1 min). run.json build_identity.game_generation=573 and game_dll_sha256=a10e5f32... equal the published gen 573 identity.
- The 2026-11-21 review's three-way generation disagreement is now explained against the source: src/game/game-module.cpp:310 sets api.generation = 1, and src/network/server.cpp:222-223 writes game.hotness.summary.generation from HostRuntime.activeGeneration(), which reads that field (src/host/host-loader.h:67,182). In run 20261123_223229 hotness reports generation=1 / runtime_revision=2 (api.version=2), while npc.phase-window.module_generation=573. The authoritative signals here are run.json game_generation=573 and module_generation=573, which agree; game.hotness.summary.generation must not be used as an on-disk generation identity.
- npc.collision-world.summary (t=8.26s): npc_triangles=headless_triangles=5465, removed=0, union=0, solid_ids_aligned=true, unknown_solid_triangles=1534 (dust2cyberiav4).
- npc.spawn.resolved fires at t~40.14s (101 events; safe_spawn_resolve_ms 0.17-4.25; corrected_m=5.893 for the shown spawns; safe z=2360.33; raw_inside_solid=false, safe_inside_solid=false), so the spawn path executed on fresh NPCs before the fall samples.
- npc.collision.fall-sample: 2,924 events, t=42.21s to 606.69s, 19 actors; candidate_count=0 in 2,006 (68.6%); world_contact=false in 2,645 (90.5%), true in 279 (9.5%); candidate_count max 35; grounded_this_tick=false in all samples, by construction (the event emits only when !groundedThisFrame && vel.z < -5.0f, rate-limited to 30 ticks per actor: src/physics/movement/physics-collision-glb-main.cpp:615-646).
- Per-actor final-z ranges stay near the authored floor: z-min spans ~2303.7-2351.2 and z-max ~2367.1-2404.3. The largest first-to-last z change is NPC-100011 -14.95 m, and every large drop recovers (NPC-100011 reaches z-max 2392.3). No actor shows monotonic descent out of the world.

INFERENCE / UNCERTAINTY
- On the tested map/mode, current gen-573 fresh NPCs are held near the floor and do not fall through the world. The surviving non-contact/zero-candidate samples look like ordinary ledge/step descents plus broadphase misses, not the reported continuous fall.
- This run does not satisfy the reported reproduction: it is dust2cyberiav4/funcounterstrike, not Payload/funworld3 with payload.json "arcade_collision": true. The payload world mesh is much larger (issue evidence headless_triangles=31270) than dust2cyberiav4's 5465, so the dust2 result does not transfer, and candidate gathering (68.6% zero-candidate samples here) remains the leading unverified hypothesis.
- The acceptance phrase "world_contact=true and grounded_this_tick=true" cannot be established from npc.collision.fall-sample alone, because the event is emitted only while descending and not grounded. A positive grounded/contact event or position-stability evidence is still required.
- Proof 41 is stale on the server-npcs.cpp hash; a fresh Payload pass would still need the proof re-recorded against current source hashes.

CONTRARY EVIDENCE
- Case-insensitive scan of every retained run.json (map/mode fields and run names) under logs/ finds no Payload/funworld3/arcade run. Recent retained fresh-NPC runs are dust2cyberiav4 with juggernaut horde, fun counter strike, or ssbm; the live dev-loop runtime is mimita-duels-map-v3/bloodtest with 0 NPCs.
- The original gen-420 fixture journal remains pruned; only the copied fields in the issue evidence survive, so the reported failure cannot be re-inspected.
- Some fall samples have world_contact=true and candidate_count up to 35, so the world query is not uniformly empty.

NEXT DISCRIMINATING CHECK (pinned)
1. Via the root dev-loop start one isolated run on map funworld3 with gamemode payload (payload.json has "arcade_collision": true) and a small fresh NPC count, pinned to the current published generation (573, or gen 574 once published); record run id, EXE sha256, DLL generation+sha256, journal path.
2. In that exact journal require: run.json build_identity.game_generation == pinned generation and game_dll_sha256 == published code hash; npc.phase-window.module_generation == pinned generation (do NOT use game.hotness.summary.generation as identity); npc.spawn.resolved after run start; npc.collision-world.summary; and a positive grounded/contact signal.
3. Fail if any fresh-spawn NPC's final z decreases monotonically below the authored funworld3 floor z with candidate_count=0 and world_contact=false across consecutive samples. Pass only if positions stabilize at the floor and a grounded/contact signal is true with no sustained negative-z series.
4. If it fails, record the first divergence as gatherGLBTriangles/broadphase coverage for the NPC capsule at physics-collision-glb-main.cpp:615-645, and capture unknown_solid_triangles and solid_bounds for funworld3.

CONFIDENCE
- Facts (readiness, proof, hashes, counts, retained-run scan, generation-field semantics): high. Gen-573 no-fall-through on dust2cyberiav4: high for that run. Reported Payload/funworld3 bug: still unproven. Candidate-gathering hypothesis: medium-low.
