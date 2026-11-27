# issue-acf28dc22c85: Dev-loop dashboard should cost under 1 percent CPU, with a terminal-only path as the default

## Human report (verbatim, immutable)

> pthon is also up there at 10.3 percnt, closing it = i close the mimita dashboard / i want to switch to using the terminal version of the gui, but not sure how / or just optimize the usage of teh gui? not sure / the main goal with dev loop dashboard was a quicker method of launchigntesting modes but it expaned a lot / i want to makme it so its just a terminal, and make resoucre pressure super low as muhcas i can


## Agent suggestion

### 2026-11-25T21:10:46.546Z — dashboard-fps-relaunch

Read-only post-dashboard-close runtime sample, 2026-11-25T21:05:26–21:05:28Z. Root run 20261125_210344; client PID36380, server PID23536; build ff20ae8525ee; map arena1; journal logs/2026-11-25/20261125_210344/client-01/events.jsonl. The dashboard policy was false and its window had been closed. Ten recent performance.frame-slice windows showed average frame wall-time 3.98–6.65ms, p95 8.36–14.05ms, max 10.94–28.23ms. Logged scope maxima included EngineTick::Render 16.17ms, Rendering::Shadows 0.82ms, and Setup::ConfigPolling 3.55ms; no Swap-named scope appeared in these ten samples. The process was Vehicle Arena - Rocket League Free Play, not an NPC workload. DLL build attempt game-dll-20261125T210055875592Z-33009d generation646 completed at21:02:21Z; the run started at21:03:44Z, so this is not a concurrent-build startup or render sample. These are frame-scope wall times, not CPU attribution. No matched dashboard-open sample or per-process CPU measurement was collected; dashboard causation and the reported 60 FPS remain unproven.

## Agent suggestion

### 2026-11-25T22:37:52.989Z — dashboard-cpu-persistence

CPU evidence, 2026-11-25 UTC: after changing dashboard coordination snapshot throttle from 2.0 s to 30.0 s and relaunching the checkout-owned dashboard, PID 35740 used 0.672 CPU seconds over 15.03 s (22:35:59.660–22:36:14.778Z): 4.47% of one logical CPU and 0.37% of whole-machine capacity on a 12-logical-CPU host. The prior post-10-second sample for PID 34728 was 4.406 CPU seconds / 15.03 s = 29.32% of one logical CPU, 2.44% whole-machine; earlier pre-throttle sample PID 31064 was 11.52% whole-machine. Direction suggests fewer coordination snapshots reduce dashboard CPU, but these are not perfectly matched workload experiments and attribution is not proven. Runtime at sample: run 20261125_221046, Vehicle Arena Free Play / vehicle_freeplay, arena1, 0 NPCs, 1 managed server PID15988 and 0 clients; game DLL generation 654 published and no build active. Frame/tick data and top competing processes were not sampled for this window; unknown. Journal: logs/2026-11-25/20261125_221046/dev-loop/events.jsonl. Hypothesis: reducing repeated coordination snapshots lowers GUI CPU while leaving the 1-second main status refresh unchanged. Repeat matched idle/active samples and inspect duration of stale coordination panel before retaining 30-second interval.

## Agent suggestion

### 2026-11-27T00:13:14.225Z — dashboard-resource-gold-spec

Latest five-minute sample with new dashboard optimizations: PID1828 used 81.58 CPU seconds over300s=27.19% of one logical CPU=2.266% whole-host on12 CPUs. It remains above the <1% whole-host budget. Raw .dev/resource-measurements/20261127T000100Z-coordination-lazy.jsonl; sample 2026-11-27T00:02:42.616Z–00:07:42.608Z. Host CPU mean/p95/max90.83/99.8/100%; physical RAM available minimum0.4MB; commit max38,698MB with contemporaneous limit39,697MB. Runtime changed from a server-only FCS run to a TDM client/server pair during collection, so this is not a matched A/B. The coordination panel was collapsed and no dashboard.coordination events were emitted. More profiling is needed to isolate the remaining dashboard status-refresh CPU; do not infer host pressure was caused by the dashboard.

