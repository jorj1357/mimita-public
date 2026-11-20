# issue-da337d1fe2f3: Arcade collision consistency: limbs-only, capsule actor/world, no part snapping

## Human report (verbatim, immutable)

> if the mode is arcade of collisions, then its just limbs that collide, weapons dont collide with world, limbs dont collide with other limbs, actor vs world collisions all uses the capsule not any other thing , it overrides weapon collisions , and ensrue there is no snappign in and out of parts when walking into walls or colliding with them, that all is a issue to be assigned invesitgated later


## Task linked

### 2026-11-20T03:44:00.411Z — runtime-60hz

Task: arcade-collision-audit (step 0)

## Task linked

### 2026-11-20T03:44:00.788Z — runtime-60hz

Task: arcade-collision-consistency (step 0)

## Deferred

### 2026-11-20T03:44:01.044Z — runtime-60hz

Reason: User requested this be assigned and investigated later; not part of the current 60Hz/reconnect acceptance run.
Until: (not set)

## User quote

### 2026-11-20T05:02:49.536Z — runtime-60hz

Via runtime-60hz; supports issue-da337d1fe2f3:

> my own actor capsule size is editable in json somewhere ? maybe collisions.json? so that i dont go into geometry , but also if i am like dont push me out of geometry , just dont let me go into it at all, let me slide msoothly along it , in the capsuel collision mode ... capsule arcade collision mode needs work on seams like dont treat a seam as things to colldiew ith its just joined, its one big surface, dont collide with the seams its jut streated as a surface and also slopes, im getting stuck on a slope in dust2cyberiav4 ... Position: (-685.651, -22.853, 2351.122) ... big ramp ... i cant move i dont move at all i ahve to ragdoll to get out of it

## Task linked

### 2026-11-20T05:02:54.546Z — runtime-60hz

Task: arcade-capsule-json (step 0)

## Task linked

### 2026-11-20T05:02:58.655Z — runtime-60hz

Task: arcade-no-penetration-slide (step 0)

## Task linked

### 2026-11-20T05:03:03.024Z — runtime-60hz

Task: arcade-seams-slopes (step 0)

## Task linked

### 2026-11-20T05:03:07.440Z — runtime-60hz

Task: dust2-ramp-repro (step 0)

## Agent discussion and suggestions

### 2026-11-20T05:48:43.996Z — runtime-60hz

Message: `msg-97400135b3b6` (handoff.request)

Subject: Collision issue tasks + ramp repro

I added the requested collision details to issue-da337d1fe2f3: default arcade mode, bounce, JSON-editable capsule, no-penetration slide, seams as one surface, slopes, and the exact big-ramp position (-685.651, -22.853, 2351.122), where the player becomes stuck and must ragdoll out. Tasks arcade-capsule-json, arcade-no-penetration-slide, arcade-seams-slopes, dust2-ramp-repro are linked. You are active on collision; please claim/coordinate rather than duplicate.

## Context

### 2026-11-20T06:25:35.065Z — work-metrics

reproduction: Run dust2cyberiav4 and walk into the ramp at (-685.651, -22.853, 2351.122) plus seams, corners, and capsule body contacts.
expected_behavior: Default arcade collision uses a capsule, slides on seams without catching, climbs allowed slopes, and does not snap limbs or penetrate.
acceptance: Live journal and screenshots prove capsule contact, seam continuity, slope traversal, and no penetration at the reported position.
paths: ['config/collision.json', 'docs/architecture/collision/collision.md', 'src/physics/movement/physics-collision-glb-main.cpp', 'src/physics/movement/actor-collision-mesh.cpp']
evidence: {'map': 'dust2cyberiav4', 'position': '-685.651,-22.853,2351.122', 'owner': 'arcade-collision-default'}

## User quote

### 2026-11-20T06:31:01.958Z — juggernaut-npc-stuck-shooting

Via juggernaut-npc-stuck-shooting; supports issue-da337d1fe2f3:

> my own collisions as capsule vs world i still get put into blocks sometimes , cant ahve going into blocks at all like when my limbs my animation pose moves it cant like snap me out it has to be like&#x20; it doenst care about the animation limb poses for collsiiosns i tihnk in arcade mode

## Triage

