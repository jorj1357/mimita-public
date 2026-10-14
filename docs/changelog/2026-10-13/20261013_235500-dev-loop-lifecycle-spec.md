# Dev-loop lifecycle and stale-daemon specification

## Scope

Expanded `docs/specs/dev-loop/dev-loop.md` with an implementation-level
contract for the Python daemon, host supervisor, MiMITA client, and dedicated
server lifetimes.

## Documentation changes

- Defined the exact process topology and ownership boundaries.
- Explained why an already-running Python daemon does not hot-reload edited
  `dev-loop.py` source.
- Specified daemon source identity versus game executable identity checks.
- Defined controlled daemon replacement, queue handoff, and one-daemon rules.
- Distinguished visible game-client behavior from visible supervisor-console
  behavior.
- Documented the complete command evidence chain from control socket through
  fixed-tick gameplay state.
- Added a failure/recovery matrix for daemon, client, server, build, socket,
  and identity failures.
- Updated implementation phases and current-status boundaries so historical
  “not implemented” notes cannot be mistaken for current runtime proof.

## Evidence boundary

This was a documentation-only change. No source code, process, executable,
server, or running dev-loop state was modified by this changelog task.
