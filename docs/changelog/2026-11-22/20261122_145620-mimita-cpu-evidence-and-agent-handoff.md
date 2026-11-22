# MiMITA CPU evidence and cross-agent handoff

## Scope

Continued the CPU/resource investigation and added a shared evidence-handoff rule. No gameplay, networking, or build source was changed; no build was requested. Findings are appended to existing issue `issue-ed3ac775283d`.

## Findings

- In FCS config, `config/gamemodes/funcounterstrike.json:37,55` assigns capacity 10 to CT and T. `src/network/server-gamemode.cpp:3277-3286` sizes the roster as capacity minus the human on their team and full capacity on the opposing team. `buildObjectiveRoster` at `:3404` documents that headless mode fills both teams; calls occur at `:9159`, `:9193`, and `:9298`. This explains how a one-human match can produce 19 NPCs. The retained roster after leaving/switching is a measured behavior in the logs; its complete lifecycle owner still needs a matched transition trace.
- Existing run `20261122_011031`, server PID 26748, had 19 NPCs and zero players. `server.world-phase-window` at `src/game/server-runtime-module.cpp:454` measured a window with NPC simulation 3209.70ms, physical contact 529.07ms, projectiles 171.72ms, and total 3910.77ms over 8.44s. `npc.server-phase-window` at `src/network/server-npcs.cpp:3120` for the same time reports 2941.27ms AI across 19 NPCs, total 3208.78ms, worst actor 1463.30ms. The server tick window reported 1.30Hz and 744.31ms average total tick time. Earlier 10:53 CPU samples show this server at 4.53 logical cores average while no compiler/linker was active; a separate host snapshot was 94.1% total with MiMITA 463.7%, System 198.5%, Defender 127.3%, Pythonw 42.3%, OpenCode 20%, and Task Manager 15.3%. These are distinct samples and should not be combined into a single concurrent process budget.
- Current dev-loop run `20261122_144609_01` reports profile `npc-navigation`, launch `effective_mode=jugger_horde`, `effective_npcs=1`, server PID 4540. Its journal at 2026-11-22T14:54:40.801Z records 66 NPCs, zero players, and 25-tick timing: 43.34784ms average, 122.165ms p95, and 201.785ms maximum; worst NPC stage 201.012ms. Nearby NPC events use `juggernaut_swarm`. The timing sample alone does not preserve the exact active `d.matchMode`; verify it in a future transition capture. A matching process CPU sample did not complete with readable output, so this run confirms expensive work and a workload mismatch, not a CPU percentage.
- Build contention is a separate likely contributor during actual builds. `devscripts/dev_loop/game_build.py:1632-1633` passes 10 jobs and high priority; `devscripts/dev_loop/build.py:151` sets `HIGH_PRIORITY_CLASS`. The 10:53 no-op DLL attempt had no active compiler/linker, proving that a build was not required for the high server load in that sample. A matched real-build timing comparison with fewer jobs/normal priority has not been made.
- User-provided Task Manager images show MiMITA rows rounded to 0% while total host CPU was 54%. That is a momentary per-row reading, not proof that every MiMITA server is idle. Another server PID may run separately, and Task Manager's displayed sample may round small/currently sleeping work. The quoted 100% host / 43% MiMITA total-capacity claim lacks its original PID inventory, so the residual load is not attributable from that quote alone.

## Collaboration rule

- `AGENTS.md:64-78` asks every agent to append resource-pressure evidence to the appropriate shared issue, including time/sample duration, normalization, PID/role, live mode/workload, build phase/top competing processes, artifact links, and observed-versus-inferred separation.
- `docs/specs/dev-loop/agent-coordination.md:212-230` adds the durable CPU/resource evidence handoff; it says to read mode from the live journal after in-game switches, link related subsystem issues without taking over their ownership, and maintain one shared issue trail.

## Next evidence needed

1. Capture one bounded post-switch/leave interval containing the exact active mode, players, NPCs, world phases, and server/client process CPU in the same time window.
2. Compare a true empty-room state with the current retained-NPC state to locate the first lifecycle step that leaves actors simulating.
3. Separately compare a real incremental build under no-game and representative-game load; record wall time, jobs, priority, compiler/linker CPU, and concurrent game CPU before changing build concurrency policy.
4. Obtain an authorized owner-level or stack profile if possible; previous WPR CPU profile attempt failed with `0xc5585011`, so per-function attribution remains incomplete.

No user-facing 1% active-game CPU claim is supported. The measured bottleneck is the server's NPC/world simulation and physics workload when many actors remain; build concurrency can add contention during actual compile/link work, while the highest host samples also include substantial non-MiMITA processes.