### 2026-11-20T06:32:35.998Z — juggernaut-npc-stuck-shooting

Summary: Resume the existing deferred Arcade collision report after the user reports current capsule/world penetration. Audit the shared actor capsule/world path and limb-pose interactions; verify no penetration or limb-driven snap in a pinned runtime, while preserving slide and seam behavior.
Owner: juggernaut-npc-stuck-shooting
Priority: high

## Agent discussion and suggestions

### 2026-11-20T06:33:01.066Z — juggernaut-npc-stuck-shooting

Message: `msg-4ed30943f406` (ask)

Subject: Review current Arcade capsule penetration report

Please review issue-da337d1fe2f3 for the current report that the player capsule sometimes enters world blocks in Arcade mode, and the concern that limb animation poses can affect collision or snap the actor. Identify the capsule/world owner, whether skeletal/limb poses enter its contact path, and the smallest diagnostic or runtime scenario that distinguishes them. Suggest corrections; do not edit source.

## Agent suggestion

### 2026-11-20T06:45:16.314Z — juggernaut-npc-stuck-shooting

Static source trace: with arcadeCollision=true, physics-collision-glb-main.cpp skips the actor-triangle and body/weapon movement pass and proceeds through the root capsule sweep/slide. However, the player still calls containActorFinalPose after the weapon update in engine-tick-combat.cpp, and npc.cpp calls it at post_animation; those call sites have no arcadeCollision guard, and the final-pose owner tests animated body/weapon collision meshes and may translate the root. This is a concrete candidate for pose-driven correction despite the capsule movement path. Verify the logged stage and root correction (player.pose.weapon_contained/player.pose.reset, npc.pose.contained/npc.pose.reset) in a pinned Arcade run before changing the shared collision owner.

## User quote

### 2026-11-20T06:51:22.168Z — juggernaut-npc-stuck-shooting

Via juggernaut-npc-stuck-shooting; supports unspecified:

> need to ensure bounciness bc i get stuck sometimes as capsule on clinders like it kills velocity

## Agent suggestion

### 2026-11-20T06:51:22.838Z — juggernaut-npc-stuck-shooting

User specifically reports Arcade capsule loses velocity/sticks on cylinders and wants all weapon world collision disabled. Static call sites show movement's Arcade branch skips actor-triangle/weapon collision solvers, but post_weapon player and post_animation NPC still call containActorFinalPose, whose sampleFinalPose includes physical body and weapon spheres and whose correction translates actor root. Review this against the capsule-only Arcade contract before changing code; verify bounce/slide/seams and melee contact in pinned runtime.

## Agent suggestion

### 2026-11-20T14:32:52.710Z — arcade-collision-investigation

Source investigation (read-only): Arcade is selected in simulate-tick.cpp from activeMode.features.arcadeCollision. In physics-collision-glb-main.cpp the actor-triangle solver is explicitly gated by !arcadeCollision, so Arcade always falls through to the legacy capsule pipeline. That pipeline calls doGLBSweepSlide, which gathers collectPlayerBodyCollisionSamples and adds their union candidates even though Arcade skips doBodyWeaponCollisionPhase; this leaves a plausible limb-sample influence in the sweep/broadphase path and needs direct narrowphase trace verification. Capsule slide projects remainingMove against hit normals, while applyCollisionContact in physics-collision-core.cpp calls respondVelocityAgainstNormal (velocity projection); the configured shared bounce response is not invoked by the legacy root capsule sweep/contact path, explaining why bounce config can be enabled while root capsule impacts do not bounce. Separately, player/NPC final-pose containment is called outside Arcade checks (simulate-tick.cpp / engine-tick-combat.cpp / npc.cpp); actor-final-pose.cpp samples animated body and weapon geometry and can translate the root, so it can still snap/penetrate relative to the capsule-only contract. Actor-pair capsules are owned by src/physics/movement/actor-pair-collision.cpp. Runtime proof is currently unavailable: dev-loop status is lifecycle=launch_failed, published_stale, server pid dead (run 20261120_141916); do not treat current source inference as proven user-visible cause. Next: trace all Arcade path stages and instrument first divergence on pinned dust2cyberiav4 run at existing reported ramp coordinate, plus cylinder/seam and actor-pair scenarios; prove no pose correction and capsule-only contacts, no penetration, continuous slide and configured bounce.

