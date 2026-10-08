# NPC avatar queue retry fix

Date: 2026-10-08

## Scope

Make avatar background-load queue saturation retryable and add structured evidence for queue depth, queue limit, retry attempts, readiness, and rebind outcomes.

## Source changes

- Exposed the existing replay worker queue limit to the avatar owner.
- Added an `AvatarSystem` retry set for requests rejected because the shared worker queue is full.
- Retry dispatch occurs only when worker capacity is available; actual metadata/atlas preparation failures remain terminal failures.
- Added `avatar.load.queue-rejected` and `avatar.load.queued` structured events.

## Build evidence

- Canonical developer loop build succeeded and published build 1760, then launched build 1761 after the loop relinked the runnable executable.
- Runnable executable: `.dev/builds/1761/mimita.exe`
- Build SHA-256: `bf8ae69ad384713928717ceed96caaf247ec3621763b3c67aa1e9302350fed26`
- `--versioninfo` confirmed the executable and emitted its events-journal path.

## Runtime evidence

Juggernaut developer-loop run:

- Run ID: `20261008_204345`
- Journal: `logs/2026-10-08/20261008_204345/events.jsonl`
- Server and client both used build 1761.
- Server reported 67 NPCs / 68 entities, 10 snapshot chunks, and zero empty avatar names.
- 25 distinct asynchronous avatar assets reached `avatar.load.ready`.
- 57 queue-rejection events were recorded and retried.
- 57 NPC avatar rebinds succeeded; zero rebinds failed and zero retained fallback models in those rebind events.
- `avatar.load.failed` count was zero.
- Snapshot chunk rejection and timeout counts remained zero.

## Acceptance boundary

The structured runtime evidence confirms the queue/retry and rebind paths are functioning in the real Juggernaut run. A direct human visual pass is still the final confirmation that every visible NPC appearance is correct on screen.
