# issue-acf28dc22c85: Dev-loop dashboard should cost under 1 percent CPU, with a terminal-only path as the default

## Human report (verbatim, immutable)

> pthon is also up there at 10.3 percnt, closing it = i close the mimita dashboard / i want to switch to using the terminal version of the gui, but not sure how / or just optimize the usage of teh gui? not sure / the main goal with dev loop dashboard was a quicker method of launchigntesting modes but it expaned a lot / i want to makme it so its just a terminal, and make resoucre pressure super low as muhcas i can


## Agent suggestion

### 2026-11-25T21:10:46.546Z — dashboard-fps-relaunch

Read-only post-dashboard-close runtime sample, 2026-11-25T21:05:26–21:05:28Z. Root run 20261125_210344; client PID36380, server PID23536; build ff20ae8525ee; map arena1; journal logs/2026-11-25/20261125_210344/client-01/events.jsonl. The dashboard policy was false and its window had been closed. Ten recent performance.frame-slice windows showed average frame wall-time 3.98–6.65ms, p95 8.36–14.05ms, max 10.94–28.23ms. Logged scope maxima included EngineTick::Render 16.17ms, Rendering::Shadows 0.82ms, and Setup::ConfigPolling 3.55ms; no Swap-named scope appeared in these ten samples. The process was Vehicle Arena - Rocket League Free Play, not an NPC workload. DLL build attempt game-dll-20261125T210055875592Z-33009d generation646 completed at21:02:21Z; the run started at21:03:44Z, so this is not a concurrent-build startup or render sample. These are frame-scope wall times, not CPU attribution. No matched dashboard-open sample or per-process CPU measurement was collected; dashboard causation and the reported 60 FPS remain unproven.

## Agent suggestion

### 2026-11-25T22:37:52.989Z — dashboard-cpu-persistence

CPU evidence, 2026-11-25 UTC: after changing dashboard coordination snapshot throttle from 2.0 s to 30.0 s and relaunching the checkout-owned dashboard, PID 35740 used 0.672 CPU seconds over 15.03 s (22:35:59.660–22:36:14.778Z): 4.47% of one logical CPU and 0.37% of whole-machine capacity on a 12-logical-CPU host. The prior post-10-second sample for PID 34728 was 4.406 CPU seconds / 15.03 s = 29.32% of one logical CPU, 2.44% whole-machine; earlier pre-throttle sample PID 31064 was 11.52% whole-machine. Direction suggests fewer coordination snapshots reduce dashboard CPU, but these are not perfectly matched workload experiments and attribution is not proven. Runtime at sample: run 20261125_221046, Vehicle Arena Free Play / vehicle_freeplay, arena1, 0 NPCs, 1 managed server PID15988 and 0 clients; game DLL generation 654 published and no build active. Frame/tick data and top competing processes were not sampled for this window; unknown. Journal: logs/2026-11-25/20261125_221046/dev-loop/events.jsonl. Hypothesis: reducing repeated coordination snapshots lowers GUI CPU while leaving the 1-second main status refresh unchanged. Repeat matched idle/active samples and inspect duration of stale coordination panel before retaining 30-second interval.
