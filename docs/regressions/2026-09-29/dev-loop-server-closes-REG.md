# Dev-loop server closed when the loop/client lifetime changed

Time created: 2026-09-29T13:56:27Z
Time last updated: 2026-09-29T13:56:27Z

Status: ATTEMPTED FIX (1)

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
