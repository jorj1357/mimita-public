# Payload / Mimita Pipes join crash: root-cause evidence

Date: 2026-11-28
Agent: `rocket-league-arena2`
Trigger: user context `ctx-9543f5ce1f70` (wake `wake-1398ecb3d7a0`)

## Task

Investigate why joining a Payload server on `mimita-pipes` crashes, make the false/unusable condition loud and obvious, and ensure an issue exists. Issue intake was already done by other woken agents, so this session contributes evidence and coordination instead of a third duplicate.

## Findings (evidence-backed)

- Three client crash artifacts share one signature:
  - `.dev/crashes/20261128_054617_01/crash-2026-11-28_01-09-29-12028.txt` (client-01 pid 12028)
  - `.dev/crashes/20261128_054617_01/crash-2026-11-28_01-10-01-24360.txt` (client-02 pid 24360)
  - `%LOCALAPPDATA%\MiMITA\crashes\crash-2026-11-28_01-21-43-18776.txt` (client-01 pid 18776)
  - All: `ACCESS_VIOLATION (0xc0000005)`, read at `0xa`, faulting through `MultiByteToWideChar`.
- `addr2line` against `build/game/generation-808/game-g808.dll` (PE ImageBase `0x180000000`) resolves the stack to:
  `MultiByteToWideChar` <- `tinygltf::ReadWholeFile` / `LoadBinaryFromFile` <- `modelLoadThreadFunc` (`src/entities/player-loader.cpp:1975`) <- `Player::requestModelLoad` detached job (`player-loader.cpp:2025`) <- `mimita::game::runDetached` (`src/game/job-bridge-module.cpp:22`).
- The faulting string is `d->resolvedPath`. The gen-808 build snapshot (`.dev/build-snapshots/game-dll-20261127T065859749721Z-de4e7c/source/src/entities/player-loader.cpp` lines 2020-2025) still passes a raw `PendingPlayerModel*` (`mPendingModel.get()`) to the detached job. When a `Player` is destroyed during join/scene churn (many entities stream in on a `mimita-pipes` Payload join), the thread reads freed `d->resolvedPath`, giving the garbage pointer and the `read at 0xa` fault. This is a use-after-free, not a malformed map/route.
- The current working tree already contains the fix (detached job captures the owning `shared_ptr`) plus loud `player.model.load.invalid-path` events with `condition`/`expected`/`actual`/`owner`/`suggestion`. Pending build note `9dd132be9fa2` and request `build-request-59479dc50ebe` are already queued by the issue owner.

## Actions

- Appended the resolved stack and gen-808 vs working-tree comparison to canonical issue `issue-c9b65fb9948e` (owner `rocket-league-aerial`).
- Appended a duplicate pointer on `issue-a1d9a695459d` directing work to the canonical issue; no third issue created.
- Acknowledged user context `ctx-9543f5ce1f70`.

## Limits

No source change, build, or runtime proof was made by this session. Build and runtime proof of the fix remain with the request owner. Crash/dump/journal paths are the durable evidence above.
