# Phase 2: hot actor avatar policy

Date: 2026-09-23

## Result

Implemented the hot avatar-selection policy contract without changing the
existing NPC spawn or respawn callers. Phase 3 will connect those callers.

## Changes

- Added `GAME_EVENT_ACTOR_AVATAR_POLICY` and the fixed-size POD
  `ActorAvatarPolicyV1` candidate/result envelope to `src/hot-reload/game-api.h`.
- Added `LiveBehavior::dispatchActorAvatarPolicy`.
- Added the hot `actor.avatar-policy` handler. It selects a deterministic,
  random-looking candidate from the kernel-provided list using EntityId and
  life generation, then returns the selected name.
- Added a live-code self-test for handled state, valid index, and selected name.

## Evidence

- The new hot source compiled successfully with the project toolchain to
  `build/hotreload/obj/src__hot-reload__modules__actor-avatar-policy.o`.
- `git diff --check` passed; only line-ending warnings for existing working
  files were reported.
- No NPC spawn/respawn path was changed in this phase.
- No runtime, multiplayer, or visual hot-edit acceptance was performed.

## Next phase

Change initial NPC creation and authoritative NPC respawn to build an avatar
policy request from `AvatarSystem::listAvatars()`, dispatch it to the hot
module, store the returned name, and replicate that authoritative name to
clients. Preserve the existing asynchronous avatar asset loading path.
