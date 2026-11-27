# issue-87d8c9c7ffb1: Payload cart is not collidable in Arcade mode

## Human report (verbatim, immutable)

> cant collide with payload in the arcade collision mode


## Triage

### 2026-11-21T14:56:16.298Z — payload-arcade-collision-fix

Summary: Fix the Arcade capsule contact path so actors collide with Payload cart physical geometry; verify against the active Payload scene without disturbing the shared run.
Owner: payload-arcade-collision-fix
Priority: high

## Scheduled independent review

### 2026-11-21T20:39:03.109Z — cpp-live-generation-timers

A bounded independent review was queued as wake wake-40413bf735f4 after 166 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (unproven)

### 2026-11-21T20:46:25.601Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review 2026-11-21T20:4xZ (issue-review-sweeper). Scope: issue brief/readiness, proof status, linked artifacts, journals. No source edits, build, restart, or fix claimed.

OBSERVED FACTS
- Readiness: ready=true, score 8, state=triaged; one open task payload-cart-arcade-contact owned by payload-arcade-collision-fix (agent inactive since 2026-11-21T15:55:08Z). Issue has no linked build or run records (builds=[], runs=[]).
- Source fix is present in the working tree: src/physics/physics-types.h adds SweepHit entityId/surfaceVelocity; src/physics/movement/physics-collision-glb-sweep-slide.cpp adds a bounded dynamic physical-box candidate pass and emits collision.arcade_entity.sweep (around line 836); server-npcs.cpp spawn candidate cap 24->128. git diff --stat for the three files: 208 insertions, 19 deletions.
- Source hashes current vs published DLL snapshots: sweep-slide=6b5fd3cec1d3deab... and physics-types=f3f5ca679e7e59c3... match build snapshots for generations 428 (game-dll-20261121T193255143084Z-1b7951), 429 (194526...8036b9), 430 (195311...0a1309), 431 (200541...5b40e3), and 432 (201417...2d9df4). So the fix IS compiled into the current published DLL (build/current.json: generation 432, status ok, published 2026-11-21T20:17:09Z).
- Runtime evidence is absent: rg found zero 'collision.arcade_entity.sweep' occurrences in every file under logs/. The only extant run journal is 20261121_144350, and its server (PID 7556) loaded build\\game\\generation-420\\game-g420.dll (event game.host.probe.started, launch_id 2026-11-21T14:43:50.330Z) - pre-fix. players=0; last journal write 15:29:59Z.
- dev-loop --status: lifecycle=starting, build=published_stale, run=RUNNING (server PID 7556, uptime ~5h59m), latest_source_stale=True. The live server has not activated generations 428-432.
- Proof record task-proof-7d953b610754: result=unproven, status=runtime_proven, journal_path='', run_id='', game_generation=0, verified_utc=''. The board/status lists this task as unproven.

INFERENCE / UNCERTAINTY
- The fix is compiled and published but has never been exercised in a run pinned to a published post-fix generation. This is a proof/activation gap, not a source gap.
- The 'status=runtime_proven' with empty journal/run/generation and empty verified_utc is internally inconsistent; likely a recording/verification bookkeeping mismatch rather than a behavior claim. Exact semantics of 'status' vs 'result' not confirmed from this sweep.
- Static reading cannot establish that the dynamic box sweep actually blocks the capsule, that contact is stable, or that static-world Arcade behavior is unaffected; only a pinned run can.

CONTRARY EVIDENCE
- None found supporting success. The existing generation-420 payload.cart-trace events describe the pre-fix state (cart_entity=1 present; many NPCs world_contact=false) and cannot validate this change.
- Note for the user's 'live game should count' rule: the running game does not count here because its journal pins generation 420, not the fixed generation.

NEXT DISCRIMINATING CHECK
- Through the root dev-loop, launch (or restart only if the scenario requires) a Payload/Arcade run pinned to published generation 432 (or any of 428-432 with matching source hashes) and drive a player root capsule into cart entity id 1. In that exact run journal require: game.host.probe.started showing generation 428-432; at least one collision.arcade_entity.sweep with entity_id=1 and actor=<player>; no capsule/cart penetration across the contact; and unchanged static-world Arcade contact. If only an isolated test run is used, keep the shared run 20261121_144350 untouched and report the pinned artifact IDs separately. Keep the issue open until that journal exists or an explicit proof exemption is recorded.

