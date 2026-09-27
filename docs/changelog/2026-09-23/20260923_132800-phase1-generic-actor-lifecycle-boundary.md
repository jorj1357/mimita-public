# Phase 1: generic hot actor lifecycle boundary

Date: 2026-09-23

## Result

Implemented the first boundary-only phase for the planned hot actor lifecycle
migration. Existing NPC and player behavior was intentionally not rerouted.

## Changes

- Added `GAME_EVENT_ACTOR_LIFECYCLE` and the stable POD
  `ActorLifecycleStateV1` envelope to `src/hot-reload/game-api.h`.
- Added the EXE-side `LiveBehavior::dispatchActorLifecycle` bridge.
- Added the hot `actor.lifecycle` boundary handler. It only marks the payload
  handled and preserves all incoming state in this phase.
- Added a live-code self-test proving the envelope crosses the hot boundary
  without changing identity, life generation, or health.

## Evidence

- `git diff --check`: passed; only line-ending warnings for existing working
  files were reported.
- The new hot translation unit compiled to
  `build/hotreload/obj/src__hot-reload__modules__actor-lifecycle-boundary.o`.
- The wrapper hot-DLL link/result did not complete in the available build
  window. No full executable build was performed.
- No runtime or visual acceptance was performed.

## Next phase

Route initial NPC creation and NPC respawn through this envelope, then move
avatar selection into the hot handler. Do not remove the existing avatar path
until that new path has deterministic, multiplayer, and live-edit evidence.
