# REG: Cutover-1 was treated as complete with networking policy still resident

Date: 2026-11-17  
Time last updated: 2026-11-17  
Status: CONFIRMED — specifications corrected; network source split and live proof pending  
Owner: hot-reload boundary / networking ownership  
Related changelog: `docs/changelog/2026-11-17/20261117_115247-networking-hot-boundary-cutover-regression.md`

## Symptom

Cutover-1 was treated as working because the EXE linked, a game generation
activated, packet paths reported `path=dll`, and ordinary gameplay could run.
That was not enough. Several networking files still contain MiMITA policy and
are owned as `resident`, so parts of join, ICE negotiation, ICE listener
behavior, peer lifecycle, and server session orchestration still require an EXE
rebuild and process restart to change.

The gap was small enough to look like a file-ownership detail. It is not small
for the goal: a networking bug in those paths cannot be fixed live.

## Expected behavior

Cutover-1 requires every ordinary behavior source to be hot except the named
cold core, and specifically requires join, reconnect, session, packet,
simulation, publish, gamemode, and error paths to be hot
(`docs/specs/hotreload/hotreload.md:51-71`).

The networking specification is more explicit:

> “All MiMITA networking code is hot and replaceable. Only external mechanisms
> are cold: libjuice and its callbacks/threads, OS socket/WSA syscalls, OS
> timers, and process/window/loader lifetime.”
> `docs/specs/networking/networking.md`

A resident adapter may keep the socket, ICE agent handle, callback thread,
timers, and raw event queue. It must not keep game policy or protocol decisions.

## Actual behavior

At source-manifest revision
`c3163e7580943a09cdce2c9ba215ba3d1f9042fa7d264ef96433bfc0db7cb50d`, the
cold guard reported:

```text
COLD  src/network/server.cpp
COLD  src/network/server-ice.cpp
COLD  src/network/ice/ice-agent.cpp
COLD  src/network/ice/ice-server.cpp
COLD  src/network/net_common.cpp
HOT   src/network/multiplayer-packets.cpp
HOT   src/network/coordinator-client.cpp
```

The source manifest gives all five cold files the same owner reason: “socket,
ICE, and per-session server loop.” That reason hides the mismatch:

- `src/network/server.cpp` contains the scheduler/pump, which is allowed to be
  cold, but also session and lifecycle decisions that belong hot.
- `src/network/server-ice.cpp` contains ICE listener publication, coordinator
  ICE completion, pending-peer admission/expiry, peer lifecycle, player
  mapping, and send policy. Those are policy, not only primitive calls.
- `src/network/ice/ice-agent.cpp` contains the allowed libjuice adapter, but
  also candidate filtering and state policy.
- `src/network/net_common.cpp` contains pure address and map-id helpers that are
  duplicated by the hot `src/game/net-common-module.cpp`.
- `src/network/ice/ice-server.cpp` is an ICE CLI probe path, not normal
  gameplay networking.

The game-side ICE and transport bridges already avoid passing libjuice pointers
or socket ownership into the DLL:

- `src/game/ice-session-bridge-module.cpp` holds an opaque host agent handle.
- `src/game/transport-session-bridge-module.cpp` calls resident session
  start/stop/tick operations.
- `src/game/socket-bridge-module.cpp` uses the resident socket capability.

That is the right direction, but the game-side proxy currently destroys its
host agent on unload. A DLL swap can therefore end an otherwise healthy ICE
connection instead of reattaching to the resident agent.

## First missing or incorrect step

The first missing step was treating a file-level cold owner as proof that the
whole file was correctly cold. No one separated immutable mechanism from
editable policy inside `server-ice.cpp` and the ICE adapter, and no acceptance
step proved that a live connection survives a generation swap by reattaching
to a resident agent.

The incorrect assumption was:

> The network links, the packet path is in the DLL, and a swap worked; therefore
> networking is hot.

A correct claim needs:

- each changed owner resolves to `game`;
- the behavior event proves the DLL path;
- the same PID and session survive;
- resident primitives are the only cold exceptions;
- the connection is not silently recreated by an unload.

## Why it looked complete

Real work was completed and should not be dismissed:

- the game DLL activates in the same process;
- client and server packet handlers are game-owned;
- room registration and join produce runtime evidence;
- `player_core` v1→v2→v3 migration and rejection are proven;
- the cold-link trial reached zero undefined symbols;
- repeated generation swaps worked for ordinary hot behavior;
- packet `path=dll` events exist.

Those prove a large part of Cutover-1. They do not prove that every networking
decision was moved out of resident files.

## Impact

Until the split lands:

- ICE negotiation bugs may require an EXE rebuild/restart.
- ICE listener, peer-admission, peer-expiry, and room-publication bugs may
  require an EXE rebuild/restart.
- Some join/reconnect/session roots may be cold even when the visible symptom
  is in a hot packet handler.
- Network performance work inside `server-ice.cpp` cannot be iterated live.
- A live DLL swap can force a reconnect by destroying the host ICE agent.
- Cutover-1 cannot be claimed honestly for networking.

The next reconnect or networking failure must still be traced to its first
incorrect step. It is not automatically cold just because it involves ICE, and
it is not automatically hot just because it is a “networking” bug.

## Corrective action

1. **Normative boundary:** record that ICE negotiation and ICE listener policy
   are hot, while libjuice, OS sockets, timers, and the ABI are cold.
   Completed in the networking, hot-reload, and hotness-matrix specs.
2. **Ownership audit:** use the compiler dependency graph and cold guard to
   separate policy from primitive code.
3. **Split ICE:** keep a resident agent/socket/callback/queue adapter; move
   SDP, candidate, retry, timeout, listener admission, publication, and peer
   mapping policy into the game generation.
4. **Preserve the connection:** retain the resident agent across generation
   unload and reattach by stable connection/session ID. Do not call
   `juice_destroy` for an intentional swap.
5. **Split the server:** keep the scheduler/pump cold; move session
   orchestration, accept/reject, peer lifecycle, player mapping, and send policy
   hot.
6. **Clean up:** share or hot-own pure `net_common` helpers; classify
   `ice-server.cpp` as a tool; keep `udp-echo.cpp` diagnostic.
7. **Prove it:** same-PID live edit, repeated swaps while connected, no
   swap-caused `Reconnecting`, join/reconnect evidence, and human review.

Execution plan and file ownership:
[`docs/handoffs/networking-hot-boundary-split.md`](../../handoffs/networking-hot-boundary-split.md).

## Prevention

- A source-owner file is not proof that the whole boundary is correct.
- A successful build, zero cold-link symbols, or one successful swap is not
  proof that all networking policy is hot.
- Every cold entry must name its owner, reason, hot replacement target, and
  removal gate.
- Use `devscripts/dependency_graph.py` to find hot-to-cold include edges and
  `devscripts/cold_change_guard.py` before editing.
- Do not add a resident duplicate as a fallback when a hot path is missing.
- Do not claim Cutover-1 networking until ICE listener/negotiation policy and
  server session policy are game-owned or the specification is deliberately
  changed by a human.

## Current status

Specification and regression record: corrected in this change.  
Network source split: not yet implemented; networking owner work remains.  
Live connection-survives-swap proof: pending.  
Human acceptance: pending.
