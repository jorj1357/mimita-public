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
