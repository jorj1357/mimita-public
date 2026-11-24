# Runtime gate and next CPU experiment

# Task

- Task ID: session-4db3be6ecea2
- Session ID: session-4db3be6ecea2
- Owner: resource-profiler
- Focus: Evidence-backed MiMITA CPU optimization loop
- Summary: Revalidated the current root runtime after daemon reload, traced the highest-ranked existing CPU/tick candidate, and recorded the current low-memory server-only observation. The daemon now includes the sampler correction, but the restart dropped the client registration. The surviving run is Bloodtest with one NPC and no players, so no matched-pair optimization A/B was safe in this activation.
- Status: BLOCKED
- Date, time, timezone: 2026-11-24T14:38:09+00:00
- Runtime check: At 14:37Z, server PID25840 remained alive; client PID34756 was absent. Fresh server journal rows at 14:37:33Z report Bloodtest/slaughterhouse1, DLL generation594, NPC module generation593, 1 NPC/0 players. Host had 821MiB available of 16,025MiB. No build was active; host CPU rate and top-process attribution were not collected.

# Evidence and next experiment

- Existing CPU issue `issue-ed3ac775283d` already ranks removing the redundant per-tick `NpcDifficultyConfig::pollReload()` call first. The measured 09:30:56.994Z tick had a 39.7208ms config-poll wall-time outlier in a 58.6017ms total tick; this is not CPU-time attribution. The call exists in `src/network/server-npcs.cpp:2103`, while the existing server policy poll is invoked from `src/network/server.cpp:1400` and `:2301` via `src/game/server-policy-module.cpp:444`. Commit `45580410` introduced the extra simulation call.
- Hypothesis: deleting only the redundant simulation poll reduces filesystem checks and poll-attributed tick-tail stalls while preserving config reload through the policy path. Falsify if three or more matched before/after windows show no reduction beyond noise, reload latency worsens, or 60 Hz tick/gameplay behavior regresses.
- Required before edit/build: confirm `server-npcs.cpp` owner and lease, reattach one client through root dev-loop without replacing server PID25840, verify actual mode/workload/build/module generations and available RAM, then collect paired baseline. Do not attribute CPU reduction from wall-time alone.
- Existing next check has been shared with `root-mode3-latency`; no source edit, DLL build, WPR capture, or extra runtime was started.

# Files changed in this activation

- Shared issue record: appended one point-in-time host memory/process identity observation to `issue-ed3ac775283d`; this is an evidence handoff, not an optimization result.
- No C++ source, build configuration, profiler code, or runtime process was changed by this activation.
