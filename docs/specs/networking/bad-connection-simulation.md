# Bad-connection simulation

**Status:** desired behavior contract for per-client network impairment testing.

## Purpose

Let a developer reproduce realistic last-mile network problems on one machine
and verify that prediction, reconciliation, interpolation, reconnection, and
gameplay recovery hold up. The simulator is per executable, not a second
server or a global network emulator.

## Where the impairments come from

Each impairment approximates a real condition:

| Impairment | Real-world source |
|---|---|
| Latency | physical distance, routing detours, mobile RAN delay |
| Jitter | queuing, radio scheduling, congestion, handoff |
| Loss | bad Wi-Fi, congested uplink, lossy transit, interference |
| Burst loss | Wi-Fi retransmit storms, brief congestion |
| Reorder | multipath, route changes, queue scheduling |
| Duplicate packets | network duplication, retransmission artifacts |
| Blackout | handoff, tunnel, suspend/resume, route withdrawal |
| Periodic latency spike | bufferbloat, retransmit recovery, background load |
| Bandwidth cap | mobile throttling, saturated uplink, shared hotspot |

These are synthetic client-side conditions. They do not simulate server CPU
pressure, coordinator outages, or an ISP's exact behavior.

## Model

Each `mimita.exe` owns its simulator. Impairments apply per direction:

```text
in    server -> this client
out   this client -> server
both  both directions
```

Supported fields:

- `latency`: `base_ms` plus correlated `jitter_ms` (or legacy `min_ms`/`max_ms`).
- `loss`: `min_percent`/`max_percent` plus optional
  `burst_percent`/`burst_probability`.
- `reorder`: `min_percent`/`max_percent` within `window`.
- `blackout`: probability per second, min/max ms, cooldown, direction.
- `duplicate`: percent of packets delivered more than once.
- `latency_spike`: interval, duration, extra latency, direction.
- `bandwidth`: bytes per second cap with queued/dropped sends per direction.
- `seed`: deterministic impairment sequence for repeatable tests.

Activation:

- Launch flag: `--badconn-preset <id>`.
- Live terminal in that client: `badconn <id>` / `badconn 0`.
- Dev loop per-client: `--start-client ... --badconn-preset <id>`.

The simulator must never affect coordinator, ICE, STUN, or TURN control traffic.

## Presets

Existing presets 1–10 stay valid. Required additions:

| ID | Name | Approximates |
|---|---|---|
| 11 | Asymmetric uplink | slow home uplink, fast downlink |
| 12 | Duplicate packets | duplication in transit |
| 13 | Periodic latency spikes | bufferbloat / retransmit recovery |
| 14 | Bandwidth cap | mobile throttling |
| 15 | High jitter only | unstable radio scheduling |
| 16 | Worst combined | latency + loss + reorder + duplicates + spikes |

Preset values live in `config/networkingconfig.json` under
`badconn.presets`. That file is the single active source. Preset edits hot-load.

## Required evidence

Every impaired client must write bounded records:

- `network.snapshot.timeout` when authoritative snapshots stop.
- `network.session.health` on every connection transition.
- `badconn.metrics` periodic summary: preset, latency, loss, reorder,
  duplicates, blackout, queue depth.
- `dev-loop.instance.state` when the dashboard state changes.

## Acceptance

A test passes only when the journal shows, for the impaired client:

- expected impairment counters;
- no silent last-frame replay;
- either recovery or an explicit `frozen`/`stale`/`reconnect-failed` state;
- death/respawn recovery after reconnect;
- correct server roster after the client returns;
- server fixed tick within budget during the impairment.

A healthy client on the same server must remain playable throughout.

## Boundaries

- The simulator is not a bandwidth benchmark and must not be used to claim
  server capacity.
- It does not replace real multi-machine or WAN testing.
- It must not create a second logging path; evidence goes to the per-instance
  canonical journal.
- Human visual review is still required for perceived smoothness and feedback.
