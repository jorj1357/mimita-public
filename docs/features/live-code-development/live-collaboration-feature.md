# Live collaboration feature record

The v2.1.0 foundation adds generic resource revisions, a bounded operation
queue, server arbitration, revision packet IDs, and named JSONL probes.

Implemented foundation:

- `ResourceRevisionV1` and revision states.
- `LiveOperationV1` and operation state names.
- Priority-aware in-process operation queue with JSONL transition events.
- Server revision acceptance, stale-base draft preservation, active revision
  lookup, and rollback pointer changes.
- Bounded live revision packet definitions.
- `LiveProbe` and `LiveProbeScope` forwarding to the existing JSONL logger.
- Hot probe rules in `config/debuglogger.json`.

Remaining integration work:

- Wire revision packets into client/server transport handlers.
- Connect file watchers and hot-build candidates to revision proposals.
- Transfer and publish non-code resources through the revision protocol.
- Add terminal commands for queue, revisions, rollback, and probes.
- Add two-process human acceptance with simultaneous edits.
