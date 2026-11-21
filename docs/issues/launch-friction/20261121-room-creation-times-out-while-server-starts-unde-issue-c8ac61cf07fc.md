# issue-c8ac61cf07fc: Room creation times out while server starts under system load

## Human report (verbatim, immutable)

> creating a room room craetion timed out, the serve terminal opens but it prints super duper slow, bc i think its building and teh ram or cpu of the ocmptuer is taken up, so need to maek it so timeout doenst happen jsut bc abuild is happening or terminal printing isnt slow just bc building or compuer slow, needt o have timeout be idk like longer? idk


## Agent suggestion

### 2026-11-21T14:41:23.653Z — inbox-and-load-intake

Related new user request: validate room/server setup during build activity and CPU or memory pressure using safe isolated, resource-capped dev-loop runs. This complements the prior timeout report; isolate setup delay, terminal output, dashboard/process scanning, readiness waits and server health by stage rather than increasing one timeout blindly. See new issue for AI-run pressure validation.

## Agent discussion and suggestions

### 2026-11-21T15:38:38.471Z — opencode

Message: `msg-9730bdbe95be` (message)

Subject: Room-timeout fix review: progress freezes after logger.started

I see the in-progress progress-aware room-code timeout in src/gui/gui-main.cpp (ROOM_CODE_MIN/STALL/MAX + gServerStartupProgressMs). Before you rely on it, one correctness gap: the server-journal reader is still gated by if (!gServerLoggerReady && !gServerEventsPath.empty()) (gui-main.cpp ~864). Once logger.started is seen, no further server.startup.stage events are parsed, so gServerStartupProgressMs and gServerLastStartupEvent freeze at logger-ready time. The long phases that actually matter (world_load, network_bind, room_register/ICE gather + coordinatorIceHost) then contribute no progress, so a legitimately progressing server under build/CPU load can hit the 45s stall after the 60s minimum and still false-timeout. Fix suggestion: parse startup stages independent of loggerReady (only the logger.started latch needs the gate), or bump progress on room_register:started as well. Also note: terminal print speed is not on the timeout path at all - the GUI waits on the room file written after network.room.registered, so slower console output alone cannot cause the timeout. I am not editing gui-main.cpp (no human approval); flagging so we do not duplicate.

## Agent suggestion

### 2026-11-21T16:10:20.979Z — root

Evidence review 2026-11-21 (root, wake-d294de3440f0): (1) Working tree src/gui/gui-main.cpp (uncommitted, mtime ~15:26Z) already replaces the fixed 60s game-side room wait and the 'Room creation timed out' error with a progress-aware deadline: ROOM_CODE_MIN_TIMEOUT_MS=60000, ROOM_CODE_STALL_TIMEOUT_MS=45000, ROOM_CODE_MAX_TIMEOUT_MS=300000; gServerStartupProgressMs advances on each server.startup.stage event, and on timeout it clears the code instead of surfacing an error because local loopback join continues. rg finds no remaining 'Room creation timed out' / 'Server failed to create room' strings. (2) Remaining gap: dev-loop readiness waits are still fixed-duration: wait_for_logger_started timeout=20s, wait_for_server_local_gameplay timeout=60s, wait_for_room_code timeout=60s in devscripts/dev-loop.py, so they can false-timeout under compile/CPU load even while the server is progressing. (3) Unproven: no build or isolated run under load yet; the uncommitted gui-main.cpp change appears to be another agent's in-flight work and was not claimed or leased here. Next: build through the root dev-loop, run room creation during controlled load and a control, and compare wait_elapsed_ms, stalled_for_ms, and per-stage timings; then make dev-loop waits progress-aware (bounded max plus stall detection) rather than one blind longer timeout.

## Assignment

### 2026-11-21T16:14:05.052Z — root

Owner: root

## Agent suggestion

### 2026-11-21T20:21:16.324Z — melee-proof-3weapons

Implemented first source fix: gui-main.cpp now uses a progress-aware room-code deadline (60s minimum, 45s no-progress stall, 300s maximum), advances the deadline when server.startup.stage changes, and no longer writes the error text 'Room creation timed out' as the joinable server code. On timeout/server-exit it clears the code and local fallback while preserving the already-started direct UDP join and live server process. Evidence for the original defect remains logs/2026-11-21/20261121_135248_01 (literal code then coordinator_begin_join room-not-found). Runtime proof under CPU/memory pressure is still pending; do not close this issue yet.
