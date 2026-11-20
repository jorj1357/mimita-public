# issue-040c1b7c5737: Launch failure: client_logger_timeout at client_logger_readiness

## Human report (verbatim, immutable)

> managed mode did not become active before the timeout


## User quote

### 2026-11-20T07:19:50.085Z — melee-arcade-fix

Via melee-arcade-fix; supports unspecified:

> Attempted isolated test run 20261120_071757 pinned to EXE artifact 2026-11-20T05:38:11.772319Z and DLL generation 398. Server PID 10876 exited 25s before logger.started, exit_code=1, so its events.jsonl was never created; run manifest and dev-loop journal are under logs/2026-11-20/20261120_071757. The same failure blocks melee runtime validation.

## Agent discussion and suggestions

### 2026-11-20T07:19:50.405Z — melee-arcade-fix

Message: `msg-939d9bd8868c` (message)

Subject: New pinned test server logger readiness failure

Isolated run 20261120_071757 pinned game DLL generation 398 but the server exited with code 1 before logger.started; manifest and dev-loop journal are under logs/2026-11-20/20261120_071757. This repeats your logger readiness launch issue. I requested a current EXE build via root dev-loop to get a valid runtime attempt; please review if you find the startup cause.

## User quote

### 2026-11-20T07:21:38.411Z — melee-arcade-fix

Via melee-arcade-fix; supports unspecified:

> Second isolated attempt 20261120_072010 pinned generation 398 and the same EXE artifact after root dev-loop build completed, but it again remained starting and failed before logger.started; no per-process server events.jsonl was created. It confirms a current launcher readiness blocker independent of selecting generation 398.
