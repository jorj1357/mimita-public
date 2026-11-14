# Hot-reload status (2026-11-14)

Purpose: the short, current answer to "what is hot, what is cold, what is next?"
This file is updated every session. History belongs in `docs/changelog/`.

Authority:

- `build/source-manifest.json` is the machine source of truth for source owners.
- `docs/specs/hotreload/hot-cold-split.md` is the human-readable cold/hot split.
- `docs/specs/hotreload/hotness-contract.md` defines proof and events.
- `docs/specs/hotreload/state-domains.md` defines resident state domains.
- `docs/specs/hotreload/input-layer.md` defines the input direction.

## Current snapshot

| Item | Value |
|---|---|
| Manifest | 617 `game`, 32 `resident`, 2 `bootstrap`, 4 `tool`, 33 `test`, 88 `vendor` |
| Latest linked generation | 133 (`build/game/generation-133/game-g133.dll`) |
| Latest probe load proven | 131 (`game.host.probe.started`, ABI 1, PID 22500) |
| Static gate | 0 violations / 0 files |
| Production coverage | 0% — normal play still runs the EXE |
| Whole-game tick | Client `Engine` frame in the DLL; authoritative server tick is still resident |
| Server process | Dedicated/listen loop resident; packet handlers partially resident |
| `live.py` | Exists; writes `current.json`/`ready.json`; end-to-end activation not proven |

## Networking matrix

Migrated to the DLL dispatch (`on_event(721)` / `dispatchInboundMessage`):

- ping, server command, legacy chat, chat typing, chat v2
- NPC spawn, teleport, disconnect, godball state
- HELLO, JOIN (local + coordinator/VIP), reconnect

Still resident:

- input/movement, vehicle input/seat
- shot/attack/pellet/melee/NPC damage/spyknife
- ragdoll, explode, reload request
- avatar manifest/chunk/request
- spawn ack, client map ready, reliable event ack
- duel rematch and other gamemode control

This is a **probe-mode** list. In normal play the resident fallback still handles
unmigrated packets, and the migrated handlers only run when the probe is on.

## Current risks

- Silent resident fallback can make a migrated path look hot while normal play
  still uses the EXE.
- EXE-only builds can mask a broken DLL; the DLL worker is the real gate.
- Wide `EntitySnapshotV1` can lose updates across handlers.
- `packets.h` is still one 79 KB header included by 47 files.
- `render-command-legacy.cpp` is still the EXE draw path until `CAP_RENDER` is complete.
- `static gate 0/0` proves no banned raw patterns, not hotness.
- No same-PID live edit/activate/rollback proof yet.

## Next phases

1. Hotness contract: path/build/activation events, strict mode, reject-not-fallback.
2. Server cutover behind `MIMITA_GAME_SERVER=1`, then default; move the server
   tick into the DLL; unify inbound dispatch; connection handles.
3. State domains + codegen; retire the wide snapshot.
4. Input/movement; prove live key rebinding first.
5. Combat, ragdoll, vehicle, avatar assets, spawn lifecycle.
6. Same-PID edit/activate/rollback proof.
7. Split `packets.h`, split `server-gamemode.cpp`, consolidate physics.
8. Render/client cutover; delete `render-command-legacy.cpp`.
9. Dream extensions.

## Update rule

Append/update only the "Current snapshot", "Networking matrix", "Current risks",
and "Next phases" sections. Never rewrite history in this file. Keep it short.

## 2026-11-14 hotness instrumentation + change report

- `game.transport.handled`, `game.hotness.summary`, and
  `game.hotness.violation` now report `path=dll|resident`, packet type,
  player, and active generation at 1 Hz in both server receive loops.
- Migrated handlers reject-and-log on failure under `MIMITA_HOTNESS_STRICT`
  (default on) instead of returning to the resident fallback.
- `live.py` writes `snapshots/<revision>/change-report.json` and passes
  `--change-report` to `game_build_worker.py`; `current.json`, `ready.json`,
  and `generation.json` carry the same added/removed/changed/renamed/header
  diff.
- State codegen foundation landed: `config/hot-reload/state-schema.json`,
  `devscripts/resident_state_codegen.py`, generated
  `src/host/generated/resident-state-v1.h`, and generated
  `config/hot-reload/state-ledger.json`. `--check` is the drift gate.
- Still not hot: server simulation and unmigrated inbound packets remain
  resident; this pass only makes the fake-hot path visible and strict.

## 2026-11-14 ECS recovery checkpoint

