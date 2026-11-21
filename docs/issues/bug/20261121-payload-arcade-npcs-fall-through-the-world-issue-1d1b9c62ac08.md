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
