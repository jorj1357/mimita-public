# ICE packet receive metrics parity

- Task: Advance the canonical end-to-end connection reliability issue `issue-cf9cd77eb056` with an observability fix.
- Evidence: In pinned historical TDM run `20261120_152743` (EXE build `2026-11-20T07:47:09.981634Z`, game DLL generation 404, client PID 34144), `network.metrics.window` at 15:28:01.158Z reported `snapshots_received_total=207` and `packets_received_total=0`; a later window reported 325 snapshots and still 0 packets. The same journal has complete `network.client.snapshot.applied` events.
- Cause: `mpTick` increments `ctx.packetsReceived` in the raw UDP receive loop, but the generic transport poll branch did not increment it before packet impairment/dispatch. ICE snapshots were therefore processed while packet input metrics stayed zero.
- Change: `src/network/multiplayer-tick.cpp` now adds the number of datagrams returned by `ctx.transport->poll(pkts)` to the same counter before `badconn::processIncoming`, matching the raw path's pre-filter counting semantics. No new per-packet logging or allocation was added.
- Coordination: Build note `b2a0a8bbd450` was queued before saving the compiler input. Added umbrella task `ice-packet-receive-counter` and sent the source/runtime evidence to the active issue owner.
- Build: Root dev-loop status at 17:49:57Z showed source generation 24, existing generation 408 still building from source generation 23, and automatic request `auto-24` queued. No build of this edit is complete yet.
- Validation: Source diff reviewed. No synthetic test added. Matching pinned ICE runtime proof remains pending; do not claim metric correction as runtime-proven until `network.metrics.window` reports nonzero receive packet totals/deltas in a run on the matching generation.
- Scope limit: This corrects observability only. It does not fix reconnect delivery, establish the cause of the TDM packet gap, or complete the lifecycle, HUD, Payload NPC visual, performance, or reliability-soak requirements.
