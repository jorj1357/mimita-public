# Hybrid aimbody lifecycle-safe startup and GUI-equivalent dev-server persistence

Time (UTC): `2026-09-29T20:54:55Z`
Time (local): `2026-09-29 16:54:55 EDT`
Branch: `afad20a-rebuild`
Base commit: `e27a5a9a`

## Summary

Two defects were fixed with one change set.

1. With `config/aimbody.json` `"mode": "hybrid"`, the client simulated local
   gameplay from the `Player::reset()` fallback position `(1, 5, 60)` before the
   authoritative server spawn arrived, then teleported and dragged a stale aim
   body across the discontinuity. Local gameplay now refuses to simulate until
   the authoritative spawn transform is installed, and the aim body is rebuilt
   at the authoritative pose on every lifecycle discontinuity.
2. The dev-loop dedicated server could not be distinguished from daemon-owned
   children and had weaker process ownership/diagnostics than the GUI external
   server path. The dev-loop now mirrors the GUI argument semantics, keeps a
   durable server record, checks liveness without ever terminating the server,
   and reports bounded `[DEV SERVER]` evidence.

`config/aimbody.json` remains `hybrid`; no fallback mode was forced in C++ and
no global hybrid tuning was weakened.

## Root cause (hybrid startup)

Per render frame, `engineTickReplay` runs `simulateTick` (local physics and the
aim body) before `engineTickNet` runs `mpReconcileLocalPlayer`. `JoinAcceptPacket`
has no restore position, so `ctx.hasLocalServerPosition` stays false until the
first local snapshot after `ClientMapReady`, and the client simulated and fell
from `(1,5,60)`. `simulateTick` only called `activateAim` when
`!ragdoll.aimActive()`, so the body was never rebuilt when `player.pos` teleported
to the authoritative spawn, and `tetherAimRoot` pulled stale physical state
across the discontinuity — the post-teleport lag.

## Files and exact changes

### New: `src/network/local-gameplay-readiness.h`

Header-only, dependency-free owner of the readiness decision and the composite
lifecycle identity:

- `LocalGameplayReadiness` (plain bools) and
  `localGameplaySimulationReady(const LocalGameplayReadiness&)`. Single-player
  (`networked == false`) is never gated. Networked play requires: connected, map
  ready for the local player, not waiting for map load, has server position, no
  pending spawn transform, has spawn generation, has server epoch, outgoing and
  applied epochs match the server epoch, and model parts exist. It deliberately
  does **not** wait for `SpawnActivated`, per the chosen tradeoff, so instant
  respawn stays responsive.
- `localLifecycleId(serverEpoch, spawnGeneration)` composite id.

### `src/network/multiplayer-context.h`

- Include `network/local-gameplay-readiness.h`.
- `inline uint64_t mpLocalLifecycleId(const MultiplayerContext&)` now delegates
  to `localLifecycleId(ctx.localServerEpoch, ctx.lastKnownSpawnGeneration)`.
- Declared `bool mpLocalGameplaySimulationReady(const MultiplayerContext&, const Player&)`
  and `void mpApplyAuthoritativeTransform(MultiplayerContext&, Player&)`.

### `src/network/multiplayer-tick.cpp`

- `mpApplyAuthoritativeTransform` installs `pos`, `vel`, `yaw`, `ground.onGround`,
  zeroes `externalImpulse`, syncs legacy layers, refreshes world transforms, and
  advances the outgoing epoch. This is the single transform-application owner.
- `mpLocalGameplaySimulationReady` fills `LocalGameplayReadiness` from the
  context/player and calls the pure decision.

### `src/network/multiplayer-reconcile.cpp`

- Both authoritative snap blocks (epoch change at the old lines 55-61 and
  `applyPosition` at the old lines 161-168) now call
  `mpApplyAuthoritativeTransform(ctx, player)` instead of duplicating the
  transform writes. Behavior is unchanged; ownership is now single.

### `src/engine/engine-tick-net.cpp`

