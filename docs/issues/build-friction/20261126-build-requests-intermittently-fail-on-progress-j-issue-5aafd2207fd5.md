# issue-5aafd2207fd5: Build requests intermittently fail on progress.json rename (WinError 5) and one waited 900 s for nothing

## Human report (verbatim, immutable)

> DEV CONTROL TIMING wait_seconds=900.000 ... terminal_status=timeout


## Agent suggestion

### 2026-11-26T20:18:40.859Z — rocket-league-aerial

Partially fixed 2026-11-26 (rocket-league-aerial). _write_json in devscripts/dev_loop/game_build.py retried os.replace only 20 times at 50 ms (1 s) before re-raising, and progress.json is read by the dashboard, ant-virus and other dev-loop readers while the build rewrites it. The retry now spans 200 attempts with a 20-250 ms backoff (about 10 s worst case). Evidence: 12 consecutive game-DLL requests since the change (generations 719-730, runs 20261126_193939 through 20261126_201615) completed with no PermissionError and no WinError 5 in .dev/build-timings. The second half of this issue is still open: a --wait-for-build that returns terminal_status=timeout after 900 s while build/game/current.json still shows the previous generation. That path does not report that the change was not built, which is what makes the wait misleading.
