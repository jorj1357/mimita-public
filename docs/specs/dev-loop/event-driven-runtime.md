// 2026-11-27
/* purpose
* define the target event-driven runtime for MiMITA and its tooling
* replace polling, sleeps, and timer timeouts with causal event cascades
* make nodes and typed connections the shared model for runtime and knowledge
* this file DOES NOT remove the fixed 60 Hz simulation step
* this file DOES NOT authorize a big-bang rewrite
*/

# Event-driven runtime

**Status:** Target architecture, phased migration.

## The idea

A timer says "check again later". An event says "this happened; these are the
consequences". The finished system is a graph of nodes and typed connections
where every state change emits an event and every consumer reacts to it.

The fixed 60 Hz tick remains as a **scheduler event**, not as the mental model.
Inside a tick, work is a cascade of events with explicit causal order. Between
ticks, nothing wakes up just to ask whether something changed.

## Event envelope

```json
{
  "event_id": "evt-...",
  "type": "contact.hit",
  "version": 1,
  "time_utc": "...",
  "monotonic_ns": 0,
  "source": "server-01",
  "causation_id": "evt-...",
  "correlation_id": "shot-...",
  "tick": 12345,
  "payload": {},
  "provenance": { "owner": "...", "build": "...", "run": "..." }
}
```

Rules:

- `event_id` is unique and stable.
- `causation_id` names the event that directly caused this one.
- `correlation_id` groups a workflow across processes.
- `tick` is present for simulation events and absent for tooling events.
- Payloads are versioned; a consumer rejects an unknown major version.
- Events are append-only. Current state is a projection.

## Delivery semantics

- At-least-once delivery with idempotent consumers.
- Ordering is guaranteed per source and per correlation, not globally.
- A consumer that falls behind applies backpressure; it does not drop silently.
- Critical events have a durable log; high-rate cosmetic events may be sampled.
- Replay of the log reconstructs the projection deterministically for a pinned
  build.

## Replace timers

A timer is allowed only as a **scheduler** for a real deadline, such as a
weapon cooldown or a match clock. A timer MUST NOT be used to discover that
another component finished.

Forbidden patterns:

```text
while status != done: sleep(...)
sleep(2); check again
poll every 100 ms for a file
timeout -> assume success
```

Required patterns:

```text
subscribe(build.finished, request_id)
subscribe(run.ready, run_id)
subscribe(issue.proven, issue_id)
wake on event; end the activation
deadline exceeded -> emit wait.timeout, do not loop
```

## The tick as an event

The fixed step remains deterministic:

```text
tick.started(tick N)
  input.sampled
  actor.intent.updated
  movement.step
  contact.resolved
  damage.applied
  state.snapshot.produced
tick.completed(tick N)
```

- Systems subscribe to the phases they need.
- A phase emits events; the next phase consumes them.
- Within a tick, order is explicit and testable.
- A slow consumer is visible as a phase duration, not as a mysterious lag.
- The render and UI phases subscribe to `state.snapshot.produced`; they do not
  mutate simulation state.
- Variable render rate cannot change simulation results.

The 60 Hz scheduler MAY be replaced later by an event clock when the event
graph can prove equivalent determinism. It MUST NOT be removed before that
proof exists.

## Nodes and connections

The runtime, the tooling, and the knowledge base share one graph vocabulary.
See [`nodes-and-connections.md`](../architecture/nodes-and-connections.md).

Runtime nodes:

```text
actor, part, hit-volume, weapon, projectile, contact, damage,
packet, session, tick, snapshot, effect
```

Runtime edges:

```text
owns, parent-of, socket-of, targets, hits, damages, spawns,
replicates-to, caused-by, observed-by
```

Tooling nodes:

```text
file, header, symbol, spec, issue, task, build, check, run, agent,
lease, proof, metric, event
```

Tooling edges:

```text
depends-on, includes, implements, specifies, blocks, proves,
owns, leases, produces, consumes
```

Projections:

```text
graph view, board, 3D scene, timeline, dependency view, agent context
```

A projection never mutates the graph. An explicit edit does.

## Migration phases

1. **Coordination.** Build, check, approval, issue, wake, and process events are
   event-driven. No polling loops remain in agent tooling.
2. **Process lifecycle.** Runs are leases with release events; cleanup is
   event-driven.
3. **Build.** Check and build progress are events; no `--wait-for-build` sleep.
4. **Runtime boundaries.** Module load/unload, runtime start/stop, and config
   reload emit events.
5. **Simulation.** The tick is a phase cascade with explicit causation.
6. **Render and UI.** Render consumes snapshots and input events; it does not
   poll simulation state.
7. **Knowledge.** Issues, specs, builds, runs, and proofs expose graph edges so
   an agent can answer "what depends on this" without grep.

Each phase is independently shippable and must leave the tree buildable.

## Backpressure and budgets

- Every event consumer has a bounded queue and a measured service time.
- A full queue blocks the producer or sheds a declared low-priority class; it
  never silently drops a critical event.
- Per-frame event work has a budget; overflow is reported as a named event.
- High-rate events are coalesced at the source, not filtered at every consumer.

## Acceptance

- No agent tool contains a sleep-and-check loop for build, run, approval, or
  issue state.
- A build or run completion wakes the owner without polling.
- A pinned event log replays to the same projection.
- Every tick phase reports its duration and emitted event counts.
- Render rate changes do not change simulation results.
- A consumer that falls behind is visible in the journal with its queue depth.
- The graph view can answer "what includes this header", "what proves this
  issue", and "what owns this process" from the same model.
