# CPU profiler authorization coordination

**Date:** 2026-11-23 16:20 UTC  
**Agent:** `resource-efficiency-goal`  
**Issue:** `issue-ed3ac775283d`

## Finding

The Mode 3 collaborator reported that the localhost-only `adminadmin` server guard checked `MIMITA_DEV_LOOP_SERVER=1`, but root dev-loop sets that marker in the client environment. I independently verified the source path: `devscripts/dev-loop.py:1808` sets the marker for the client, while lines 5041–5046 assemble the server environment with nonempty run metadata and `MIMITA_PROCESS_ROLE=server`. Current `src/network/server-packet-chat.cpp:54–75` checks server role, nonempty run metadata, the actual bound loopback address, and the enabled ignored local config. The wrong-password denial behaved as expected; the earlier correct local unlock was rejected by the mismatched guard in the active DLL. The corrected source is pending in the shared batch and has not yet been runtime-proven.

## Coordination and evidence

- Appended the authorization mismatch and proposed guard requirements to CPU issue `issue-ed3ac775283d` and messaged the Mode 3 owner.
- Asked NPC owner `npc-elite-fcs` to confirm readiness for the shared root batch. Root status showed generation 9 pending with `src/npc/npc.h`, `src/npc/npc.cpp`, `src/network/server-npcs.cpp`, and `src/network/server-packet-chat.cpp`.
- The server auth correction has build note `5f136a2c480d`. No source edit, build request, restart, or CPU probe was performed by this agent in this session.
- The last observed runtime was run `20261123_144429`, server PID 27384 and client PID 29644; collaborator reports the client is connected as one non-admin. Verify fresh journal state again before sending any command.
- Next proof, once the shared batch publishes: prove wrong-password denial, prove the correct local unlock, run one bounded 15-second CPU probe, revoke admin access, and retain server/client journal evidence. If the client journal is still capped, report client callsite attribution as unknown.

## Connected pre-build baseline

Captured `.dev/perf-captures/cpu-connected-prebuild-20261123T1621Z.csv` for 15 seconds on the same server/client PIDs while no build was active. UTC sample rows span 16:26:03.223–16:26:18.312; the host reported 100% CPU throughout and 96.17% memory use with 585 MB minimum available RAM. Server PID 27384 averaged 159.42% of one core (13.28% host) and 128.9 MiB RSS; client PID 29644 averaged 112.95% (9.41% host) and 177.1 MiB RSS. Pair total was 22.69% host and about 306 MiB RSS. A server journal event at 16:26:47.849, 29.5 seconds later, confirms generation563, FCS, 19 NPCs and one player; exact in-window workload is not independently verified. Client journal is capped at 99,999,948 bytes and its newest readable event is stale at 16:20:22, so client FPS and top competing process attribution are unknown. This is a process-level baseline only, not function attribution. Evidence appended to `issue-ed3ac775283d`.

## Generation 564 build and crash evidence

Attempt `game-dll-20261123T162701884743Z-d98804` published generation564 in 724.396 seconds. Its per-attempt JSON records 642 units, 137 compiled, 506 skipped, changed-source preflight 201.832 seconds, compile 472.025 seconds, link 5.645 seconds, and publish 0.432 seconds. `build/game/generation-564/generation.json` reports `src/npc/npc.h` fanout 136; the attempt lists 133 invalidated objects with that header as the reason. This confirms broad transitive rebuild cost. The user context is related to both CPU and DLL-latency issues.

Windows Application Error events at 16:39:09.797Z and 16:39:09.847Z report read access violations in `game-g564.dll`. Using the retained DLL DWARF symbols, PID 3640 offset `0x3df54f` resolves to `serverRuntimeSimulateWorld` at `src/game/server-runtime-module.cpp:311`; PID 27384 offset `0x83a446` resolves to `NpcSystem::refreshDifficultyTuning` at `src/npc/npc-spawn.cpp:439`. Dumps remain at `%LOCALAPPDATA%\CrashDumps\mimita.exe.27384.dmp` and `mimita.exe.3640.dmp`. Server PID 27384 activated generation564 at 16:39:08.334Z. The near-immediate timing and the 13 new fields added to `Npc` in `src/npc/npc.h` make a hot-reload object-layout/state mismatch a strong hypothesis; this does not prove Efficiency mode was unrelated or that the layout change caused the fault. The shared run is stopped. No process restart or profiler command was performed.

Build stages and per-translation-unit seconds are tracked in the attempt artifact; CPU milliseconds per source line are not. No matched in-build host/process sample was captured. The runtime callsite profiler has not run, and its CPU share output remains unknown.

## Limits

The profiler has not yet run under the developer grant. No new CPU percentage or function-attribution claim follows from this coordination update. The measured target remains the applicable live workload; a 1% total-host target is not promised during active gameplay.
