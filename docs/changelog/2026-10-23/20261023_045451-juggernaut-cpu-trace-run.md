# Juggernaut CPU trace run and late-frame evidence

- Recorded: 2026-10-23T04:54:51Z (2026-10-23 00:54:51 EDT, America/New_York)
- Branch: `20261011zt`
- HEAD before this record: `cbfa4b0d`
- Commits created: none
- Focus: run the existing developer-loop Juggernaut profile while the user's Windows Performance Recorder CPU profile was active; inspect the matching JSONL journal.

## Worktree and source changes

No gameplay, performance, logging, launch-mode, or build source was edited in this session. The worktree was already dirty, including `src/perf/perf-frame.cpp`, `src/engine/engine-tick.cpp`, `src/network/server-gamemode.cpp`, `devscripts/dev-loop.py`, and untracked `src/map/map-authored-geometry.cpp` / `src/map/map-authored-geometry.h`. These pre-existing edits were preserved.

The only file added by this session is this changelog. It had no prior text. No source before/after quote applies because no source text changed.

## WPR capture

The user-provided elevated PowerShell status showed WPR recording the `CPU.Verbose.File` profile, file logging, zero lost events, and system stack capture for `CSwitch`, `ReadyThread`, and `SampledProfile`. The profile is appropriate for sampled CPU stacks and scheduling evidence.

From this agent's non-elevated shell, `wpr -status` reported `WPR is not recording`; attempting `wpr -stop` returned `There are no trace profiles running` / `0xc5583000`. The elevated recording could not be stopped or saved from this shell. The ETL file is therefore pending. Required save command, to run in the same elevated PowerShell session:

```powershell
wpr -stop "C:\mimita-v9\logs\2026-10-23\juggernaut-cpu-20261023.etl" "MiMITA Juggernaut run 20261023_045012"
```

The trace spans unrelated system/build time before the game launch as well as the run. PID and timestamp filtering will be needed when analyzing it.

## Runtime evidence

The developer loop launched run `20261023_045012`, mode 9 (`juggernaut`), map `dust2cyberiav4`, using debug build 2881 at `.dev/builds/2881/mimita.exe`, SHA-256 `fe60637c825e9ba9c5a5dabff051975ec40a9b8ad2bf9c96a05df69f17568e28`. Client PID was 33988 and server PID was 20016. The authoritative journal was `logs/2026-10-23/20261023_045012/events.jsonl`.

Build 2881 is stale: it contains source generation 12 while the current source generation reached 32. The current dev-loop build failed in pre-existing untracked `src/map/map-authored-geometry.cpp`, at line 221 (`HeadlessWorld cube` has incomplete type) and line 223 (`appendHeadlessWorldInstance` is not declared in this scope). Therefore runtime observations below describe build 2881, not the current source tree.

The journal recorded 134 distinct `npc.roster-profile-resolved` actor IDs: 126 team-0 `juggernaut_swarm` and 8 team-1 `juggernaut_limited`. Resolutions occurred in two batches of 67, during run seconds 10-20 and 40-50. Server-side NPC movement, state, collision, and stuck-recovery events continued through approximately second 82.9. This proves NPC simulation was active; it does not, by itself, prove the exact number alive at every instant. The client performance field `max_npc_count` remained zero, so that client counter did not represent these networked NPCs in this run.

The client performance journal showed:

- At second 6.8: 3.21 ms average frame time, with 18.3% of frames above the 4.17 ms 240-FPS budget.
- At second 74.5: 11.98 ms average, 24.86 ms p95, 35.55 ms maximum, and 60/60 frames (100%) above budget.
- At second 74.5: working set 719 MB and private bytes 1,558 MB, compared with 213 MB / 342 MB at second 6.8 and 212 MB / 342 MB at second 5.9. This shows substantial growth during this run but does not prove a memory leak or establish system-wide free RAM.
- The early startup window at second 5.9 included a 2.98-second maximum frame. Later windows around seconds 69-74 were consistently far below the 240-FPS target.

In the 60-frame window near second 74, the largest recorded scope totals were `EngineTick::Work` 717.30 ms, `Rendering` 275.79 ms, `Replay` 170.04 ms, `Rendering::Actors` 148.72 ms, `Networking` 145.27 ms, `Network::MpTick` 143.47 ms, `Rendering::Actors::NetworkEntities` 143.43 ms, and `Simulation::SimulateTick` 133.11 ms. These are nested scope totals and must not be added together. They narrow the next profile review to the main frame path, replay, networked actor rendering, networking, and simulation; they do not identify a single proven root cause.

The journal continued to receive events through second 82.94, so this run did not reproduce the prior quiet-journal cutoff. The client/server pair launched for this run was stopped through the developer loop; the journal recorded `client stopped` and `server stop_result` with `owned=true` and `stopped=true`. Existing unrelated MiMITA processes were left untouched.

## Validation and remaining evidence

- Canonical developer-loop launch: performed; stale-build limitation recorded above.
- Runtime journal inspection: performed for the exact run path above.
- WPR ETL stack analysis: pending the elevated `wpr -stop` command above.
- 240-FPS acceptance: failed by a wide margin; no zero-miss window was observed.
- Source build: failed on the existing map geometry compile errors; no source fix was made.
- Tests: none added or run; the task was a live runtime/profile capture.
- Human visual/gameplay review: not performed.

## Documents and focused review

Read `docs/ROUTER.md`, `docs/specs/performance/performance.md`, `docs/specs/debug-logging/debug-logging.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/operations/build-and-exe/build-and-exe.md`, `docs/operations/task-completion/task-completion.md`, `docs/architecture/live-development/live-development.md`, `docs/architecture/terminal-commands/terminal-commands.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, `docs/skills/efficiency-checker-v1.md`, and `docs/skills/logging-checker-v1.md`.

Efficiency review was evidence-only: the measured costs and counters are listed above; no hot-path change was proposed or implemented without the saved ETL stacks. Logging review found that the client `max_npc_count` field did not match the server's observed networked actor activity, making it unsuitable as a whole-match NPC count for this run.
