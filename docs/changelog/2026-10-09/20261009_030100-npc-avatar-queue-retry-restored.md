# NPC avatar queue-retry fix restored

Date: 2026-10-09

## Scope

Restore the NPC avatar background-load retry and structured queue diagnostics that had been removed by a repository reset.

## Restored changes

- Exposed the existing replay worker queue limit to the avatar system.
- Added retryable tracking for avatar requests rejected only because the worker queue is full.
- Retry dispatch waits for available worker capacity.
- Actual avatar metadata/atlas failures remain terminal and continue to emit `avatar.load.failed`.
- Restored `avatar.load.queue-rejected` and `avatar.load.queued` structured events with queue depth, queue limit, and retry information.

## Build and runtime evidence

- Developer-loop build succeeded and published build 1893; the fresh Juggernaut run used build 1894.
- Server and client matched build 1894.
- Journal: `logs/2026-10-09/20261009_025836/events.jsonl`
- 25 avatar assets reached `avatar.load.ready`.
- 57 queue rejections were recorded and recovered through retry.
- 52 rebinds had succeeded at inspection time, with zero rebind failures.
- `avatar.load.failed` count was zero.
- Snapshot reassembly and server NPC-avatar summary events continued to appear.

## Acceptance boundary

The source restoration and runtime path are validated. Direct visual confirmation of every NPC’s rendered avatar remains a human acceptance step.
