# authoritative server lifecycle JSONL diagnostics

Date: 2026-09-23
Status: implemented; build verification pending

## Changes

- Added authoritative `NETWORK` JSONL events for player join and rejoin with
  player/entity ID, name, server tick, spawn generation, death state,
  respawn policy, stale-connection state, and host status.
- Added authoritative initial-spawn and respawn events with server tick,
  spawn generation, transform epoch, health, and respawn policy.
- Added authoritative player-death events with victim identity, killer player
  or NPC identity, source/weapon, damage and health transition, server tick,
  spawn generation, death count, respawn policy, and stale-connection state.
- Passed the server tick into the shared authoritative spawn owner so lifecycle
  records identify the exact simulation tick.

## Evidence

- Source changes: `src/network/server-packets.cpp`,
  `src/network/server-players.cpp`, `src/network/server-damage.cpp`,
  `src/network/server.cpp`, and `src/network/server.h`.
- Existing unrelated working-tree changes were preserved.
- Build was started with `python build_agent.py`; the previous build result was
  already failed before this diagnostic patch, and the current verification
  process had not produced a final result when this record was written.

## Runtime acceptance still required

Run the game/server through join, death, respawn, disconnect, and reconnect,
then verify the server process's own `events.jsonl` contains the lifecycle
events and continues after the client dies. A clean server shutdown/crash
record is still needed to identify the original process-exit cause.
