# Stale remote NPCs survived a client map switch

Time created: 2026-11-06T05:56:43Z
Time last updated: 2026-11-06T05:56:43Z

Status: FIXED IN SOURCE; initial-load journal proof recorded; non-zero map-switch
runtime proof pending

Related specification:
`docs/specs/networking/networking.md` (map agreement, join readiness, and
session teardown), `docs/specs/gamemodes/gamemodes.md`.

Related changelog:
`docs/changelog/2026-11-06/20261106_005643-map-switch-stale-remote-actors.md`

---

## Regression Occurrence 1

### Observed

Run `20261106_050835`, client journal:

```text
C:\mimita-v9\logs\2026-11-06\20261106_050835\client-01\events.jsonl
```

- `t=139.97` client loaded `assets/maps/skyhouse.glb`.
- `t=140.05` `active_map=skyhouse`, `remote_npcs=0`.
- `t=141.05` `active_map=skyhouse`, `required_map=""`, `remote_npcs=9`,
  `sample_npc_position=[-61.0,-2.4,264.5]`.
- `t=141-144` the same 9 remote NPCs continued at badhouse coordinates
  (`~[-60,-5,264]`) while the client world was skyhouse
  (`bounds x=-351..-187, y=-151..-67, z=1295..1342`).
- `t=145.07` `remote_npcs=0` after the old session was finally torn down.

The local-join sequence that preceded it:

```text
t=26.20 network.gui-server.launch map=badhouse
t=26.30 network.client.pending-connect-dispatched address=127.0.0.1:1357 map=badhouse
t=138.97 network.client.pending-connect-dispatched address=127.0.0.1:1357 map=skyhouse
t=140.04 network.client.requested-map-loaded requested_map=skyhouse
t=141.05 client.npc.viewport-window active_map=skyhouse remote_npcs=9
```

### Expected Behavior

When the client changes authoritative map or starts loading a new join map, no
remote actor, projectile, ragdoll, or replicated body from the previous map may
remain visible. The new map must start with an empty remote world and repopulate
only from the new server's snapshots.

### Actual Behavior

The client loaded the new map but retained remote NPCs from the previous map /
server session. The NPCs rendered at old-map coordinates against the new map's
geometry, appearing to walk on invisible or unrelated world geometry. A user
also reported being unable to spawn during this state.

### Why This Is Bad

The visible world and the replicated actors came from different maps. Combat,
navigation, and spawn evidence from that state are invalid, and the user cannot
trust what is walkable.

### Wrong Code

- `PACKET_MAP_CHANGE` updated `ctx.requiredMapId` without clearing remote actors:
  `src/network/multiplayer-tick.cpp` map-change handler.
- The pending-connect map-load path loaded the requested map but did not clear
  the prior session's remote actors:
  `src/engine/engine-tick-state.cpp` `network.client.requested-map-loaded`.
- The server-required map sync loaded a new map but only reset the local player,
  corpses, and runtime objects; remote NPC/player/projectile maps survived:
  `src/engine/engine-tick-net.cpp` `[CLIENT MAP SWITCH]`.

### Fix

Added `mpClearRemoteWorldState()` in `src/network/multiplayer-packets.cpp` and
declared it in `src/network/multiplayer-context.h`. It clears remote players,
NPCs, interpolation state, ragdolls, replicated bodies, corpses, projectiles,
projectile terminals, sword states, pending shot/knockback/damage events, and
remote player-registry rows while preserving the connection, local player, and
chat/session state. It emits `network.client.map-world-cleared` with the reason
and pre-clear counts.

Called from:

- requested join map load (`engine-tick-state.cpp`);
- server-required map switch (`engine-tick-net.cpp`);
- `PACKET_MAP_CHANGE` when the required map changes (`multiplayer-tick.cpp`).

Also added `network.gui-server.launch-rejected` when a local host is attempted
while connected, and `network.gui-server.replaced` when a different local-server
preset replaces a running one, so multiple local servers cannot silently feed
the same client.

### Verification

- Source compile passed for the changed translation units.
- Dev-loop build published artifact `2026-11-06T05:52:47.141763Z`.
- Journal proof of initial map-load clearing (run `20261106_055507`):

```text
network.client.map-world-cleared reason=requested-map-loaded
remote_npcs_before=0 remote_players_before=0
```

The non-zero map-switch case still needs a controlled run where a client has
remote NPCs, then changes map and confirms `remote_npcs_before > 0` followed by
zero remote NPCs before the new map's first snapshot.
