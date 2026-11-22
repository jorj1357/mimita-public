// 2026-11-22 02:34 EST
/* purpose
* keep authoritative player slots through reliable-event delivery failure
* record the live launch, stall, and reconnect evidence behind the change
* separate build, runtime, and pending-throttle proof claims
* this file DOES NOT replace the feature record or claim human acceptance
*/

# Reliable-event delivery failure keeps the player slot

# Task

- Task ID: wake-930b08a1148f
- Session ID: session-980eb2726ed3
- Owner: website-deploy-authority
- Focus: Open MiMITA in fun counter strike, trace what prevents launch or causes in-game reconnect/disconnect, fix the first incorrect disconnect step, and verify by runtime journal.
- Status: IMPLEMENTED, runtime-proven on game generation 440; throttle-only follow-up built as generation 441 with the final runtime recheck pending an EXE-build queue drain.
- Date, time, timezone: 2026-11-22T02:34:00-05:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 07ba9003a6be33556ff350eb97be042fd18a99dc
- Final commit: none (not committed)

# Pre-existing changes

- The checkout already contained uncommitted edits from other active agents, including `src/network/server.cpp`, `src/npc/npc.cpp`, `devscripts/dev-loop.py`, `devscripts/dev_loop/game_build.py`, `devscripts/dev_loop/gui.py`, `devscripts/source_manifest.py`, and many issue/changelog docs. This session modified only `src/network/reliable-gameplay-events.cpp` and added this changelog.

# Requested behavior

User report: launching fun counter strike through the dev loop sometimes does not work (disconnected, room not found, stuck creating room); in game the session slows down, stops, shows reconnecting/disconnected, and a reconnect to the same room then works. The dev loop's displayed mode also stays stale after changing mode in the executable. The user asked agents to open the executable, start modes, and track what prevents launch, then make fixes and test again.

# Evidence

- Run `20261122_011031`, launch mode 45 (fun counter strike), map `dust2cyberiav4`: dev-loop launch completed in 34.8s (`request_accepted` 7ms, `client_connection` ready 9.96s, `mode_start` ready 30.6s, `complete` succeeded). FCS therefore launched in this run.
- Server `network.room.registered` reported `mode=funcounterstrike`, `room_code=VWSF22J`, `startup_npc_count=0` while the dev-loop status showed `npcs=1`.
- Client `network.join.map-required-missing` fired at 01:10:41.190 with `required_map_id=""` before welcome; `network.client.join.accepted` then appeared twice within 474ms.
- Client `client.snapshot.chunk-timeout` at 01:14:40 with `expected_chunks=4`, `received_chunks=2`.
- Server `network.server.timeout.stale` at 01:19:25.937 with `silence_ms=2503` against `stale_threshold_ms=2500`.
- Server `network.server.reliable-event.connection-failed` at 01:19:35.941 with `silent_ms=12507`, `packet_type=30`, `pending_reliable_events=56`, `players_before=1`, `players_after=0`. The removal owner in `src/network/server-packet-chat.cpp` documents the real deadline as `client_timeout_ms + reconnect_grace_ms = 70000ms`, so the reliable-event path removed the slot 57.5s early.
- Isolated TDM run `20261122_021421` on pinned generation 440 (`tdm networking validation`, `atdm`) reproduced client main-thread freezes up to 11.66s in `engineTickSimulate` (`State`/`Replay`). The stall stack passed through `NpcPerceptionModuleAPI`, `ChunkCollisionModuleAPI`, `MapGeometryModuleAPI`, and `InfiniteWorldgenModuleAPI`. Every server `network.server.timeout.stale` in that run aligns with a client send gap. This is the slow-down/stop root cause, not transport loss.
- Isolated FCS run `20261122_020138` failed readiness with "did not observe active mode" because `devscripts/dev_loop/parallel_test_runs.py` waits for `network.mode.start` `stage=active`, while the FCS client emitted `stage=phase` with `phase=4` (INTERMISSION). The run was otherwise online and entering mode.
- Dev-loop status still renders `launch_mode` as raw `45` and `run.json` instance `map` as empty while launch args carry `dust2cyberiav4`; `server_args` also contains a duplicate `--mode ssbm` before `--mode funcounterstrike`.

# Exact implementation changes

## File: `src/network/reliable-gameplay-events.cpp`

- Removed `markReliableConnectionUnhealthy`, which erased the `ServerPlayer` on reliable-event delivery failure.
- Added `noteReliableDeliveryFailure`, which emits `network.server.reliable-event.delivery-degraded` with `action=keep-connection`, `slot_state=held`, and `players_after=players_before`, then drops only the undeliverable event.
- The normal connection-health owner in `src/network/server-packet-chat.cpp` remains the only slot-removal owner; it releases the slot at `hard_timeout_ms + reconnect_grace_ms` (70000ms).
- The diagnostic is throttled to one structured line per second per process so a dead peer's grace window cannot turn into per-event journal flushes; the existing aggregate failure counter still counts every failure.
- `queueReliableGameplayEventToAll` now advances past a degraded player instead of assuming the map entry was erased.
- Retry send failure keeps the pending event and slot, retries on the next maintenance tick, and is bounded by the reliable-event TTL.

# Validation

- Build note `f39e8caa3220` attached to successful game generation 440 (changed paths `src/network/reliable-gameplay-events.cpp`, `src/npc/npc.cpp`); `game-g440.dll` contains `delivery-degraded` and no longer contains the old `connection-failed` string.
- Build note `c1b6c89c4156` (throttle) remained pending because generation 441 started at 02:29:36.769Z and the note was written at 02:29:39.786Z. Generation 441 still compiled the change (`changed_paths` includes the file, `success=true`) and `game-g441.dll` contains `delivery-degraded`.
- Runtime proof on generation 440, run `20261122_021421`: after the isolated client was killed, 225 `network.server.reliable-event.delivery-degraded` events over ~50s all reported `action=keep-connection`, `players_after=1`, with `silent_ms` from 20500 upward. No `network.server.reliable-event.connection-failed` occurred. The player was removed only by `network.server.timeout.expired` at `silent_ms=70028` with `removal_reason=reconnect_grace_expired` and `reconnect_token_retained=true`.
- Final generation 441 runtime recheck was blocked by a concurrent EXE build in the root dev loop ("root dev-loop has a pending or active EXE build"); the only delta from the proven 440 build is the one-line-per-second diagnostic throttle.
- No visual/human acceptance is claimed.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `src/network/reliable-gameplay-events.cpp` | Reliable-event expiry/backlog erased the player | Delivery failure keeps the slot and drops only the event; throttled diagnostic | Slot removal happened at ~12.5s instead of the documented 70s grace deadline | `git checkout -- src/network/reliable-gameplay-events.cpp` |
| `docs/changelog/2026-11-22/20261122_023400-reliable-event-keep-slot-on-delivery-failure.md` | absent | this record | Required session changelog | delete file |

# Risk and follow-up

- The client main-thread stalls (NPC perception, chunk collision, map geometry, infinite worldgen) remain the primary cause of the perceived slowdown and reconnect. They belong to the NPC/performance owners, not this network removal path.
- `devscripts/dev_loop/parallel_test_runs.py` readiness uses a phase name that FCS does not emit; FCS isolated test runs fail before proof even though the client entered mode.
- The dev-loop still mixes launch-request mode/map with runtime-observed values and emits duplicate `--mode` arguments.
