// 2026-09-28T19:30:32Z
/* purpose
* record the investigation of host-only commands being rejected in local play
* identify the first missing step without changing gameplay or network code
* does NOT claim a runtime fix or human acceptance
*/

# Task

- Summary: Inspect why `procedural_world_start infinite_dungeon_slayer` and
  other host commands report `rejected: not host` while the local process is
  hosting.
- Status: FIX_APPLIED / SOURCE_INSPECTED / RUNTIME_VALIDATION_REQUIRED
- Date, time, timezone: 2026-09-28T19:30:32Z ISO 8601 UTC; display timezone
  America/New_York (2026-09-28 15:30 EDT).
- Branch/commit: `afad20a-rebuild` / `b7c7432c`.

# Findings

- The client command sender only checks that a multiplayer context is active;
  it does not decide host permission.
- The authoritative server rejects commands when the matched `ServerPlayer` has
  `isHost == false`.
- `isServerHost()` answers whether this computer is running a server, but that
  value is not used by the server command permission gate.
- The server player host flag is assigned from the configured host player name,
  or from first-join order when the configured name is empty.
- The sandbox local-server setup does not populate `ServerLaunchSettings` with
  `AuthSystem::instance().displayName()`, so it relies on fragile first-join
  order. A stale, duplicate, or earlier player can therefore make the visible
  local player non-host.
- GUI-launched servers do pass the authenticated display name, so this is most
  likely local sandbox/listen-server host identity wiring rather than a new
  Infinite Dungeon Slayer permission model.

# Evidence

- `src/terminal/procedural-world-commands.cpp`: forwards commands to the
  existing server-command packet path.
- `src/network/server-packet-chat.cpp`: common `isHost` gate returns
  `rejected: not host` before procedural start/stop/generate branches.
- `src/network/server.cpp`: `isServerHost()` is process/listen-server state;
  listen-server startup copies `settings.hostPlayerName` into the authoritative
  host name.
- `src/network/server-packets.cpp`: `computeHostFlag` assigns the per-player
  host bit from the configured name or first joiner.
- `src/engine/engine-tick-state.cpp`: sandbox settings omit `hostPlayerName`.
- `git log -S`: host-flag logic predates the current procedural commit; the
  procedural commit reuses the existing gate rather than creating a second host
  authority.

# Runtime and acceptance

- No runtime command trace was available in this inspection, so the exact
  current player id/name mismatch remains unobserved.
- No source fix was applied. Build, runtime, multiplayer, and human acceptance
  remain open.

# Applied narrow fix

- Updated `devscripts/dev-loop.py` so the launched server receives
  `--host-player` from `host_player_name` when configured, otherwise from the
  same `client_name` used to launch the client. This restores explicit host
  identity without changing server permission rules.

# Runtime and acceptance

- Python syntax validation passed after the edit.
- The running dev-loop processes must be stopped and restarted to use the new
  launcher behavior.
- Runtime command acceptance remains open until `healthall 999`, `mapchange`,
  and `procedural_world_start infinite_dungeon_slayer` are observed working.
