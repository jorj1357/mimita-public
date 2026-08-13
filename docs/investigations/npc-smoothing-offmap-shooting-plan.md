// 08 11 2026
/* purpose
* Investigation + implementation plan for NPC smoothing / networking / collision
* breakage on chainofjudgement (NPC falls to -500, renders off-map, damages the
* player "from nowhere" with no visible beam).
* Documents the confirmed root cause, the implemented fix (Changes 1-4), and how
* to reproduce and verify.
* Does NOT cover unrelated networking features or gameplay balance.
*/

# NPC Off-Map / Invisible-Damage Fix — Investigation & Implementation

## Problem summary

On `chainofjudgement`, server NPCs:
- visually bounce between ~0 and -500 units (fast up/down),
- render below the map (invisible),
- shoot and damage the player from an invisible/off-map state,
- produce no visible beam/tracer when they fire.

## Root cause (confirmed with code + a pipeline simulation)

1. The **server NPC collision world** (`npcWorld`, built by `buildNpcWorldCollision`
   in `src/network/server-npcs.cpp`) built only `collisionChunks` +
   `collisionLargeTriangles`. It never built the **coarse large-triangle grid**
   (`collisionLargeChunks` / `collisionAlwaysLargeTriangles`) that the client
   builds in `buildCollisionChunks` (`src/map/map-loader-collision.cpp`).

2. `appendChunkTrianglesForAABB` (`src/physics/movement/physics-collision.cpp`)
   **silently returns nothing for large triangles when that coarse grid is
   empty**. Every NPC query on the server (physics gather, `groundHeightAt`
   ground clamp, LOS, `targetCanSeeNpc`, combat LOS) goes through it, so any
   triangle spanning > 256 chunks is invisible to server NPCs.

3. chainofjudgement is made of giant primitives (150 m arena-floor disc at
   z ~ 73, 150 x 1000 m walls, tall cone obelisks). Simulation of the server's
   exact decimation (cell 0.5) + chunking (6 m) shows **1,037 of 26,402
   triangles exceed the cap and are dropped**. At 3 of 4 spawn points the chunk
   query finds no floor while the dropped "large" list contains it at z = 74.

4. Consequences: the NPC falls through the floor, the ground clamp never fires
   (`groundHeightAt` finds nothing), the NPC reaches `killZ = -500`
   (`config/gameplay/void-death.json`), `checkVoidDeath` kills it and
   `respawnServerNpc` respawns it at its spawn point -> clients see an
   oscillation between spawn and -500.

5. While below the map, LOS is unblocked (floor/walls invisible to the NPC LOS
   DDA), so the NPC fires through the floor/walls and hits the mirror player.
   The remote beam/tracer also points wrong because NPC `aimX/Y/Z` were never
   replicated (`makeNpcEntity`) and the client's yaw-based muzzle fallback treats
   degree yaw as radians.

6. Offline NPCs and client players were unaffected because the client world
   builds the coarse grid.

## Implemented changes

### Change 1 — Server NPC world uses the shared client chunk builder
File: `src/network/server-npcs.cpp` (`buildNpcWorldCollision`)

Replaced the manual chunk loop + `buildCollisionSubGrids` with a single call to
the public `buildCollisionChunks(npcWorld, nullptr)` — the same function the
client uses in `loadWorldFromGLB`. The server NPC world now builds `collisionChunks`,
`collisionLargeChunks`, `collisionAlwaysLargeTriangles`, and `collisionSubGrids`,
guaranteeing server/client parity and making all large triangles queryable.

Runtime confirmation:
`[SERVER NPC WORLD] built CPU collision: triangles=26402 chunks=37209 largeTris=1037 largeChunks=5715 alwaysLarge=26`

### Change 2 — NPC LOS tests the large-triangle grid
File: `src/npc/npc.cpp` (`updateOneNpc`)

Replaced the inlined chunk-only DDA with the shared `rayTraverseGridCells`
(the same trace weapon fire uses), preserving the old (0.1, losDist - 0.5)
blocking window. Big walls/floors now block NPC LOS, so NPCs stop firing through
floors/walls.

### Change 3 — Removed the hardcoded NPC void-death respawn position
File: `src/network/server-packet-chat.cpp` (`checkVoidDeath`)

Stopped overwriting `npc.pos` with `(1+(id-1)*1.5, 5, 30)`. The NPC is marked
dead and `respawnServerNpc` places it at its real map spawn point.

