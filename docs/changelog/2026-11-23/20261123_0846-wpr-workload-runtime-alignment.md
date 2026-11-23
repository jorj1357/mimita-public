# WPR workload and runtime alignment

- Agent: `resource-efficiency-goal`
- Issue: `issue-ed3ac775283d`
- Run: `20261123_055730_01`
- Trace: `.dev/perf-captures/mimita-cpu-10ms.etl`, 2026-11-23 07:02:51.327–07:03:27.760 UTC, 36.433 seconds, 10 ms sample period.

## Workload and CPU

The matching journal confirms server PID 8804 was running `jugger_horde` on `dust2cyberiav4` with 67 NPCs and one player. The client was PID 3600. Summing the per-module `Usage %` rows in the retained WPR module report gives client 18.72% of whole-host capacity (about 2.25 logical-core equivalents), server 10.39% (about 1.25 cores), and pair 29.11% (about 3.49 cores) on 12 logical CPUs. The Idle row is 49.46%, implying about 50.54% host CPU busy; MiMITA accounted for about 57.6% of estimated busy capacity. The report's module rows sum to 96.58%, leaving 3.42 percentage points unattributed or otherwise absent from the report. These are whole-machine normalized values; do not divide them by 12 again.

Other high process rows were `pythonw.exe` PID 16864 at 5.42%, `MsMpEng.exe` PID 6696 at 3.34%, `pythonw.exe` PID 23824 at 1.25%, and `Taskmgr.exe` PID 23012 at 0.84%. Their command lines and exact roles during the historical capture are unknown. No conclusion about build concurrency is made.

The ten-second instruction-sample slice counted 1,360 server samples; with the verified 10 ms sample interval this estimates about 1.36 core equivalents for the server, or 11.33% of the 12-core host. Its 327 `game-g546.dll` samples are 24.04% of server samples, or about 2.73% of host capacity. The full-trace module report's server game DLL row is 2.40% host, broadly consistent but based on a longer window and report estimator. Function samples remain diffuse (hash-table range hashing 8/327 DLL frames, `std::hash<int>` 5/327, `AabbTree::overlaps` 4/327).

## Client and server runtime

Across 36 client one-second FPS windows, mean FPS was 158.32, mean per-window p95 frame time 9.33 ms, mean p99 11.17 ms, worst frame 89.92 ms, and 5,069 of 5,714 frames (88.7%) exceeded the 4.167 ms / 240 FPS budget. This active workload missed the frame-time target.

Across 36 server one-second timing windows (2,196 ticks), `total_avg_ms` averaged 9.651; per-window `total_p95_ms` averaged 16.288 (maximum 36.068), and per-window `total_p99_ms` averaged 22.56 (maximum 42.828). Every window reported 67 NPCs and one player; no tick exceeded 100 ms. The average was under 16.667 ms, but tail timing often reached or exceeded the 60 Hz budget. Do not pool these per-window percentiles into an overall tick percentile. The reported slowest-tick lists are selected samples and are biased.

The client journal contained 95 resource windows. Its reported `main_thread_cpu_ms_average` averaged 5.954 ms per frame, using current-thread CPU time; this is not an elapsed scope timer or whole-process CPU. Mean private bytes in these resource windows were 1,080,504,104 bytes (about 1,030 MB). A separate 179-frame-slice aggregation reported mean working set about 440 MB and mean private bytes about 1,030 MB. Global allocation counters across those frame slices summed to 9,016,555 allocation events and 913.3 MB allocated bytes over about 36 seconds. This is a possible allocation-churn lead, not evidence by itself that allocations cause the CPU load or retained memory.

Largest per-scope allocation counters were `EngineTick::Simulate` (1,544,527 allocations / 589,274,240 bytes), `Networking` (712,106 / 448,388,120 bytes), `Network::MpTick` (628,776 / 432,210,528 bytes), and `Network::ReceivePoll` (529,208 / 384,382,964 bytes). These scopes nest and overlap; do not sum them or interpret them as CPU shares. The client reports `npcs_active=0` while the server reports 67, an unresolved counter-semantic or telemetry mismatch.

GPU resource windows reported zero captured GPU samples and GPU timer queries disabled; GPU usage is unknown, not zero. Disk bandwidth and network byte throughput were not measured. Server memory was not time-aligned in this sample.

