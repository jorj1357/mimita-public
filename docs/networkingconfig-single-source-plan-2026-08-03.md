// 08 03 2026, 20 05
/* purpose
* Records the networkingconfig.json single-source investigation and implementation plan.
* Names the exact code paths that own remote-player snapshots, interpolation, events, and badconn presets.
* Defines the scoped patch and validation needed for coherent bad-connection rendering.
* Does NOT replace runtime documentation or hide unimplemented follow-up work.
* Does NOT describe production deployment or VPS operations.
* Does NOT change protocol behavior by itself.
*/

# networkingconfig.json Single Source Plan

## Investigation

The single config authority is `config/networkingconfig.json`. It already owns `remote_player_interpolation`, `local_player_reconciliation`, `snapshot_buffer`, `remote_entity_lifecycle`, `network_timeouts`, `debug`, and `badconn.presets`. There is no separate `badconnconfig.json`; `src/network/badconn/badconn-config.cpp` loads the `badconn` block from this same file through `badconn::configPath()`.

The typed loader is `src/config/networking-config.cpp` / `.h`. It parses only the older subset, then calls `badconn::loadConfig(badconn::configPath())`, so any new networking policy must be added there or it remains a hidden hardcoded rule.

The remote-player visual authority path is:

1. Client sends `InputPacket` in `src/network/multiplayer-tick.cpp`.
2. Server receives and accepts movement in `src/network/server-packets.cpp`.
3. Server stores broadcast samples in `src/network/server-players.cpp`.
4. Server emits `SnapshotEntity` from `makePlayerEntity()`.
5. Client inserts snapshots through `pushInterpolationTarget()` in `src/network/multiplayer-interpolation.cpp`.
6. Client renders body/aim/weapon in `updateRenderedReplica()`.
7. Shot and pellet event packets are processed in `src/network/multiplayer-shots.cpp`.
8. One-frame effects are consumed by `src/engine/engine-tick-net.cpp`.

## Root Cause

Remote body position, yaw, aim, and weapon state can be rendered from a delayed interpolation snapshot, but the one-shot movement presentation serials are still read from `interpolation.target`, the newest snapshot. That makes dash, jump, freeze, and direction-change effects fire ahead of the body when `direct_render=false` or when an adaptive buffer is active.

Shot and pellet effects have a related mismatch: event packets are presented immediately on receipt. The existing code rebases muzzle/tracer positions onto the rendered body with `mpRemoteShooterRenderDelta()`, but it does not wait for the shooter's rendered body to reach the event's visual tick. Under badconn preset 1, shots can visibly originate from a body state that has not arrived on the render timeline yet.

The current interpolation delay is fixed. `minimum_snapshots_before_rendering` is parsed but not enforced in `updateRenderedReplica()`, so thin buffers can still render too early. There is no per-remote jitter estimate, no adaptive delay, and no summary diagnostic for resolved config values.

## Hardcoded Knobs Found

- Client connect timeout: `6000` ms in `src/network/multiplayer-tick.cpp`.
- Attack retry: `100` ms, `10` attempts, `3000` ms timeout in `src/network/multiplayer-tick.cpp`.
- Ping interval: `1000` ms in `src/network/multiplayer-tick.cpp`.
- Reconnect: `1000` ms initial backoff, `10` attempts, `15000` ms max backoff in `src/network/multiplayer-packets.cpp`.
- Reliable gameplay event backlog/retry/ttl/attempts in `src/network/reliable-gameplay-events.cpp`.
- Server position history length `30` and broadcast sample cap `128` in `src/network/server-players.cpp`.
- Badconn queue caps in `src/network/badconn/badconn.cpp`.
- ICE negotiation retry/backoff windows in `src/network/multiplayer-packets.cpp`.

## Implementation

1. Extend `NetworkingConfigData` with explicit sections for runtime rates, adaptive snapshot buffering, event timeline holds, retry/reconnect policy, reliable gameplay events, and buffer limits. Parse from `config/networkingconfig.json`, clamp ranges, and log the exact resolved config once on load/reload.
2. Keep local input and local prediction immediate. Only observer-side remote rendering and observer-side remote event presentation use the buffered timeline.
3. Enforce `minimum_snapshots_before_rendering` and compute per-entity adaptive interpolation delay from snapshot arrival jitter, bounded by config min/max. The effective delay is the smaller stable delay that still preserves the configured minimum buffer depth.
4. In `updateRenderedReplica()`, read all presentation serials from the same `render` snapshot used for position/yaw/aim/weapon state. Store the last rendered server tick on the interpolation state.
5. Queue remote shot and pellet events until the shooter's rendered interpolation tick reaches the event's visual tick, with a config-bounded maximum hold so missing snapshots cannot stall effects forever.
6. Move the client timeout, attack retry, ping interval, reconnect policy, reliable event policy, and server buffer caps into `networkingconfig.json`.
7. Add `--badconn-preset <id>` to the ICE client probe path and wire `tools/test-ice-multiplayer.py --badconn-preset <id>` so preset 1 can be tested consistently by automation.
8. Add a repo-owned networking config self-test that validates required JSON keys and source invariants for the new timeline behavior.

## Validation

Required automated checks for this patch:

- `python devscripts/networking_config_selftest.py`
- `python build_agent.py`
- `python tools/test-ice-multiplayer.py --clients 2 --duration 10 --disable-relay --badconn-preset 1 --verbose --exe .\mimita.exe`
- `python tools/test-networking.py --all --no-build --clients 2 --duration 10 --disable-relay`
- `python overseer.py`

Manual follow-up if the automated ICE run cannot be completed locally: run two clients with `--badconn-preset 1`, enable config diagnostics, and verify remote body, movement animation bursts, weapon flash, tracers, pellet impacts, and projectiles all advance on the same visible timeline.
