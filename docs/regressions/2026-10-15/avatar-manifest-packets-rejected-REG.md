# Avatar manifest packets silently rejected by server packet gate

Time created: 2026-10-15T20:17:04Z
Time last updated: 2026-10-15T20:17:04Z

Status: ATTEMPTED FIX (1)

Related specification:
`docs/specs/networking/networking.md`
`docs/specs/griever-monster/griever-monster.md` (7.1 avatar-change capability resolution)

Related changelog:
`docs/changelog/2026-10-15/20261015_202000-griever-slices-1-3-live-fix.md`

---

## Regression Occurrence 1

### Observed

Time:
`2026-10-15T20:11:00Z`

While validating Griever slice 2 (data-driven capability resolved on both sides at
avatar change), the client republished its avatar manifest on `avatar.load jason`
(`griever.manifest.republish` appeared in the client journal), but the server
never logged any received manifest and never re-resolved the movement capability.
The server kept the join-time `mode=roll` capability while the client had already
switched to `mode=walk`.

### Expected Behavior

The client publishes its avatar manifest to the server; the server accepts the
manifest chunk packets, and a mid-session avatar change updates the server-side
avatar identity so both sides resolve the same movement capability.

### Actual Behavior

`server-packets.cpp::isKnownPacketType()` only accepted the numeric range
`PACKET_HELLO..PACKET_SERVER_NOTIFICATION` (1..61) plus `PACKET_RAGDOLL_STATE`
(69). `PACKET_AVATAR_MANIFEST` (62), `PACKET_AVATAR_ASSET_REQUEST` (63), and
`PACKET_AVATAR_ASSET_CHUNK` (64) are client -> server packets that fell outside
that range, so the server rejected every one of them at the packet gate before
`handleAvatarManifestPacket()` ran.

### Why This Is Bad

Remote avatar manifest/asset sharing never reached the server, and the server
could not learn a player's current avatar mid-session. Griever slice 2's
"resolve on avatar change on both sides" cannot hold, and switching from a walk
avatar to a roll avatar left the server simulating the wrong movement mode.

### Specification

`docs/specs/networking/networking.md`
`docs/specs/griever-monster/griever-monster.md`

Relevant requirement (Griever spec 7.1): at spawn, respawn, and avatar change,
both client and server resolve the same avatar rig movement capability; the
server must not keep a join-time mode after an avatar change.

### Wrong Code

File:

`src/network/server-packets.cpp`

```cpp
static bool isKnownPacketType(uint8_t type)
{
    // 2026-08-29: TODO use an explicit switch here so future packet types
    // cannot be silently rejected by an outdated numeric range.
    if (type >= PACKET_HELLO && type <= PACKET_SERVER_NOTIFICATION)
        return true;
    // Newer gameplay packets above the legacy numeric range. Add each here.
    return type == PACKET_RAGDOLL_STATE;
}
```

### Confirmed Cause

The numeric-range allow-list predates the avatar network packet types. The
avatar packets (62-64) are above `PACKET_SERVER_NOTIFICATION` (61) and were never
added, so `handleAvatarManifestPacket()` and the avatar asset handlers were dead
code on the receive path.

Evidence:

Fresh journal `C:\mimita-v9\logs\2026-10-15\20261015_200757\events.jsonl` for
build 2397 showed the client `griever.manifest.republish` events but no
server-side manifest arrival and no server `griever.capability.resolved` for the
new avatar.

### Attempted Fix 1

Time:
`2026-10-15T20:12:00Z`

Change:

Added the three avatar packet types to `isKnownPacketType()`.

### Corrected Code

File:

`src/network/server-packets.cpp`

```cpp
    return type == PACKET_AVATAR_MANIFEST ||
           type == PACKET_AVATAR_ASSET_REQUEST ||
           type == PACKET_AVATAR_ASSET_CHUNK ||
           type == PACKET_RAGDOLL_STATE;
```

### Fix

The server packet gate now accepts the avatar manifest/asset packet types, so the
existing `handleAvatarManifestPacket()` path runs. The server additionally parses
a completed manifest's `avatarName` and updates the player's avatar, causing the
already-existing capability re-resolution in `server-players.cpp` to run.

### Proof

Human review:

Not yet performed.

Automated proof:

After publishing build 2399/2401 and repeating the switch scenario, journal
`C:\mimita-v9\logs\2026-10-15\20261015_201147\events.jsonl` shows, in order:
client `griever.manifest.republish jason`, server `griever.manifest.arrived jason`
(chunk_count 12), then server `griever.capability.resolved jason`, and later
server `griever.capability.resolved monstergriever previous=jason`. This proves
the manifest reached the server and the movement capability changed with the
avatar.

### Solution

`ATTEMPTED FIX (1)`

Runtime evidence proves the manifest path now executes and the server resolves
avatar changes. Human review of in-game remote-avatar rendering is still needed
before marking a confirmed solution.
