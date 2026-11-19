# GUI room startup could strand the client on a white gameplay screen

- Recorded: 2026-11-19T00:48:20Z
- Status: CLIENT WHITE-SCREEN MITIGATION IN SOURCE; SERVER EXIT CAUSE UNRESOLVED; LIVE VERIFICATION PENDING
- Changelog: [20261119_004821-gui-room-startup-join-state.md](../../changelog/2026-11-19/20261119_004821-gui-room-startup-join-state.md)

## Expected behavior

Creating a room for Juggernaut Horde on `juggernaut6` should keep the client in
a visible loading/join state until the local server accepts it and the
server-required map is ready. If server startup fails, the user should remain
able to see the failure instead of being stranded on a blank gameplay frame.

The networking specification says: “An initial join or game/map
synchronization is **Loading**, with the current stage and total elapsed time
visible to the client, until local readiness is reached.”

## Observed evidence

The available current run was not a clean reproduction: its run manifest,
processes, and journals did not identify one coherent fresh GUI room attempt.
The user clarified that the reported white screen occurs on the client while
joining after creating the room. The current GUI-created Juggernaut Horde
server process was alive when inspected; no white screen or crash dump was
observed in this session.

A prior matching GUI room-startup trace in
`logs/2026-11-17/20261117_211306/client-01/events.jsonl` records:

- `network.gui-server.launch` for server PID 30824, `sandbox` on
  `dust2cyberiav4`.
- The client immediately queued a direct `127.0.0.1:1357` join.
- `network.join.map-required-missing` reported player ID 0, empty required map,
  and zero snapshots.
- 46.6 seconds later `network.gui-server.startup-failed` reported the server
  exited with code `3221225786` (`0xC000013A`), before logger readiness and
  without a last startup stage or server journal.

This older trace establishes a server-exit and incomplete-join sequence, but
does not prove the user’s exact Juggernaut Horde reproduction or why the
server exited. The later process-reaper incident is separate and is not
attributed as the cause of this earlier exit.

## Cause

`src/gui/gui-main.cpp::launchServerPresetAndJoin` started the asynchronous
local UDP join and set `GAME_STATE = GAME_PLAYING` immediately, before any
server welcome, player assignment, required map, or snapshot had arrived. On
the failed startup trace, the client had no accepted player or map. Forcing
the renderer into gameplay at that point could expose an empty/white world
while connection health correctly kept the client in the join path.

The server’s `0xC000013A` termination remains unexplained: the trace has no
server-owned journal, no startup stage, and no dump proving who or what ended
the process. Do not treat this client-state correction as a fix for that
server-process exit.

## Correction

Removed the eager `GAME_STATE = GAME_PLAYING` assignment from the GUI local
join queue. The network owner already moves the client through map loading
and into gameplay after receiving and applying the server-required map. This
keeps room startup from independently forcing the display into gameplay
before join state exists.

## Validation and remaining work

- Source review confirms the GUI room join no longer assigns gameplay state
  before the asynchronous join.
- No build or live run was performed in this session under the shared-checkout
  build-authority rules. The source fix is not yet proven in an executable.
- Reproduce creating Juggernaut Horde on `juggernaut6`; capture both the
  client and server `events.jsonl` from the same run and verify the client
  remains visibly Loading until map readiness, or surfaces a startup error.
- Capture the server’s own exit diagnostics or crash dump to identify the
  still-unknown process termination cause.
