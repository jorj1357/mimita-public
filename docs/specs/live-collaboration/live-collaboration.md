# Live collaboration and revisions

Status: v2.1.0 implementation foundation

MiMITA uses one shared model for live code, JSON, models, animations, sounds,
textures, and future editor resources:

```text
resource -> revision -> operation -> server decision -> active revision
```

The server chooses the active revision. Client drafts are never silently
deleted. A rollback changes the active pointer and preserves every revision.

The same `ResourceRevisionV1` and `LiveOperationV1` concepts apply to every
resource kind. The implementation foundation is in
`src/live-code/resource-revision.*`, `src/live-code/live-operation.*`, and
`src/live-code/live-operation-queue.*`.

Hot C++ behavior remains governed by the existing hot-reload generation system.
The running executable is not replaced. JSON and binary resources use the
existing content-addressed artifact cache and resource publication path.

The server-side revision foundation is in
`src/network/server-live-collaboration.*`. Network packets are versioned in
`src/network/packets.h`; transport handlers must activate only verified bytes.
