# issue-d7ea7a00420f: Networking health: ICE send errors, ghost hits, and TDM rejoin timeout with auto-flagged evidence

## Human report (verbatim, immutable)

> d while ICE is not connected
> 11:27:36 ERROR   agent.c:674: Send while ICE is not connected
> ... also unrelated butthis make this an issue add as an issue, leaving tdm to rejoin bc i was gtting ghost hit, like hit at a place that i was not at at that timebut the server still tihnks im there, i rejoin to try and fix and i cant join connection timed out even tho the serv is running waiting for servr acceptance i get stuck on conncetion tiemd out


## Agent suggestion

### 2026-11-27T17:38:30.632Z — rocket-league-input-fix

Evidence from repo scan (agent rocket-league-input-fix). This context is already linked here; the other wake targets should link, not duplicate. Propose narrowing this issue with four concrete points:
1. The log string comes from vendored libjuice: external/libjuice/src/agent.c:674 fires when the ICE agent state is not JUICE_STATE_CONNECTED. The MiMITA caller is IceAgent::send (src/network/ice/ice-agent.cpp:146-159), which guards only null/init and calls juice_send with no state check; the generic ICE SEND failed warning follows. So the MiMITA-side owner is IceAgent::send plus its callers, not the library.
2. waiting for server acceptance is ConnectionState::WaitJoinAccept rendered at src/network/multiplayer-packets.cpp:2313-2314. Stuck there means the client sent join and never received accept/reject before timeout, so the first missing step is after join receipt and before accept. Discriminator: client join.requested vs server network.server.join.received/accepted/rejected vs first snapshot; JoinReject already emits network.client.join.rejected near src/network/multiplayer-tick.cpp:1786.
3. Ghost hits overlap canonical issue-0c39edc4f9b8 (freecam stale position and damage backlog); create/rejoin lifecycle overlaps issue-cf9cd77eb056. Keep this issue as the bounded-health-event and auto-classification owner and cross-link instead of re-implementing those fixes.
4. network.health.* events do not exist under src/ or devscripts/ (rg found none), so the acceptance-required events are the concrete missing diagnostic: bounded, rate-limited ice_send_disconnected, ghost_hit (client vs server position/tick), and rejoin_timeout (with last stage), each carrying run/build/session identity.
Smallest next check: one pinned TDM rejoin run with those three events added at the existing owners, then use the stage discriminator to name the first divergence.

## Agent suggestion

### 2026-11-27T17:39:03.550Z — rocket-league-drive

Routing review (rocket-league-drive, ctx-1948c6362744): the 'ghost hit' symptom reported here is the same failure class as existing issue-0c39edc4f9b8 'Freecam exit leaves stale position and damage backlog; server still resolves hits at the entry position' (paths: src/network/multiplayer-tick.cpp, multiplayer-interpolation.cpp, engine-tick-net.cpp). Recommend treating the ghost-hit part as routed to that owner and keeping this issue focused on the two unowned signatures: bounded network.health.ice_send_disconnected (currently agent.c:674 'Send while ICE is not connected') and network.health.rejoin_timeout at 'waiting for server acceptance'. Acceptance for those should pin a run journal and rate-limit; do not duplicate the freecam reconcile fix. Unknowns: exact client build/generation and the run journal for the 11:27:36 ICE error were not provided, so no runtime proof yet.

## Agent suggestion

### 2026-11-27T17:46:57.249Z — rocket-league-drive

Static trace evidence (rocket-league-drive, read-only, no build/run): (1) ICE send spam owner = src/network/ice/ice-agent.cpp:146 IceAgent::send(), which calls juice_send() every call with only an mAgent/mInitialized guard and no connected()-state guard. libjuice external/libjuice/src/agent.c:674 logs JLOG_ERROR('Send while ICE is not connected') and returns -1 when selected_entry is null, so any caller that keeps sending during Idle/Connecting/Failed floods ERROR. IceTransport::send at src/network/ice-transport.h:29 also does not consult connected(). Bounded fix candidate: in IceAgent::send, if state() is not Connected/Completed, emit one rate-limited network.health.ice_send_disconnected event (include connection state, build/session id) and return false without calling juice_send; keep a counter. (2) Rejoin 'waiting for server acceptance' path lives in src/network/ice/ice-server.cpp join/reconnect flow (~lines 500-640, logs 'no join response'/'join rejected'/'reconnected as player') with the client GUI status from src/network/multiplayer-packets.cpp:1317 'ICE: waiting for server answer...' and the stalled-snapshot 'Waiting for server' path at multiplayer-packets.cpp ~2043. Proposed network.health.rejoin_timeout should carry the stage that stalled (signaling/answer/join-accept/map-ready) using these existing stage names, not a new packet. Not claimed; left unowned for whoever takes networking diagnostics.

## Assignment

### 2026-11-27T17:54:05.738Z — rocket-league-settle

Owner: rocket-league-settle

## Agent suggestion

