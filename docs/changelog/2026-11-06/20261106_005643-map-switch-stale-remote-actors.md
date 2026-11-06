# Map-switch stale remote actors and local-host collision — 2026-11-06

## Session record

- Timestamp: 2026-11-06 00:56:43 EST (2026-11-06T05:56:43Z).
- Branch: `20261011zt`.
- Commit: not created.
- Pre-existing worktree: Bloodtest-agent changes in `src/combat`, `src/network`,
  and `src/entities` were preserved. The dev loop was running a Bloodtest
  two-client mode while this fix was built.

## Investigation finding

The user report "world chunks near me aren't existing, NPCs are walking on it,
and I can't spawn" had two separate causes:

1. The live run was not on a map with a generation zone. `world-stream` event
   count was zero in run `20261106_051506`; the active maps were
   `dust2cyberiav4`, `badhouse`, and `skyhouse`, not `funworld3`.
2. Remote NPCs from the previous local server/map survived a client map switch.
   In run `20261106_050835`, the client loaded `skyhouse.glb` at t=139.97, then
   at t=141.05 reported `active_map=skyhouse`, `remote_npcs=9`, with the sample
   NPC at badhouse coordinates `~[-61,-2,264]`. Those stale NPCs persisted until
   the old session was torn down at t=145.07. That matches NPCs walking on
   geometry the client does not have.

Spawn itself was working when the session was stable: journal events show
`client.actor.spawn_packet_applied` for dust2 and badhouse. The failure was
session-transition state, not a universal spawn bug.

## Implemented

- Added `mpClearRemoteWorldState()` in
  `src/network/multiplayer-packets.cpp` and declared it in
  `src/network/multiplayer-context.h`.
- It clears remote players, NPCs, interpolation, ragdolls, replicated bodies,
  corpses, projectiles, projectile terminals, sword states, pending
  shot/knockback/damage events, and remote player-registry rows while preserving
  the connection, local player identity, and chat/session state.
- It emits `network.client.map-world-cleared` with the reason and pre-clear
  counts.
- Called on:
  - requested join map load (`src/engine/engine-tick-state.cpp`);
  - server-required map switch (`src/engine/engine-tick-net.cpp`);
  - `PACKET_MAP_CHANGE` when the required map changes
    (`src/network/multiplayer-tick.cpp`).
- Added `required_map_id`, `server_address` to
  `client.actor.spawn_packet_applied` for spawn diagnostics.
- Added `network.gui-server.launch-rejected` when hosting is attempted while a
  multiplayer session is active.
- Added `network.gui-server.replaced` and stopped the old child server when a
  different local-server preset replaces a running one, so an old server cannot
  keep feeding snapshots into a new map.

## Files changed

- `src/network/multiplayer-context.h`
- `src/network/multiplayer-packets.cpp`
- `src/network/multiplayer-tick.cpp`
- `src/engine/engine-tick-state.cpp`
- `src/engine/engine-tick-net.cpp`
- `src/gui/gui-main.cpp`
- `docs/regressions/networking/stale-remote-npcs-map-switch-REG-2026-11-06.md`
- `docs/regressions/networking/README.md`

## Validation evidence

- Source compile (canonical debug flags, PCH, MinGW PATH): all changed
  translation units compiled with exit code 0.
- Dev-loop build: success, artifact
  `2026-11-06T05:52:47.141763Z`; artifact contains the new
  `network.client.map-world-cleared`, `network.gui-server.replaced`, and
  `network.gui-server.launch-rejected` strings.
- Runtime, run `20261106_055507`:
  - client-01 and client-02 each emitted
    `network.client.map-world-cleared` with
    `reason=requested-map-loaded`, `remote_npcs_before=0`,
    `remote_players_before=0`.
- Not yet proven: a controlled map switch with `remote_npcs_before > 0`. The
  dev loop was running the Bloodtest agent's two-client acceptance mode, so this
  session did not interrupt it with a map-change test.
- Visual/human acceptance: not performed.

## Runtime evidence still required

1. With at least one remote NPC visible, change map and confirm
   `network.client.map-world-cleared` reports `remote_npcs_before > 0`, then
   confirm the viewport reports zero remote NPCs until the new map's snapshot
   arrives.
2. Verify a local host attempt while connected is rejected without changing the
   active map.
3. Verify replacing a local-server preset stops the old child server and no
   packets/NPCs from the old map remain.
4. On `funworld3`, confirm `world-stream.anchor`/`chunk.applied`; on maps without
   `generation_zones`, explicitly expect zero streamed chunks.
