# Networking Cutover-1 finish handoff (2026-11-17)

- Status: active handoff for the next agent.
- Owner: networking hot/cold split and Cutover-1 networking acceptance.
- Checkout: `C:\mimita-v9`, current shared checkout.
- Authority: `AGENTS.md`, `docs/ROUTER.md`,
  `docs/specs/hotreload/hotreload.md`,
  `docs/specs/networking/networking.md`.
- This is coordination documentation, not a build, runtime, or acceptance claim.

## Do not overwrite

The working tree contains concurrent work from other agents: new hot policy
files, resident `net-core-primitive`, transport ABI v5, `ServerPlayer` handle
changes, and perf changes. Preserve all unrelated edits. Do not reset, clean, or
rewrite another agent's files.

One writer per file. Do not run a compiler, `build.py`, `build_agent.py`, or a
second dev-loop. Save source and let the root dev-loop build.

## What this session changed

Networking event and config work:

- `src/network/network-events.h` — common event envelope, config revision,
  generation fields, ping/RTT fields, event-rate limit.
- `src/config/networking-config.*`, `config/networkingconfig.json` —
  `network_observability`, `hot_swap`, and `ice_policy`.
- `src/network/multiplayer-packets.cpp` — reconnect scheduling, attempts,
  terminal/rejected events, health rebase, metrics window.
- `src/network/multiplayer-tick.cpp` — join/map/spawn/transport/disconnect
  events, snapshot events, client metrics.
- `src/network/server-packets.cpp`, `server-packet-chat.cpp` — server join,
  reconnect, timeout, spawn, snapshot, metrics events; canonical renames.
- `src/engine/engine-tick-net.cpp`, `engine-tick-state.cpp` — map/connect events.
- `src/engine/whole-game-entry.cpp` — first tick and queue-drain event.
- `src/host/host-runtime.cpp` — generation swap begin/end/suspend/resume events.
- `src/game/transport-bridge-module.*` — query resident swap state.
- `devscripts/network_trace.py` — journal trace/assertion tool.
- `docs/specs/networking/network-event-contract.md` — exhaustive event table.
- `docs/specs/dev-loop/dev-loop.md` — one-stack/windowless-command contract.
- `config/source-owners.json` — clarified resident networking mechanisms now
  delegate policy to game-owned policy files.
- `src/network/server-ice*`, `server.cpp` — removed resident policy fallback
  defaults where practical; ICE policy values now come from `ice_policy`.

## Current build/runtime state

- Latest successful open server is PID `12272`, run `20261117_170234`, built
  `2026-11-17T17:02:29.218920Z`.
- Game generation `328` is published.
- The open client `5152` is on the older artifact; it did not reach a
  new-generation connection.
- Current EXE build is not green. Last observed link failure referenced a stale
  `build/obj-debug/network_server.o` with `NetworkingConfig::instance()`; the
  source include was removed and that object was rebuilt. Verify a fresh link
  before treating it as still broken.
- The dev-loop status can remain `building`/`failed` while no compiler is
  running. A stale `.dev/build-serialization.lock` has already caused this
  twice. Check the lock PID before removing it; remove only when the PID is dead.
- `build/hot-reload/status.json` is stale (generation 294/older), and
  `safe-unload-gate.json` is from generation 177. Do not use either as current
  Cutover-1 evidence.

## Remaining work

### 1. Get a fresh green build

1. Check `python devscripts/dev-loop.py --status`.
2. If a build lock exists, verify its PID is dead before removing it.
3. Save source or use the dev-loop control path; do not invoke a compiler.
4. Confirm the EXE and game generation published, then rerun the build-target,
   cold-link, and exe-cutover gates.

### 2. Finish the networking hot split

- `src/network/server-ice.cpp` still contains the resident ICE agent/session
  mechanism and some orchestration. It now calls hot decisions for timeouts,
  admission, drain limits, and session publication. Decide what remains
  mechanism and move the rest to the game-owned policy path.
- `src/network/server.cpp` still contains room/session orchestration and
  resident fallback text. Its `decideSessionPolicy` adapter now delegates to the
  hot decider; remove any remaining resident policy/defaults.
- `src/network/ice/ice-agent.cpp` still filters/selects candidates and applies
  relay policy. Move negotiation/filtering policy to a hot policy decision or
  document it as config-driven primitive behavior.
- `src/network/server-ice-policy.cpp` now reads `ice_policy` from
  `networkingconfig.json`. Confirm no timeout, admission, or poll value remains
  hardcoded in networking policy.
- Update `docs/specs/hotreload/hotness-matrix.md` and the source-owner reasons
  only after the split is real.

### 3. Client transport continuity

- `src/host/net-core-primitive.*` is currently a resident registry/mechanism.
  There is no real client pump/queue drain, and no client registration across a
  generation swap.
- Implement the client resident transport/session handle path or explicitly
  keep this as an open Cutover-1 gap. Do not claim `queue_drained` is proven.
- `network.hotswap.queue_drained` currently reports the resident swap snapshot
  and remains `reason_code=resident_pump_not_installed`.

### 4. Runtime proof

Use the real dev-loop and exact journals, not a new synthetic self-test:

- join by room code; identify client/server generation and PID;
- reconnect after a real drop: configured 6s attempt cadence, 60s grace,
  bounded terminal reason;
- intentional DLL swap: `swap_begin → suspend → old_shutdown → new_init →
  first_tick → first_snapshot → resume → health_rebased → queue_drained`;
- no reconnect during the intentional swap;
- no stale callback/job/resource after unload;
- rollback restores the previous generation and state;
- server remains alive and authoritative.

Required events:

`game.generation.swap_begin`, `.candidate_loaded`, `.old_shutdown`,
`.new_init`, `.first_tick`, `.first_snapshot`, `.swap_end`,
`network.hotswap.suspend`, `.resume`, `.health_rebased`, `.queue_drained`,
`network.metrics.window`, `network.client.reconnect.attempt`,
`network.server.reconnect.received/lookup/accepted/rejected/expired`,
`network.session.health`, `network.server.snapshot.build/send`,
`network.client.snapshot.received/applied/gap/recovery`.

Run:

```powershell
python devscripts/network_trace.py --run <run-id>
```

It validates attempt spacing, grace/terminal ordering, swap reconnect
suppression, and server rejection evidence.

### 5. Cutover-1 acceptance

- `gameplay_live_fix_proven` is still false; it is hardcoded false in
  `devscripts/hot_reload_status.py` and needs real same-PID/session evidence.
- Regenerate source manifest, audit, build gates, safe-unload, and status from
  the current revision.
- Human review is still required for visible play, room join/reconnect UI, and
  no false connected/disconnected state.

## Relevant files

- `src/network/server-ice.cpp`
- `src/network/server-ice-policy.*`
- `src/network/server-session-policy.*`
- `src/network/server.cpp`
- `src/network/ice/ice-agent.cpp`
- `src/host/net-core-primitive.*`
- `src/host/host-server-transport-adapter.cpp`
- `src/host/host-server-entity-adapter.cpp`
- `src/host/host-runtime.cpp`
- `src/host/abi/transport-capability.h`
- `src/host/abi/game-server-abi.h`
- `src/network/network-events.h`
- `devscripts/network_trace.py`
- `build/hot-reload/status.json` (stale until regenerated)
- `build/hot-reload/safe-unload-gate.json` (stale until regenerated)
