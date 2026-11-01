# Actor-world final-pose containment and flush seam response

Time created: 2026-10-31T18:06:36Z
Time last updated: 2026-10-31T18:06:36Z

Status: IMPLEMENTED; RUNTIME EVIDENCE PARTIAL; HUMAN VISUAL REVIEW PENDING

## Goal

Implement the specification decisions for:

- NPC/monster final animated body and held-weapon containment against static
  world geometry;
- no visible NPC/monster limb or weapon overlap;
- no flush block-seam wall that collapses forward movement;
- useful AI evidence for speed collapse and direction deviation;
- actor-vs-world collision through actor triangles, with actor-vs-actor left as
  the separate capsule contract.

## Changed

- Added `src/physics/movement/actor-final-pose.{h,cpp}`.
  - Samples all final body and held-weapon collision meshes against nearby
    static world triangles.
  - Translates only the actor root.
  - Uses a 1-2 cm standoff.
  - Keeps a last-clear position/yaw/ground state and a bounded
    `resolveSafeStandingSpawn` fallback.
  - Zeroes velocity on reset.
  - Does not rewrite limb transforms.
- Called final-pose containment from `src/npc/npc.cpp` after `updateNpcAim()`,
  so it consumes AimBody/arm-ragdoll output rather than the pre-animation pose.
- Added bounded events:
  - `npc.pose.penetration`
  - `npc.pose.contained`
  - `npc.pose.last_clear`
  - `npc.pose.reset`
- Reset final-pose state in `finalizeServerNpcSpawn()`.
- Added flush-seam suppression in `actor-triangle-solver.cpp`.
  - A near-feet vertical contact that blocks movement and belongs to a triangle
    no taller than one step above its walkable support is treated as a seam, not
    a wall.
  - Tall walls keep their full height and remain blocking.
- Added `collision.speed_collapse` diagnostics.
  - Reports horizontal speed collapse, held input, grounded/freeze state,
    before/after velocity, contacts, normals, triangle ids, and penetration.
  - Landing-only vertical speed loss no longer produces false collapse events.
- Extended `devscripts/scenario-analyze.py` with speed-collapse and direction
  deviation reporting and `--fail-on-speed-collapse`.
- Added scenarios:
  - `devscripts/scenarios/user/dust2_seam_playback_v1.json`
  - `devscripts/scenarios/npc_final_pose_soak_v1.json`
- Added `--actor-final-pose-selftest`.
- Minimal build unblock: replaced an undeclared `Audio::playSound` call with the
  existing shared `playSound` owner and included `audio/audio.h`.

## Validation

- Build: successful artifact `build=4003`; source changes after that build were
  followed by unrelated concurrent source edits, so the newest source build is
  not claimed as complete.
- Final-pose selftest: PASS.
- Actor-triangle selftest: flush-seam checks PASS. Two pre-existing failures
  remain:
  - `actor is not glued to an old wall contact`
  - `weapon contact does not launch the player`
- Runtime seam evidence: run `20261031_175544`, build 4003,
  `agent.move 1 0 420` crossed from x `-821` to x `-681` with zero
  `collision.speed_collapse` and zero `collision.progress.stall` events.
- Runtime NPC evidence: four NPCs were spawned and simulated with no
  `npc.pose.penetration` and no `npc.pose.reset` events.
- Pre-fix scenario analysis: `scenario-20261031_143812-collisss-merged.jsonl`
  reports frame 377 horizontal speed `20.00 -> 0.40`.
- Playback limitation: the user recording had no client footer and no server
  actor track. A merged copy was produced for analysis; scenario playback still
  rejected it with `spawn_not_ready`, so playback is not claimed as acceptance.

## Still open

- Human visual acceptance of NPC/monster limbs and weapons against blocks.
- Server respawn hook when both last-clear and safe-point containment fail.
- Player final-pose containment is intentionally not changed in this pass.
- Actor-vs-actor remains one root capsule per preset. A multi-capsule big-monster
  profile is a later migration; actor-vs-world uses the monster's actual
  triangles.
- The two pre-existing actor-triangle selftest failures.
