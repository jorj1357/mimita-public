# NPC avatar names truncated on the wire

Date: 2026-11-15
Time last updated: 2026-11-15T02:20:00Z
Status: OPEN — wire capacity and hot prefix resolver implemented; runtime acceptance pending
Owner: snapshot schema / avatar resolution

## Symptom

NPCs did not receive avatars. Their snapshots carried an avatar name, but the
client could not resolve the corresponding avatar folder.

## Evidence

- `MAX_AVATAR_NAME_BYTES` was 24.
- Remote avatar ids are 31 characters, for example
  `remote_81bd6aa8fbb5fda8aa34686a`.
- The server copied 23 characters plus null at
  `src/network/server-npcs.cpp:2994-2995`.
- The client requested `assets/avatars/remote_81bd6aa8fbb5fda8`, which does not
  exist; the actual folder has the full 31-character name.
- `server.npc-avatar.snapshot-summary` reported
  `avatar_field_capacity_bytes: 24` and `npc_empty_avatar_count: 0`, proving the
  name was present but truncated.

## Corrected behavior

Hot-client path:

- `AvatarSystem::resolveAvatarName` resolves a truncated `remote_<prefix>` to a
  unique full local avatar folder.
- `npc.avatar.resolve` records successful resolution or ambiguity.
- Async avatar application now makes cosmetic image paths absolute from the
  resolved avatar's base path.

Wire path:

- `MAX_AVATAR_NAME_BYTES` is 32.
- `EntitySnapshotV1.avatarName` is 32 bytes and `kGameHostAbiVersion` is 2.
- `CompactEntityData` size is 187 and `SnapshotChunkPacket` size is 1154, still
  under the 1200-byte datagram budget.

## Acceptance

- NPC avatars load from full remote avatar ids.
- A truncated transitional name resolves to the unique local folder.
- No NPC avatar is silently blank.
- Visible hot-client logs show `npc.avatar.resolve` and loaded NPC models.
