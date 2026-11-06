# Avatar model fallback stayed as a green capsule

Time created: 2026-11-06T07:17:03Z (2026-11-06 02:17:03 EST)  
Time last updated: 2026-11-06T07:17:03Z

Status: FIXED IN SOURCE — runtime model load and movement verified; visual review pending

## Summary

A Bloodtest client showed a green wireframe capsule and could not move. Two
separate causes were confirmed:

1. The avatar model was never requested after an avatar instance was bound.
   `render-player.cpp` set `mLazyLoadRequested = true` but only called
   `AvatarSystem::requestModelLoad` when `avatarInstance` was null. If avatar
   binding produced an instance without a queued model, the player stayed in
   the fallback collision capsule forever with `model_loaded=false` and
   `lazy_load_requested=true`.
2. The procedural meat/bone layer generation added in the Bloodtest actor-body
   slice called `gTextures.getPath` from the async model-load worker. That GL
   texture lookup could prevent the async model from reaching `ready`, so
   `finalizeModelIfReady` never ran.

The “frozen in the air” observation was compounded by the screenshot coming
from a stale client. Room `XNNZ98Q` belonged to run `20261106_045336`, whose
clients had become stale and were replaced by later dev-loop runs. The stale
window kept its last frame.

## Evidence

Before the fix, current-run journals showed:

```text
client.actor.render_state:
  avatar=jasonheadless
  avatar_bound=true
  model_loaded=false
  body_mesh_count=0
  lazy_load_requested=true

avatar.pose.live:
  avatar=jasonheadless
  model_root_active=false
  parts=[]
```

`avatar.load.failed` also appeared for the remote avatar
`remote_dfda63e32319afbf05e5d944` with reason `avatar.json missing or invalid`,
followed by repeated `avatar.cache.pending-or-failed` with `load_state=3`.
`getOrLoadAvatar` did not retry normal parse/atlas failures.

## Fix

- `src/render/render-player.cpp`: always queue the model load when a player has
  an avatar but no model, using the avatar's model path or the canonical default
  body.
- `src/entities/player-loader.cpp`: moved procedural meat/bone layer generation
  out of the async model worker. Layers are now built on the main thread after
  the model is finalized, after a synchronous load, and after a cache hit.
- `src/entities/player-loader.cpp`: log `player.model.load.failed` and
  `player.model.empty-body`, and fall back to the default body once.
- `src/avatar/avatar-atlas.cpp` / `avatar.h`: failed avatar loads now retry with
  bounded backoff (5 attempts, 0.5s to 4s) and expose exact failure reasons.
- `src/avatar/avatar-atlas.cpp`: model paths are validated, with default-body
  fallback.
- `src/avatar/avatar.cpp`: an avatar that produces no body parts triggers one
  default-body reload.
- `src/network/avatar-network-client.cpp`: remote avatar JSON/texture writes are
  atomic (`.tmp` then rename) so readers cannot parse a partial file.

## Runtime evidence

Run `20261106_071418` with the fixed build
`2026-11-06T07:13:46.557145Z:09c24a1226a71fa92f4bdba704ecb11af6a0a88dc77a7e827b112be4fd0ca0e6`:

```text
client.actor.render_state:
  avatar=jasonheadless
  model_loaded=true
  body_mesh_count=6
  fallback_batch_count=6

avatar.pose.live:
  parts=[head,leftArm,leftLeg,rightArm,rightLeg,torso]
```

Movement test on client-02:

```text
before_move: 13.231671,-120.375031,87.813560
agent.move 1 0 90
after_move:  27.943876,-146.253433,87.896713
```

The player moved roughly 30 meters, so the frozen/stale movement symptom is no
longer reproduced in a clean run.

## Remaining review

- Human visual check: the avatar should no longer appear as a green capsule.
- Confirm both clients render the default body and their custom avatar
  consistently after a long session.
- The earlier Bloodtest part-damage runtime acceptance items remain separate.
