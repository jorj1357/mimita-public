# CPU experiment candidate and current run gate

UTC: 2026-11-25 00:46
Issue: issue-ed3ac775283d

## Direct source/history observation

Commit `45580410` (2026-11-22) introduced an FCS movement-metrics block in `NpcSystem::updateOneNpc`, now `src/npc/npc.cpp:3813-4033`. For each matching FCS NPC update it maintains displacement/dwell/contact metrics and calls several `glm::length` calculations; when its one-second accumulator expires it builds a JSON object and writes `npc.movement-metric`. The calculation is outside a `shouldLog` guard. The checked-in `config/debuglogger.json` sets `npc_movement` to `important`; the per-update calculation is unconditional for the two profile IDs even if the category is disabled.

This is a plausible CPU candidate, not a causal conclusion. Existing bounded callsite evidence from the 2-NPC Bloodtest run estimated the entire `updateOneNpc` function at 2.46% of server CPU (27 site samples across 14 complete windows). That upper-bounds the candidate's possible contribution in that run; it does not explain a ~45% whole-host four-process observation. The FCS phase values are wall durations, not CPU time.

## Current runtime gate

At 2026-11-25 00:42 UTC, root dev-loop reported run `20261124_221014`, server PID27372, client PID26492, launch identity funcounterstrike, and failed source-preflight state. The fresh server journal reported funcounterstrike, 19 NPC, 0 players, host DLL generation627, and NPC module generation625. One 61-tick window averaged 10.69ms with p95 31.14ms and max 58.62ms; NPC phase total was 211.52ms, including 80.62ms collision and 111.20ms physics. No client frame evidence was aligned to that timestamp.

A process listing also found PIDs15988 and38696 using the same pinned executable build directory, but neither appeared in the runtime registry and their run/mode/owner are unknown. The runtime registry returned empty. Do not treat these as a matched pair or stop them based on this observation.

## Proposed first experiment (not implemented)

Add a specific, default-off switch for this diagnostic block, preserving the metric as opt-in. First capture a clean, connected, generation-aligned baseline on the same pinned executable and workload. Then compare on/off with the same pair, mode, NPC/player count, frame cap, and no concurrent build. Measure per-process and whole-host CPU, per-thread CPU if available, memory, diagnostic event rate, client frame-time distribution, server tick rate/tails, and gameplay/journal correctness. Prediction: turning off metrics reduces server CPU by more than baseline noise and removes only `npc.movement-metric` events; gameplay state and tick/frame behavior remain unchanged. Disprove if the CPU delta is within noise or behavior/timing gates regress.

Do not request a build from this activation: the root run is source-stale, not player-connected, and not generation-aligned; there are additional unregistered MiMITA processes. The root `agent-self-improvement` spec already requires each optimization build to contain one evidence-selected change and matched before/after runtime proof. This activation made no runtime optimization change and has no before/after result.

## References

- Source history: `git show 45580410 -- src/npc/npc.cpp`
- Current runtime journal: `logs/2026-11-24/20261124_221014/server-01/events.jsonl`
- Prior function-level evidence: issue `issue-ed3ac775283d` and `.dev/perf-captures/cpu-bloodtest-pair-20261124T1513Z.csv` plus the corresponding server probe rows documented in that issue.
- Process inventory source: Windows `Get-Process mimita*` and targeted `Win32_Process` read at 2026-11-25 00:42 UTC; no new CPU sample was taken in this activation.

## Follow-up evidence, 2026-11-25 00:54–01:00 UTC

A targeted host/process sample ran 00:54:42.688–00:55:01.093 UTC (18.405s, 12 logical CPUs). Host CPU averaged89.79% (peak94.9%); available RAM averaged1205.8MiB (minimum1169MiB); commit charge averaged61.47%. Root server PID27372 used13.58% whole-host and root client PID26492 used20.37% whole-host, 33.95% combined. Their working sets at sample end were130.0MiB and70.7MiB; private bytes481.5MiB and896.6MiB. PowerShell sampler PID4144 accumulated1656ms CPU (~0.75% whole-host), so measurement overhead was material to a 1% target.

This sample is partial: its target list was stale while short `parallel-run` pairs were rotating. At least one second pair (run20261125_005333, Bloodtest; its server/client PIDs were later observed as6532/35508) was live around this interval but not included in process deltas. The artifact `.dev/perf-captures/cpu-known-four-pids-20261125T005442Z.json` therefore proves only the two root PID shares, not total MiMITA CPU. No top competing process list or client frame sample is aligned to this exact interval.

In the same root client journal, a later frame slice at00:58:33 UTC reports43 frames, average4.442ms, p95 6.564ms, p99 10.416ms, with16/43 frames over4.167ms. Scope totals include Audio::MusicConfigPoll10.90ms (inside Audio::MusicUpdate11.08ms) and Setup5.27ms. `src/audio/music-manager.cpp:584-592` runs `std::filesystem::last_write_time(mConfigPath)` from `MusicManager::update` each frame. This is elapsed scope time, not thread CPU. The server journal near00:55:50 reports FCS,19 NPC,0 players, host DLL627/NPC module625; the client frame event reports one player/zero NPCs. They do not prove a connected same-workload pair.

Next code candidate remains an opt-in-compatible 250ms maximum music-config timestamp poll interval; expected benefit is lower repeated filesystem-poll wall/CPU cost with config reload delayed no more than250ms. First A/B must be same PID, build and workload, with process/thread CPU, frame-time distribution, reload proof and server tick/gameplay gates. The A/B is not yet implemented or proven. Another measurable scope candidate is Setup::ConfigPolling; its existing scope is broad, so locate the dominant child before changing code.