## Assignment

### 2026-11-20T14:39:24.557Z — arcade-collision-elite-proof

Owner: arcade-collision-elite-proof

## Context

### 2026-11-20T14:48:02.228Z — arcade-collision-elite-proof

expected_behavior: Arcade static-world contacts are resolved solely by the root player capsule with continuous sweep, stable slide along walls/seams/slopes, and configured bounce on physical impacts. Capsule actor-pair contacts remain the only actor-to-actor collision shape. Animated body/weapon poses do not push the Arcade root. Movement does not pass into world geometry; depenetration is a bounded safety fallback rather than normal movement.
acceptance: A fresh pinned Arcade run on Dust2CyberiaV4 repeats the reported ramp coordinate (-685.651,-22.853,2351.122), plus long cylinders, joined seams, slopes, walls/corners, and capsule actor contacts. Journal and visual evidence show root-capsule world contacts, no Arcade pose-root corrections, no penetration, sustained tangent movement unless blocked by a real corner, and collision.bounce.applied for capsule impacts. The AI stress controller later covers these surfaces during unscripted movement and fights.
paths: ['config/collision.json', 'config/actor-collision.json', 'src/physics/movement/physics-collision-glb-main.cpp', 'src/physics/movement/physics-collision-glb-sweep-slide.cpp', 'src/physics/movement/physics-collision-core.cpp', 'src/physics/movement/actor-final-pose.cpp', 'src/physics/movement/actor-pair-collision.cpp', 'docs/architecture/collision/collision.md', 'docs/workflows/runtime-scenario-validation.md']
evidence: {'text': 'User report linked via ctx-b8e608fcffdb and ctx-ebc8d9f357f9. Static source trace found the legacy capsule sweep built candidate triangles but did not call capsuleTriangleSweep; root contact processing projected velocity without invoking the configured bounce response; and final-pose containment could translate the root from animated body/weapon samples. Source corrections are in progress; current live proof is not yet available.'}
risk: Restoring a previously absent capsule sweep may change behavior for modes using the legacy pipeline, not only Arcade. Bounce at simultaneous contact planes can alter momentum in corners. Runtime checks must cover non-Arcade collision to detect regressions, and capsule sweep must preserve fixed-60-Hz execution, cached broadphase, and bounded contact logging.
proof_expectation: Publish a DLL from the shared dev-loop with all build notes. Run a fresh pinned test pair in an Arcade mode and inspect client/server journals for capsule sweep hit/slide, bounce response, actor-pair capsule contacts, and absence of pose-correction events across the recorded ramp and actual cylinder/seam/corner cases. Verify position remains clear throughout motion and only the known corner constraint removes velocity. Perform an unscripted high-speed client AI run on the same artifact. Compare a non-Arcade control. Screenshots/visual review and exact pinned run identity remain separate evidence.

## Agent discussion and suggestions

### 2026-11-20T15:08:15.699Z — consistency-evidence-reproduction

Message: `msg-4d1062497909` (question)

Subject: Coordinate Payload visual proof run and avoid duplicate sessions

I accepted the consistency issue's reproduce-lifecycle-payload-evidence task, and root asked me to coordinate with your existing payload-cart-visual-proof task instead of duplicating runs. Please share your planned pinned run/build/DLL generation, whether the Payload scene is active already, and the best safe capture window/command for screenshot_at_tick plus cart/NPC/ground telemetry. I'll inspect existing Payload logs meanwhile and won't start another Payload runtime until we agree.

## Agent suggestion

### 2026-11-20T16:54:54.283Z — arcade-collision-elite-proof

Source trace confirms Arcade world collision skips actor-triangle and body/weapon collision phases; it uses root-capsule sweep, capsule depenetration and floor recovery. Actor-vs-actor contacts are owned by actor-pair-collision and use the shared swept root capsules with tangential remainder projection. Remaining plausible catch points are legacy step-up rejection and seam projection in physics-collision-glb-sweep-slide.cpp; this is not yet a proven ramp root cause. Candidate generation 407 compiled player-ai.cpp and is now in object_cache_scan; still needs live Dust2 ramp replay plus NPC route evidence.

## Agent suggestion

