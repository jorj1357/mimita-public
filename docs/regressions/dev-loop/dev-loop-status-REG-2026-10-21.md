# Dev-loop status write race and startup timeout

Time created: 2026-10-21T23:20:30Z
Time last updated: 2026-10-21T23:20:30Z

Status: ATTEMPTED FIX (1)

## Expected behavior

The dev-loop entry point must reuse or start one watchdog and daemon, wait for
the authenticated daemon status endpoint, and remain usable when invoked soon
after another launch. The current specification says: “It starts whichever
required process is absent, then waits until the daemon is ready before
attaching the view. Concurrent launches must reuse the same watchdog and
daemon.” See `docs/specs/dev-loop/dev-loop.md`, section 22.2.

## Actual behavior

The canonical build/launch path crashed while saving its state:

```text
PermissionError: [WinError 32] The process cannot access the file because it is being used by another process: 'C:\\mimita-v9\\.dev\\state.json.tmp' -> 'C:\\mimita-v9\\.dev\\state.json'
```

A later launch also failed replacing `current-run.json` with `WinError 5`.
Repeated concurrent status/build updates shared the same fixed `.tmp` path.
Separately, the entry point treated an authenticated status call taking about
2.9 seconds as unavailable because its startup probe timed out at 2 seconds.
Before the timeout adjustment, `--ensure-running` eventually returned
`daemon/watchdog recovery failed` even though the daemon's control socket was
listening.

## Incorrect and corrected implementation

Before, `devscripts/dev-loop.py::write_json` used one temporary filename and
performed one immediate replacement:

```python
temporary = path.with_suffix(path.suffix + ".tmp")
temporary.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
os.replace(temporary, path)
```

It now serializes in-process writes, uses a unique temporary file beside the
target, flushes it, and retries transient Windows access/sharing errors before
failing. `wait_for_daemon_ready` now gives each authenticated status request 8
seconds while leaving dashboard polling unchanged.

## Cause and fix

Daemon status saves can come from the control and build threads. Their shared
temporary filename allowed overlapping writes, and Windows can also briefly
deny replacement while a status reader has the destination open. The fixed
2-second control timeout was shorter than a measured status query that checked
the adopted server's Windows process identity. The fix gives writes unique
temporary paths and serialized replacement, retries transient replacement
locks, and gives startup readiness checks enough time for the authenticated
response.

## Evidence

- `python -m py_compile devscripts/dev-loop.py` succeeded.
- `git diff --check -- devscripts/dev-loop.py` succeeded.
- Before the readiness fix, `python devscripts/dev-loop.py --ensure-running --json`
  waited through recovery and returned `daemon/watchdog recovery failed`.
- After the fix, the same entry point returned exit code 0 in about 3 seconds,
  reporting the existing watchdog and daemon. A controlled daemon restart then
  loaded the final script as PID 21600 while retaining server PID 15984.
- The daemon's canonical no-op build published build 2743 successfully, and a
  subsequent `--ensure-running` call again returned success in about 2 seconds.
- No client was running, so no game client launch or human visual acceptance
  was performed. Human confirmation that the dashboard remains stable during
  repeated real launches is still required before marking this regression
  solved.

See `docs/changelog/2026-10-21/20261021_232030-dev-loop-start-recovery.md` for
the session record.
