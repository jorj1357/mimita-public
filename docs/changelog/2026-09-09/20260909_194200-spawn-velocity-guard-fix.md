// 2026-09-09T19:42:00Z
/* purpose
* fix spawn velocity being zeroed by the server-side lifecycle guard
* the guard at server-players.cpp:655-658 zeroed player.vel every tick while
  spawnState == AwaitingSpawnAck, destroying the spawn impulse before the
  client could apply it
* the fix preserves velocity during AwaitingSpawnAck so snapshots carry the
  correct spawn impulse to the client
*/

# Spawn velocity guard fix

## Session

- UTC timestamp: `2026-09-09T19:42:00Z`
- Local display time: `2026-09-09 15:42:00 America/New_York`
- Branch: `8292026stash`
- Commits: no commit created
- Pre-existing changes: preserved all existing spawn velocity and lifecycle work

## Request and finding

The spawn velocity feature was intermittent — working roughly once every 15
respawns. The feature record at `docs/features/spawnvelocity/spawnveloc.md`
documents the human observation: "sometimes it wont do the velocity when i
spawn, so ordering might still be wrong?"

The root cause was the lifecycle guard at `src/network/server-players.cpp:655-658`.
After `completeAuthoritativeSpawn()` set `spawnState = AwaitingSpawnAck` and
delivered the correct velocity in `player.vel`, the next server tick's
`simulatePlayer()` reached the guard which checked `spawnState != Active` (true)
and zeroed `player.vel`. The zeroed velocity propagated into snapshots via
`makePlayerEntity()` (fallback path at line 1196) and overwrote the client's
`localServerVelocity` at `multiplayer-tick.cpp:207`.

The velocity survived only when client physics moved the player before the
zeroed snapshot arrived — a timing-dependent race that produced the
intermittent behavior.

## Exact changes

### `src/network/server-players.cpp`

- Old (lines 655-664): the guard unconditionally set `p.vel = glm::vec3(0.0f)`
  whenever `spawnState != Active` or the transform-ack was pending.
- New: the guard now checks `spawnState != AwaitingSpawnAck` before zeroing
  velocity. When the player is awaiting a spawn ack, the velocity is the
  authoritative spawn impulse and must survive so snapshots carry it to the
  client. The rest of the guard (clearing externalImpulse, clientStateUpdated,
  early return) still applies to freeze movement simulation until the ack
  arrives.

## Why this is safe

- When `spawnState == AwaitingSpawnAck`: velocity is preserved (the fix).
  Movement simulation is still frozen (early return at line 669). Snapshots
  carry the correct velocity via the `makePlayerEntity` fallback path
  (line 1196: `out.vx = player.vel.x`).
- When `spawnState != Active` for other reasons (e.g. stale transform on an
  already-active player): velocity is still zeroed (existing behavior).
- When the client's SpawnAck arrives (`server-packets.cpp:2690-2692`):
  `spawnState = Active`, the guard no longer fires, normal simulation resumes
  with the correct velocity.
- The transform-ack timeout (`server-players.cpp:643-654`) clears
  `awaitingAuthoritativeTransformAck` but does not change `spawnState`, so
  velocity still survives until the ack or timeout resolves.
- NPC spawns are unaffected — they have no `spawnState` guard.

## Validation

1. `python build_agent.py` reported `Status: SUCCESS` at `2026-09-09 15:42:31`.
2. `server-players.cpp` compiled and linked into `mimita.exe`.
3. No other files were changed.

## Remaining human review

Run a real client/server respawn trial with `config/spawnvelocity.json`
enabled (`enabled: true`, `mode: "look"`, `speed: 40.0`). Kill the player
repeatedly (use the `explode` terminal command or take damage). Verify:
- Server log shows `[DuelPacketSend] ... velocity=(non-zero, ...)` on every
  respawn.
- Client log shows `[DuelPacketRecv] ... velocity=(non-zero, ...)` on every
  respawn.
- The player is launched in the look direction on every respawn.
- The velocity works consistently, not just sometimes.
