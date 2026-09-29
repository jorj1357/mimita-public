# Keep dev-loop servers alive like GUI-created servers

Date: 2026-09-29

## Investigation

The GUI launches the dedicated server with `CREATE_NEW_CONSOLE` and keeps its
process independent from the client window. The dev-loop instead stored the
server in its daemon-owned child list. `stop_processes()` terminated that list
when the loop restarted or exited, which made the server disappear even though
the GUI-created server would remain open.

The dev-loop watcher also walked about 908 source files every 150 ms. On this
checkout each scan took about 60 ms, creating unnecessary background CPU work
during client startup.

## Fix

`devscripts/dev-loop.py` now:

- keeps the dedicated server in a separate persistent process slot;
- stops only the dev-loop client when replacing a client or leaving the loop;
- reuses the existing server room code instead of launching a second server;
- leaves the server console open until the user closes it or explicitly stops
  the server; and
- polls the watched source tree every 500 ms instead of every 150 ms.

## Validation

- `python -m py_compile devscripts/dev-loop.py`: passed.
- `git diff --check -- devscripts/dev-loop.py`: passed.
- Live startup FPS and server-persistence acceptance remain required after
  restarting the currently running dev-loop.
