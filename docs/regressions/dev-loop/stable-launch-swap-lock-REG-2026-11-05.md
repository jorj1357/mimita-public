# Stable launch path swap raced process exit

- First observed: 2026-11-06T02:06:13Z (2026-11-05 21:06:13 EST)
- Status: ATTEMPTED FIX — bounded retry is in source; live daemon restart is deferred while the healthy game run is active
- Related implementation: `devscripts/dev-loop.py`, `activate_stable_launch()`
- Related launch journal: `logs/2026-11-06/20261106_020611/dev-loop/events.jsonl`
- Related session record: [artifact pointer recovery](../../changelog/2026-11-06/20261106_021300-artifact-pointer-recovery.md)

## Expected behavior

After an explicit latest-artifact launch, the dev loop must safely replace its
fixed `.dev/current` launch bundle and start the selected verified artifact.
The dev-loop contract permits process replacement only through its controlled
lifecycle path and requires the selected executable identity to be verified.

## Actual behavior

The launch selected verified artifact `2026-11-06T02:05:45.057794Z` and staged
its bundle. It then failed at `stable_launch_activation` with Windows
`[WinError 5] Access is denied` while renaming
`C:\mimita-v9\.dev\current` to a backup directory. The old server and clients
had already exited, and this launch had no ready client. The old stable path
remained intact; a later dashboard launch completed against the selected
artifact after the path was available.

## First incorrect step and cause

`activate_stable_launch()` made one immediate `os.replace()` attempt on the
existing `.dev/current` directory. On Windows, process termination can be
reported before every executable/file handle is released. The swap did not
retry transient access denial, so a brief post-exit lock became a failed
launch.

## Fix attempt

The source now retries the atomic directory move on `PermissionError` for a
bounded 10 seconds, sleeping 250 ms between attempts. It still raises and
preserves the old stable directory if the lock remains, rather than deleting
the active path. The running daemon predates this source change. Per the
watcher contract, its source-only restart is deferred while a healthy game run
is active; runtime proof of the retry itself remains pending a safe daemon
restart and a subsequent latest-artifact replacement.

## Evidence

- Run `20261106_020611` records artifact selection and validation as successful,
  then records `stable_launch_activation=failed` with the access-denied reason.
- A later direct dashboard request completed and run `20261106_020803` reached
  `mode_start=ready` and `complete=succeeded` on verified artifact
  `2026-11-06T02:05:45.057794Z`, SHA-256
  `f6cea7c92bad9729d8b123f088663674376dc4197bcd12d8c281289f3fc73ed4`.
- The live server/client pair is ready and matching. This proves the dashboard
  path works when the stable bundle is available; it does not prove the new
  retry code because the healthy-run source restart was deferred.
