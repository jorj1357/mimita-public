# Shared build unblock and cross-agent signal

## Scope

This session added the automatic cross-agent ownership handshake, opened the
open build-friction issue for the unresponsive owner, unblocked the shared
compile with a declaration-order-only fix, built generation 576, attempted the
next pinned FCS run, and coordinated CPU evidence with `resource-profiler`.

No NPC behavior proof was produced in this session. The two gen576 readiness
attempts failed before a usable active-mode measurement.

## Cross-agent signal

- Added `devscripts/agent-signal.py`: matches board activity by path/build/run,
  asks the likely owner, and broadcasts a response-required message when no
  owner is found.
- Added the CROSS-AGENT SIGNAL rule to `AGENTS.md`.
- Added the "Cross-agent signal handshake" section to
  `docs/specs/dev-loop/agent-coordination.md`.
- Added the tool to `docs/operations/devscripts/script-inventory.md`.
- Live use: the tool matched `melee-parity` for
  `src/network/multiplayer-projectiles.cpp`. An ownership question and the
  exact fix were sent; no reply arrived.

## Open issue

Created `issue-d75f928fbd6f` (build-friction, owner melee-parity): the shared
batch failed at `src/network/multiplayer-projectiles.cpp:443` because
`projectileTrailConfigFor` is defined at `:513`. The issue records the exact
build attempt, the no-response state, the minimal declaration-order fix, and
the acceptance.

## Unblock and build

- User authorized the minimal unblock. Applied a forward declaration before the
  first use; declaration order only, no behavior change.
- Build note `d57339951817`.
- Build attempt `game-dll-20261124T003901444604Z-3c8d4e` succeeded in 949.243 s,
  completed 2026-11-24T00:54:50.688Z, published generation 576, SHA-256
  `06f3229efcd629dd82e0ce017bd9ef4ddaa08ae92c24cf7e6e446cbb143c9eb5`.
- The batch also compiled many other owners' files; the declaration-order fix
  was only one input.

## Generation 576 readiness attempts

- Run `20261124_005506`: test client exited before active mode after reaching
  phase 4 and receiving NPC visuals. No `network.mode.active`; no behavior
  proof.
- Run `20261124_005656`: the new health gate reported `server stalled before
  active mode: tick 308->1083 in 51.8s, max_tick_stall_ms=409.3,
  over_100ms_percent=5.7`. This confirms the readiness classification works,
  but no gameplay proof.
- No concurrent isolated run was left running. Both attempts failed cleanly.

## CPU evidence coordination

`resource-profiler` supplied Juggernaut Horde evidence from preserved run
`20261123_195307` (67 NPC/1 player, server PID 22184):

- `performance.server-tick-window`: sample 15, total avg 59.232 ms,
  p95/p99/max 131.759 ms, NPC avg 53.901 ms/max 129.369 ms, 26.7% of sampled
  ticks over 100 ms.
- `npc.phase-window`: 1,027 updates, total 459.736 ms, physics 239.569 ms,
  collision 141.699 ms, decision 39.155 ms.
- Callsite estimates: `updateOneNpc` at `src/npc/npc.cpp:1580` 12.40% server
  CPU (~3.043% host); `doCollisions` at
  `src/physics/movement/physics-collision-dispatch.cpp:181` 5.33% (~1.308%
  host). These are partial 1/64 estimates, not full attribution.

Recorded in `docs/features/npc-behavior.md` with links to
`issue-be1adb51b6ac` and `issue-ed3ac775283d`.

## Evidence and limits

- Build success is compilation only.
- Readiness failures are not gameplay failures; the second one was explicitly
  classified as `server_stalled`.
- No defuse, stuck, speed, TTK, or kills-per-round claim is made for gen576.
- Header H1/H2 remain deferred.

## Next steps

1. Retry gen576 on a free host and reach active mode.
2. Measure defuse starts per plant, stuck, speed, dash/bhop/air, TTK, and
   kills/round versus gen571.
3. Keep the cross-agent signal tool in the normal flow for any future
   unresponsive owner or shared-batch conflict.

## Additional gen576 attempt: client never applies active phase

Run `20261124_010043` connected and received NPC snapshots, but the client
journal stopped at `network.mode.start` phase 4 (`stage=phase`) at t=8.14. The
server reached `round.go` at t=23.35 and `round.active` at t=24.13. The runner
advanced 2407 server ticks and reported `client did not observe active mode`;
max tick stall was 781.6 ms. Gen571 worked with the same runner and mode.

Opened `issue-d2a8614db704` (bug, network-mode-replication). Signals were sent
through `agent-signal.py` to `melee-parity` (matched `multiplayer-tick.cpp`),
`hot-reload-latency-root`, and `damage-lightning-backlog`. No owner reply yet.
This is a shared network-mode replication/readiness blocker, not an NPC
behavior result.