### 2026-11-20T17:36:40.392Z — arcade-collision-elite-proof

Measured prior live TDM run 20261120_162116 on Dust2CyberiaV4 / Arcade generation 405: client journal has 586 collision.arcade_capsule.final events (root capsule), 6 collision.bounce.applied events (strength 0.35; floor and wall normals; all penetration=0), 0 emergency escapes, max sweep hits=2, and no collision.progress.stall events. 585/586 final capsule frames have zero penetration and zero post-sweep root correction; the sole exception is tick 1 spawn initialization, 0.049609m reported penetration and 0.05957m upward recovery. Server journal has 583 final capsule frames and 1,056 bounce events. This proves bounce and mostly clean sampled motion, but not the reported ramp replay or the strict no-penetration acceptance; old EXE source generation 3 means it is not the required fresh-build proof.

## Agent suggestion

### 2026-11-20T17:37:21.417Z — arcade-collision-elite-proof

The sole 0.049609m penetration / 0.05957m recovery event in run 20261120_162116 is tick 1 at 16:21:48.111Z. The client applied its authoritative network spawn (spawn_generation=3) at 16:21:48.488Z, 377 ms later. This points to the pre-authoritative placeholder pose during initialization, not a recurring gameplay correction; retain it as a startup exception until the new pinned live run confirms zero after spawn.

## Agent suggestion

### 2026-11-20T18:42:07.768Z — arcade-collision-elite-proof

Code finding: both failed step-up sample consistency and missing landing floor restored the original pose then continued the outer sweep loop with unchanged remainingMove, bypassing normal slide projection. Changed those failure branches to restore and fall through to seam/normal capsule slide; successful step-up still consumes stepMove and retries. Build note 2323ba098f28 is attached to failed sourcegen26 attempt game-dll-20261120T182421005313Z-3d8438; the canonical fresh attempt is generation409, sourcegen29, attempt game-dll-20261120T182726149737Z-df8100 and includes the patch.

## Agent suggestion

### 2026-11-20T19:32:44.841Z — arcade-collision-elite-proof

Current source recheck confirms actor-vs-actor has one shared owner: sim/simulate-tick.cpp builds local/remote player and NPC ActorPairCollisionActor records then calls resolveActorPairCollisions; actor-pair-collision.cpp derives each actor shape with makeActorCapsule and uses capsuleDistance for contacts. Server NPC pairs call the same resolver in server-npcs.cpp. Arcade's world path skips body/weapon pass and actor-triangle solver in physics-collision-glb-main.cpp. This is static contract evidence only; runtime actor-pair proof remains unverified.

## Agent discussion and suggestions

### 2026-11-20T19:47:26.494Z — arcade-collision-elite-proof

Message: `msg-71ac976de3e0` (shared.request)

Subject: Share fresh pinned TDM run for Arcade proof

I see your plan to start a pinned TDM run on generation 410 once sourcegen32 publishes. Please send the run ID and exact server/client journal paths after logger startup is confirmed; I will use that same run for Arcade collision and AI observations if the profile permits, and will not start a duplicate test.

## Agent discussion and suggestions

### 2026-11-20T19:50:45.880Z — arcade-collision-elite-proof

Message: `msg-f9bd99ea7c24` (shared.request)

Subject: Need client instance for AI collision proof

One detail for reusing the next successful run: failed manifest 20261120_194633 lists server-01 only, though the server has five NPCs. Collision issue proof also requires an AI-driven client (`player_control ai`), since the local PlayerAiController does not drive those server NPCs. If your run profile starts a client, please preserve it and share that client ID/journal too; otherwise let me add one after your logger/startup checks pass.

## Agent discussion and suggestions

### 2026-11-20T19:51:57.658Z — arcade-collision-elite-proof

Message: `msg-700965ec8595` (shared.request)

Subject: Player AI aim gate sourcegen33 now building

I made a small client AI edit to stop turning toward the live position of an occluded target while retaining pursuit movement and LOS-gated firing. Root dev-loop now reports sourcegen33 building. Please pin your startup-recovery run to the latest published EXE when ready (sourcegen33 if it succeeds) plus generation410; that will let the same run cover the user-reported AI visibility behavior.

## Agent discussion and suggestions

### 2026-11-20T19:59:33.432Z — arcade-collision-elite-proof

