# MiMITA CPU: horde roster size and regression lead

## Scope

Continued the MiMITA CPU investigation for context `ctx-81cc5682b52c` (linked to `issue-ed3ac775283d`). No source, config, build input, or runtime was modified; no build was requested. Findings were appended to the existing CPU issue and a broadcast reminder about resource-evidence logging was posted to the coordination brain.

## Measured evidence

- Matched 5s CPU sample at 2026-11-22T16:02Z on host with 12 logical CPUs, shared dev-loop run `20261122_144609_01`:
  - server PID 4540 = 0.95 logical cores / 7.94% of whole machine
  - client PID 30076 = 1.57 logical cores / 13.05% of whole machine
- Same PIDs at 15:33Z were 1.45 / 12.06% and 2.68 / 22.37%, so load scales with scene and sample state.
- Live server journal (`server-01/events.jsonl`) confirms mode `jugger_horde` with `living_npcs=66`, `moved_npcs=65` (not idle). It is not a one-NPC server.
- `server.world-phase-window` at 16:02:21Z: `npc_sim_ms=697.56`, `physical_contact_ms=261.54`, `ticks=10` over `seconds=1.131`, `total_avg_ms=96.32` versus a 16.7ms tick target (~5.8x over budget). 15:33Z windows showed `npc_sim` 635-739ms and `physical_contact` 165-236ms.
- Client (`client-01/events.jsonl`) is render/present-bound, not simulation-bound: `performance.fps` at 16:02Z reports `fps=67.9`, `avg_frame_ms=14.48` against a 4.166ms budget, `37/68` frames over budget, `p95=61.8ms`, `max=261ms`; the largest frame-budget scope was `Swap` at ~12ms.
- At 16:02Z only the two shared-run processes existed. Transient extra processes seen earlier (isolated FCS test run `20261122_150906`, PIDs 25332/29824) had already exited; they were not a persistent CPU leak.

## Why a one-NPC launch still runs many NPCs

- `config/gamemodes/jugger_horde.json` owns roster size: fighters `capacity: 64` (line 40) with `respawn_seconds: 0.01` (line 41) and `npcs: true` (line 52); juggernauts `capacity: 4` (line 58); `npc_spawn.max_count: 67` (line 286); `wave_start_count: 1` (line 267).
- The dev-loop `--npcs 1` launch flag does not cap the horde roster, so 66 living NPCs with the `juggernaut_swarm` profile is the configured endless-respawn behavior, not a stuck one-NPC run.

## Regression lead

- The user reports (`ctx-afee6cb3e97f`) that `maxFrames=999` was present the whole time and a different change raised CPU. Corroborated: recent `config/video-settings.json` edits only toggle `fullscreen` between 6 and 0; `maxframes`/`maxFrames` is unchanged.
- Recent commits touching runtime hot code: `70f3c051` (2026-11-22, `src/network/server-npcs.cpp`, `src/network/multiplayer-tick.cpp`), `9fd637d5` (2026-11-21, `src/network/server.cpp`, `reliable-gameplay-events.cpp`, `multiplayer-packets.cpp`), `d89884f6` (2026-11-21, `engine-tick-net`, `server-npcs`, physics). `70f3c051` added per-tick stage timers plus reliability work; the server AI phase already shows hundreds of `target_scans` per window.
- These commits are candidate bisection points; no commit is yet proven to have raised CPU.

## Collaboration rule

- The requested "log this stuff to the appropriate issue as it comes up" rule already exists in `AGENTS.md` (RESOURCE EVIDENCE HANDOFF) and `docs/specs/dev-loop/agent-coordination.md` (CPU and resource-pressure evidence handoff). A broadcast coordination message (`msg-4f9d0e03e512`, appended to `issue-91e3384b219b`) reminded agents to use it as events occur.

## Next evidence needed

1. Bisect the recent server hot-path commits under the same `jugger_horde` scenario and equal NPC count, measuring server `npc_sim_ms`, `physical_contact_ms`, tick Hz, and process CPU.
2. Reduce per-NPC AI/targeting or physical-contact cost, or explicitly cap living horde NPCs, and re-measure the server win.
3. Consider a lower background/over-budget client frame cap only after the server regression is characterized; do not change VSync or the 60Hz authoritative tick.

No user-facing 1% active-game CPU claim is supported.
