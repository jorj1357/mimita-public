# Juggernaut frame and memory diagnostics

- Task: Add source-located performance scopes, reconnect resource probes, and an external process-memory sampler; build and inspect a fresh Juggernaut run.
- Result: `PASS_WITH_FOLLOW_UP` — build and live performance evidence collected. A severe client memory-pressure episode was observed; the exact allocation owner and reconnect path were not proven.
- Time: `2026-10-22T21:08:22Z` (America/New_York display: 2026-10-22 17:08:22 EDT)
- Branch: `20261011zt`
- HEAD: `cbfa4b0df4e56b6f217c45dbe986b49df599b50a` (no commit created)

## Source changes

### `src/perf/perf-frame.cpp`

- Resource-window scope identity now includes source file, source line, function, and scope name. The window JSON emits those values and separates scopes that reused a label at different call sites.
- The fixed per-window unique-scope table was raised from 128 to 256; dropped-scope accounting remains present.
- This reports call counts for instrumented scopes, not every function in the program. Existing global allocation counters still count `new` calls/bytes and do not identify allocation stacks or retained bytes; their scope deltas can overlap or include concurrent-thread allocations.

### `src/network/multiplayer-packets.cpp`

- Added bounded `network.reconnect.resource-probe` events on reconnect start/send outcomes and before/after session teardown.
- Probe fields include remote actor/interpolation counts, avatar and snapshot payload counts/bytes, and pending request counts. No reconnect tokens or credentials are logged.

### `devscripts/perf-watch.py`

- Added a Windows-only, standard-library sampler that reads one target PID every second and writes a run-specific `host-resource-samples.csv` beside the canonical game journal.
- Samples include process working set/private/pagefile usage, system available physical/pagefile memory, and process CPU as a percent of one logical core. It runs outside MiMITA, so it continued sampling after game events stopped.

## Build and runtime evidence

- Dev-loop profile: `npc-navigation`; mode 9 Juggernaut; visible; map `dust2cyberiav4`.
- Build 2849 succeeded through `python devscripts/dev-loop.py --request build --wait-for-build`; output reported 1 compiled unit and 563 skipped units. The produced client/server build identity was `2849:6111d7906acb2821b49bb670724543fc050f06624f800d264d25fdf248610dac`.
- Exact build path: `C:\mimita-v9\.dev\builds\2849\mimita.exe`.
- `--versioninfo` identity probe: `logs/10-22-2026/20261022_165931/events.jsonl`.
- Gameplay run: `20261022_205937`; canonical journal: `logs/2026-10-22/20261022_205937/events.jsonl`; client PID 9128, server PID 32016. Runtime source-located scope fields were present in its `performance.resource-window` events.
- The latest complete 60-frame resource window at `2026-10-22T21:00:40Z` measured 8.46 ms average frame time, 20.66 ms p99, all 60 frames over the 4.17 ms 240-FPS budget, and 7.81 ms average main-thread CPU per frame. It counted 203,039 global `new` calls and 20.6 MB requested in that window; this is allocation churn, not retained-memory proof.
- Largest named self-time scopes in that window: Rendering 56.57 ms/60 frames; Network::RemoteEntities 41.47 ms; Swap 36.79 ms; Rendering::Effects::EffectParts 35.75 ms; Simulation::SimulateTick 35.40 ms (31 calls); RagdollCorpseUpdate 30.50 ms (31 calls); Rendering::Actors::PlayerBody 28.77 ms (4,020 calls, 67 calls/frame).
- `Replay::RemoteNPC::BodyParts` ran 2,077 times in the 60-frame window at `src/engine/engine-tick-replay.cpp:793`, 67 times per each of 31 fixed simulation ticks. Its inclusive time was 7.01 ms in the window. This confirms replay capture visits all 67 remote NPCs each simulation tick in this run; the measured scope alone does not prove it caused the memory spike.
- The outer sampler wrote 91 one-second rows from `2026-10-22T20:59:58Z` through `21:01:28Z`, then 10 additional rows from `21:06:00Z` through `21:06:09Z`. In the first capture, client private memory peaked at 12.03 billion bytes and system available physical memory fell to about 8 MB. In the later capture, private memory reached 16.67 billion bytes (15.53 GiB); working set rose from about 4.18 GiB to 5.24 GiB in 9 seconds. Process CPU averaged 82.4% of one logical core and peaked at 185.9% in the first capture.
- The client’s last game-authored event was `PERFORMANCE_SPIKE` at `21:00:41Z` (`Frame exceeded budget`). Dev-loop control records continued afterward, but no later client frame/resource summary appeared. The journal was about 12.76 MB, below the logger’s 100 MB per-file limit. The log record reported six earlier dropped records; the reason for those drops is unknown.
- A requested `connstate reconnect` command was queued but not delivered. Therefore the new reconnect probes were present in the executable but did not run, and this run does not establish the memory owner during reconnect.
- The test client remained present; no Windows application-crash event was found in the inspected interval. It was stopped through the dev loop after sampling to relieve memory pressure. Windows available physical memory recovered to about 6.1 GiB. This is evidence of a severe memory-pressure/hang episode, not a confirmed crash or a proven code-level leak.

## Validation and limits

- `git diff --check` passed for the changed C++ sources.
- `devscripts/perf-watch.py` parsed successfully and produced the reported samples.
- The scope source fields and reconnect-probe string were verified in build 2849; source-located scope fields were confirmed in the live journal.
- No synthetic gameplay test was added. No release build, reconnect reproduction, allocation-stack profile, or human visual acceptance was completed.
- Next evidence needed to name the memory owner: capture allocation stacks or add retained-size/count snapshots to the likely replay/NPC storage owners, then reproduce reconnect in a responsive run. Keep external sampling active so a stopped game journal does not erase the timeline.

## Pre-existing work

The user's existing edits and untracked changelogs were preserved. No unrelated source or documentation files were changed, and no commit was created.