Message: `msg-ad26a4cb4aa1` (evidence)

Subject: Arcade ramp scenario stall assertion proof gap and owner fix

Current-source audit: devscripts/scenarios/user/dust2_ramp_collision_v1.json forbids collision.progress.stall, but before this edit that event was emitted only inside runActorTriangleCollisionStep at actor-triangle-solver.cpp:1599. Arcade dispatch returns before that solver and uses doGLBSweepSlide, so the absence assertion could pass without measuring any Arcade stall. I added bounded collision.progress.stall / .end and collision.speed_collapse emission at the Arcade capsule owner, reusing CollisionState progress and speed-collapse counters; event payload includes input wish, requested/achieved displacement, contact planes, and likely_corner_stop. Build note 073a3ff417e9 is queued, and the root dev-loop reports the new DLL revision building; this needs fresh pinned runtime validation before treating the route assertion as meaningful.

## Agent discussion and suggestions

### 2026-11-20T20:01:33.609Z — arcade-collision-elite-proof

Message: `msg-7ed5e06127f5` (evidence)

Subject: Ramp playback now requires capsule contact evidence

Updated devscripts/scenarios/user/dust2_ramp_collision_v1.json to require at least one collision.capsule-world.solve event with arcade_mode=true and world_shape=root_capsule, plus one collision.arcade_capsule.final event. JSON parsing succeeds and git diff --check passes for the scenario file. This prevents playback completion plus a vacuously absent stall event from passing without observing the Arcade capsule world path. Runtime assertion behavior remains unverified until a pinned playback succeeds.

## Agent discussion and suggestions

### 2026-11-20T20:05:40.526Z — arcade-collision-elite-proof

Message: `msg-ab10fad4d7b4` (shared.request)

Subject: Gen411 snapshot predates Arcade solver edit

Important artifact qualification: generation411 attempt e1b74d started at 19:55:23Z and its changed_paths/attached notes list only player-ai.cpp and structured-log.cpp. My physics-collision-glb-sweep-slide.cpp save is timestamped 19:58:02Z, after that attempt started. A second automatic whole-game DLL build should be required to include the new Arcade stall diagnostics; build note 073a3ff417e9 is still queued for that attempt. Please do not use gen411 as proof of the stall instrumentation; pin the later published revision/generation.

## Agent discussion and suggestions

### 2026-11-20T20:07:54.185Z — arcade-collision-elite-proof

Message: `msg-e18a9a50d28d` (evidence)

Subject: Correction: sourcegen34 compiled capsule file at current revision

Correction to my preceding warning: e1b74d records translation_unit.started for physics-collision-glb-sweep-slide.cpp at 20:04:45Z and dev_loop.file_output_handled status=compiled at 20:06:32Z, revision f75e8d2c; the file was saved 19:58:02Z. So gen411 likely includes the new capsule diagnostics once published. Its initial build context still lists only player-ai.cpp note aec08352d40d and not collision note 073a3ff417e9, so I am flagging a build-note trace gap separately; compile-time/source revision evidence is stronger than my previous assumption that the edit was excluded.

## Agent discussion and suggestions

### 2026-11-20T20:21:29.722Z — arcade-collision-elite-proof

Message: `msg-bbc3a5da135f` (update)

Subject: Current pinned artifacts published

EXE sourcegen38 and DLL generation412 are published. Please proceed with the planned single pinned TDM startup and share run ID and journal; I will use that run for collision proof.

## Agent discussion and suggestions

### 2026-11-20T20:25:22.895Z — arcade-collision-elite-proof

Message: `msg-f9e627f7f089` (evidence)

Subject: Arcade sweep bounce candidate to verify

Source inspection found a plausible Arcade bounce/slide timing defect to test in the next healthy pinned run: at src/physics/movement/physics-collision-glb-sweep-slide.cpp around the TOI response, ungrounded root-capsule impacts call respondVelocityAgainstNormal (which can set outward bounced velocity), but after advancing to TOI the remainingMove is always clipped by subtracting its inward normal component. That preserves tangent slide but removes the reflected normal component from this tick's displacement. The next tick may move outward, or input acceleration may cancel it; current runtime evidence is needed before changing it. Include a wall/corner case that compares TOI velocity_after to actual remaining displacement and next-tick normal displacement.

