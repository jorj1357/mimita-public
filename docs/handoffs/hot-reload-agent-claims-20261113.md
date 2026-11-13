# Hot-reload agent claims (2026-11-13)

**Protocol:** add one row before editing; claim the smallest stable prefix.
Release only after build and verification. Never edit an unclaimed shared path.
No branches, worktrees, lanes, or second checkouts.

| Agent | Path or prefix | Task | Started | Finished |
|---|---|---|---|---|
| A | `src/host/**`, `src/game/*-module.*`, `docs/specs/hotreload/**` | integrator/resident boundary | 2026-11-13 | — |
| B | `src/network/server-*.cpp`, `src/network/server.h`, `src/network/multiplayer-*.cpp`, `src/network/client.cpp`, `src/network/udp-*`; owner-authorized `src/host/abi/transport-capability.h`, `src/host/transport-adapter.h`, `src/host/host-*-transport-adapter.*`, `src/host/host-runtime.*`; `src/game/transport-bridge-module.*`, `src/game/network-compat-bridge-module.cpp` | server packet send → CAP_TRANSPORT (incl. endpoint send extension) | 2026-11-13 | — |
| C | `src/engine/**`, `src/render/**`, `src/entities/player-render*.cpp` | whole-game tick/render + CAP_RENDER | 2026-11-13 | — |

## Agent C boundary note (2026-11-13)

`renderContext` is the resident `Renderer*` (CAP_RENDER_DEVICE `acquire` returns
it; `Engine::init` uses `acquireHostRenderer` under `MIMITA_GAME_DLL`). C is
providing the whole-game entry in `src/engine/whole-game-entry.*`; Agent A owns
the `src/game/game-module.cpp` wiring and the host `driveTick`/`driveFrame`
dispatch. C does not touch `src/host/**`, `src/game/*-module.*`, or `main.cpp`.

Render-contract handoff (2026-11-13): `src/render/render-command.h` is C's
authoritative missing-operation list for `CAP_RENDER`. `render-command-legacy.cpp`
is the `!MIMITA_GAME_DLL` raw-GL implementation; the DLL bridge is added only
after A implements the declared operations. No call sites have moved yet.

## Agent B status (2026-11-13)

Source pass complete. Owner authorized B to extend `CAP_TRANSPORT` with a
plain-integer endpoint send (`send_to_endpoint`) so pre-auth replies no longer
use `sendto` in game-owned sources. All 27 raw `sendto` sites are now accounted
for: 23 game-owned sites route through `serverSendToPlayer` /
`serverSendToEndpoint`, and 4 remain inside resident `server.cpp` /
`server-ice.cpp` transport ownership. Client outbound (`mpSendPacket`,
`mpShutdown`, `sendReconnectRequest`) now prefers `CAP_TRANSPORT::send` under
`MIMITA_GAME_DLL` with the resident path as fallback. Packet IDs, schemas,
ordering, and retransmit logic are unchanged. Inbound command uses
`on_event(722)` and inbound `PACKET_PING` uses
`on_event(721)` + `CAP_TRANSPORT::send(peer)`; the remaining handlers still use
`SOCKET`/`sockaddr_in` and are blocked on resident player/NPC/world handles.
Build and runtime verification remain A-owned;
B holds the claim until A confirms the host and whole-game builds, then fills
`Finished`.

## Reserved shared paths

Only by explicit request to Agent A:

- `src/main.cpp`, `src/main-init.cpp`, `src/main-systems.cpp`
- `src/game/game-objects-module.cpp`
- `src/terminal/terminal-state.h`
- `docs/handoffs/*` evidence files
- `config/source-owners.json`

## Build lock

Only one agent may run the dev-loop host build or the whole-game worker at a
time. Check `python devscripts/dev-loop.py --status` first and wait for the
current build to finish.
