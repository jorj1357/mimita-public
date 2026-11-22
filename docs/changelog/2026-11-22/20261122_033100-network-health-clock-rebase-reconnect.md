# Network health clock rebase reconnect fix

## Report

Investigated the reported FCS reconnect loop using the dev-loop isolated run `20261122_024031` and its client journal. That run showed a generation health-clock rebase after a long snapshot gap, followed immediately by a reconnect scheduled as `health_lost`, then snapshot recovery. The health ages were computed before the rebase and reused afterwards.

## Source change

In `src/network/multiplayer-packets.cpp`, `mpUpdateConnectionHealth` now clears the sampled `lastHeardAge` and `snapshotAge` after rebasing their timestamps. This keeps the same health update from treating the pre-rebase snapshot age as current. The `heardSinceDisconnect` check remains based on the original timestamps.

## Build evidence

- Initial candidate attempt `game-dll-20261122T031729161388Z-2185e7` failed changed-source preflight because its frozen source snapshot contained stale `const` declarations; no candidate DLL was published by that attempt.
- Retry `game-dll-20261122T032247328342Z-79f775` compiled and published DLL generation 446, source generation 6, revision `a7150409b063036ea53a031ceaa538c38455d8d5e81e470485ef7c9d5b3b0bd6`.
- The original build note `4e4c058bef69` was attached to the failed attempt. The retry was an automatic rebuild after the shared source changed and had no new source edits.

## Runtime evidence and limits

- Reproduction run `20261122_024031` used EXE build `2026-11-22T02:37:06.972018Z:c94488e6bcdca96b94e61c2985a5ef3f30bb6b70250e32f1cb1bd36b8401e344` and DLL generation 441. Its trace showed one reconnect/recovery, no server rejection, and no terminal disconnect.
- Verification run `20261122_032953` was started through dev-loop pinned to the same EXE and DLL generation 446, mode 45/FCS on `dust2cyberiav4`. The client connected and received/applied snapshots, but snapshots were intermittent (over 1.3 seconds between some snapshots; 213 missed by tick 2058). No health-rebase or reconnect event was observed before this record, so the specific fix remains unverified.
- The same run exposed a separate severe client stall: `performance.stall-watchdog` held frame 216 for over 17 seconds in `engineTickSimulate`'s Networking phase. Captured stack samples waited in `RtlEnterCriticalSection` under `GetMimitaInfiniteWorldgenModuleAPI`. This is a new, independently actionable lock-contention lead requiring its own focused source trace; this changelog does not claim it fixed.
- The user asked for agent-console UI interaction rather than computer-use automation. No computer-use tool was used. The earlier GUI-console input observation in run `20261122_024031` reported a click-edge mismatch; that is separate from this network fix and remains open.
