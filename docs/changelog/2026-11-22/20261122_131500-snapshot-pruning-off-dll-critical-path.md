# Snapshot pruning off the DLL build critical path

- Agent: `cpp-latency-digest-cache`
- Work session: `work-838b9d59be46`
- Task: `cpp-latency-snapshot-prune`
- Issue: `issue-136d4553ff92`
- Date: 2026-11-22
- Checkout: `C:\mimita-v9`, branch `checkpoint/one-checkout-condensation-20261111`

## Change

`devscripts/dev_loop/game_build.py` now removes stale snapshots from the active snapshot directory with same-volume renames, then drains recursive deletion through one bounded background worker. A queue limit of 64 keeps cleanup work bounded; cleanup and telemetry exceptions are recorded or ignored as housekeeping and do not fail the candidate. Low-disk recovery retains synchronous cleanup. Timing appends are guarded so a background cleanup event cannot interleave with the build attempt JSONL. Leftover trash from an interrupted daemon is re-queued on a later prune pass.

The current working-tree build-driver edits from earlier sessions were preserved. Coordination decision `decision-4ede57383368` records the inspected current tree as the baseline for this patch; the unrelated `game_build_worker.py` conflict remains open and untouched.

## Verification

- `python -m py_compile devscripts/dev_loop/game_build.py devscripts/quality/test_game_build_snapshot.py` passed.
- `python -m unittest devscripts.quality.test_game_build_snapshot` passed: **16 tests**.
- New tests verify that an old snapshot is renamed then asynchronously deleted, that the newest snapshot is preserved, that a rename failure does not stop pruning, and that a non-`OSError` cleanup exception is recorded while the worker continues to process later work.
- `git diff --check` passed for the three paths changed in this task; Git printed only the existing LF-to-CRLF notices.
- The root dev-loop daemon was restarted through its control path from PID 34472 to PID 2276 to load the updated Python module. The healthy server PID 26748, run `20261122_011031`, remained running throughout. Final status: `game_build_succeeded`, generation 492, no build pending.
- Task proof `task-proof-5d4b1255b84c` is verified against `game.hotness.summary` with generation 492 in `logs/2026-11-22/20261122_011031/server-01/events.jsonl`. An earlier malformed field predicate remains unproven; the corrected record is proven.

## Root-loop measurements

A comment-only C++ canary was used so the candidate had one real changed translation unit while preserving gameplay behavior. The normal MinGW compiler, LLD selection, object cache, full game-DLL publication, and live DLL activation path remained in use. Build note `f4461711430f` was attached before editing `src/effects/zombie-tower-effects.cpp`.

| Attempt | Outcome | Total | Snapshot prune | Background deletion | Compiled / skipped | Link / publish | Runtime evidence |
|---|---|---:|---:|---:|---:|---:|---|
| `game-dll-20261122T121228440102Z-3aba91` | gen490 published | 80.609s | 11.248s synchronous deletion | not separated | 2 / 641 | 10.102s / 0.737s | gen490 activated in existing server |
| `game-dll-20261122T125133309168Z-46b1e5` | gen491 published | 68.864s | 31.765ms; 1 snapshot deferred | 11.255s, completed before build end | 2 / 641 | 9.576s / 0.703s | `game.generation.activated`, gen491, PID26748 |
| `game-dll-20261122T130015674891Z-687e4d` | gen492 published | 61.943s | 37.012ms; 1 snapshot deferred | 11.753s, completed before build end | 2 / 641 | 8.073s / 0.419s | later `game.hotness.summary` reports gen492 on PID26748 |

The first post-change canary is 11.745 seconds faster than the 80.609-second baseline, close to the 11.248-second old-snapshot deletion removed from the foreground path. The next canary was 18.666 seconds faster, but compile and link timings also improved, so that entire delta cannot be attributed to pruning. The direct observation is the prune subphase falling from 11.248 seconds to 32–37 milliseconds while deletion still takes about 11–12 seconds asynchronously.

The gen492 attempt compiled only `src/effects/zombie-tower-effects.cpp` (24.540s) and the generation identity provider (1.582s), skipped 641 objects, linked 642 objects with LLD in 8.073s, and published in 0.419s. Its request list contains both `auto-1` and explicit request `2026-11-22T13:00:31.139Z-6b8ed743`, coalesced into the same attempt. The live DLL remained on the same server PID. A one-second host/process sample at the end of the attempt read 70.6% host CPU across 12 logical CPUs and 451.6% of one core for the server. Compiler child processes had already exited, so this does not establish that game CPU caused the compile duration. This was an FCS-like server with 19 NPCs, not the requested Juggernaut/AI-controller pressure scenario; the comment canary is not gameplay behavior proof.

## Remaining measured costs and next step

- Source snapshot creation remained about 12.5 seconds for the 2,899-file tree.
- The edited TU still took 24.540 seconds; bypassing or reducing PCH load is a separate experiment and has not been applied.
- LLD link remained 8.073 seconds for 642 objects and 480,395,963 object bytes.
- Warm no-op was 6.89 seconds in the prior daemon and 11.89 seconds immediately after daemon restart. Restart validation is a separate cost from steady-state no-op.
- A directory-symlink capability probe failed with Windows `WinError 1314`; a contained `cmd.exe mklink /J` junction probe succeeded and confirmed removing the junction preserves its target. The next snapshot experiment should prototype a junction-backed immutable base plus changed-directory overlays, retain a hardlink full-snapshot fallback, and compare timings and compiler depfile paths before adopting it.
- No Juggernaut pressure run, compile-time CPU attribution, or NPC behavior change was tested in this session. The active goal remains open.
