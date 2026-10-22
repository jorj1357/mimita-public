# Dev-loop start recovery and status writes

- Time: 2026-10-21 19:20:30 EDT (`2026-10-21T23:20:30Z`)
- Branch: `20261011zt`
- HEAD: `b29ab344a18dcf464aebf5cd1168c6947b0eefe2`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Summary

Fixed the dev-loop entry path that could report a healthy daemon unavailable
when its authenticated status response took longer than two seconds. Made JSON
status writes safe for concurrent daemon threads and resilient to brief Windows
file replacement locks. Repeated `--ensure-running` calls now reuse the
existing infrastructure. The daemon was restarted through its control path;
its adopted persistent server was preserved.

## Existing workspace changes

Before editing, `git status --short` showed unrelated pre-existing changes in
`docs/specs/movement/movement.md` and the untracked `docs/specs/fortnite/`
directory. They were left untouched.

## Source changes

### `devscripts/dev-loop.py`

At lines 23, 351–389, `errno`, a module write lock, unique same-directory
temporary files, flush/fsync, cleanup, and bounded retries for Windows
access/sharing failures were added to `write_json`.

Before (the original lines 353–355):

```python
temporary = path.with_suffix(path.suffix + ".tmp")
temporary.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
os.replace(temporary, path)
```

After (lines 351–389):

```python
_JSON_WRITE_LOCK = threading.Lock()
_JSON_REPLACE_RETRIES = 8


def write_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = json.dumps(value, indent=2) + "\n"
    temporary = None
    with _JSON_WRITE_LOCK:
        try:
            with tempfile.NamedTemporaryFile(
                mode="w", encoding="utf-8", newline="\n",
                dir=path.parent, prefix=f".{path.name}.", suffix=".tmp",
                delete=False,
            ) as handle:
                temporary = Path(handle.name)
                handle.write(payload)
                handle.flush()
                os.fsync(handle.fileno())

            for attempt in range(_JSON_REPLACE_RETRIES):
                try:
                    os.replace(temporary, path)
                    temporary = None
                    break
                except PermissionError as error:
                    winerror = getattr(error, "winerror", None)
                    retryable = winerror in (5, 32, 33) or error.errno in (
                        errno.EACCES, errno.EBUSY, errno.EPERM,
                    )
                    if not retryable or attempt + 1 == _JSON_REPLACE_RETRIES:
                        raise
                    time.sleep(0.025 * (attempt + 1))
        finally:
            if temporary is not None:
                try:
                    temporary.unlink()
                except FileNotFoundError:
                    pass
```

At lines 73 and 3381–3389, startup authenticated-status calls now allow 8
seconds; the dashboard's 0.25-second status polling remains unchanged.

Before (line 3384):

```python
if request_daemon_status(current, timeout=2.0) is not None:
```

After (lines 3385–3386):

```python
if request_daemon_status(
        current, timeout=DAEMON_READY_STATUS_TIMEOUT_SECONDS) is not None:
```

The new setting at line 73 is:

```python
DAEMON_READY_STATUS_TIMEOUT_SECONDS = 8.0
```

## Reason and specification review

The daemon can save status from control and build threads. The old fixed
`.tmp` path allowed overlapping writes; Windows `os.replace` also failed with
sharing/access errors. Startup readiness used a 2-second socket timeout, but a
status request that inspected the adopted server took about 2.9 seconds.
Therefore an already-running daemon was misclassified as unavailable.

`docs/specs/dev-loop/dev-loop.md`, section 22.2, requires the entry point to
wait until the daemon is ready and concurrent launches to reuse one watchdog
and daemon. The fix aligns the startup check with that contract.

## Validation and runtime evidence

- `python -m py_compile devscripts/dev-loop.py`: passed.
- `git diff --check -- devscripts/dev-loop.py`: passed.
- Before the timeout fix, `python devscripts/dev-loop.py --ensure-running --json`
  timed out after repeated probes and returned `daemon/watchdog recovery failed`
  while the daemon control port was listening.
- After the timeout fix, `--ensure-running --json` returned exit code 0 in
  about 3 seconds and reported the existing watchdog and daemon.
- The daemon was stopped through the dev-loop control path and restarted. The
  new daemon PID was 21600; watchdog PID 28924 remained active. It adopted the
  existing server PID 15984 without terminating it.
- Startup published build 2743 successfully (`Nothing changed`, so no C++
  source was recompiled). A subsequent `--ensure-running --json` returned
  success in about 2 seconds and reused the same process IDs.
- The status-write crash was reproduced in the old daemon log as WinError 32
  on `state.json`; the reported run also contained WinError 5 on
  `current-run.json`. The replacement daemon completed status writes and build
  publication without reproducing either failure during this validation.

No game client was launched and no visual/gameplay acceptance was performed.
Human review should confirm repeated dashboard starts and a real client launch;
the regression remains `ATTEMPTED FIX (1)` until that review.

## Documents and focused review

- Read `docs/ROUTER.md` and the supplied `AGENTS.md` instructions.
- Read `docs/specs/dev-loop/dev-loop.md`,
  `docs/operations/build-and-exe/build-and-exe.md`,
  `docs/operations/task-completion/task-completion.md`,
  `docs/architecture/live-development/live-development.md`,
  `docs/workflows/runtime-scenario-validation.md`,
  `docs/specs/debug-logging/debug-logging.md`,
  `docs/architecture/terminal-commands/terminal-commands.md`,
  `docs/skills/spec-behavior-review-v1.md`, and
  `docs/skills/terminal-command-checker-v1.md`.
- `spec-behavior-review-v1`: pass; the change aligns the implementation with
  the existing startup/readiness contract. Runtime and human acceptance remain
  separate.
- `terminal-command-checker-v1`: not applicable to game terminal commands;
  the changed command is the dev-loop process entry point.

## Files created

- `docs/regressions/2026-10-21/dev-loop-status-write-race-REG.md` — records
  the confirmed failure and marks the fix attempted pending human review.
- This final session changelog.
