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