## Agent discussion and suggestions

### 2026-11-27T00:32:50.821Z — dashboard-resource-gold-spec

Message: `msg-10e068dfc34c` (question)

Subject: Dashboard CPU sample and GUI ownership

I ran a 5-minute read-only dashboard-open sample on run 20261127_001718. Are you actively editing/restarting devscripts/dev_loop/gui.py or reproducing the dashboard FPS/self-open behavior right now? I want to avoid conflicting with your task before making the next refresh-path change.

## Agent suggestion

### 2026-11-27T00:35:46.790Z — dashboard-resource-gold-spec

Direct observation, UTC 2026-11-27 00:21:44.841–00:26:44.906: dashboard PID 1828 stayed visible for a 300.06 s sample on a 12-logical-CPU host, run 20261127_001718, exact dev-loop identity mode team_deathmatch / gamemode tdm / map atdm, managed server PID 11572 and client PID 29544. The collector wrote 284 one-second nominal rows to .dev/resource-measurements/20261127T002100Z-dashboard-open-current-run.jsonl; collector overhead averaged 0.260% whole-host CPU (p95 0.521%). Dashboard CPU averaged 28.019% of one logical CPU = 2.335% whole-host, p95 58.498% one-core = 4.875% whole-host, peak 84.375% one-core = 7.031% whole-host. Working set mean/p95/max 94.93/112.28/134.08 MB; private memory mean/p95/max 114.30/130.39/159.92 MB. This misses the <1% whole-host CPU and p95 private <=100 MB budgets; working-set p95 <=125 MB passed. Host CPU mean/p95/max 99.711/100/100%; physical RAM available mean/min 2294.56/55.31 MB; commit used max 15969.2 MB of a contemporaneous 16024.51 MB limit; pagefile used max 7363.88 MB. The sample overlapped game DLL generation 744 attempt game-dll-20261127T002302708617Z-988794 from 00:23:02.708 through sample end (about 3m42s); post-sample dev-loop status showed it building. No pre-build baseline or system-wide top process sample was collected, so build/resource causation and untracked external top contributors are unknown. Known same-window MiMITA contributors: managed client PID 29544 averaged 20.027% whole-host CPU (p95 26.542%) and ended at 4599.92 MB private; managed server PID 11572 averaged 6.682% whole-host (p95 10.509%) and ended at 1396.25 MB private; unverified server PIDs 20596 and 5440 averaged 2.686% and 1.646% whole-host CPU and ended at 517.83 MB and 303.02 MB private. These process facts do not prove dashboard causation.

Client journal logs/2026-11-27/20261127_001718/client-01/events.jsonl produced 175 performance.fps windows from 00:21:44.579 to 00:24:39.519 (telemetry did not cover the full resource sample): mean/min/max FPS 150.998/59.988/203.335; mean per-window avg frame 6.75 ms, mean per-window p95 8.999 ms, mean per-window p99 13.165 ms, worst reported max frame 875.987 ms. Server journal logs/2026-11-27/20261127_001718/server-01/events.jsonl reported one player and mean nine NPCs across 168 tick windows; over-100-ms ticks averaged 0.754%, max 10%. This run includes momentary dips near 60 FPS and substantial spikes, but the concurrent DLL build and incomplete telemetry prevent attribution to dashboard visibility.

Separate direct timing observation from dashboard PID 1828 stdout, 00:29:18–00:33:35 UTC: 20 dashboard.refresh.slow records, max 2061.9 ms; 43 dashboard.stage.slow records, including process_rows max 1146.6 ms, game_queue_panel max 329.1 ms, configured_modes max 255.2 ms, process_scan_request max 228.4 ms; three dashboard.status.slow records averaged 4552 ms and maxed at 6031 ms. These are wall-time stage observations after the resource sample, not CPU attribution, and the dashboard stdout was not persisted as a separate journal artifact. The precise expensive sub-operation inside the status provider remains unknown. Next step: split timing around status_provider, game-DLL progress/queue reads, process scan, and Tk row rendering; use matched idle/active workload samples after other dashboard GUI ownership is clarified.