- In the `pendingAuthoritativeSpawn` block, after clearing pending knockbacks,
  the full authoritative transform is installed immediately via
  `mpApplyAuthoritativeTransform`, and `lastAppliedEpoch`/`localPlayerReconciled`
  are updated. This guarantees the next fixed simulation tick starts from the
  server spawn, before the next frame's reconcile would otherwise apply it. The
  `SpawnVelocityReset` warning is preserved.

### `src/ragdoll/ragdoll-mode.{h,cpp}`

- Refactored the body of `activateAim` into private `buildAimBody(Player&)`
  (rest-pose bind, `mAim = RagdollBody{}`, `initParts`, velocity seeding from
  `player.vel`, `mAimActive`).
- Added public `rebindAimToAuthoritativePlayer(Player&)`, which rebuilds the
  active body with `buildAimBody` and logs
  `[AIMBODY] rebound to authoritative lifecycle ...`.

### `src/sim/simulate-tick.cpp`

- Include `network/multiplayer-context.h`.
- After `clearReplicatedPose()`, return early when
  `MP_CONTEXT.active && !mpLocalGameplaySimulationReady(MP_CONTEXT, *sim.player)`.
  This stops movement, gravity, collision, aimbody, ragdoll, NPC-vs-player
  collision, and void death before the authoritative transform is applied, while
  networking, map load, room connection, packet receive, and the spawn handshake
  continue in `engineTickNet`. `sim.tick` is never read, so the early return is
  safe; the player freezes in place (no visible fall).
- Before the movement/aim branch, compute `lifeId = mpLocalLifecycleId(MP_CONTEXT)`
  and, when the aim body is active and the id changed, call
  `rebindAimToAuthoritativePlayer`. One reset path covers join, respawn, instant
  respawn, teleport, duel, map change, reconnect, and true discontinuities.
- Removed the now-duplicate inner `auto& ragdoll` declaration (function-scope
  reference is used everywhere; no shadowing).

### `devscripts/dev-loop.py`

- `build_server_args(exe, map_name, room_file_path)` now mirrors the GUI
  `launchServerProcess()` flag semantics: `--server`, `--bind`, `--name`,
  `--map`, `--mode`, `--max-players`, `--weapon-set`, `--map-rotation-minutes`,
  `--password-protected`, `--password`, `--room-file`, `--host-player` (with the
  client-name fallback), `--npcs`/`--no-npcs`, `--no-map-rotation`,
  `--no-discord-notification`, and `--duel --gamemode` only for duel. The
  previous unconditional `--gamemode` duplication was removed. Profile values
  supply dev settings; `CREATE_NEW_CONSOLE` and no listen-server path are
  preserved.
- Durable independent-server record in `__init__`: `server_pid`, `server_exe`,
  `server_args`, `server_launch_ms`, `server_unavailable`, plus the existing
  `server_process`, `room_file_path`, `room_code`. The server remains outside
  `self.processes`.
- `server_health()` and `check_server_after_client()`: after a client exit the
  server is only observed. Alive → retained and reported; unexpectedly exited →
  exit code reported, stale room code and temp room file cleared, server marked
  unavailable. The server is never terminated by cleanup.
- `maintain_process()` calls `check_server_after_client()` on the transition to
  no live clients.
- `launch_latest()` reuses a healthy server (`[DEV SERVER] reusing ...`),
  otherwise clears stale state and launches a fresh one with
  `[DEV SERVER] launched pid=... args=...` and `[DEV SERVER] room=... alive=1`.
- Shutdown terminates only tracked clients and prints
  `[DEV SERVER] shutdown requested; server left running pid=... room=...`.
- Status panel shows `SERVER: pid=... room=... alive|exited`.

### New: `tests/local-gameplay-readiness-test.cpp`

Standalone test for the pure readiness rule and lifecycle identity: single-player
never gated; the all-ready case; each required fact individually blocks; lifecycle
id changes on epoch and spawn-generation transitions.

## Reason the dev-loop server closing was not "fixed" by assumption

