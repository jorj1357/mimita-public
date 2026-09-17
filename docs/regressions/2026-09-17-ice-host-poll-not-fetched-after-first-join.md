// 2026-09-17T03:40:00Z
/* purpose
* TODO regression record: after the first client joins, the ICE host stops
* fetching later join requests and the room expires, so further joins fail.
* Records the evidence and the working assumption that this is NOT the
* coordinator / server.js (unchanged for days); the cause is server-side.
* this file DOES NOT replace docs/regressions/regressions-v1.md
* this file DOES NOT rewrite historical regressions
*/

# Regression TODO: ICE host stops fetching join requests after first client (OPEN)

Status: OPEN / UNRESOLVED TODO

## Occurrence 1 — 2026-09-16 ~22:54 local

### Observed

- A client joins a live room; a later client tries the same room.
- Later client: `[ICE CONNECT] begin-join accepted ... request=...` then
  `[ICE CONNECT] request stale, retrying begin-join`, `[ICE CONNECT] server answer timeout`,
  then `[ICE CONNECT] coordinator ICE begin-join failed error=room-not-found`,
  `[ICE CONNECT] FATAL: all 5 attempts failed`.
- Client log spams `[NET TICK] sock=INVALID_SOCKET state=NatNegotiating connected=0 active=1 transport=0`.

### Expected Behavior

- The host fetches each pending join request and answers it; the room stays alive
  while the host is running; any number of clients can join.

### Actual Behavior

- The host's `host-poll` returns `has_request:false` while pending requests exist,
  the room's heartbeat lapses, the room expires, and later joins fail.

### Confirmed Evidence

- Coordinator log (`/root/.pm2/logs/mimita-coordinator-out.log`), room `4XXAPLG`:
  ```
  [ICE ROOM REGISTER] code=4XXAPLG ...
  [ICE JOIN BEGIN] req=fa2b594d2acc -> [ICE HOST POLL] pending -> [ICE HOST ANSWER] -> connected
  [ICE JOIN BEGIN] req=54f901c47f18 -> (no HOST POLL) -> [ICE REQUEST COMPLETE]
  [ICE JOIN BEGIN] req=e2b5fdfe911c -> (no HOST POLL) -> [ICE REQUEST COMPLETE]
  [ICE JOIN BEGIN] req=c5b8e1a7aa88 -> (no HOST POLL)
  [LIST] rooms=0
  ```
- Server log (`logs/09-16-2026/Server_log_225429.txt`): continuous
  `ICE HOST POLL code=4XXAPLG players=1 hasRequest=0 status=200`, then
  `[SERVER POST-LEAVE] players=0`.

### Why This Is (Probably) NOT the Coordinator / server.js

- `git log -1 -- coordinator-server/server.js` = **2026-09-01**; not touched since.
- VPS file mtimes: coordinator `server.js` = **2026-09-01**, website
  `server.js` = **2026-09-07**. Only gameplay code changed in the days since.
- Deployed coordinator `server.js` SHA256 is **byte-identical** to the repo.
- Synthetic replay of the exact sequence (via `127.0.0.1:3001` and via
  `https://mimita.fun`): `ice/host -> ice/begin-join -> ice/host-poll`, including
  join -> host-answer -> request-complete -> second join -> host-poll, **always
  returns the second request (`has_request:true`)**.

### Working Hypothesis (server-side)

- The dedicated server runs `hotReload.pollAndAdvance(tick)` and
  `tickIceCoordinator(...)` on the same fixed-tick loop (`server.cpp:872`,
  `:972`). The room heartbeat is the host's 500 ms `host-poll`. If the server
  main loop is starved or stalled long enough (heavy gameplay hot-reload churn /
  DLL build CPU saturation / activation), the heartbeat lapses past
  `ROOM_TIMEOUT_MS (30 s)` and the room expires.
- This matches "only gameplay code changed" and the observed generation churn.

### Next Steps

- [ ] Add a heartbeat watchdog + self-heal re-register when the room no longer
  exists, so the server stays joinable without a restart.
- [ ] Run the coordinator poll early in the tick / decouple it from heavy
  gameplay work so it cannot be starved.
- [ ] Instrument heartbeat gaps to confirm the stall.
- [ ] Make the server gameplay code hot-reloadable so iteration no longer stalls
  the loop (see the server gameplay seam phases).