## Scheduled independent review

### 2026-11-22T20:52:38.733Z — cpp-latency-small-tu-benchmark

A bounded independent review was queued as wake wake-7291a4028e38 after 331 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (unproven)

### 2026-11-22T20:58:55.197Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review 2026-11-22T20:5xZ (issue-review-sweeper). Scope: issue brief/readiness, proof status, artifacts, journals. No source edits, build, restart, or fix claimed.

OBSERVED FACTS
- Readiness: ready=true, score 8, state=in_review; one open task payload-cart-arcade-contact, owner payload-arcade-collision-fix (agent still inactive). Issue has no linked build or run records.
- Fix is now committed in HEAD (not just dirty working tree). src/physics/movement/physics-collision-glb-sweep-slide.cpp:836 emits collision.arcade_entity.sweep; the physical-box candidate pass is at ~L266-284 (PhysicalEntitySystem, Box shapes); source imports physics/physical-entity.h. git diff of the issue paths now shows only src/network/server-npcs.cpp (+78, unrelated work); the sweep/physics-types fix is part of HEAD.
- Source identity matches the proof record and the current published build: sha256(physics-collision-glb-sweep-slide.cpp)=6b5fd3cec1d3deab... and sha256(physics-types.h)=f3f5ca679e7e59c3... exactly equal proof task-proof-7d953b610754 source_hashes and the generation-519 snapshot source tree (build snapshot game-dll-20261122T204052787084Z-49370a). Latest 20:54-20:56Z snapshots still carry the same sweep hash. build/current.json: generation 519, revision be87d2a8..., status ok, published 2026-11-22T20:41:12Z.
- A post-fix run DID occur: logs/2026-11-22/20261122_204150 loaded generation 519 (game.hotness.summary generation=519; launch_id 2026-11-22T20:41:50.138Z), 1 player, 19 NPCs, Arcade capsule path active (collision.speed_collapse from doGLBSweepSlide, collision.actor-world). BUT its mode/map is funcounterstrike/dust2cyberiav4, not Payload/funworld3; no cart entity, no payload.cart* events, no collision.arcade_entity.sweep.
- Zero collision.arcade_entity.sweep occurrences exist anywhere under logs/ (rg -l across all events.jsonl returned none). No run has yet exercised a player root capsule against the Payload cart.
- dev-loop --status at review time: lifecycle=game_build_queued, build=published_current, run=(none)/NOT RUNNING; game-build generation=519 failed on a later revision (src/npc/recast-navigation.cpp) and is waiting for retry. Latest run ended ~20:52Z.
- Proof record task-proof-7d953b610754 still reads result=unproven, status=runtime_proven, journal_path='', run_id='', game_generation=0, verified_utc=''. The board lists the task as unproven.

INFERENCE / UNCERTAINTY
- Compilation/identity is now strongly established: the exact fixed source is in the current published generation 519. This is still build evidence, not behavior proof.
- The remaining gap is a targeted activation/proof gap: no pinned Payload run exercised player-vs-cart contact. The 204150 run proves the generation loads and serves a player, and that the Arcade sweep path runs, but not that the dynamic box blocks the capsule.
- The status/result mismatch in the proof record persists across two reviews; most likely a bookkeeping/verification-recording inconsistency, not a hidden success claim.
- NPC fall samples in 204150 (world_contact=false, grounded_this_tick=false) concern the separate payload NPC-grounding issue, not cart collidability; do not use them here.

CONTRARY EVIDENCE
- None supporting success for this issue. No cart contact events exist in any generation including 519.