`runServer()` is `while (true)`; the only clean exit is `--timeout`
(`src/network/server.cpp` lines 556-564), which the dev profile does not pass, and
`handleClientTimeout` never exits the server. A closing server is therefore an
external termination, a console/job-lifetime effect, or a crash. This session did
not assert a cause; it made the dev-loop observe and report the process and its
exit code, and removed any Python-side path that could terminate it. The prior
committed "keep server alive" change was preserved.

## Documents and skills

- Read: `docs/ROUTER.md`, `docs/specs/networking/networking.md`,
  `docs/operations/build-and-exe/build-and-exe.md`,
  `docs/operations/task-completion/task-completion.md`,
  `docs/regressions/README.md`,
  `docs/skills/spec-behavior-review-v1.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`.
- `docs/skills/spec-behavior-review-v1.md`:
  `spec-code disagreement`: the networking spec says there is one
  client/server gameplay path and that spawn/respawn is authoritative. The code
  gated no local simulation on authoritative readiness, which contradicted that
  intent; this change aligns the implementation. No spec text was changed.
- Pre-existing spec note: `docs/specs/20260929plan.md:19` records that
  server-side hitbox parity and ragdoll limb replication remain outstanding; not
  in scope here.

## Evidence (separated)

Source evidence:

- `src/network/local-gameplay-readiness.h` (new), `src/network/multiplayer-context.h`,
  `src/network/multiplayer-tick.cpp`, `src/network/multiplayer-reconcile.cpp`,
  `src/engine/engine-tick-net.cpp`, `src/ragdoll/ragdoll-mode.{h,cpp}`,
  `src/sim/simulate-tick.cpp`, `devscripts/dev-loop.py`,
  `tests/local-gameplay-readiness-test.cpp`.

Focused test evidence:

- `tests/local-gameplay-readiness-test.cpp`: `13 passed, 0 failed`
  (`g++ -std=c++17 -O2 -Iinclude -Isrc -DGLM_ENABLE_EXPERIMENTAL`).
- Regression re-runs: `tests/physical-aim-torque-test.cpp` `5 passed, 0 failed`;
  `tests/ragdoll-replication-test.cpp` `4 passed, 0 failed`.

Static/build evidence:

- `python -m py_compile devscripts/dev-loop.py`: passed.
- `git diff --check`: only a pre-existing trailing-whitespace warning in
  `docs/regressions/2026-09-20/cold-build-required-REG.md` (from an earlier
  session, not introduced here).
- `python build_agent.py` (cold): the first attempt hit a stale incremental
  object for `ragdoll-mode.cpp` (`undefined reference to
  RagdollModeSystem::rebindAimToAuthoritativePlayer`). After deleting the stale
  objects for the touched translation units and rebuilding:
  `Status: SUCCESS`, return code 0, executable `C:\mimita-v9\mimita.exe`
  (`2026-09-29 16:53:54`). Cold-build debt recorded as occurrence 24.

Runtime/log evidence and live visual/gameplay evidence: **not performed this
session.** Server-persistence evidence: **not performed this session.** These
remain for human review.

## Human review still needed

- Launch `devscripts/dev-loop.py` with `"mode": "hybrid"`. Confirm no visible fall
  from `(1,5,60)`, that the first active position is the server-selected spawn,
  and that hybrid body parts align immediately with no startup lag spike.
- Move, jump, collide, look around. Confirm no post-teleport lag after respawn,
  instant respawn, and a server teleport.
- Leave the client and confirm the external server console stays alive, the room
  is reused by a later client, and `[DEV SERVER]` lines report the correct
  pid/room/health. If the server does exit, capture the reported exit code and
  the server console's last lines.

## Pre-existing edits

The working tree already had uncommitted edits before this session:
`config/accounts/default.json`, `config/analytics.json`,
`config/networking/presets/default.json`, `config/networkingconfig.json`,
`config/ragdoll.json`, `docs/changelog/2026-09-29/20260929_132503-aimbody-physical-mode.md`,
`docs/regressions/2026-09-20/cold-build-required-REG.md`,
`src/config/networking-config.h`, `src/network/movement-validation.h`,
`src/network/multiplayer-reconcile.cpp`, `src/network/server-players.cpp`. None
were reverted or claimed. This session's edits are limited to the files listed
above.
