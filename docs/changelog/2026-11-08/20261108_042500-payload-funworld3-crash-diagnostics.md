# Payload/FunWorld3 crash diagnostics

## Source changes

- Added separate per-process event journals for a GUI-launched local server, plus parent-side startup readiness, last-stage, stall, and process-exit records. This corrects an existing environment inheritance path that placed the local server's events into the client's journal.
- Added structured server startup stage records for module/configuration loading, world/map loading, network bind, mode activation, and room registration. Tick-window records now include map/mode/packet context and a bounded list of slow tick stages.
- Added server join/reconnect receipt, validation, session lookup, player reuse/create, and send-result records. Client join/reconnect application, transition timing, attempt, and resource-probe context are also recorded without authentication secrets.
- Added map load/save/autosave revision, content hash, counts, and outcome context; added a process-identity crash breadcrumb to associate crash reports with the owning journal.
- Added dev-loop logger-readiness start/result events including wait duration, child exit code, and the last matching child journal event.

## Evidence and limits

- Static review: Python syntax parsing passed for `devscripts/dev-loop.py`; targeted `git diff --check` passed (Git reported only existing line-ending conversion notices).
- Build: the canonical dev-loop build request failed before compilation because `build.py` could not acquire/read `.dev/build-serialization.lock` (`PermissionError: [Errno 13]`). No source compiler result was obtained; no executable containing these changes was published.
- Runtime: no Payload/FunWorld3 sequence was run. The available prior process was a Fun Counter Strike run and stopped during this work. No crash cause or fix is claimed.
- Human/gameplay acceptance: not performed.

## Follow-up

Once the checkout-wide build lock is available, rebuild through the existing dev-loop, then reproduce Payload on FunWorld3 with map edits, leave/rejoin, and the later join against one identified server/client pair. Inspect both journals and any crash report/dump before selecting a code fix.
