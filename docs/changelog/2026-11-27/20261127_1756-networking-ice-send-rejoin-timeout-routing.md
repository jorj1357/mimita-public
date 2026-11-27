# Networking health: ICE send flood and TDM rejoin timeout routing

Written 2026-11-27 by `rocket-league-settle`. Coordination and read-only log
tracing; no source or config file was edited and no build or run was started.

## Context routed

High-priority user context `ctx-1948c6362744` was already linked to
`issue-d7ea7a00420f` ("Networking health: ICE send errors, ghost hits, and TDM
rejoin timeout") by `anim-socket-crouch`. I acknowledged the context, assigned
the issue to `rocket-league-settle`, and appended the evidence below with
`issue suggest`. No duplicate issue was created.

## Evidence located

- `logs/11-27-2026/Server_log_110526.txt`
  - line 42447 `11:27:33 WARN agent.c:1126: Lost connectivity` (vendored
    libjuice, player 3 `Guest1007`).
  - lines 42448-81622: 36,365 `ERROR agent.c:674: Send while ICE is not
    connected` from 11:27:33 to 11:29:35 (7990 @11:27, 17804 @11:28, 10571
    @11:29), about 300 per second sustained.
  - the slot stayed alive through the flood: line 43356 `[SERVER STALE] id=3
    ... lastHeard=2593ms`, line 45666 `[SERVER PEER LOST] id=3 silent=10027ms`,
    removal only at line 68235 `[NET DISCONNECT] reason=grace_expired id=3
    silent=70039ms`.
  - rejoin spawned a duplicate identity: line 60759 `[SERVER ACCOUNT] playerId=4
    name="Guest1007(2)"`, with the same send flood until line 81621
    `reason=grace_expired id=4 silent=70005ms`.
- `logs/11-27-2026/Gameterminal_log_105409.txt`
  - lines 689150-689188: leave, rejoin G98DH3X, fresh ICE established, join
    request token `fc3b7442af07...`.
  - lines 689327-690519: 11 identical join retries with no `join accepted`.
  - lines 690723-690724: `[NET CONNECT] timeout server=ice:G98DH3X`, then
    `[NET TEARDOWN] policy=connection-failure reason=Connection timed out`.

## First divergence and owners

`src/network/ice/ice-agent.cpp:146` (`IceAgent::send`) calls `juice_send` with
no local state gate, and `src/network/server-ice.cpp:852` (`serverSendToPlayer`)
has no ICE-connected check, so the server kept sending snapshots and reliable
events to a dead peer for the whole grace window. The rejoin used a fresh
coordinator join token, so the exact-token match in
`src/network/server-packets.cpp:1908-1918` could not rebind the stale slot and
created `Guest1007(2)`. No `network.health.*` event exists in the tree yet.

The ghost-hit half of the report is the separate stale-body/freecam path in
`issue-0c39edc4f9b8`; it was not merged into this issue.

## Proof gap

No `network.health.ice_send_disconnected`, `network.health.ghost_hit`, or
`network.health.rejoin_timeout` event exists, so the issue acceptance is unmet.
The proposed smallest fix is a rate-limited `IceAgent::send` gate plus server
send suppression on dead ICE; it was recorded on the issue and not implemented
because the single-flight build was busy and any behavior claim still needs a
pinned rejoin reproduction. Confidence: high on the log facts and source owners,
medium on why player 4's JoinAccept was not received, since the plain logs do
not record `network.server.join.accepted` acceptSent or the transport used.
