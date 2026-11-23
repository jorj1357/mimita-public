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