## Measurement owners and next work

- `src/perf/perf-frame.cpp:456-493` writes 200 ms frame slices, allocations, process memory, and resource snapshots.
- `src/perf/perf-frame.cpp:552-560` measures current-thread CPU time for resource windows.
- `src/perf/perf-frame.cpp:572-633` assembles the resource-window event; GPU sample fields are conditional at lines 589-594.
- `src/perf/perf.cpp:1530-1531` writes one-second FPS summaries.
- `.dev/perf-captures/mimita-cpu-10ms-profile.txt` contains the full-trace process/module usage summary; `.dev/perf-captures/mimita-cpu-10ms-pid8804-sampledprofile-stack.html` and the retained ETL support the function-sampling analysis.
- The 1.44 GB temporary raw xperf text export was removed after streaming analysis; the original ETL was retained.

Repeat the matched 67-NPC capture after the current PCH batch and cleanup, then separately capture idle/background runtime. Add server memory, client GPU timing, and network/disk throughput. Prioritize source changes only when matched samples identify a stable hot owner; then compare CPU, allocations, memory, 240 FPS frame times, 60 Hz tick behavior, and gameplay under the same workload.

No MiMITA source code or build inputs were changed.

## Profiler telemetry gaps

All 179 client `performance.frame-slice` events in the matched interval report `logger_events_total=0`, `logger_bytes_total=0`, and `logger_flushes_total=0`, although those events are present in the canonical journal alongside 95 `performance.resource-window` events. Those counters cannot establish zero logging or disk activity: they either omit this logging route or are reported incorrectly. The current source reads them at `src/perf/perf-frame.cpp:475-476`; `StructuredLogger::ioStats` reads cumulative values at `src/debug/structured-log.cpp:960-978`; the JSON write path increments event/byte counts at `src/debug/structured-log.cpp:1793-1794`. Cause remains unresolved.

The 95 resource-window records are represented in the journal as one `fields.raw` string of exactly 16,384 characters; its embedded JSON ends mid-array, so trailing fields are unavailable. The source passes a JSON object at `src/perf/perf-frame.cpp:629-633`; the wrapping/truncation owner is not yet known. Do not interpret missing GPU, memory, or other trailing fields as zero. Track this as diagnostic pipeline debt before relying on these resource windows for future comparisons.

## Module-level audio and kernel check

The retained WPR module report assigns the client PID 3600 18.72% whole-host CPU; the largest module rows are `ntoskrnl.exe` 7.71%, `game-g546.dll` 4.57%, `ntdll.dll` 1.32%, `nvoglv64.dll` 1.13%, `FLTMGR.SYS` 1.00%, and `Ntfs.sys` 0.76%. Server PID 8804 totals 10.39%, led by `ntoskrnl.exe` 4.36%, `game-g546.dll` 2.40%, `ntdll.dll` 0.87%, `FLTMGR.SYS` 0.64%, and `Ntfs.sys` 0.52%. Audio-related client rows were `AudioSes.dll` 0.02% and `IntcAudioBus.sys` 0.01%; the system `audiodg.exe` process used 0.20% of host capacity. These values do not show audio as a dominant CPU owner, although audio code inside the game DLL remains possible and sampled functions were sparse. `Audio::StartSound` allocation counters are allocation evidence, not CPU evidence. Kernel and graphics driver rows (`ntoskrnl.exe`, `nvoglv64.dll`) need stack attribution before source or driver work can be recommended.

## Post-cleanup live capture privilege blocker

The post-cleanup stack-enabled WPR attempt was denied in the non-elevated shell: error `0xd0000061` (“A required privilege is not held by the client”) and `0xc5585011` while enabling `CPU.Verbose.File`. The user had already authorized normal UAC elevation. A single bounded hidden PowerShell capture request was then launched through `Start-Process -Verb RunAs`; Windows `consent.exe` PID 22480 remains active, but WPR reports not recording and neither the target ETL nor its log exists yet. The UI automation surface cannot see the secure prompt (`GetCursorPos` access denied; no consent window in `sky.list_windows`). The request will write `.dev/perf-captures/mimita-postcleanup-2026-11-23-0858.etl` and a companion log if the user clicks Yes, then stop after 15 seconds and reset the interval. No elevation bypass or code change was made.
