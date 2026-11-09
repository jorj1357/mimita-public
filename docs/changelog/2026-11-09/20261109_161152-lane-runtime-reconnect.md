# Repository-local lane and runtime capture

- Date: 2026-11-09T16:11:52Z (11:11:52 EST)
- Branch: `checkpoint/pre-live-kernel-20261108`
- Coordinator HEAD: `95c55bd00232e09253f31224e7459c9069eeb702`
- Lane: `reconnect-lane-local-20261109` on `codex/agent-reconnect-lane-local-20261109`
- Lane baseline commit: `6812030fda25513c5a2aef438cb04f3ce5f7a069`
- Result: repository-local lane paths are enforced and the lane launched a mode 56 FunWorld3 server/client pair. The reconnect symptom reproduced in live journals. No gameplay fix was made.

## Changes made

- `devscripts/agent-lane.py:29`, `LANE_ROOT`: replaced `ROOT.parent / f"{ROOT.name}-agent-lanes"` with `DEV_ROOT / "agent-lanes"`, so lane worktrees and the existing lane metadata registry are under the checkout. `devloop()` now passes the coordinator root at line 139, and `create_lane()` rejects a resolved lane root outside the coordinator at lines 156-159.
- `devscripts/build_scheduler.py:23-29`, `scheduler_root()`: removed the `%LOCALAPPDATA%`/system-temp fallback. Scheduler state defaults to `<coordinator>/.dev/build-scheduler`; a configured scheduler path outside the coordinator raises an error.
- `build.py:418-422`: replaced the per-worktree `build/ccache` default with `<coordinator>/.dev/ccache`, and reject a cache path that escapes the coordinator.
- `devscripts/dev-loop.py:3349-3352`, room-code creation: replaced the default system-temp `mkstemp()` call with a file under `ROOT/.dev/tmp`. The same correction was applied to the active lane's copy. An initial version used nonexistent `self.root`; the failed launch journal identified it, and it was corrected to module-level `ROOT` before the successful launch.
- No `*.txt` log writers or build-time tracker were changed in this session.

## Existing edits preserved

At session start, the coordinator already had edits to `AGENTS.md`, `config/analytics.json`, `devscripts/blender-ai`, the live-development and build/task docs, and networking/dev-loop/open-gameplay-issues specs, plus the existing changelog `20261109_151417-agent-repository-local-workspace-policy.md`. During this work, `20261109_154436-single-dashboard-instance.md` and further user changes appeared. They were left intact. Lane-local edits to `build.py` and the untracked `devscripts/dev_loop/build_timing.py` also appeared during the cold build and were not edited or removed here.

## Build and runtime evidence

