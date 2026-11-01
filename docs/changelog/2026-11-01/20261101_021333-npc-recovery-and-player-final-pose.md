# NPC recovery, post-actor-pair containment, and player final pose

Time created: 2026-11-01T02:13:33Z
Time last updated: 2026-11-01T02:13:33Z

Status: IMPLEMENTED; RUNTIME EVIDENCE PARTIAL

## Goal

Implement the next collision fix from the mace/NPC recording:

1. remove downward-through-solid NPC recovery;
2. re-run final-pose containment after actor-pair and recovery;
3. add player final-pose containment after animation/AimBody and actor-pair.

## Changed

- `actor-final-pose.cpp`
  - final-pose narrowphase now reports and fails closed on
    `gActorNarrowphase.budgetExceeded`;
  - configured weapon spheres/capsules are included in final-pose sampling;
  - the safe-point fallback no longer uses the 200 m downward
    `resolveSafeStandingSpawn`; it uses a bounded lift/ring search;
  - `validateAt` restores the actor when a candidate fails.
- `server-npcs.cpp`
  - `npc.capsule.recovered` no longer calls `groundHeightAt(..., 100 m, ...)`;
    it now uses final-pose containment and respawns only if both last-clear and
    safe-point recovery fail;
  - the NPC ground clamp only accepts a floor within 4 m, not through a lower
    floor;
  - NPC final-pose containment is re-run after actor-pair collision.
- `simulate-tick.cpp`
  - local player final pose runs after animation/AimBody and again after
    actor-pair;
  - local NPC final pose runs again after actor-pair.
- `player.h` / `player.cpp`
  - added player `ActorFinalPoseState` and reset it on `Player::reset`.
- `scenario-player.cpp`
  - playback uses the recording’s `networked` field instead of always using
    `MP_CONTEXT.active`.

## Validation

- Build `30` succeeded.
- `--actor-final-pose-selftest`: PASS.
- `--actor-triangle-solve-selftest`: PASS for the step-up checks; two
  pre-existing failures remain, plus the flush-seam checks still fail in this
  build and need separate investigation.
- Live NPC soak on build 30: spawned 20 NPCs, ran 20 seconds.
  - `npc.capsule.recovered`: 0
  - `npc.pose.penetration`: 0
  - `npc.pose.reset`: 0
  - no NPC below-map far-fall was observed in the recovery events; the lowest
    stuck-recovery position was z `2303.12`, which is still under investigation
    as a possible lower map level.
  - `npc.stuck-recovery`: 321, so navigation stuck behavior remains.
- Exact recording playback:
  - original recording still fails `start_readiness_timeout` because the server
    spawn/position does not match the recording;
  - client-only copy now starts, but diverges at frame 91 because the client is
    still in an active multiplayer context and server snapshots override the
    playback. Exact before/after route comparison is still blocked by tooling,
    not by the collision change.

## Remaining

- Re-run the full mace recording against a true offline or server-matched
  playback.
- Investigate the flush-seam selftest failures.
- Investigate the remaining NPC navigation stuck episodes.
- Human visual review of mace/ground and NPC/world clipping.

## Follow-up validation — 2026-11-01T02:13:33Z

- The flush-seam selftest failures were caused by requiring a support contact
  before suppressing a low vertical face. The suppression now uses the actor's
  feet height when no support contact is present. Build 31 passes both
  flush-seam checks.
- Build 31 live run: 20 spawned NPCs produced zero `npc.capsule.recovered`,
  zero `npc.pose.penetration`, and zero `npc.pose.reset` events.
- Player final-pose filtering is active: `player.pose.contained` events were
  emitted with `post_animation` stage and no player resets.
- Exact recording playback remains blocked: the original recording still fails
  server start readiness, and the client-only copy starts but diverges at frame
  91 because the client is still in an active multiplayer session.

## Build 31 live recheck — 2026-11-01T02:20Z

- Launched build 31 on Dust 2, spawned 20 NPCs, equipped the mace, ran a
  mace swing, and waited 12 seconds.
- `npc.capsule.recovered`: 0
- `npc.pose.penetration`: 0
- `npc.pose.reset`: 0
- lowest `npc.stuck-recovery` position: z `2339.12`
- `player.pose.contained`: 21 (stage `post_animation`)
- `player.pose.reset`: 0
- `collision.speed_collapse`: 33; these still need classification, but no
  player final-pose reset occurred.
