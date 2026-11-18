# Dev loop targeted an unowned GUI server during process cleanup

Recorded: `2026-11-17T21:23:13Z`  
Status: **OPEN — cleanup target ownership is not verified**

## Expected behavior

The dev loop may terminate a process only when it can prove that the loop
launched or explicitly adopted that process and can match its checkout/run
identity. An unrelated server started from the same checkout must remain
outside the active run and must not be targeted during ordinary cleanup.

## Observed behavior

During run `20261117_211306`, the GUI launched server PID `14608` with
`--server --bind 0.0.0.0:1357 --map dust2cyberiav4 --mode sandbox`. The process
audit at `2026-11-17T21:23:13.102Z` found three same-checkout `mimita.exe`
processes and invoked `taskkill /PID 14608 /T /F` for that GUI child. The audit
recorded `taskkill_rc=128`; this result does not establish whether that child
was terminated by the command. The same audit did stop the dev-loop-owned
server PID `5636` and client PID `6608` (`taskkill_rc=0`).

## Cause

`devscripts/dev-loop.py::checkout_mimita_processes` discovers every
`mimita.exe` under the checkout without proving run ownership.
`DevLoop.reap_stray_mimita_processes` then calls `taskkill` on every discovered
PID whenever the configured process cap is one. The source path has no
ownership-marker or active-run identity check before that destructive action.

This conflicts with `docs/specs/dev-loop/dev-loop.md` sections 2 and 8.1–8.2:
the loop does not authorize killing arbitrary user-owned game/server
processes, and unproven ownership must result in `NEEDS_USER_ACTION`. It also
conflicts with `docs/specs/dev-loop/multi-instance.md`, which excludes external
processes from active-run ownership and requires explicit process-stop action.

## Related GUI startup failure (not attributed to this regression)

Earlier, GUI server PID `30824` exited after `46614ms` before room-code
registration. The client journal recorded exit code `3221225786`
(`0xC000013A`), `logger_ready=false`, and no last startup stage. That status is
consistent with control-C termination, but these records do not identify its
sender. The process audit above happened later and therefore cannot explain
that earlier exit. The cause of PID `30824`'s termination remains unresolved.

The GUI launch events for PIDs `30824` and `14608` also recorded empty
`server_events_jsonl` fields. The visible server log for PID `14608` pointed
to the client journal and contained no server startup event after logger
initialization. This is a separate diagnostics gap; it is not evidence of the
server's failure cause.

## Verification

- Inspected `logs/2026-11-17/20261117_211306/client-01/events.jsonl` and
  `logs/2026-11-17/20261117_211306/dev-loop/events.jsonl`.
- Inspected `logs/11-17-2026/Server_log_162233.txt`; it contains no stack trace
  or server startup-stage event.
- Source inspection confirms the cleanup path does not check ownership before
  targeting the GUI PID.
- No source fix, build, or fresh GUI launch was performed in this
  investigation. The dev-loop's current separate sandbox run was left intact.

See [the investigation changelog](../../changelog/2026-11-17/20261117_213105-gui-server-join-startup-investigation.md).
