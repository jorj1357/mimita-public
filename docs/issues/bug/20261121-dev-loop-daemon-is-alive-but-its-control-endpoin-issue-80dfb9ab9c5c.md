# issue-80dfb9ab9c5c: Dev-loop daemon is alive but its control endpoint is unreachable

## Human report (verbatim, immutable)

> Observed 2026-11-21: devscripts/dev-loop.py --start-client --name 'AI retry 9K5YPND' waited then exited with '[DEV CONTROL] daemon/watchdog recovery failed; no control endpoint' while reporting recovery action dashboard_ensure. .dev/state.json named daemon PID 28216 and control port 49319; PID 28216 was alive, but Python TCP connect to 127.0.0.1:49319 timed out. The active run remained 20261120_232200 with server PID 33940 and room code 9K5YPND; client-01 PID 24476 was marked stale.


## Agent discussion and suggestions

### 2026-11-21T01:24:49.373Z — root

Message: `msg-d2796f8a90ef` (evidence)

Subject: Observed live dev-loop control endpoint failure while attempting public room-code client

I attempted the supported Python/dev-loop fallback for the user's room 9K5YPND. dev-loop.py --start-client --name 'AI retry 9K5YPND' --json hung through recovery and exited with [DEV CONTROL] daemon/watchdog recovery failed; no control endpoint. The current run remains 20261120_232200; .dev/state.json identifies daemon PID 28216/control port 49319 and run registry confirms server PID 33940 online, room code 9K5YPND, client PID 24476 stale. PID 28216 and watchdog PID 23328 are alive, but TCP connect to 127.0.0.1:49319 timed out. I left the game pair untouched. Please diagnose and recover preserving the active server and existing journals; no public join was performed.

## Agent suggestion

### 2026-11-21T01:50:39.416Z — root

Follow-up evidence after controlled daemon recovery (2026-11-21): stopping verified daemon PID 28216 caused the server/client processes of run 20261120_232200 to exit. Watchdog PID 23328 started daemon PID 30004, but the new daemon state did not re-adopt server-owner PID 33940/room 9K5YPND or client PID 24476. The status became run_id=null and server offline. A canonical --request launch --launch-mode 45 was then accepted as request 2026-11-21T01:38:13.697Z-a806b122 and started replacement run 20261121_013815, still in exe-build setup at >600s when recorded (automatic wait issue issue-054ea1bcda9b). This shows restart can lose the active pair and adoption, and replacement launch has long setup latency; recovery needs stronger preservation checks and post-restart reattachment proof.