## Agent suggestion

### 2026-11-27T00:37:23.714Z — dashboard-resource-gold-spec

Correction to this sample entry: the reported 15969.2 MB used of 16024.51 MB was mislabeled as system commit charge/limit. The collector derived it from psutil virtual_memory total minus available and total RAM; these are a physical-RAM-use estimate and installed/visible RAM total, not Windows commit charge/limit. System commit for this sample was not captured and is unknown. The raw artifact .dev/resource-measurements/20261127T002100Z-dashboard-open-current-run.jsonl has been corrected: fields are now physical_ram_used_estimate_mb, and host commit is explicitly marked not captured. All other figures in the evidence entry are unchanged.

## Agent suggestion

### 2026-11-27T00:44:24.216Z — dashboard-resource-gold-spec

Cross-agent ownership check timed out. agent-signal.py sent the exact GUI path/run/build question to matching owners, then an explicit ask was sent to dashboard-fps-relaunch at 2026-11-27T00:34Z on issue issue-acf28dc22c85 with a 00:37:50Z deadline. No answer appeared in this agent inbox by 00:41:45Z; the only unread coordination item was an unrelated map-editor user context. The board still lists dashboard-fps-relaunch as active with only a received phase, no task issue/path/run/build reported, and no edit lease on devscripts/dev_loop/gui.py. Record this as no response/timeout, not permission or proof of inactivity. I will restrict the next patch to low-overhead timing fields in the existing dashboard instrumentation and will not close/restart the shared GUI or game until ownership is resolved.

## Agent suggestion

### 2026-11-27T00:55:33.621Z — dashboard-resource-gold-spec

Follow-up code and build evidence. Source devscripts/dev_loop/gui.py now adds thread CPU time to the existing slow UI-stage and refresh events. The asynchronous status reader records separate wall/thread CPU durations for status_provider, game_dll_progress, and game_dll_queue_view; the UI emits dashboard.status.stage when a stage takes at least 50 ms wall time or 10 ms thread CPU, rate-limited per stage. python -m py_compile devscripts/dev_loop/gui.py devscripts/dev-loop.py passed. This is diagnostic instrumentation, not a CPU reduction, and the running dashboard PID 1828 has not loaded it yet. No restart was made because the current dashboard screen showed one active DLL build and another agent's FPS/self-reopen work remains unanswered; the live game pair was preserved.

The sample-overlapping generation 744 attempt game-dll-20261127T002302708617Z-988794 is confirmed in logs/2026-11-27/20261127_001718/dev-loop/events.jsonl. It began manifest_snapshot at 00:23:06.140Z and eventually emitted dev-loop.game-build.failed at 00:39:59.232Z, with link failed in 25.703s; this confirms it continued after the sample ended. The link output included undefined symbols for ClientCollisionWorldView vtable and PlayerAiController::setMeleeWeapon/setActive. No matched prebuild or per-process compiler resource sample exists, so do not attribute dashboard or game pressure to this failed build. A different attempt game-dll-20261127T005142888202Z-0200fc was building at 00:51:53.139Z; preserve it for its owner. The build-owner signal has not received a reply yet.

## Agent suggestion

### 2026-11-27T01:19:56.137Z — dashboard-resource-gold-spec

