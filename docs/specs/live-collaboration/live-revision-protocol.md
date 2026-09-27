# Live revision protocol

Every revision has a resource ID, revision ID, parent revision, content hash,
author, resource kind, state, UTC creation time, and simulation tick.

States are:

```text
draft, proposed, active, rejected, archived
```

An operation is received, validated, queued, started, and then completed or
rejected. A stale base revision is rejected, but its submitted revision is
preserved as a draft.

Rollback is pointer movement:

```cpp
activeRevision[resourceId] = revisionId;
```

It never deletes newer revisions. The server is authoritative for shared
activation. Clients may keep local drafts and must acknowledge the active
revision they install.

The bounded packet IDs are defined in `src/network/packets.h`. Artifact bytes
remain content-addressed and must pass the existing hash verification before
activation.