## Agent discussion and suggestions

### 2026-11-20T20:36:14.199Z — arcade-collision-elite-proof

Message: `msg-ad5a4c975460` (evidence)

Subject: TOI residual bounce implementation queued for runtime proof

Implemented the TOI residual rebound change under the collision file lease and queued build note acd7721a2ecd before saving. The fixed-tick solver now preserves tangent remainder and replaces inward normal remainder with configured outward speed × exact remaining tick time for ungrounded bounce contacts; it resweeps the result and records bounce_displacement_contacts/vector in collision.capsule-world.solve. Root-loop attempt game-dll-20261120T203251717649Z-0ac652 compiled this TU successfully for sourcegen39 / generation413 but is still in object_cache_scan, so not yet published or runtime-proven. Existing sourcegen38/gen412 startup still fails before logger.started; no collision claim yet.

## Agent suggestion

### 2026-11-20T21:55:45.734Z — arcade-collision-elite-proof

Acceptance gap found in devscripts/scenarios/user/dust2_ramp_collision_v1.json: the human-recorded playback currently asserts only that capsule/final events exist and collision.progress.stall is absent. That does not prove configured bounce, no post-sweep root snap, bounded penetration, tangent progress, or absence of unexplained speed collapse. Use the existing collision.capsule-world.solve and collision.arcade_capsule.final fields (bounce_displacement_contacts, velocity_before/after, max_penetration_m, post_sweep_root_correction, emergency_escape) to report those values on the exact route; set numeric tolerances only after a successful current-generation replay establishes normal contact/recovery ranges. Preserve corner/intentional-stop exceptions rather than failing every velocity reduction.

## Agent suggestion

### 2026-11-20T22:43:01.997Z — arcade-collision-elite-proof

Pinned schema-1 input-only derivative of the human Dust2 route reached playback start check but played 0 frames: after server teleport, authoritative position was [-617.938,127.096,2351.122] versus recorded [-614.892,127.148,2351.150], a 3.05 m mismatch; playback failed start_readiness_timeout. Original schema-2 capture has only client_input/client_actors and is correctly rejected as missing_server_actor_track. This is not ramp movement proof; preserve original and report as invalid start-state evidence. Gen416 build request remains blocked by the disk guard.

## Agent suggestion

### 2026-11-20T22:53:18.582Z — arcade-collision-elite-proof

Source audit update: the current Arcade branch bypasses actor-triangle/body/weapon collision and routes through the legacy capsule pipeline. doGLBSweepSlide now calls capsuleTriangleSweep, runs seam-normal blending, calls espondVelocityAgainstNormal for non-floor impacts, and carries rebound through remaining tick movement. config/collision.json has bounce enabled at strength 0.35, friction 0, minSpeed 0.01. However the same path then does bounded post-sweep depenetration (four iterations, each capped at 2m) and a legacy emergency search that can teleport and erase velocity after repeated overlap. smoothSeamDot is 0.9999, which blends only nearly coplanar seams and is a plausible snag source at ramp/crease joins, not yet runtime-proven. Existing gen416 live logs showed max_penetration_m as a pre-recovery peak and post-sweep correction, so those fields do not prove final residual penetration. The exact Dust2 route playback still failed readiness; no valid ramp replay or post-edit DLL exists because the dev-loop disk guard reports 2031MB free vs 2048MB required. Older large logs belong to live processes and remain untouched. Note movement.md's general limb-world collision prose conflicts with the user's explicit Arcade-only capsule-world/capsule-actor-pair requirement; scope that Arcade override in a spec edit before changing shared policy.

## Agent suggestion

### 2026-11-20T22:53:52.983Z — arcade-collision-elite-proof

Runtime/build gate: request a fresh game-DLL attempt only after the volume meets its 2048 MB threshold. Current C: free space is about 1996 MiB. Pruning generation 400 skipped its locked DLL because PID 3940 is a live payload server; the other large segments are also in online runs 162116 and 220344. Do not compress/delete their live logs or stop those unrelated runs. The schema-2 Dust2 input recording cannot run as captured (missing server actor track); a schema-1 derivative was rejected at start_readiness_timeout, so the ramp behavior has not been exercised.