Post-reload direct dashboard timing evidence, UTC 2026-11-27 01:15:17–01:18:29, PID 7820, run 20261127_001718, team_deathmatch / tdm / atdm, generation 748 published, no active compile at sample precheck. dashboard.status.stage emitted 80 records: status_provider wall mean/max 81.7/420.7 ms and thread CPU mean/max 34.1/93.8 ms; game_dll_progress 89.2/1063 ms wall and 36.3/78.1 ms thread CPU; game_dll_queue_view 0.1 ms wall, thread CPU field 15.6 ms at Windows timer resolution. The separate 300-second process sample 2026-11-27T01:06:58.665Z–01:12:01.537Z measured dashboard PID 7820 at 0.817% whole-host CPU mean, p95 1.741%; private bytes p95 53.29 MB, working set p95 75.48 MB. Raw resource sample: .dev/resource-measurements/20261127T010558Z-dashboard-refresh-open.jsonl. Per-stage data came from dashboard stdout session (not persisted as a dedicated artifact). The process mean and RAM targets pass in this run, but the host was saturated at 99.996% CPU and mean available physical RAM was 726.65 MB; no causal comparison with the earlier build-overlapped sample is claimed.

Falsifiable next hypothesis: asking the status provider and DLL progress/queue path once every two seconds instead of every one second, while preserving the one-second Tk repaint and control handling, will reduce dashboard mean whole-host CPU from 0.817% to <=0.60% over a matched five-minute live run, keep status age <=2.5 seconds, and retain private p95 <=100 MB and working-set p95 <=125 MB. The sampled status-provider plus progress CPU suggests the opportunity; the next matched sample will decide. If any response becomes older than 2.5 seconds or a queued/active build state is missed, revert the interval change. The next sample must use the same run/mode and no active build; do not claim the saving until measured.

## Agent suggestion

### 2026-11-27T01:37:18.386Z — dashboard-resource-gold-spec

Resource sample after dashboard PID 6996 restarted with the two-second status-poll guard. Direct Windows API collector artifact: `.dev/resource-measurements/20261127T012806Z-dashboard-two-second-poll-idle.jsonl`; sample 2026-11-27T01:29:35.583Z–01:34:49.577Z, 300 rows, 1.051s mean interval, p95 1.176s, max 1.339s. Host has 12 logical CPUs. Direct observation: dashboard PID 6996 averaged 0.491% whole-host CPU (5.890% of one logical CPU), p95 1.032%; private bytes p95 49.62 MB and working-set p95 32.99 MB. Host CPU averaged 99.971% (p95 100%); physical RAM available averaged 2064.48 MB and minimum was 653.02 MB; system commit peaked at 41094.70/43064.11 MB. Two other `mimita.exe` PIDs were present: 5900 averaged 6.986% whole-host CPU and 1390.66 MB private p95; PID 34092 averaged 14.461% whole-host CPU and 880.20 MB private p95. Their role, purpose, launch reason, mode and journal are unknown; no managed dev-loop run or FPS/tick data existed in this sample. The dev-loop later showed game DLL attempt `game-dll-20261127T013303208483Z-a405dd` building generation 749, started 01:33:03.208Z, overlapping the last 1m46s of this sample and remaining in progress at the 01:36:22 status read. This sample is not a matched A/B and cannot attribute host pressure or FPS to the dashboard or the build. Dashboard CPU/working set are lower than the preceding PID7820 sample (0.817% host mean, 75.48 MB working-set p95), but collector and workload/build overlap differ; treat as directional only. Current instrumentation says status stage CPU was previously about 34–36 ms mean per provider/progress stage; this post-change sample did not capture stdout timing or status-age evidence, so the <=2.5s freshness guard remains unverified.

## Agent suggestion

### 2026-11-27T01:53:31.624Z — dashboard-resource-gold-spec

Second optimization hypothesis: focused profiling of `_game_dll_progress()` while no managed run was active measured 50–77 ms total wall per call, with `_recent_game_generations()` consuming 33–46 ms of that. The history directory contained 298 game-DLL summary JSON files; sorting their modification times took 35.64 ms and reading the newest eight took 14.68 ms mean in a direct microprofile. I added a 5-second cache only around those completed-generation summaries. Live `progress.json`, `current.json`, daemon `status_provider`, and queue state remain read on each status refresh. Expected result: fewer repeated history directory stats/reads and dashboard mean CPU <=0.45% host in a clean no-build/no-game sample, with private p95 <100 MB and working-set p95 <125 MB. Status freshness is a separate guard: the prior trace during a saturated build showed `dashboard.status.slow` mean 1589.5 ms, p95 3532 ms, max 4157 ms, and stage-event gaps up to 4.557 s. This is not proof of UI age because the events are thresholded/rate-limited, but it means <=2.5 s freshness is not established. Do not claim it passes; next verify displayed/status timestamps under an active build or reconsider how slow DLL history work is attached to base status.

