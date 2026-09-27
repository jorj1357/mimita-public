# Dev-loop single server/client pair guard

- time_utc: 2026-09-27T20:08:08Z
- display_timezone: America/New_York
- display_time: 2026-09-27 16:08:08 EDT
- branch: `afad20a-rebuild`
- result: `PASS_WITH_HUMAN_REVIEW`

## Request and evidence

The dev-loop output showed build 61 being launched repeatedly. Process
inspection confirmed two independent `dev-loop.py` daemons for this checkout:
PIDs 11552 and 3996. Each daemon owned its own build/server/client lifecycle,
so both could launch a pair.

## Corrective behavior

`devscripts/dev-loop.py` now:

- scans for another same-checkout dev-loop process before acquiring its lock;
- refuses to start a second daemon and prints the conflicting PID(s);
- keeps one server/client pair per daemon;
- launches a replacement only after a successful new build or explicit `[1]`;
- waits for `[1]` after a server/client pair exits instead of automatically
  launching three retries and a fallback client;
- reports the waiting state as `no MiMITA process running; press [1] to launch
  one server/client pair`.

The status line now says `AUTO-RESTART: BUILD UPDATES ONLY` so the behavior is
visible instead of implying crash retries.

## Validation

- `python -m py_compile devscripts/dev-loop.py`: passed.
- `python devscripts/dev-loop.py --help`: passed.
- `git diff --check -- devscripts/dev-loop.py`: passed.
- Existing active processes were not killed. Both old dev-loop windows must be
  closed with `Q`; then start one fresh dev-loop instance.

## Human review still needed

After restarting exactly one loop, verify one server and one client appear,
that a server crash leaves the loop waiting for `[1]`, and that a successful
source update replaces the pair once.