- In-memory Python syntax compilation passed for `devscripts/agent-lane.py`, `devscripts/build_scheduler.py`, `devscripts/dev-loop.py`, and `build.py` after the path changes.
- `python devscripts/agent-lane.py create reconnect-lane-local-20261109` created the worktree at `C:\mimita-v9\.dev\agent-lanes\reconnect-lane-local-20261109`.
- The lane dev-loop performed its own initial cold build. The valid published executable was 237,906,222 bytes with SHA-256 `21fc90de18162762e174fd10e6181d7932e0c046163fbf11ea5fa48d4d45d47e`. Later no-source-change publications reported 3.69 sec and 4.00 sec in the dev-loop build output. No agent build command or direct compiler command was run.
- The shared scheduler reported `C:\mimita-v9\.dev\build-scheduler`; ccache populated under `C:\mimita-v9\.dev\ccache`; the room-code file was created under the lane's `.dev\tmp`.
- Successful runtime: run `20261109_155925`, mode 56, map `funworld3`; server PID 23932, client PID 22240, dev-loop PID 2168. The run uses build ID `2026-11-09T15:59:21.487623Z:21fc90de18162762e174fd10e6181d7932e0c046163fbf11ea5fa48d4d45d47e`. Both game processes and the dev-loop were still alive at final verification.
- The server journal records `map.server-load-success` at 15:59:32Z. The client journal records `Connecting -> Connected` at 15:59:51Z, an authoritative snapshot at tick 649, and spawn acknowledgement/activation at 15:59:52Z. This run did not reproduce a map-synchronization hang.
- By 16:10:40Z, the client journal records 47 `network.snapshot.timeout` events and 47 `network.ice-reconnect.start` events, with 95 connection-state transitions. The first timeout was at 15:59:57Z with `snapshot_age_ms=2508`, threshold `2500`, and `last_heard_age_ms=2091`; later timeout samples reached 2501-2523 ms for both ages. The event owner is `mpUpdateConnectionHealth()` in `src/network/multiplayer-packets.cpp:2106-2164`; it sets `snapshotStarved` when snapshot age exceeds the configured stale threshold (`config/networkingconfig.json:209`, 2500 ms). Server journal entries show reconnect requests accepted for room `DG4NQ2M` on FunWorld3. The repeated timeout/reconnect loop is proven; why snapshots stop long enough to trigger it remains unresolved.
- The first two launch requests failed because the already-running Python daemon retained the old function body. Its journal showed `AttributeError: 'DevLoop' object has no attribute 'root'`. The lane daemon was then stopped through its control request and restarted; the successful run has no such error. The later successful status reports server/client ready and `run_alive=true`.
- No human visual or feel acceptance was performed. The client and server were left running for the user.

## Build-time tracker direction

The existing `docs/specs/dev-loop/build-time-observability.md` specifies one `.dev/build-timings/<attempt-id>.json` record. The requested `logs/YYYY-MM-DD/build-times/HH-MM-SS_<attempt-id>.jsonl` layout is a change to that contract. A suitable record should be append-only JSONL with UTC timestamps and monotonic durations, keyed by attempt ID, lane, source generation, mode, and artifact identity. The dev-loop owns debounce, scheduler wait, and publication phases; `build.py` owns PCH, per-translation-unit queue/start/end, cache results, and link timing; `build_scheduler.py` owns wait intervals. The C++ `StructuredLogger` is not the build worker's logger, so a small Python writer shared by those owners is the appropriate implementation seam, with `events.jsonl` retaining the run/build identity link.

Initial source search found text writers to review before disabling them: the main managed log and `latest-log-path.txt` in `src/debug/log-manager.cpp`; `map_spawn_debug.txt` in `src/combat/death-system.cpp` and `src/game/spawn-utils.cpp`; `spawn_weapon_debug.txt` in `src/combat/weapon-runtime.cpp`; weapon benchmark `.txt` output and `latest-weapon-bench-path.txt` in `src/terminal/weapon-bench-commands.cpp`; and `performance_profile.txt` in `src/perf/perf.cpp`, whose writer currently returns immediately as deprecated. `src/debug/debug-diag-commands.cpp` writes `diag-log.txt` outside `logs`. This is a focused search result, not a guarantee that every dynamic text-path writer has been inventoried.

## Documents and review

- Read: `AGENTS.md`, `docs/ROUTER.md`, `docs/specs/dev-loop/agent-lanes-build-authority.md`, `docs/specs/dev-loop/dev-loop.md`, `docs/specs/dev-loop/build-time-observability.md`, `docs/operations/build-and-exe/build-and-exe.md`, `docs/operations/task-completion/task-completion.md`, `docs/workflows/runtime-scenario-validation.md`, and `docs/regressions/README.md`.
- Validation: Python syntax compilation, lane creation/ensure/status/run through `agent-lane.py`, scheduler/cache/temp path inspection, and live client/server journal inspection. No synthetic test was added or run.
- Human review still needed: observe the visible client; inspect why authoritative snapshots pause; decide whether build timing should keep the current `.dev` JSON contract or move to the requested dated JSONL path; approve the scope for disabling text logs after a complete writer inventory.

- The lane's first C++ build rewrote tracked generated dependency output src/pch.h.d; this build-generated lane diff was left in the isolated worktree and not copied to the coordinator source tree.