### 2026-11-27T17:54:20.827Z — rocket-league-settle

Bounded read-only trace from the human logs for this context (no build, no code change).

EVIDENCE (plain-text stdout logs, not structured dev-loop journals)
- Server: logs/11-27-2026/Server_log_110526.txt
  - 42447: `11:27:33 WARN agent.c:1126: Lost connectivity` (vendored libjuice, player 3 Guest1007).
  - 42448-81622: 36,365 x `ERROR agent.c:674: Send while ICE is not connected`, 11:27:33 -> 11:29:35. Per-minute: 7990 @11:27, 17804 @11:28, 10571 @11:29 (~300/s sustained).
  - Stale slot kept during the flood: 43356 `[11:27:36] [SERVER STALE] id=3 ... lastHeard=2593ms ... slot kept for reconnect`; 45666 `[11:27:43] [SERVER PEER LOST] id=3 silent=10027ms`; removal only at 68235 `[11:28:43] [NET DISCONNECT] reason=grace_expired id=3 silent=70039ms`.
  - Server kept re-accepting duplicate JoinRequest for the dead slot: e.g. 42474 `[SERVER JOIN] duplicate join token for existing player=3; skipping validation`, 42476 `[SERVER REJOIN] id=3 ... addr=10.0.0.2:3`, roughly every 0.5s.
  - Rejoin created a duplicate identity: 60758 `[SERVER JOIN] coordinator validated token for Guest1007`; 60759 `[SERVER ACCOUNT] playerId=4 name="Guest1007(2)"`; 60760 `[SERVER PLAYER SPAWN] id=4`. Player 4's ICE then lost connectivity at 68381/68383 `11:28:54 WARN agent.c:1126: Lost connectivity`, same send flood until 81621 `[NET DISCONNECT] reason=grace_expired id=4 silent=70005ms`.
- Client: logs/11-27-2026/Gameterminal_log_105409.txt
  - 689150-689188: leave room, rejoin G98DH3X, fresh ICE established, `[NET CONNECT] join request sent token=fc3b7442af07...`.
  - 689327-690519: same JoinRequest retried 11 times with the same token; no `[NET CONNECT] join accepted` line.
  - 690723 `[NET CONNECT] timeout server=ice:G98DH3X`; 690724 `[NET TEARDOWN] policy=connection-failure reason=Connection timed out` = the exact "waiting for server acceptance" -> "connection timed out".

FIRST DIVERGENCE
The server ICE selected entry for player 3 disappeared at 11:27:33, but the server kept calling juice_send every tick for the whole 70s grace window instead of suppressing sends until the transport recovered or the slot expired. The rejoin then spawned a second identity (`Guest1007(2)`) because the stale slot still owned the old join token/name; the client retried JoinRequest 11x and timed out without a logged join accepted.

SOURCE OWNERS (current tree)
- src/network/ice/ice-agent.cpp:146 `IceAgent::send` calls `juice_send` with no local state gate; on failure it only does `Debug::warn("ICE SEND failed: %d")`. The literal log text is vendored external/libjuice/src/agent.c:674.
- src/network/server-ice.cpp:852 `serverSendToPlayer` -> `sendTransport` has no ICE-connected check, so snapshots/reliable sends keep hitting a dead peer.
- src/network/server-packets.cpp:1908-1918 duplicate-session match uses exact `joinToken`; a new coordinator token on rejoin cannot rebind the stale slot, so a second player/name is created.
- No `network.health.*` event exists today (only network.client.health.waiting, network.connection-state.changed, network.client.reconnect.*), so the three acceptance events are not implemented.

PROPOSED SMALLEST CHECKS (not implemented)
1. IceAgent::send: if `juice_get_state(mAgent)` is not CONNECTED/COMPLETED, emit one rate-limited `network.health.ice_send_disconnected` (state, bytes, suppressed count, build/run id) and return false without calling juice_send. Removes the 36k libjuice ERROR lines and gives the required bounded event.
2. Server: track per-player ICE health from the state callback / first failed send and skip per-tick sends until reconnect or grace expiry; emit bounded `network.health.peer_send_suppressed`.
3. Rejoin policy: either rebind an existing not-dead slot by account/name/address before grace expiry, or wait for grace expiry, instead of spawning a duplicate player.
4. Ghost hits at the old position are the separate stale-body/freecam path in issue-0c39edc4f9b8; keep that distinct from the ICE-send/rejoin work.

CONFIDENCE
- High: all log facts above (exact lines, timestamps, counts, duplicate player id, 11 retries, timeout).
- Medium: why player 4's JoinAccept never reached the client; plain logs do not record `network.server.join.accepted` acceptSent or the transport used.
- Unknown: whether server-side `Lost connectivity` was TURN/network loss or the client tearing down its old agent; the transition does not print the selected candidate pair.

No source, build, or runtime test was performed in this review. Related: issue-0c39edc4f9b8, issue-cf9cd77eb056, issue-55bd775c8026.
