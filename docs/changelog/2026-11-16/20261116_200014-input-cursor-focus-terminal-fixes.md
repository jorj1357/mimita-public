# Input cursor, focus, and terminal toggle fixes

- Date: 2026-11-16 20:00 EST
- Owner: opencode
- Branch: `checkpoint/one-checkout-condensation-20261111`
- Scope: resident host input bridge, cursor-mode normalization, terminal toggle
  ownership, and runtime evidence.

## Summary

The resident host now delivers real mouse-button events to the game DLL, starts
the game window focused, and exposes cursor mode in the game's normalized
`0/1/2` space instead of raw GLFW enum values. The terminal grave shortcut now
has one owner when host key events are active. The game-side logging bridge no
longer lets a JSON serialization error terminate the server.

## Changes

- Added `GameInputKind::MouseButton` and `HostRuntime::inputMouseButtonCallback`;
  host mouse-button events now reach `InputRouter::ingestMouseButton`.
- Added `GameStartupV1::unlockCursor` and forwarded it into the game-owned
  `gUnlockCursorOnLaunch`.
- Focused and showed the resident game window after render-device creation.
- Normalized `windowGetCursorMode` to `0=normal, 1=hidden, 2=disabled` and
  mapped `windowSetCursorMode` back to GLFW modes. Gameplay and UI code already
  compares against `2`, so raw `212995` previously made `cursorCaptured` false.
- Removed the duplicate polling-based terminal toggle in
  `engineTickState` when host key events are present; the event owner remains
  `handleGameInputKey`.
- Removed stale `MIMITA_GAME_DISABLE` fallback branches from the resident path
  (previously changed in this working tree) and kept resident gameplay non-
  fallback.
- Added focused terminal open/close events at `Terminal::toggle`.
- Hardened `mimita::game::log_bridge::emitEvent` to dump fields with
  `error_handler_t::replace` and catch serialization exceptions. Previously an
  uncaught JSON exception in the log bridge terminated the server.

## Runtime evidence

- Run `20261117_005045`, client PID `688`, EXE
  `2026-11-17T00:50:03.657687Z`, game generation `290` then `291`:
  - `input.host-key` key `96` reached the game DLL.
  - Exactly one `terminal.opened`; a second press produced one
    `terminal.closed`.
  - `input.host-mouse` button `0` reached the game DLL.
  - `weapon.attack.local_request` showed `weapon=revolver`,
    `gameplay_active=true`, and a fired shot after `lc` set cursor mode `2`.
  - Pause menu set cursor mode `0`; cursor was visible.
- Build identity: EXE `2026-11-17T01:01:29.903571Z`; game generation `294`.
- Build target gate: `PASS` (`exe=46`, `cold=45`, `tests=0`,
  `game_in_exe=0`, `cold_undefined=0`).
- Audit triage: `UNRESOLVED=0`.
- Cutover-1 remains green except `gameplay_live_fix_proven=false`.
- Server crash reports for `PID 2304` and `PID 22292` show
  `CXX_EXCEPTION_GCC`; the symbolized stack enters
  `mimita::game::log_bridge::emitEvent` and `nlohmann` serialization. The
  reports are in `%LOCALAPPDATA%\Mimita\crashes\crash-2026-11-16_19-51-39-2304.txt`
  and `crash-2026-11-16_19-59-19-22292.txt`.
- Run `20261117_135530`, server PID `28212`, game generation `294`:
  - The server survived three `server.death.transition` events from NPC rocket
    explosions (`spawn_generation` 4, 5, and 6) and was still alive with no new
    crash report.
  - The client received `input.host-mouse` button `0`; the press produced a
    `weapon.attack.local_request` (dry fire because the player was dead at the
    time), proving the click path remained live.
- The dev-loop game-build worker was wedged behind a stale
  `.dev/build-serialization.lock` owned by a dead PID. The lock was removed and
  the daemon restarted with permission; generation `294` published.