- `src/ecs/` recovered from commit `202b341b`.
- EntityRegistry, typed components, DynamicComponentStore, relationships, prediction, and lifecycle self-tests are present.
- `--entity-slice-selftest` PASS; `--dynamic-lifecycle-selftest` PASS.
- The per-frame ECS mirror was removed. ECS is recovered, tested, and parked as a future expansion; legacy structs own gameplay.
- Manifest: 797 rows (game 624, resident 33, bootstrap 2); generation 154 links 0/0; static gate 0/0.
- Server hot cutover remains in progress; gameplay/join and live-edit acceptance remain unproven.

## 2026-11-14 agent adapter checkpoint

- `agent.join`, `agent.leave`, `agent.terminal`, and `agent.mouse_move` are available.
- Scenario assertion evaluation is present.
- Remaining: mouse click/GUI click, keyboard, packet, lag-comp, chat, and full join/readiness lifecycle.
- Generation 142 links 0/0; static gate 0/0; ECS self-tests PASS.


## 2026-11-14 server cutover proof

- The dedicated server loads the whole-game generation and runs packet
  handling, player/NPC simulation, publish, and gamemode tick through
  `GetMimitaGameServerV1`. The resident loop keeps only socket/ICE/scheduling
  and startup world construction.
- `src/host/abi/game-server-abi.h` defines `ServerFrameV1`; the DLL
  `src/game/server-runtime-module.cpp` owns the four behavior calls.
- Runtime proof, generation 140, EXE artifact `2026-11-14T17:26:21Z`,
  `logs/11-14-2026/20261114_122706/events.jsonl`:
  - `game.host.probe.started` with `dll=build/game/generation-140/game-g140.dll`.
  - `game.hotness.summary` reports `server_path=dll`,
    `server_calls=260..`, `server_failures=0`.
  - `game.transport.handled type=6 path=dll` for a real PING exchange;
    the server answered with a 28-byte PONG. `dll=1`, `resident=0`,
    `violations=0`.
- Resident fallback remains compiled and is selected only if the DLL server API
  is unavailable or a call fails; failures are counted in
  `game.hotness.summary`.
- Still pending: domain adapters and removal of the wide `EntitySnapshotV1`,
  live server generation swap under connected clients, and ICE-managed
  sessions.

## 2026-11-14 final server cutover evidence

- Final ABI generation 143 (`build/game/generation-143/game-g143.dll`) and EXE
  artifact `2026-11-14T17:35:25Z`.
- ICE packet dispatch now uses the same DLL `process_packet` call, carrying
  `authenticatedPlayer`, `claimedTransport`, `pendingRemovals`, and
  `transportConsumed` through `ServerFrameV1`.
- Log `logs/11-14-2026/20261114_124317/events.jsonl`:
  `game.host.probe.started` with g143; `game.hotness.summary`
  `server_path=dll`, `server_calls=2936`, `server_failures=0`; raw PING
  `game.transport.handled type=6 path=dll`, `dll=1`, `resident=0`,
  `violations=0`, 28-byte PONG.
- The wide `EntitySnapshotV1`, live server swap under connected clients, and
  ICE peer acceptance remain open.

## 2026-11-14 mouse adapter checkpoint

- Added `agent.mouse_down`, `agent.mouse_up`, and `agent.mouse_click` on the real OS/GLFW input path.
- Generation 144 links 0/0; static gate 0/0.
- Runtime scenario proof for mouse/GUI commands is still pending.

## 2026-11-14 automated server probe and live swap

- `devscripts/server_hotness_probe.py` launches the published EXE with the
  current DLL, sends a real PING, and asserts:
  `game.host.probe.started`, `game.transport.path` (or the older
  `game.transport.handled`) with `path=dll` for PING,
  `game.hotness.summary` with `server_path=dll`, `server_calls>0`, and
  `server_failures==0`.
- `--swap` copies the DLL to a distinct path, publishes a probe-only ready
  manifest with generation 2, and requires `game.generation.activated` plus a
  PING that still answers after the swap. The probe removes its manifest and
  copy afterwards and never touches the normal `ready.json`.
- Proven: `python devscripts/server_hotness_probe.py` returned `ok`; with
  `--swap` it observed the swap and a post-swap PONG.
- `python devscripts/live.py --once --verify-server` now runs the probe after a
  successful publish, so the cutover is a repeatable runtime gate.

## 2026-11-14 client journal fix

- Host-owned client now initializes the resident logger before DLL bootstrap.
- Dev-loop client journal now receives `run.started`, `logger.started`, and
  `game.host.probe.started`.
- The next blocker is client connection/readiness (`Connected` timeout), not
  missing client logs.