## Agent suggestion

### 2026-11-27T02:05:12.095Z — dashboard-resource-gold-spec

Second dashboard sample after adding the five-second cache for completed DLL history only. Raw artifact `.dev/resource-measurements/20261127T015409Z-dashboard-history-cache-idle.jsonl`; sample 2026-11-27T01:55:22.131Z–02:00:37.961Z, 300 rows, 12 logical CPUs, mean interval 1.056s (p95 1.171, max 1.475). Dashboard PID 12196 averaged 2.197% whole-host CPU (26.36% of one logical CPU), p95 4.868%; private p95 117.83 MB, working-set p95 130.14 MB. Host CPU averaged 99.924%; physical available RAM averaged 809.69 MB, minimum 6.47 MB; system commit peaked 45254.95/46033.16 MB (~98.3%). The process set included MiMITA PIDs 14572 (11.361% whole-host CPU mean, p95 17.130%, private p95 11277.19 MB, working-set p95 2962.27 MB) and 25844 (8.251% CPU mean, p95 10.049%, private p95 311.46 MB). Their roles, launch reasons, modes, command lines and journals were not captured. This sample is not comparable to the previous 0.491% dashboard sample: this host had ~6 MB minimum physical RAM available and ~98% commit, two high-CPU MiMITA processes, and a new managed run began only during the final 23 seconds (run 20261127_020014, Vehicle Arena Rocket League Free Play, arena2); a DLL build started after the sample ended. Do not attribute the result to the cache or dashboard code. The dashboard's thresholded stdout during this window showed status-slow events mean 3175 ms, p95 4282 ms, max 11672 ms; stage events mean 1285 ms, p95 3158 ms, max 10027 ms. Poll freshness is not met/verified under this pressure; investigate splitting base status from expensive DLL progress/history work. The history cache remains a narrow optimization hypothesis, not a measured CPU win.

A separate live run while dashboard PID 29964 was open: run `20261127_014331`, server PID30580/client PID19564, `Vehicle Arena - settling fuzz probe` / vehicle_freeplay, arena2, one player and zero NPCs in client samples. Client journal `logs/2026-11-27/20261127_014331/client-01/events.jsonl` has 1272 `performance.frame-slice` records from 01:43:44.580Z through 01:48:21.944Z (33,686 frames / 270,558.3 measured ms; weighted 124.51 FPS). Window average frame time median/p95/max was 7.13/16.22/3031.01 ms; average per-window p95 frame time was 31.88 ms; worst frame was 4819.60 ms. Scope means per recorded call: EngineTick::Render 5.47 ms, Rendering 2.47 ms, Shadows 0.636 ms, World 0.669 ms, Setup::ConfigPolling 1.21 ms. The run used build ID `2026-11-27T01:43:27.658897Z:f50b710...`; the run manifest later recorded `build_match=false`, and a game-DLL build started near the end. This is not an A/B and does not establish dashboard causality or represent a normal human gameplay run. The visible `Swap` scope was not among the recorded scope names; the user’s 60-FPS report remains unresolved.

## Agent suggestion

### 2026-11-27T02:13:20.177Z — dashboard-resource-gold-spec

Correction to the 20261127T015409Z sample: the maximum commit-charge row is 45254.95 / 46033.16 MB at 01:56:50.778Z (98.31%), but that is not the maximum utilization. The highest commit-to-limit ratio is 44949.43 / 44989.67 MB = 99.91% at 01:56:37.307Z. The associated spec/changelog note will distinguish peak charge from peak utilization.
