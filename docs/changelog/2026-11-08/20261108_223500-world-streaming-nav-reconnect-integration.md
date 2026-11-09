# World streaming, map-sync, reconnect, and navigation integration

## Status

Code integration only. Building, compiling, running, and waiting are
intentionally deferred by user directive. No compile result, runtime scenario,
or human acceptance is claimed.

The obvious next-step checklist, including the lane-to-one-EXE merge procedure,
is in the main checkout at
`C:\mimita-v9\docs\operations\build-and-exe\agent-lane-integration-and-pending-validation.md`.

## Implemented source changes

- `world_streaming` is now a typed gamemode setting with `enabled=false` by
  default. Every gamemode is canonicalized with the field disabled except
  `connected_world`.
- Runtime validation rejects `world_streaming.enabled=true` on any gamemode
  other than `connected_world`.
- `connected_world.maps` is empty so the mode is selectable on any map.
- Client `InfiniteWorldRuntime` is gated by the active gamemode. A map zone or
  `generate_whole_world` flag can no longer opt a non-connected gamemode into
  streaming.
- `InfiniteWorldRuntime` synthesizes a connected-world zone when the mode is
  `connected_world` and the map has chunks but no authored zone.
- Server streamed collision has an explicit enable flag. Disabling it strips
  the streamed collision tail once and logs it.
- The initial authored streamed footprint is gated by the active gamemode.
- The client base collision broadphase is no longer rebuilt from every chunk
  install. It rebuilds only when the base triangle count or map render revision
  changes.
- The server streamed collision tail now uses `World::streamedCollisionTree`
  instead of rebuilding the whole map collision grid.
- Recast navigation now ignores the streamed collision tail and agent profile
  variants when deciding to rebuild. Static base geometry owns the navmesh.
- Recast publishes a `npc.navmesh.profile-kept` diagnostic when a profile
  variant reuses the published mesh.
- Client map-ready is retried until an authoritative spawn is received.
- Duplicate welcome/join-accept packets no longer clear map-ready for the same
  player/map/epoch.
- `network.map-sync.state` reports the exact map-sync stage once per second.
- `ConnectionState::WaitingForServer` holds the session open while snapshots
  are stalled but the hard timeout has not expired.
- `network-policy` now owns `SessionHealthPolicyV1`, `MapSyncPolicyV1`, and
  `CombatValidationPolicyV1` events. Cooldown grace and hit-claim origin
  tolerance are policy-driven.

## Compile verification

Not run. The user explicitly requested that the agent skip building, compiling,
running, and waiting. The source changes are present; compile and runtime
verification are deferred.

## Static validation run

Run without building, launching the game, or waiting for a dev-loop build.

- PASS `git diff --check`.
- PASS `gamemode_json_sync.py --check` (36 definitions, 0 errors).
- PASS `weapon_json_sync.py --check`.
- PASS JSON parse of all 24 modified config files.
- PASS `world_streaming.enabled` is true only for `connected_world` across all
  22 gamemodes.
- PASS `modules.json` has 8 modules and 43 unique events; all source/header
  paths exist; `navigation-policy` owns event `800`.
- PASS `module-abi.h` defines every manifest event exactly once (43/43).
- PASS `python -m unittest devscripts.dev_loop.test_artifacts -v` (4 tests).
- PASS spec/related-document links resolve.
- FAIL `config_selftest.py` on existing avatar schema issues (no avatar files
  were changed by this lane).
- FAIL `animation_json_check.py` with 4 pre-existing invalid keyframe ticks
  (no animation files were changed by this lane).
- RED `quality/lint.py --gate`: damage authority `88 > 81`, contact branches
  `43 > 41`, repeated windows `556 > 551`. The lane base `dc7dea21` already
  reports `88`, `43`, and `552`; the cumulative +4 repeated windows come from
  line-shifted windows in `multiplayer-packets.cpp`, not a new owner.
- FAIL `check_scenario_roundtrip.py`: no recorded scenario gate JSONL exists;
  this needs the runtime matrix.
- `agent-gate.py --no-build --skip-scenarios` cannot complete because
  `latest_exe()` returns `None` when no build identity exists.

## Runtime tests required

### Payload / FunWorld3

- No `world-stream.anchor`, `world-stream.update`,
  `world-stream.chunk.applied`, or `world-stream.collision.sync`.
- No `world-stream.collision.sync` base-tree rebuild.
- Server stays at 60 Hz without `performance.server-tick-stall`.
- Client has no reconnect loop and no `network.snapshot.timeout`.
- Client reaches 240 FPS target with no multi-second frame.
- No “Loading: synchronizing map” stall.

### Connected World

- Streaming starts only in `connected_world`.
- Chunk generation and installation remain smooth while moving continuously.
- No full base broadphase rebuild per chunk.
- Server stays at 60 Hz; client targets 240 FPS.
- Player collision and NPC collision work on generated chunks.
- Recast does not rebuild on profile variants or streamed collision changes.
- `npc.navmesh.rebuild` diagnostics identify any remaining rebuild reason.
- Verify cross-tile Recast paths: route NPCs across tile boundaries, not only
  within one 32 m tile.
- Verify dirty-tile rebuild progress does not starve a route for more than the
  configured budget.

### Map synchronization

- `network.map-sync.state` reaches `ready`.
- Map-ready retries are visible when spawn is delayed.
- A duplicate welcome/join-accept does not reset a valid map-ready.
- A lost map-ready packet recovers without a manual reconnect.

### Session health

- During a snapshot gap, state becomes `WaitingForServer`, not `Reconnecting`.
- Reconnect begins only at the configured hard timeout.
- A fresh authoritative snapshot returns directly to `Connected`.
- No `Connected → Reconnecting → Connected` loop while the server is healthy.

### Combat authority

- Client hit claims match server accepted/rejected verdicts.
- No unexplained damage mismatch.
- No unexplained cooldown rejection.
- No unexplained weapon-not-equipped rejection.
- No unexplained position correction or hard snap.

### Server tick budget

- Profile projectile, contact, and NPC work after the navmesh changes.
- Confirm 60 Hz over a five-minute run, not just a build result.
- Confirm 240 FPS over a five-minute run, not just a build result.

## Known follow-up code work

- Recast now builds a multi-tile Detour navmesh and rebuilds dirty tiles one
  at a time through `navigation-policy`. Dirty-tile rebuilds are bounded per
  query and keep the previously published tiles available.
- Client chunk collision composition pauses new chunk installation for 250 ms
  when a compose exceeds its 8 ms frame budget.
- Server projectile/contact tick budgets remain pending.
- Equip-grace and damage-mismatch combat diagnostics remain pending.




