# Replay consumed-batch requeue fix

- Branch: `8292026stash`
- Starting commit: current worktree after prior replay cursor/export-loop changes
- Timestamp: `2026-09-07T19:15:00Z`
- Scope: stop replay effects, sounds, and killfeed entries from being requeued after consumption.
- Pre-existing changes: all user and other-agent configuration edits, replay source changes, previous regression entries, and previous changelogs were preserved.

## Changes

### `src/engine/engine-tick-camera.cpp`

- Old behavior: static reusable `effects` and `sounds` vectors received batches through `swap()` and were processed, but retained their contents after processing. The next `takeTriggered...()` call swapped the old batch back into `ReplayPlayer`.
- New behavior: `effects.clear()` runs after effect dispatch and `sounds.clear()` runs after sound dispatch.
- Result: an effect can expire normally without being inserted again solely because the previous caller buffer still owned it.

### `src/engine/engine-tick-ui-hud.cpp`

- Old behavior: the reusable `killEvents` vector retained the consumed killfeed batch after `takeTriggeredKillfeedEvents()`.
- New behavior: `killEvents.clear()` runs after `KillfeedManager::onKill()` processing.
- Result: one historical killfeed event is not requeued and shown repeatedly.

## Evidence and diagnosis

- The latest export log showed thousands of replay dispatches despite a much smaller source event count. The swap-based consumer was a direct requeue path consistent with the user's observation that effects disappear and then return.
- The supplied screenshot is treated as visual evidence only; no instructions were inferred from it.

## Validation

- `python build_agent.py`: `Status: SUCCESS`; canonical `mimita.exe` linked.
- `mimita.exe --replay-export-selftest --timeout 60 --no-coordinator`: `26/26 passed`.
- `git diff --check`: passed before final documentation.
- No fresh live visual export was performed.

## Remaining

- Camera first/second/third-export startup recurrence remains unresolved and requires a fresh source replay plus camera-ready logs.
- Replay rockets and hit visuals still use replay-specific reconstruction instead of the complete shared gameplay collision/effect path.
- Left-leg orientation and full visual parity remain unresolved.
