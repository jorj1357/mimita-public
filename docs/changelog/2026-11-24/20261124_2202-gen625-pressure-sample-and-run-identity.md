# CPU loop: build pressure and live workload identity

## Purpose

Continue the MiMITA resource goal with evidence that can select a CPU optimization, then prove a single source change against a matched live workload. The user's report that the whole PC feels faster after closing MiMITA remains an important observation, not yet a controlled before/after result.

## Does not

- This work did not change MiMITA runtime code or request a build.
- It does not establish that any particular function consumes a stated percentage of CPU.
- It does not prove a 240 FPS client or a consistent 60 Hz server.
- Builds from other agents are not CPU optimization experiments for this goal.

## Evidence

### Generation 625 build pressure

Attempt `game-dll-20261124T215807388728Z-84be9e` published generation 625 in 182.618 seconds. The two source inputs belonged to another task, so this is not a CPU-focused code change. Its resource sidecar has 142 samples on a 12-logical-CPU host: mean CPU 99.984%, mean RAM use 94.62%, mean available RAM 780.8 MiB and minimum 12,595,200 bytes, with mean pagefile use 36.45 GiB. The tracked client/server/child-server samples sum to about 32.11% whole-host CPU, but sidecar runtime alignment failed: server PID 5272 was Juggernaut Horde with 67 NPCs/0 players; client PID 16088 was Vehicle Arena and its child server PID 10704 belonged to that other workload. Do not treat them as a matched pair.

Artifact: `.dev/build-timings/game-dll-20261124T215807388728Z-84be9e.resource.jsonl`.

### Generation 626 build and process pressure

Attempt `game-dll-20261124T220623155095Z-9c49ca` completed at 2026-11-24 22:11:42.325 UTC after 319.167 seconds and published generation 626. Its summary lists `src/effects/effect-part-blood.cpp` and `src/main-systems.cpp`; this was another agent's batch, not a CPU optimization experiment. The resource sidecar contains 195 samples from 22:06:29.262–22:11:42.231 UTC, nominal cadence 1 second but actual mean interval 1.637 seconds. Host CPU averaged 99.998% (max 100%), RAM use 96.96%, available RAM averaged 405.2 MiB and fell to 0.62 MiB; pagefile use averaged 42.74 GiB. Across the tracked MiMITA processes, per-sample summed CPU averaged 23.78% of whole-host capacity. These data show MiMITA was a material CPU consumer amid a saturated host, but do not prove the effect of closing MiMITA or which function caused the pressure. Disk totals were unavailable; GPU/network and complete per-thread function CPU were not measured.

The sidecar attached the dev-loop state for run `20261124_215719_01` and reported Juggernaut Horde/one NPC. The live server journal later in that same root run instead reports `funcounterstrike`, 19 NPCs and one player. This is evidence that launch/profile metadata can be stale after an in-game mode switch; use authoritative journal events for workload identity.

Artifacts: `.dev/build-timings/game-dll-20261124T220623155095Z-9c49ca.json` and `.dev/build-timings/game-dll-20261124T220623155095Z-9c49ca.resource.jsonl`.

### Live server scope and tick evidence

The server journal `logs/2026-11-24/20261124_221014/server-01/events.jsonl` advances through 22:16:47 UTC. It reports live mode `funcounterstrike`, 19 NPCs and one player. `game.hotness.summary` reports host DLL generation 626, while `npc.phase-window` reports module generation 625; the behavior module is not generation-aligned. Recent NPC scope totals show collision around 63–192 ms and physics around 80–205 ms per emitted window, with sensing around 10–24 ms. These are elapsed scope totals, not function-level CPU time. `probe=true` in hotness does not prove that callsite rows were emitted; no `performance.cpu-callsite-window` rows were found in the inspected server tail.

Recent server tick windows vary: examples include 63 ticks with 8.19 ms mean, 20.44 ms p95 and 66.01 ms max; 51 ticks with 12.57 ms mean, 48.8 ms p95 and 153.08 ms max; and 40 ticks with 20.05 ms mean, 97.06 ms p95 and 312.16 ms max. The latest inspected windows do not demonstrate consistent 60 Hz. Client frame metrics were not recovered from this read, so the 240 FPS target remains unproven.

## Next step

After the pending game batch owner releases the build window, verify one connected client/server pair from fresh journals, authoritative current mode and NPC/player counts, same host and NPC module generation, and a writable event sink. Capture a low-overhead matched process baseline, then run the existing bounded server callsite probe and verify actual function rows. Use those measurements to select one falsifiable CPU code change, queue its build note before editing, request a single root-dev-loop build, and compare the same workload afterward. Keep or revert based on whole-host/process CPU and function attribution while separately checking every client frame against 4.167 ms, server tick behavior against 16.67 ms, and gameplay correctness. Until that matched profile exists, collision/physics are candidates from elapsed scope data, not confirmed CPU winners.

## Coordination

Evidence and the user's request are linked to CPU issue `issue-ed3ac775283d`; build latency cross-reference is `issue-136d4553ff92`. The pending `src/physics/physical-entity.cpp` batch belongs to another task and was left untouched. No capture, build request, runtime restart, or source edit was performed for this changelog.