### Change 4 — Replicate NPC aim so remote beams point correctly
Files: `src/network/server.h` (new `ServerNpc::aim`), `src/network/server-npcs.cpp`
(`rebuildServerNpcMap` fills it from `n.currentFacing`, `makeNpcEntity` copies it
into `aimX/Y/Z`).

Snapshot `aimX/Y/Z` were already consumed by the client interpolation and by
`mpRemoteShooterMuzzle`; NPCs simply never sent them, so the client fell back to
a degree-as-radian yaw. Now the remote muzzle/tracer starts where the NPC's gun
faces. Player aim/yaw are untouched (yaw stays degrees; only NPC aim is added).

### Change 6 — Post-blackout position self-correction (duels / remote players)
Files: `src/network/movement-validation.h`, `movement-validation.cpp`,
`multiplayer-context.h`, `multiplayer-tick.cpp`, `multiplayer-reconcile.cpp`

Symptom: during a badconn blackout in a duels game, a client's locally predicted
position drifts far from the server's authoritative simulation (e.g. 1,1,1 ->
20,1,1). Neither the client nor the server corrected it — the client only
hard-snaps on Major divergence (>= 100 units, so a ~19-unit drift never fired)
and the server's `TooFarFromAuthoritative` reject was commented out, so it
blindly accepted the drifted report. The wrong spot stuck for both clients until
a death forced an epoch snap.

Fix (both sides so they re-converge regardless of packet ordering):
- Server (`validateClientMovementReport`): if a client skipped
  `postGapCorrectionMinTicks` (30) simulation ticks AND reports a position
  `postGapCorrectionDistance` (8 units) away from the server's simulation,
  correct it back to the server position instead of accepting the drift.
- Client: a snapshot tick gap > 30 ticks (~500ms) arms `postGapResync` for 1s;
  `mpReconcileLocalPlayer` then snaps the local player to the server position
  even for a Medium divergence (> 1.5 units), so the client re-converges right
  after the blackout instead of waiting for death.

## Files changed (all changes 1-6)
File: `src/network/server-npcs.cpp` (`simulateSharedNpcs`)

`if (!nearest) continue;` skipped the ENTIRE NPC simulation whenever no live
player existed (no client joined yet, or the host was dead/stale). NPCs then
froze at their spawn height forever, floating in the air without touching the
floor. The loop now always advances `updateOneWithTarget`; when no live player
is found it feeds the NPC a neutral dead mirror, so gravity, landing, and
collision still run (NPC falls from spawn and stands) while targeting/combat/
damage stay off. The damage block is guarded by `nearest != nullptr`.

## Reproduction

- Map: `chainofjudgement`
- Dedicated server: `mimita.exe --server --map chainofjudgement --npcs 2 --timeout 30`
- Join a client; watch the NPC spawn/fall; check `badconn 0` then `badconn 1`.

Before the fix: NPC falls through the floor, `[SERVER VOID DEATH] npcId=... z=-...`
appears, NPC bounces between ~0 and -500, damages you while invisible.
After the fix: NPC stands on map geometry, no `[SERVER VOID DEATH]`, stable
position/HP, no damage from below the map.

## Verification

- Build: `python build_agent.py` -> `Status: SUCCESS`
- Runtime: server log shows `largeChunks=5715 alwaysLarge=26`; no `VOID DEATH`;
  `[SERVER NPC]` positions stay on the map.
- Client: `[CLIENT NPC ALIGN]` shows `auth` ~= `render`; beams/tracers visible
  when the NPC fires; no damage from invisible/off-map NPCs.
- Regression: offline NPCs and other maps (`mimita-funworld-v4`, `coolplace`).
- Final gate: `python overseer.py` -> `Overall Status: PASS`.

## Files changed

- `src/network/server-npcs.cpp` — Changes 1, 4, and 5
- `src/npc/npc.cpp` — Change 2
- `src/network/server-packet-chat.cpp` — Change 3
- `src/network/server.h` — Change 4 (new `ServerNpc::aim` field)
- `src/network/movement-validation.h` / `movement-validation.cpp` — Change 6 (server)
- `src/network/multiplayer-context.h` — Change 6 (client fields)
- `src/network/multiplayer-tick.cpp` — Change 6 (gap detection)
- `src/network/multiplayer-reconcile.cpp` — Change 6 (post-gap resync)

## Known residual / follow-ups (not in this pass)

- NPC model yaw stays degrees (consistent with remote players/render-player);
  only the client muzzle fallback math for entities without aim remains
  degree-as-radian, but NPCs now always send aim so it is never reached for them.
- The empty `[SERVER NPC FIRE-STATE]` line prints every second even with zero
  NPCs; pre-existing noise, not introduced here.
