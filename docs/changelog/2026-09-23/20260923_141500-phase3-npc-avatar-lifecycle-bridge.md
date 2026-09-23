# Phase 3: NPC avatar lifecycle bridge

Date: 2026-09-23

## Result

Connected initial NPC creation and authoritative NPC respawn to the Phase 2
hot avatar-selection policy.

## Changes

- `npcAvatarNameForLife()` now discovers the validated, sorted avatar list from
  `AvatarSystem`, builds `ActorAvatarPolicyV1`, dispatches it to the hot
  module, and uses the returned avatar name.
- Initial NPC construction already calling `npcAvatarNameForLife()` now uses
  the hot policy.
- Authoritative respawn already calling `assignNpcAvatar()` now uses the hot
  policy after incrementing the life generation.
- Forced-avatar configuration remains an explicit override.
- If the hot module is unavailable or fails, the prior deterministic fallback
  remains active.
- Existing avatar application and asynchronous client loading paths were not
  replaced.

## Evidence

- `src/npc/npc-avatar.cpp` compiled successfully with the project toolchain.
- `git diff --check` passed; only line-ending warnings for existing working
  files were reported.
- No full executable link was performed.
- No live spawn/respawn, multiplayer, or visual acceptance was performed.

## Next phase

Verify client-side new-life application and stale asynchronous-load protection,
then perform same-session hot editing: change the hot avatar policy, spawn or
respawn an NPC, observe the new result, introduce a compile failure, and verify
the previous generation remains active.
