// 2026-11-22 03:07 UTC
/* purpose
* record the evidence-backed diagnosis of the in-game reconnect loop
* separate client load stalls from the mid-game server snapshot starvation
* name the next diagnostic and fix owner without editing game source
* this file DOES NOT replace the feature record or claim human acceptance
*/

# In-game reconnect loop is server NPC-phase collapse, not client stalls

# Task

- Task ID: wake-6b54f4d66b2c
- Session ID: session-ad9f55b738e4
- Owner: codex-ingame-gui-reconnect
- Focus: user context `ctx-879a2e9d2b03` — "make diagnostics and run them and get as clear of an idea of what to do based on evidence" for the in-game reconnect/disconnect loop.
- Status: diagnostics-only. No game source edited.
- Date, time, timezone: 2026-11-22T03:07:00Z
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 07ba9003a6be33556ff350eb97be042fd18a99dc
- Final commit: none (not committed)

# Pre-existing changes

- Other agents had already modified `src/network/server.cpp`, `src/npc/npc.cpp`, `src/network/reliable-gameplay-events.cpp`, and the dev-loop scripts, and had published game generation 441 and a newer EXE. This session only added this changelog and appended coordination evidence.

# Requested behavior

Stop the in-game "slow down, reconnecting, disconnected" loop and make the first failing step evidence-based. The user asked for diagnostics to be run so the next fix is chosen from evidence instead of another reconnect-state branch.

# Evidence

Pinned isolated run `20261122_021421`: EXE `2026-11-21T02:26:50.306117Z`, game generation 440, TDM on `atdm`, 1 connected player, 9 NPCs. Journals: `logs/2026-11-22/20261122_021421/{server-01,client-01}/events.jsonl`.

## Timeline

- Server loop is ~60 Hz from startup until `02:21:46`.
- At `02:21:47` the NPC phase expands: `decision_ms=429.8`, `navigator_ms=406.5`, `sense_ms=78.7`, while every NPC reports `stuck:true`; `trace_containment_ms` reaches 261-280 ms.
- Server loop then falls to 4.6 Hz and stays ~0.1-4.6 Hz until `02:25:20`.
- `performance.server-loop-window` records one outer iteration of `52,825 ms` (`loop_max_ms=52,822`, `tick_count=5`) at `02:24:06`, then `6,119 ms` at `02:25:05`, then recovery to 60-84 Hz at `02:25:21`.
- `performance.server-tick-stall` appears 337 times. The largest fixed-tick stalls are NPC-phase dominated with only 9 NPCs: tick 22160 `npc_ms=2395` (total 2459), tick 22162 `npc_ms=1755` (total 2095), tick 22174 `npc_ms=2159` (total 2337). `snapshot_ms` stays 0-100 ms, so snapshot serialization is not the stall.
- Client `performance.stall-watchdog` appears 15 times and only at load: `02:15:08`-`02:15:25`, frame 1-2 (`Audio`, then `State`), longest 11.66 s; last stall `02:16:05` at frame 4987. There is no client stall after `02:16:05`.
- At `02:24:09`-`02:24:16` the client keeps logging `network.client.snapshot.gap` once per second while `last_snapshot_tick` stays frozen at 22165 and `last_heard_age_ms` stays 80-745 ms. Client transitions `WaitingForServer -> Reconnecting` at `02:24:16`, and `Reconnecting -> Connected` at `02:24:56`, exactly when the stalled server tick completes.
- Server-side stale logs in the same window (`02:24:04`, `02:24:58`, `02:25:02`) are consistent with a server main thread too busy to process input, not a client send gap; the client was still receiving and logging during the freeze.

## Reliable-event path (verified working)

- 225 `network.server.reliable-event.delivery-degraded` events with `action=keep-connection`, `slot_state=held`, `players_after=1` from `02:26:58`.
- Exactly one removal: `network.server.timeout.expired` at `02:27:55.787`, `silent_ms=70028`, `removal_reason=reconnect_grace_expired`. No early erase near 12.5 s.

## Data caveat

- `network.server.snapshot.build/send` and `network.client.snapshot.received` are diagnostic-cadence events (about one per `snapshotDiagnosticsIntervalMs`), not the actual per-tick send rate. Reconnect reasoning must use `snapshot_age_ms`, not these event counts.

# Diagnosis

Two separate failures were previously conflated:

1. Mid-game reconnect loop: the authoritative server collapses to 0.1-4.6 Hz because the NPC phase takes hundreds of ms to seconds per tick when the 9 TDM NPCs become `stuck:true`. Snapshot production then starves past the 2500 ms client threshold, the client enters `WaitingForServer` then `Reconnecting`, and it reconnects when the server tick finally completes. This is server-side, not client input/heartbeat loss.
2. First-join delay: the client has 1.5-11.66 s main-thread stalls at frame 1-2 (`Audio`/`State`/`Rendering`) during initial load. This delays join but does not cause the mid-game loop.

Residual unknown: the 52.8 s outer-loop iteration is not explained by the measured tick phases (5 stalled ticks ≈ 6 s), so roughly 46 s sits in outer-loop work between fixed ticks (packet drain, `endFixedTick`, ICE poll, `fflush`, sleep) or in an unmeasured section.

# Recommended next steps

1. Bound server NPC navigation/decision/trace-containment work when NPCs are stuck or clustered so one fixed tick cannot take hundreds of ms to seconds; preserve 60 Hz snapshot production. This also reduces CPU and log volume.
2. Add one bounded `performance.server-loop-stall` diagnostic when a single outer-loop iteration exceeds 2 s, carrying packet-drain count, fixed ticks, cumulative npc/projectile/snapshot/contact/gamemode ms, ICE poll ms, `endFixedTick` ms, and sleep ms; rerun the pinned TDM scenario and name the first dominant stage.
3. Treat the client first-frame load stalls (`Audio`/`State`/`Rendering`) as a separate join-latency task.
4. Keep the reliable-event `keep-connection` behavior; do not add another reconnect-state branch.

# Validation and limits

- Read-only analysis of the pinned run journals and source paths. No build, no source edit, no process mutation, no human acceptance claimed.
- A fresh isolated run on current source was blocked: `--start-test-run` reported the pinned gameplay DLL source generation was not current while root dev-loop generation 443 was still building (`game-dll-20261122T030300829638Z-7fb0c2`, `source_snapshot` started `03:05:44`).

# Coordination

- Evidence appended to `issue-cf9cd77eb056` and `issue-ed3ac775283d`.
- Replied to `msg-df110dd4dead` (website-deploy-authority) confirming the keep-slot fix and correcting the client-stall causation.
- Acknowledged `ctx-879a2e9d2b03`.
- Left `src/network/reliable-gameplay-events.cpp` untouched.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `docs/changelog/2026-11-22/20261122_030700-in-game-reconnect-npc-stall-diagnosis.md` | absent | this record | Required session changelog | delete file |
