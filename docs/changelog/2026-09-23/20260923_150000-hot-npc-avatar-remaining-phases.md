# Hot NPC avatar remaining phases

Date: 2026-09-23

## Result

Completed the remaining code-side work after the hot NPC avatar lifecycle
bridge.

## Changes

- Added request-time avatar identity to `PendingPlayerModel`.
- Discarded stale asynchronous model completions when an actor receives a new
  avatar before the worker finishes; the current avatar is requested again on
  the next render pass.
- Strengthened the hot avatar self-test to prove deterministic selection for
  the same EntityId and life generation.
- Added `npc_avatar_selected` JSONL evidence containing actor ID, life
  generation, selected avatar, result (`hot`, `forced`, or `fallback`), and
  active hot generation.
- Preserved the existing hot-build candidate validation and rollback pipeline;
  no new cold reload path was added.

## Evidence

- `player-loader.cpp`, `multiplayer-tick.cpp`, `npc-avatar.cpp`, and
  `live-code-selftest.cpp` compiled successfully with the project toolchain.
- `git diff --check` passed; only line-ending warnings for existing working
  files were reported.
- No executable link, live code edit, multiplayer trial, or visual acceptance
  was performed in this session.

## Remaining acceptance

- Start the running game and edit `actor-avatar-policy.cpp` while preserving
  the same process/session.
- Spawn and respawn NPCs and verify the changed policy appears live.
- Introduce a compile error and verify the previous generation remains active;
  fix it and verify activation/rollback evidence.
- Perform two-client visual and repeated-respawn acceptance.
- The larger Entity -> Actor -> Player/NPC state migration remains future work
  and is not required for the avatar feature to function.
