# NPC Avatar Background Queue

Time: 2026-09-09T11:52:00Z
Branch: 8292026stash
Commit before changes: 96e53beffc95b35ee9439852c3dc888ec741597c

## Pre-existing changes

The working tree already contained unrelated changes before this task:

- `config/duel-maps.json` was deleted.
- `config/gamemode-good-maps.json` was untracked.
- `docs/regressions/README.md` was modified with the new date-directory regression format.

These changes were preserved and were not claimed as part of this task.

## Work performed

Changed NPC avatar preparation so it uses the existing persistent `ReplaySaveWorker` instance at below-normal Windows priority. Added avatar background result state, bounded completion polling, failed-load caching, and CPU atlas preparation on the worker. The render path now skips queued/loading/failed avatar names instead of retrying them every frame.

Exact files changed:

- `src/replay/replay-factory-worker.h`: added `LowPriorityTaskWorker` alias to document reuse of the existing worker.
- `src/avatar/avatar.h`: added worker attachment, avatar load states, pending result queue, mutex, and prepared atlas pixel storage.
- `src/avatar/avatar-atlas.cpp`: queued JSON parsing and CPU atlas preparation, cached failed results, published completed results, and reused prepared pixels during GPU upload.
- `src/engine/engine-tick-setup.cpp`: polls at most one completed avatar result per engine frame.
- `src/main-systems.cpp`: attaches the existing replay worker to `AvatarSystem`.
- `src/render/render-player.cpp`: prevents repeated render-frame avatar requests.
- `docs/regressions/2026-09-09/npc-avatar-frame-stall-REG.md`: recorded the confirmed repeated-retry/frame-stall regression using the new README format.

## Hypothesis

> A single bounded low-priority worker queue with non-blocking result polling and bounded main-thread GPU work will produce better overall frame times than synchronous avatar preparation or the current detached-avatar/repeated-retry design.

This hypothesis remains unverified. It must be tested using current, fallback-only, and worker-queue runs while measuring maximum, p95, and p99 frame time, time-to-avatar-ready, worker queue depth, GPU upload duration, and gameplay/network responsiveness.

## Skills and documents

- `docs/ROUTER.md`
- `docs/specs/performance/performance.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/efficiency-checker-v1.md`
- `docs/skills/logging-checker-v1.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/regressions/README.md`

## Validation

- `git diff --check`: passed.
- `python -m py_compile build_agent.py`: passed.
- `python build_agent.py`: passed.
- Build status: `SUCCESS`.
- Canonical output: `C:\mimita-priv-v8\mimita.exe`.

No fresh two-client/NPC runtime performance run was available in this session. Human visual acceptance, controlled frame-time comparison, despawn/respawn safety, and hot-reload retry behavior remain open.