## Documentation added

- Added `docs/handoffs/networking-hot-cold-contract-20261117.md`, the approved
  coordination brief between the ICE hot/cold split and the
  reconnect/health/event/config work. It records file ownership, event
  ownership, config keys, resident transport core API, swap/reattach invariants,
  shared-file sequencing, and the validation gate.
- This is coordination documentation only; it is not build or runtime evidence.

## Networking event and dev-loop work

- Added `NetworkObservabilityConfig` and `NetworkHotSwapConfig`, loaded from
  `config/networkingconfig.json` sections `network_observability` and
  `hot_swap`.
- Added hot `src/network/network-events.h` with the common semantic envelope.
- Reconnect attempts now use `reconnect_interval_ms` from config; the policy
  module no longer overrides it with a hardcoded 1s backoff.
- Added/renamed connection events:
  - `network.client.reconnect.scheduled`
  - `network.client.reconnect.attempt`
  - `network.client.reconnect.terminal`
  - `network.client.snapshot.gap`
  - `network.client.snapshot.recovery`
  - `network.client.join.accepted`
  - `network.server.reconnect.accepted` / `.rejected`
  - `network.server.peer.health`
  - `network.server.timeout.expired`
  - `network.server.snapshot.build` / `.send`
  - `network.server.join.accepted`
- `network.session.health` now carries connection ID, attempt ID, role,
  transport, stage, result, reason code, and next expected event.
- Added `docs/specs/networking/network-event-contract.md` and linked it from
  `docs/specs/networking/networking.md`.
- Added `devscripts/network_trace.py`; it reads run journals and config and
  checks attempt spacing, grace/terminal ordering, swap reconnect suppression,
  and server slot evidence.
- Added the dev-loop `19.4b` one-stack/windowless-command contract and a TODO
  to audit commands that can open dashboard/console windows.
- Stopped the duplicate Python312 dev-loop watchdog/daemon stack. One
  Python314 dashboard/watchdog/daemon stack remains.
- Game generation `320` built successfully with the new events. Existing run
  evidence shows `network.server.snapshot.build/send`; a fresh reconnect run is
  still required to prove the client reconnect chain.
- `network_trace.py` against run `20261117_170234` reported the legacy
  reconnect attempts at about 1.0s spacing versus the configured 6.0s, which is
  the expected pre-fix evidence. It also found ten legacy server reconnect
  rejections and no new-generation recovery/terminal event in that mixed run.
- The duplicate Python312 watchdog/daemon stack was stopped; the only remaining
  dev-loop stack is watchdog PID `4740` and daemon PID `24944` (Python312).
  `--ensure-running` reused them without opening a dashboard or console.

## Canonical event rename and lifecycle completion

- Renamed existing event names to the contract names:
  - `network.client-snapshot-window` -> `network.client.snapshot.applied`
  - `network.client.hello-sent` -> `network.client.hello.sent`
  - `network.player.session.removed` -> `network.server.player.removed`
  - Updated `devscripts/dev-loop.py`, the melee scenario, and the multi-client
    workflow to the new snapshot event name.
- Added client connect/transport/map/spawn/disconnect events:
  - `network.client.connect.requested` / `.preflight`
  - `network.client.transport.open` / `.ready` / `.failed` / `.closed`
  - `network.client.join.request_sent` / `.rejected`
  - `network.client.map.sync.start` / `.loaded` / `.load_failed` / `.ready_sent`
  - `network.client.spawn.applied`
  - `network.client.reconnect.accepted`
  - `network.client.disconnect.requested` / `.sent`
  - `network.client.snapshot.received`
- Added server lifecycle events:
  - `network.server.transport.open` / `.ready` / `.closed`
  - `network.server.join.received` / `.validated` / `.rejected`
  - `network.server.map.ready_received`
  - `network.server.spawn.assigned`
  - `network.server.reconnect.received` / `.lookup` / `.expired`
  - `network.server.disconnect.received`
  - `network.server.timeout.stale`