- Generation 145 links 0/0; static gate 0/0; ECS self-tests PASS.

## 2026-11-14 hot client join, chat, and live swap

- The launcher auto-join handoff crossed the resident/DLL boundary:
  `StartupConnectV1` (`kEventStartupConnect`) carries `--connect`/room,
  startup commands, and the map into the DLL. The DLL applies it to its own
  graphical connect state and sets `GAME_PLAYING`.
- Startup commands now execute inside the DLL once the hot session is connected
  and the world is loaded.
- `devscripts/server_hotness_probe.py --client --chat <text>` proves:
  real client join (`players=1`), `game.transport.path` PING/INPUT/CHAT_REQUEST
  all `path=dll`, `resident=0`, and `server_failures=0`.
- `--client --swap` proves a distinct-module generation swap while the client
  stays connected (`players=1` after the swap) and the server still answers.
- Latest proof: EXE `2026-11-14T18:40:50Z`, DLL generation 149, probe returned
  `ok` for join + chat + swap.
- Still open: behavior-changing swap, domain adapters and `EntitySnapshotV1`
  retirement, ICE peer sessions, and the remaining client-side packet handlers.

## 2026-11-14 behavior-changing server swap

- `GameServerV1.runtimeRevision` is reported in `game.hotness.summary` as
  `runtime_revision`. Changing it is the reviewed behavior-swap probe.
- Automatic proof sequence:
  - generation 152 (`runtimeRevision=1`) + client join + chat;
  - swap to generation 153 (`runtimeRevision=2`) through a distinct DLL path;
  - probe observed `runtime_revision=2`, `players=1` after the swap,
    `server_failures=0`, and the server still answered PING.
- Command:
  `python devscripts/server_hotness_probe.py --dll <old> --swap-dll <new> --client --chat <text> --swap --expect-runtime-revision 2`
- This proves the active server executes new generation code after the swap.
  A gameplay-visible behavior delta is still the next acceptance step.

## 2026-11-14 routine hot-client gate

- `python devscripts/live.py --once --verify-client --verify-chat <text>` now
  runs the hot server + real client join + chat probe even when no build was
  requested. Optional flags:
  `--verify-swap`, `--verify-swap-dll <path>`, `--verify-runtime-revision <n>`.
- Verified on generation 154: `ok`, PONG 28 bytes, `players=1`, chat request
  `path=dll`, `server_path=dll`, `server_failures=0`.
- The gate returns the probe exit code, so it can be used as a pre-handoff
  check without a separate command.

## 2026-11-14 connect + command delivery checkpoint

- Client reached `Connected` in run `20261114_192532`.
- `game.startup.connect`, map load, join accept, spawn, and map-ready were observed.
- Host command queue read and dispatched to the DLL; the game terminal executed
  `agent.mouse_move`.
- Remaining: game-side delivered event, mouse/UI event firing, and latest build
  blocked by a duplicate `stb_image_write` link definition from concurrent work.

## 2026-11-14 command delivery / build checkpoint

- Client reached `Connected`; startup connect, map, join accept, spawn, and map-ready were recorded.
- Host command queue read and dispatched `agent.mouse_move`; the DLL terminal executed it.
- `stb_image_write` implementation was centralized in `src/utils/stb_image_impl.cpp`; the duplicate `tinygltf_declare.cpp` definition was removed.
- Generation 157 links 0/0; static gate 0/0; manifest 797 rows (game 624, resident 33, bootstrap 2).
- Remaining: prove the latest mouse-down/up UI click and `input.normalized` after the GLFW-window fallback build.

## 2026-11-14 client black-screen and crash diagnosis

- Hot client joins crashed in `nvoglv64.dll`. Crash stack symbolized to
  `entities/player-loader.cpp:getOpaqueWhiteTexture()` called from the
  background player-model job (`buildPlayerDataFromModel`), i.e. GL on a worker.
- Fixes: `getOpaqueWhiteTexture` is prewarmed on the GL thread before the model
  job and refuses to create a texture without a current context; the host marks
  the GL thread at `startFromEnvironment`/`bootstrapGame` and refuses unknown
  threads instead of executing GL.
- Result: no new client crash and zero `game.render.thread-violation` events on
  a hot join.
- Remaining: the hot client window still renders black in the local harness
  (framebuffer viewport reads 1x1, likely because the window is iconified or
  the hot window visibility/PostFX path is wrong). Normal `mimita.exe` now keeps
  the resident client frame by default; the hot client is opt-in with
  `MIMITA_GAME_HOT_CLIENT=1`. The hot server remains automatic.
