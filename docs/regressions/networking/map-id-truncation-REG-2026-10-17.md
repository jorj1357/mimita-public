# Map id truncation caused stale-world client spawn

Time created: 2026-10-17T00:40:00Z
Time last updated: 2026-10-17T00:40:00Z

Status: ATTEMPTED FIX (1)

Related specification:
`docs/specs/gamemodes/gamemodes.md` (2026-10-16 runtime evidence and acceptance),
`docs/specs/networking/networking.md` (map agreement and join readiness).

Related changelog:
`docs/changelog/2026-10-17/20261017_003202-priority1-2-map-spawn-weapon-identity.md`

---

## Regression Occurrence 1

### Observed

Time:
`2026-10-16T13:40:04Z` (run `20261016_134002`)

### Expected Behavior

A joining client must be told the exact authoritative map id, load the same
collision world the server loaded, and only then report ready; the server must
not admit a player whose collision world differs. Player spawns must be
standable and inside the loaded map.

### Actual Behavior

The server loaded `assets/maps/mimita-aabb-only-interior-small-v4.glb`
(0 authored spawn points), but the client was told the map id
`mimita-aabb-only-interior-small` (no `-v4`), tried to load the nonexistent
`assets/maps/mimita-aabb-only-interior-small.glb` 30 times
(`source_exists=false`), stayed on its previous world, and still sent
`CLIENT_MAP_READY`. The server's truncated advertised id matched its own
`getServerMapId()` comparison, so it spawned the client; the client interpreted
the server's in-map coordinates in a different world and the actor ended out of
world (a local corpse was reported at Z≈−501).

### Why This Is Bad

A below-world spawn makes the session unplayable and invalidates all later
combat/position evidence. It also violates the "same authoritative map before
spawn; no stale-map coordinates" contract.

### Specification

`docs/specs/gamemodes/gamemodes.md`, 2026-10-16 acceptance:

> Before a player is admitted or spawned, the server and client must agree on a
> canonical map ID/content revision... The client must not independently load a
> fallback or stale map while joining the server's room... never apply a
> stale-world coordinate.

### Wrong Code

File:

`src/network/packets.h`

```cpp
constexpr int MAX_NAME_BYTES = 32;   // used for char mapId[MAX_NAME_BYTES] below
// ... WelcomePacket / JoinAcceptPacket / MapChangePacket / DuelStatePacket /
//     ClientMapReadyPacket all store: char mapId[MAX_NAME_BYTES];
```

The server fills it with `std::strncpy(dst.mapId, gServerMapId.c_str(), sizeof(dst.mapId) - 1)`
(`src/network/server-packets.cpp`). With a 32-byte field, the 34-character id
`mimita-aabb-only-interior-small-v4` is truncated to `mimita-aabb-only-interior-small`.

File:

`src/engine/engine-tick-net.cpp`

```cpp
// Send ClientMapReady whenever the required map is ready,
// regardless of whether it was already loaded or just switched.
const bool mapReady = !mpContext.requiredMapId.empty() && worldLoaded;
```

`worldLoaded` stays true for the previous/fallback world after a failed required
map load, so the client reports ready for a map it never loaded.

### Confirmed Cause

Fixed-size packet fields (`char mapId[32]`) silently truncated content-defined
38-byte map ids (confirmed: the client requested
`assets/maps/mimita-aabb-only-interior-small.glb` while the server loaded
`...-v4.glb`), and the client readiness gate did not require the loaded world to
match the required map id. Evidence: run `20261016_134002` events
`map.load-start requested_path=assets/maps/mimita-aabb-only-interior-small.glb
source_exists=false` repeated 30× with `[CLIENT MAP READY] sent ... mapId=mimita-aabb-only-interior-small`.

### Attempted Fix 1

Time:
`2026-10-17T00:20:00Z`

Change:

- Added `MAX_MAP_ID_BYTES = 64` in `src/network/packets.h` and widened every
  `mapId` field to it; updated the affected wire-size `static_assert`s.
- Required the client's loaded world to match the required map before ready
  (`mapIdsReferToSameMap(ACTIVE_MAP_PATH, mpContext.requiredMapId)`), with a
  bounded `network.client-map-not-ready` diagnostic and a visible
  `connectionStatus` on repeated load failure.
- Added bounded `network.map-required` diagnostics at the welcome/join-accept/
  map-change writers.

Result:

Priority-1 scenario `priority1_map_spawn_v1` passed on the reported map
`mimita-aabb-only-interior-small-v4`: the client received the full id
(`network.map-required required_map_id=mimita-aabb-only-interior-small-v4`),
loaded the same file (`map.load-success requested_path=...-v4.glb`), and the
server selected a validated in-bounds spawn
(`spawn.server-selected source=derived-probe validated=true position=[262.875,32.016,120.770]`).
No `network.client-map-not-ready`, no `map.load-failed`, no
`spawn.server-no-safe-spawn`.

### Corrected Code

File:

`src/network/packets.h`

```cpp
constexpr int MAX_MAP_ID_BYTES = 64;
// ... all mapId fields now: char mapId[MAX_MAP_ID_BYTES];
```

File:

`src/engine/engine-tick-net.cpp`

```cpp
const bool mapReady = !mpContext.requiredMapId.empty() && worldLoaded &&
    mapIdsReferToSameMap(ACTIVE_MAP_PATH, mpContext.requiredMapId);
```

### Fix

Map ids have their own packet budget so the versioned id is transmitted intact;
the client will not report ready on a stale world; and the authoritative spawn
policy returns a validated in-bounds position (including a derived ground probe
for maps with no authored spawn points).

### Proof

Human review:

Not yet performed. A human should run the same reported map and confirm the
player spawns in-bounds visually.

Automated proof:

- Scenario `devscripts/scenarios/priority1_map_spawn_v1.json` returned
  `passed=True execution_complete=True assertions=10` on run
  `logs/2026-10-17/20261017_002823/events.jsonl`.
- Build `2447`/`2448` (`python build.py build-only` = BUILD SUCCESS).

### Solution

`ATTEMPTED FIX (1)` — automated scenario passed; awaiting human visual review to
mark confirmed.