- Added `network.config.loaded` / `network.config.reloaded` with config revision
  and active reconnect/timeout values.
- Added `game.generation.swap_begin`, `.candidate_loaded`, `.old_shutdown`,
  `.new_init`, `.first_tick`, `.swap_end`, plus `network.hotswap.suspend` /
  `.resume`.
- Extended the event helper with automatic `config_revision`, `build_id`,
  generation fields, local/remote endpoints, and a config-driven
  `max_events_per_second` token bucket.
- Added `NetworkingConfig::revision()` so events can identify the active config.
- Added `network.metrics.window` on client and server using
  `network_observability.metrics_window_ms`.
- Added `network.client.reconnect.rejected` for missing-token and failed ICE
  reconnect transport paths.
- Added `game.generation.first_snapshot` on the first snapshot applied by each
  game generation.
- Added `network.hotswap.health_rebased` on the first health evaluation for a
  connected local player in a generation.
- Added an honest `network.hotswap.queue_drained` placeholder on first tick:
  `result=skipped`, `reason_code=resident_pump_not_installed`, zero messages.
  Real drain evidence still requires the resident pump.
- `git diff --check` passed, `devscripts/network_trace.py` and
  `devscripts/dev-loop.py` pass `py_compile`, and `networkingconfig.json`
  parses through the trace loader.
- Runtime attempt used the open server run `20261117_170234` and client-04.
  The open server journal contains `network.server.snapshot.build/send` and
  `network_trace.py` reports `events=21063`, `connections=[1]`, `attempts=10`,
  ten server rejections, and nine spacing errors (about 1,009–1,058 ms versus
  the configured 6,000 ms). The open client is on the older artifact and never
  reached a new-generation connection; latest game builds are failing on the
  concurrent `ServerPlayer.transport` refactor and a stale PCH `mRevision`
  error. New event proof is therefore not available yet.

## Networking hot-policy cleanup

- Added `NetworkIcePolicyConfig` and the `ice_policy` JSON section for listener
  gather, peer gather/connect, coordinator poll, pending-peer, and packet-drain
  limits.
- `server-ice-policy.cpp` now reads those values from `networkingconfig.json`;
  its hardcoded timeout/admission constants are removed.
- Removed resident fallback policy from `server-ice.cpp`: missing hot ICE policy
  now refuses admission/expiry work instead of silently using resident values.
- `publishServerIceListener` now requires the hot session-publication decision;
  it no longer publishes unconditionally.
- Removed resident session-policy fallback from `server.cpp`; an unavailable hot
  session decision no longer silently registers or publishes a room.
- `network.hotswap.queue_drained` now reports the resident swap snapshot
  (`session_count`, `queued_events`, `suspended`, `generation_attached`) while
  retaining the honest `resident_pump_not_installed` reason because no client
  queue pump exists yet.
- Updated `source-owners.json` reason text so the resident networking files are
  described as pump/ICE/socket mechanism with hot policy delegated to the
  game-owned policy files.
- Build verification is still blocked in the shared checkout: the EXE link used
  a stale `network_server.o` referencing hot config, and the game build is
  queued/building. A fresh successful build and runtime proof remain pending.

## Handoff added

- Added `docs/handoffs/networking-cutover1-finish-handoff-20261117.md` with
  current build state, the remaining hot-policy split, client pump blocker,
  stale gate warnings, and exact runtime evidence required before Cutover-1 can
  be claimed.

## Human acceptance

- Visual review: not performed.
- Gameplay review: not performed.
- Multiplayer review: not performed.

## How to reverse

- Remove the mouse-button ABI/callback and startup `unlockCursor` field.
- Restore raw GLFW cursor-mode return values and direct set mode.
- Restore the unconditional polling terminal toggle.
- Restore direct `fields.dump()` if the log-bridge hardening is not wanted.
- Keep the terminal toggle diagnostic only if needed for tracing.
