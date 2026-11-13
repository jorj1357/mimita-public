# Host foundation batch (2026-11-12)

**Status:** EXE build, runtime parity, and the first command authority slice
exist. Exact `admincmds` runs in the game generation; argument forms remain
resident. No same-PID swap and no cutover.

## What changed

### Capability ABI

- `src/host/abi/entity-capability.h`: `EntityAdapterV1`, `EntityCapabilityV1`,
  stable player/NPC handles, `PlayerFlag`.
- `src/host/abi/render-capability.h`: bounded `RenderCommandV1` stream with
  submit/flush/pending.
- `src/host/abi/lifetime-capability.h`: `JobsCapabilityV1`,
  `CallbacksCapabilityV1`, `ResourcesCapabilityV1`.
- `src/host/abi/game-host-abi.h`: added `Entity=6`, `Render=7`, `Jobs=8`,
  `Callbacks=9`, `Resources=10`; appended player flags, max health, and display
  name to `EntitySnapshotV1`.

### Resident host

- `src/host/host-runtime.h/.cpp`:
  - real `entity_read`/`entity_write` forwarding through `EntityAdapterV1`;
  - `deliverInbound(TransportMessageV1)` through
    `on_event(kEventTransportMessage)` under the barrier;
  - generation job/callback/resource leases and counters;
  - render command submit/flush/pending counters.
- `src/host/host-server-entity-adapter.h/.cpp`: maps player/NPC handles to live
  `ServerPlayer`/`ServerNpc` records and exposes bounded snapshots.
- `src/host/host-server-transport-adapter.h/.cpp`: server `send_to_player`,
  broadcast, session state, and peer count using a bound sender.
- `src/network/server.cpp`: when `MIMITA_GAME_PROBE=1`, binds the adapters and
  mirrors inbound packets to the active generation before the resident packet
  dispatch. The resident path remains authoritative. The hook is present in both
  the dedicated loop and the listen-server tick.

### Game bridges

- `src/game/entity-bridge-module.cpp`: CAP_ENTITY read/write helpers.
- `src/game/render-bridge-module.cpp`: CAP_RENDER submit/flush helpers.
- `src/game/lifetime-bridge-module.cpp`: CAP_JOBS/CALLBACKS/RESOURCES helpers.
- `src/game/probe/npc-target-probe-module.cpp`: initializes the new bridges and
  logs `game.transport.inbound` for event 721, including packet type and
  `PACKET_SERVER_COMMAND` text when present. It also computes a non-authoritative
  `admincmds`/`player_list` decision from the CAP_ENTITY player snapshot and logs
  `accepted`/`status`; the resident handler is still authoritative.

### Worklist

- `devscripts/game_link_worklist.py`
- `docs/handoffs/game-dll-undefined-worklist-20261112.md`
- `docs/handoffs/game-dll-undefined-worklist-20261112.csv`

Current worklist: 104 refs / 78 unique (after the diagnostic GLAD object, audio-helper bridge, and legacy HotReload removal).

## Evidence

- Probe build: `build/probe/npc-target/generation-35/game-g35.dll`; 10 explicit
  probe sources; 0 failures. Runtime parity used generation 16.
- Whole-game build: 584 game sources; 0 compile failures; link fails with
  104 undefined / 78 unique after the diagnostic GLAD/audio/HotReload pass; the foundation itself adds capability
  definitions rather than retiring a symbol family yet.
- `host-runtime.cpp` compiled standalone, exit 0.
- `host-server-entity-adapter.cpp` compiled standalone, exit 0.
- `host-server-transport-adapter.cpp` compiled standalone, exit 0.
- `src/network/server.cpp` passed `-fsyntax-only`, exit 0.
- Manifest: 747 total / 586 game / 23 resident; revision
  `bb1f2d422623b1cfa9201938beb0d28b5ad1fa669ddb0446433267162d237338`.
- Static triage: 66 closed, 0 candidates, 39 state, 182 adapter, 297
  runtime-proof, 26 resident-host, 135 non-game, 0 unresolved.

## What this proves

- The EXE with the new host code builds and starts.
- The probe DLL loads on both server and client.
- The server receives an inbound `PACKET_SERVER_COMMAND` and delivers it to the
  game generation.
- `CAP_ENTITY` reads a live player snapshot with host/admin flags.
- Game and resident decisions match for `admincmds`: accepted, status
  `admincmds=1 (host)`.

## What this does not prove

- That the game-side ack reaches the client for commands other than exact `admincmds`.
- That authority is switched beyond exact `admincmds`.
- That the render command stream draws anything.
- That jobs/callbacks/resources are integrated into quiesce.
- That the barrier refuses unload while leases are busy.
- That same-PID swap, compile fallback, rollback, or state continuity works.

## Next

1. Keep exact `admincmds` authoritative; it is proved.
2. Add `player_list` once a bounded player-list query exists.
3. Then switch map/team/spawn/gamemode slices.
4. Implement live activation from `build/game/ready.json` plus barrier/commit.
5. Add same-PID swap, compile fallback, rollback, and state-continuity proof.
