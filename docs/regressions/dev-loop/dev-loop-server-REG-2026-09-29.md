# Dev-loop server closed when the loop/client lifetime changed

Time created: 2026-09-29T13:56:27Z
Time last updated: 2026-09-29T20:54:55Z

Status: ATTEMPTED FIX (2)

Related specification:
`docs/specs/gamemodes/gamemodes.md`

Related changelog:
`docs/changelog/2026-09-29/20260929_135627-dev-loop-server-lifetime.md`

## Observed

The user reported that a server created by the dev-loop eventually closed
while the GUI-created server remained open. The user expected the server to
remain alive until its console window was closed or an explicit in-game/server
stop action was used.

## Expected behavior

The dedicated server must have an independent lifetime from the client window
and the development-loop controller, matching the GUI server-launch path.

## Confirmed cause

`devscripts/dev-loop.py` placed the dedicated server and client in the same
`self.processes` collection. `stop_processes()` called `terminate()` for every
entry, including the server, during loop shutdown and client replacement.

The GUI path instead launches the server with `CREATE_NEW_CONSOLE`, closes its
process handles, and only calls `TerminateProcess` from its explicit
`stopServerProcess()` path.

## Attempted Fix 1

The dev-loop now keeps the server in `self.server_process`, stops only client
children, retains the room code, and reuses a live server on later client
launches. Source polling was also slowed from 150 ms to 500 ms after measuring
the repeated 908-file scan at roughly 60 ms per pass.

Automated proof:

- `python -m py_compile devscripts/dev-loop.py`: passed.
- `git diff --check -- devscripts/dev-loop.py`: passed.

Human acceptance is still required for persistent-server lifetime and the
startup-FPS improvement.

## Attempted Fix 2

Time: `2026-09-29T20:54:55Z`

Related changelog:
`docs/changelog/2026-09-29/20260929_205455-aimbody-lifecycle-devserver.md`

The regression remained unconfirmed as fixed after Attempted Fix 1. No cause was
assumed. Investigation confirmed the server cannot self-exit: `runServer()`
(`src/network/server.cpp`) is `while (true)` and only breaks on `--timeout`, and
`handleClientTimeout` (`src/network/server-packet-chat.cpp`) never exits the
process. A closing server is therefore an external termination, a console/job
lifetime effect, or a crash.

Change:

- `devscripts/dev-loop.py` gained a durable, separate server record (`server_pid`,
  `server_exe`, `server_args`, `server_launch_ms`, `server_unavailable`) kept
  outside `self.processes`.
- `server_health()` and `check_server_after_client()` observe the server after a
  client exits without ever terminating it. An unexpected exit reports the exit
  code, clears the stale room code and temp room file, and marks the server
  unavailable; a live server is retained and reported.
- `build_server_args()` now mirrors the GUI `launchServerProcess()` flag set
  (`--max-players`, `--map-rotation-minutes`, `--password-protected`,
  `--password`, unconditional `--host-player`, duel-only `--gamemode`).
- Bounded `[DEV SERVER]` diagnostics were added; shutdown terminates only clients.

Result:

No Python-side path can terminate the server, and the server's pid/exit code is
now observable. Automated proof:

- `python -m py_compile devscripts/dev-loop.py`: passed.
- Cold build `python build_agent.py`: `Status: SUCCESS`, return code 0.

Human acceptance is still required: leave the client and observe that the server
console stays open, a later client reuses the room, and any unexpected server
exit is reported with an exit code.

## Attempted Fix 3

Time: 2026-10-05

The remaining ownership leak was in client-side duel/leave cleanup. The client
can call `stopExternalServerProcess()` when leaving a queue or match. That
helper is correct for a GUI-created server, but a dev-loop client must not use
it against the dev-loop's durable server.

`devscripts/dev-loop.py` now launches the client with the explicit environment
marker `MIMITA_DEV_LOOP_SERVER=1`. `src/duel/duel-queue.cpp` routes every
client-side external-server cleanup call through
`stopExternalServerOwnedByThisClient()`: the marker preserves the dev-loop
server, while an ordinary GUI client still calls `stopExternalServerProcess()`.

Build/source proof:

- `python -m py_compile devscripts/dev-loop.py`: passed.
- `python build_agent.py`: `Status: SUCCESS`, return code 0.
- `git diff --check -- devscripts/dev-loop.py src/duel/duel-queue.cpp`: passed.

Human acceptance remains required: with a freshly restarted dev-loop using the
new client, leave the match, confirm the client changes state without closing,
confirm the dedicated server console remains open, and rejoin the retained
room. If the server still exits, capture its console exit code and the client
line containing `[DUEL QUEUE] preserving dev-loop-owned server`.
