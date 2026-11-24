# CPU host control with MiMITA closed

At 2026-11-24 18:54:26–18:54:40 UTC, a Windows Performance Monitor `\Processor(_Total)\% Processor Time` sample collected 15 rows at approximately one-second intervals on a 12-logical-CPU host. Mean CPU was 43.76% (min 29.39%, max 69.94%). Physical memory available was 2,903.4 MiB of 16,024.5 MiB. `Get-Process -Name mimita` returned zero before and after the interval.

This is a no-MiMITA host control, not a before/after test of closing MiMITA. It cannot establish that MiMITA caused the human-reported PC slowdown or quantify its contribution. A preceding native `GetSystemTimes` sample failed due to PowerShell FILETIME binding; those readings were discarded.

No source code, build, runtime, priority, or process state was changed. No MiMITA runtime optimization has yet passed an A/B. Next evidence step is a low-overhead, generation-aligned live pair sample after the runtime owner releases the run and memory headroom is adequate, with host/PID CPU and memory, bounded callsite data, exact journal workload, frame-time, and server-tick evidence. The next CPU optimization build must contain one isolated measured code change and compare the same workload before/after.

Related issue: `issue-ed3ac775283d`; evidence was appended through agent coordination.

## Git history lead (not causal proof)

Commit `45580410a52f31bb02f702f35f7dcc597b60bb4b` is dated 2026-11-22 19:25:49 -05:00 and its message reports low FPS and 30–50% CPU. That snapshot changes three settings relevant to Juggernaut/NPC workload: `config/npc-difficulty.json` `perceptionSightRange` 99.0→1600.0; the `juggernaut_swarm` profile in `config/behavior-profiles.json` `max_attackers_per_target` 8→32; and `config/gamemodes/jugger_horde.json` target mode `opposite_team`→`player` with player and NPC inclusion enabled. All three settings remain at their increased values in the current checkout. The same commit adds per-tick `NpcDifficultyConfig::pollReload()` timestamp checks in the server simulation path (already detailed in the CPU issue).

These are temporally associated candidates, not proof of cause. They may encode intended NPC behavior and should not be reverted blindly. A matched, generation-aligned profile must first measure perception/target-selection and config-poll CPU/call counts under the same 1/19/67-NPC workloads. Select one measured owner for a semantics-preserving code optimization, then compare before/after on the same workload while checking client frame times, 60 Hz server ticks, and behavior. No source change or runtime test was performed in this session.

## Competing processes while MiMITA was absent

A separate two-snapshot process CPU delta sample ran 2026-11-24 19:04:32.160–19:04:40.407 UTC (8.25s, 12 logical CPUs); MiMITA process count was zero both before and after. Highest process shares were `cpptools` PID25648 at 142.48% of one logical CPU / 11.87% of whole-host capacity (working set395.8MiB, private552.3MiB) and `pythonw` PID16864 at54.00% of one core /4.50% host (working set358.7MiB, private521.9MiB). Other observed processes: Taskmgr PID18032 0.95% host, ChatGPT PID1164 0.79%, pythonw PID24668 0.65%, opencode-cli PID34684 0.57%, SystemSettings PID7296 0.51%, OpenCode PID32648 0.32%, codex PID15452 0.21%, and Code PID12760 0.11%. Process roles were not resolved. No synchronized host-total CPU counter was captured in this interval, so do not compare or sum these shares with the earlier 18:54 host-total sample. This demonstrates other substantial competing process load while MiMITA was absent; it does not establish the cause of the user's perceived speed improvement.