NEXT DISCRIMINATING CHECK
- Through the root dev-loop, run Payload/funworld3 Arcade pinned to generation 519 (or any generation whose snapshot matches sweep-slide=6b5fd3ce... / physics-types=f3f5ca67...) with a player. In that exact journal require: game.host.probe.started/hotness generation incl. 519; at least one collision.arcade_entity.sweep with entity_id=<cart id, previously 1>; no capsule/cart penetration across contact; unchanged static-world Arcade contact. Report pinned run_id, generation, DLL hash, and journal path. Keep the issue open and the proof unproven until that journal exists or an explicit exemption is recorded.

## Scheduled independent review

### 2026-11-23T21:32:02.677Z — hot-reload-latency-root

A bounded independent review was queued as wake wake-ba365e6b1b58 after 458 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (unproven)

### 2026-11-23T21:41:38.718Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review 2026-11-23T21:4xZ (issue-review-sweeper), third review of this issue. Scope: brief/readiness, proof status, linked artifacts, latest journals. No source edits, build, restart, or fix claimed.

OBSERVED FACTS
- Readiness: ready=true, score 8, state=in_review, missing=[]. One open task payload-cart-arcade-contact, owner payload-arcade-collision-fix (still inactive; last activity 2026-11-21T15:55:08Z, ~49h). No linked build/run records.
- Source identity unchanged and now confirmed in the current published build: sha256(physics-collision-glb-sweep-slide.cpp)=6b5fd3cec1d3deab..., sha256(physics-types.h)=f3f5ca679e7e59c3... Both equal proof task-proof-7d953b610754 source_hashes and appear in every recent build snapshot checked today (e.g. gen-570 candidate game-dll-20261123T191346937240Z-35f07c and gen-571 game-dll-20261123T200924195125Z-7401c3). build/current.json: generation 571, status ok, published 2026-11-23T20:23:13Z. So the fix is compiled into the current published generation (build evidence only).
- Still zero runtime evidence: rg across all logs found 0 occurrences of collision.arcade_entity.sweep (the proof's required event/field).
- The most recent post-fix run that loaded generation 571 is logs/2026-11-23/20261123_205333 (game.host.probe.started generation 571, multiple player.spawn events) but its map is dust2cyberiav4 / mode funcounterstrike; cart occurrences=0, arcade_entity occurrences=0. This is the same non-Payload pattern as the prior 20261122_204150 run (still present, still funcounterstrike/dust2), so it cannot validate cart contact.
- Current live server (dev-loop run 20261123_192224, PID 10164, uptime ~02:10) loaded generation 570 and launch identity map=mimita-duels-map-v3 mode=bloodtest gamemode=bloodtest npcs=0; no cart/arcade_entity events. Per the user 'live game should count' rule it does not count: wrong scenario, and not the newest published generation.
- Last server journal that actually loaded funworld3 is 2026-11-13 (pre-fix). payload.cart-spawned events exist only in 11-12/11-13 logs. Gameterminal logs on 11-22/11-23 mention Funworld3/Payload only as '[MAP SCAN] discovered' (asset discovery), not a loaded run. The pre-fix journal cited in the first review (logs/2026-11-21/20261121_144350) no longer exists; it has been rotated, so only the proof record remains as its trace.
- Proof record task-proof-7d953b610754 (DB id 40): status=runtime_proven, result=unproven, build_id='', game_generation=0, run_id='', journal_path='', event=collision.arcade_entity.sweep, field_predicate=entity_id, verified_utc=''. Source hashes match current source. The board still lists it unproven; the status/result mismatch persists across all three reviews.
- dev-loop --status at review: lifecycle=command_rejected, build=published_stale (latest_source=4, stale=True), run=RUNNING 20261123_192224.

INFERENCE / UNCERTAINTY
- Compilation and activation of the fix are strongly established (gen 570/571 loads and serves players). Behavior is not proven: no Payload/Arcade run has ever driven a player capsule into the cart in any post-fix generation.
- This is now a repeated activation/proof gap, not a source gap: the same conclusion holds 49h and generations 519->571 after the prior review. Static reading cannot prove the dynamic box blocks the capsule or that static-world Arcade contact is unchanged.
- The status=runtime_proven with empty run/generation/journal is most likely a coordinator bookkeeping default (status column not gated on verified evidence), not a success claim; exact semantics remain unconfirmed.

CONTRARY EVIDENCE
- None supporting success since the previous review: no collision.arcade_entity.sweep and no payload.cart-spawned in any generation >= 428. The NPC fall samples belong to the separate payload NPC-grounding issue and are not evidence here.

NEXT DISCRIMINATING CHECK
- Through the root dev-loop, launch an isolated Payload Arcade run (map funworld3 or payload1/payload2, whatever the scene actually uses) pinned to generation 571 (or a generation whose snapshot matches sweep-slide=6b5fd3ce... / physics-types=f3f5ca67...), with a player, and drive the root capsule into cart entity id 1. In that exact journal require: game.host.probe.started generation 570/571; >=1 collision.arcade_entity.sweep with entity_id=1 and actor=<player>; no capsule/cart penetration across contact; unchanged static-world Arcade contact. Report pinned run_id, generation, DLL hash, and journal path. Keep the issue open and the proof unproven until that journal exists or an explicit exemption is recorded. A user-owned run can satisfy this only if it is Payload/Arcade on a matching generation.

## Scheduled independent review

### 2026-11-25T00:34:28.711Z — cpu-optimization-loop

A bounded independent review was queued as wake wake-302e2d4f63c4 after 660 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (unproven)

### 2026-11-25T00:42:55.262Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review 2026-11-25T00:4xZ (issue-review-sweeper), fourth review of this issue. Scope: brief/readiness, proof status, linked artifacts, latest journals. No source edits, build, restart, or fix claimed.

OBSERVED FACTS
- Readiness: ready=true, score 8, state=in_review, missing=[]. One open task payload-cart-arcade-contact, owner payload-arcade-collision-fix (still inactive; last activity 2026-11-21T15:55:08Z, ~81h). No linked build/run records.
- Source identity: live sha256(physics-collision-glb-sweep-slide.cpp)=6b5fd3cec1d3deab... and sha256(physics-types.h)=f3f5ca679e7e59c3..., equal to proof task-proof-7d953b610754 source hashes. Every 2026-11-24 game-DLL snapshot checked (36 dirs, 00:39Z-23:09Z) carries these exact hashes, including the gen-627 source root .dev/build-snapshots/game-dll-20261124T222855538088Z-1d20e7.
- Build: build/game/current.json generation 627, status ok, published 2026-11-24T22:33:56Z, revision 7092860..., dll build\game\generation-627\game-g627.dll. dev-loop reports a later gen-627 attempt failed changed_source_preflight at 23:09 (src/engine/engine-tick-camera.cpp:1186 glm::remainder), so build=published_stale; that failure is outside this issue's paths.
- NEW EVIDENCE: the required event now exists at runtime. Run 20261124_150432 (client-01 segment events-0018.jsonl, lines ~35650-44330) contains 12 collision.arcade_entity.sweep events from doGLBSweepSlide (source src/physics/movement/physics-collision-glb-sweep-slide.cpp:847, reason "Arcade root capsule swept against moving physical box"). That run loaded build\game\generation-595\game-g595.dll (game.host.probe.started, client pid 36216, launch_id 2026-11-24T15:04:32.689Z); dev-loop launch configuration was map=slaughterhouse1, launch_mode=59, mode "Bloodtest - Slaughterhouse 1 with NPC". All 12 events have actor=NPC-1000; entity_id=2 (2 events) and entity_id=3 (10 events). No player actor, no cart entity.
- No Payload cart runtime evidence: payload.cart-spawned exists only under logs/11-12-2026 and logs/11-13-2026 (pre-fix), and no post-fix run loads funworld3/Payload. Current shared live run 20261124_221014 (RUNNING, server PID 27372, uptime ~2h26m at review) loaded generation 625 map=dust2cyberiav4 mode=funcounterstrike; it is not Payload, so per the user "live game should count" rule it does not count here.
- Proof record task-proof-7d953b610754 remains result=unproven with empty build_id/run_id/journal_path/game_generation/verified_utc, event=collision.arcade_entity.sweep, field_predicate=entity_id; the board lists the task unproven. The status/result mismatch persists across all four reviews.

INFERENCE / UNCERTAINTY
- This corrects the prior three reviews' "zero occurrences anywhere under logs/" statement: the dynamic physical-box sweep path is now confirmed to execute and emit in a real pinned post-fix generation (gen 595). That is runtime evidence that the code path runs, not evidence of the reported behavior.
- Acceptance is still unmet: the events are NPC-vs-moving-box in Bloodtest/Slaughterhouse 1, not player-vs-Payload-cart. There is no player contact, no cart entity, and no measurement of no capsule/cart penetration or unchanged static-world Arcade contact. The proof predicate (event + entity_id field) is satisfied by this unrelated scenario, so it does not by itself validate the fix.
- Entity ids 2/3 are not established to be the Payload cart; in this map/mode they are generic moving physical boxes. Whether they share the cart's physics archetype is unknown.
- The gen-595 DLL file is no longer on disk (build/game/generation-595 absent), so I could not hash the exact binary; identity rests on the event's source:line plus all same-day snapshots carrying the fix hash.

CONTRARY EVIDENCE
- No post-fix run event matches the acceptance scenario (player actor + Payload cart). The 12 NPC contacts in 20261124_150432 are contrary to treating the existing event predicate as sufficient proof.
- No Payload/funworld3 run exists after 2026-11-13.

NEXT DISCRIMINATING CHECK
- Through the root dev-loop, start an isolated Payload/funworld3 Arcade run (do not disturb shared run 20261124_221014) pinned to a generation whose snapshot carries sweep-slide=6b5fd3ce... / physics-types=f3f5ca67... (gen 595-627), with a player. Drive the player root capsule into the cart entity (previously id 1). In that exact run journal require all of: game.host.probe.started dll generation in 595-627; at least one collision.arcade_entity.sweep with actor=<player> and entity_id=<cart id>; player/cart separation stays >= capsule radius across contact (no penetration); static-world Arcade contact unchanged. Report run_id, generation, DLL hash/path, and journal path. Keep the issue open and the proof unproven until that journal exists or an explicit exemption is recorded.

VERDICT: unproven.

## Scheduled independent review

### 2026-11-26T01:35:09.721Z — daily-review-20261126

A bounded independent review was queued as wake wake-b3866944018c after 756 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (unproven)

### 2026-11-26T01:47:19.913Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review 2026-11-26T01:4xZ (issue-review-sweeper), fifth review of this issue. Scope: brief/readiness, proof status, linked artifacts, latest journals. No source edits, build, restart, or fix claimed.

OBSERVED FACTS
- Readiness: ready=true, score 8, state=in_review, missing=[]. One open task payload-cart-arcade-contact, owner payload-arcade-collision-fix, still inactive (last board activity 2026-11-21T15:55:08Z, ~110h). No linked build/run records.
- Source identity: live sha256(physics-collision-glb-sweep-slide.cpp)=6b5fd3cec1d3deab... and sha256(physics-types.h)=f3f5ca679e7e59c3..., exactly equal to proof task-proof-7d953b610754 source_hashes. The newest three build snapshots checked (.dev/build-snapshots/game-dll-20261126T013504174400Z-8ca897, ...011841619686Z-74b4f3, ...005556909723Z-fa8154) all carry the same sweep-slide hash. build/current.json: generation 664, status ok, published 2026-11-26T01:37:03Z, dll build\game\generation-664\game-g664.dll. So the fix is compiled into the current published generation 664 (build evidence only).
- Proof record task-proof-7d953b610754 (DB id 40) is unchanged: status=runtime_proven, result=unproven, build_id='', game_generation=0, run_id='', journal_path='', verified_utc=''; board lists the task unproven. The status/result mismatch persists across all five reviews.
- No new qualifying runtime evidence. rg across all logs still returns exactly one file with collision.arcade_entity.sweep: logs/2026-11-24/20261124_150432/client-01/events-0018.jsonl (the 12 already-reviewed NPC-actor events, entity_id 2/3). payload.cart-spawned still appears only in logs/11-12-2026 and logs/11-13-2026 (pre-fix; fields entity=1, size [5.0,5.5,5.0]).
- NEW since the fourth review: run 20261125_204934_01. Server-01 launched map=arena1 / "Vehicle Arena - Rocket League Free Play" with generation-644 DLL. At t≈322s (wall 2026-11-25T20:54:58Z) the client switched in-game to map=funworld3 / mode=jugger_horde on a locally launched server (network.gui-server.launch, pid 20004, port 1357) and reported network.client.connect.preflight world_loaded=true. This is the first post-fix touch of the Payload map, but mode is jugger_horde (not Arcade/Payload); that run's journals contain zero payload.cart* and zero collision.arcade_entity.sweep. No run.json anywhere under logs/2026-11-25 or logs/2026-11-26 records a funworld3/Payload launch config (run.json only captures the dev-loop launch; in-game switches are not reflected there).
- 2026-11-26 runs so far (013852, 014055_01, 013130, etc.) are arena1/Vehicle Arena. dev-loop --status: lifecycle=launching_server, build=published_current, latest_source=0 stale=False; board run=20261126_013852.
- Live read-only discovery (evidence-watch list --json): one unverified server PID 26436 started 2026-11-25T23:16:58Z with --map slaughterhouse1 --mode bloodtest --npcs 1 and "journal not found for this PID". Not Payload, and no resolvable run identity, so it cannot count.
- build_resource_gaps returned three new gap_keys (...011841619686Z-74b4f3, ...005556909723Z-fa8154, ...004443042199Z-2455a3). All three are already recorded in issue-2a5c5da53565, so this sweep appended no duplicate.

INFERENCE / UNCERTAINTY
- Compilation and identity of the fix in gen 664 are strongly established. The only remaining gap is activation/proof in the exact reported scenario (Payload cart, Arcade mode, player actor).
- The 11-25 20:54 switch shows the Payload map loads post-fix with world_loaded=true, but the gamemode is jugger_horde, no cart spawned, and it was not a dev-loop-launched Payload profile, so it is a near-miss rather than proof. Whether a cart spawns under jugger_horde on funworld3 is unknown; the only cart-spawn sample is from Payload/duel mode (11-13).
- Static reading still cannot establish that the dynamic physical-box sweep blocks the capsule at 60 Hz or that static-world Arcade contact is unchanged.

CONTRARY EVIDENCE
- No new success evidence. The only arcade_entity.sweep samples remain NPC-vs-moving-box in Bloodtest/Slaughterhouse 1 (actor=NPC-1000, entity 2/3), which do not satisfy actor=<player> + entity=<cart>.
- The 11-25 funworld3/jugger_horde switch is contrary to any claim that a qualifying Payload run has occurred.

NEXT DISCRIMINATING CHECK
- Through the root dev-loop, start an isolated Payload/funworld3 Arcade run (leave shared run 20261126_013852 and any human server untouched) pinned to a generation whose snapshot carries sweep-slide=6b5fd3ce... / physics-types=f3f5ca67... (any recent gen including 664), with a player, using the Payload gamemode profile that emits payload.cart-spawned. Drive the player root capsule into the cart (previously entity id 1). In that exact run journal require all of: game.host.probe.started dll generation with matching source hashes; payload.cart-spawned; >=1 collision.arcade_entity.sweep with actor=<player> and entity_id=<cart id>; player/cart separation stays >= capsule radius across contact (no penetration); static-world Arcade contact unchanged. Report run_id, generation, DLL hash/path, and journal path. Keep the issue open and the proof unproven until that journal exists or an explicit exemption is recorded.

VERDICT: unproven.

## Scheduled independent review

### 2026-11-27T01:55:02.097Z — build-wait-reviewer

A bounded independent review was queued as wake wake-188533acf54f after 815 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (unproven)

### 2026-11-27T02:04:37.736Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review 2026-11-27T01:5xZ (issue-review-sweeper), sixth review. Scope: brief/readiness, proof status, linked artifacts, latest journals. No source edits, build request, restart, or fix claimed.

OBSERVED FACTS
- Readiness: ready=true, score 8, state=in_review, missing=[]. One open task payload-cart-arcade-contact, owner payload-arcade-collision-fix still inactive (last board activity 2026-11-21T15:55:08Z, ~130h). No linked build/run records.
- Source identity unchanged and equal to the proof record source_hashes: sha256(src/physics/movement/physics-collision-glb-sweep-slide.cpp)=6b5fd3cec1d3deab..., sha256(src/physics/physics-types.h)=f3f5ca679e7e59c3.... build/game/current.json: generation 752, status ok, revision e9f5c0f7..., dll build\game\generation-752\game-g752.dll, published 2026-11-27T01:49:27Z. The fix is compiled into the current published generation (build evidence only).
- Proof task payload-cart-arcade-contact remains result=unproven on the board; no verified journal/run/generation.
- NEW since the fifth review: the only runtime artifact that ever showed collision.arcade_entity.sweep (12 NPC-actor events, entity_id 2/3, in logs/2026-11-24/20261124_150432/client-01/events-0018.jsonl, cited by review id 406) is NO LONGER ON DISK. `rg --no-ignore -l collision.arcade_entity.sweep logs` returns zero files; logs/2026-11-24 does not exist; logs/11-24-2026 holds only Gameterminal/Server *.txt with no run/events dirs. Earliest surviving run-log date roots are 11-25-2026 / 11-26-2026 / 2026-11-27.
- payload.cart-spawned still exists only in pre-fix logs 11-12-2026 and 11-13-2026 (7 files).
- Current live run 20261127_015112: server-01 loaded game-g752.dll (game.host.probe.started, launch_id 2026-11-27T01:51:12.233Z, pid 25844), map=dust2cyberiav4 mode=funcounterstrike, 1 player, 1 NPC, client pid 14572. That server journal has zero payload.* event names, zero cart events, zero arcade_entity events (296 raw 'payload' matches are payload_bytes fields / objective-context text, not Payload mode). Not a Payload/funworld3 run, so it cannot count.
- dev-loop --status: lifecycle=game_building (new attempt game-dll-20261127T015700314777Z in manifest_validation), build=published_current (gen 752 already carries the fix), run RUNNING 20261127_015112.

INFERENCE / UNCERTAINTY
- Compilation and identity of the fix in published generation 752 are strongly established. The remaining gap is still activation/proof in the exact reported scenario (Payload cart, Arcade mode, player actor). No Payload/funworld3 run exists after 2026-11-13 (pre-fix).
- The rotation of the only prior sweep sample means the earlier "the path emits in a real pinned generation" observation now rests on review record id 406 plus coordination messages, not on an artifact I can re-inspect. That does not change the verdict (those 12 events were NPC-vs-box, not player-vs-cart), but it removes our only on-disk confirmation that the code path emits at all, lowering confidence.
- Unknown: whether entity 2/3 in that Bloodtest map shared the cart's physics archetype; whether funworld3 under a Payload/Arcade gamemode spawns payload.cart-spawned post-fix.

CONTRARY EVIDENCE
- No evidence supporting success. The only sweep samples were NPC actors (not <player>) against non-cart entities, and are now pruned. No Payload/funworld3 run after 2026-11-13.

NEXT DISCRIMINATING CHECK
- Through the root dev-loop, start an isolated Payload/funworld3 Arcade run (leave shared run 20261127_015112 and any human server untouched) pinned to generation 752 (or any generation whose snapshot carries sweep-slide=6b5fd3ce... / physics-types=f3f5ca67...), with a player, using the gamemode whose journal emits payload.cart-spawned. Drive the player root capsule into the cart (previously entity id 1). Require in that exact journal: game.host.probe.started dll=generation-752 (or matching); payload.cart-spawned; >=1 collision.arcade_entity.sweep with actor=<player> and entity_id=<cart id>; player/cart separation stays >= capsule radius across contact (no penetration); static-world Arcade contact unchanged. COPY that journal to a protected path before it ages out of the 3-date log retention. Report run_id, generation, DLL hash/path, and journal path. Keep the issue open and the proof unproven until that journal exists or an explicit exemption is recorded.

VERDICT: unproven (compiled in gen 752; activation/proof gap persists; prior sole sweep artifact rotated away).
