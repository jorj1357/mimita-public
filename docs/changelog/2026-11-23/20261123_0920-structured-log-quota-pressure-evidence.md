# Structured-log resource and quota evidence

- Agent: `resource-efficiency-goal`
- Issue: `issue-ed3ac775283d`
- Runtime: `20261123_055730_01`, server PID 8804 and client PID 3600 in the WPR-aligned window; post-quota sample used server PID 8804 and client PID 10148. Dev-loop showed game-DLL generation 554 published, but active runtime generation was not verified.
- WPR-aligned workload (07:02:51–07:03:27 UTC): actual live mode `jugger_horde`, map `dust2cyberiav4`, 67 NPCs and one player; host 12 logical CPUs. For the 09:27 post-quota sample, live mode and NPC/player counts were unknown because journaling had stopped.
- Analysis captured: 2026-11-23 09:18 UTC.

## Matched journal and WPR window

The retained WPR trace `.dev/perf-captures/mimita-cpu-10ms.etl` covers 07:02:51.327–07:03:27.760 UTC (36.433 seconds, 10 ms sampling). The aligned server journal segment is `logs/2026-11-23/20261123_055730_01/server-01/events-0029.jsonl`.

The interval contains 20,841 server events and 17,027,887 journal bytes (16.239 MiB): 572 rows/s and 0.446 MiB/s of JSONL output. This measures serialized journal volume, not physical storage bandwidth. Five event emitters account for 9,973,610 bytes (58.6% of interval journal bytes):

| Event | Rows | Bytes | Share | Emission owner |
|---|---:|---:|---:|---|
| `npc.movement-commitment-created` | 4,528 | 3,492,762 | 20.5% | `src/npc/npc.cpp:1257` |
| `npc.attack-gate` | 1,968 | 1,906,149 | 11.2% | `src/npc/npc.cpp:1433`, `emitAttackGate` |
| `npc.collision.fall-sample` | 1,494 | 1,578,162 | 9.3% | `src/physics/movement/physics-collision-glb-main.cpp:626` |
| `weapon.contact.shape` | 2,389 | 1,570,219 | 9.2% | `src/network/server-physical-contact-actors.cpp:601` |
| `npc.shot` | 1,316 | 1,426,318 | 8.4% | `src/npc/npc-combat.cpp:134` |

These percentages are of journal bytes, not CPU time. The old WPR report for this exact interval estimated the pair at 29.11% of whole-host CPU (client 18.72%, server 10.39%). It does not give a stable logger function CPU share: only 327 server game-DLL frames were in the earlier 10-second instruction sample.

## Writer path and hypothesis

`StructuredLogger::writeEvent` requests flush for `Important` rows at `src/debug/structured-log.cpp:1580`. The five events above are logged at `IMPORTANT`. `writeJsonLine` serializes, enters shared write/journal/quota locks, appends via `fwrite`, then invokes `fflush` for requested rows (`src/debug/structured-log.cpp:1723-1801`). This is per-record stdio flush; it is not evidence that each row reached physical media. At 572 rows/s this path is a concrete potential CPU and tick-time cost. Function attribution is too sparse to quantify it yet.

## Journal quota

At 09:18 UTC, the entire date tree `logs/2026-11-23/` contained only this active run: 75 files totaling 6,999,999,730 bytes, against the configured 7,000,000,000-byte tree limit; only 270 bytes remained. The active server segment `events-0069.jsonl` last changed at 09:00:28.307 UTC and client segment `events-0068.jsonl` at 08:58:57.511 UTC. Dev-loop still reported both processes alive. WPR currently reported stopped. The 270-byte remainder is too small for a normal event. `reserveEventLine` can prune completed runs and returns false if a record still cannot fit (`src/debug/structured-log.cpp:489-547`); only the current active run was present. Inference: new event writes are likely being quota-rejected, and `writeJsonLine` increments `mQuotaDroppedRecords` on reservation failure (lines 1770-1776). There is no post-stop drop counter, so rejected-record count is unknown. No logs were deleted.

`config/debuglogger.json:11-12` explicitly sets 100,000,000 bytes per segment and 7,000,000,000 bytes per date tree. The debug-logging spec says its default tree quota is 1,000,000,000 bytes; this is a configured override and may be intentional for captures, so it was not changed.

## Runtime outcome and evidence limits

In the same active interval, client mean FPS was 158.32; 5,069/5,714 frames (88.7%) exceeded 4.167 ms. The server averaged 9.651 ms per tick window, while per-window p95 averaged 16.288 ms and peaked at 36.068 ms. Thus this workload already missed the client frame-time target and approached/exceeded server tick tails. This does not prove logging caused those misses.

GPU usage, physical disk bandwidth/latency, and network throughput remain unknown. The exact build phase during this earlier WPR sample is also unknown; no build-causation claim is made. A later bounded stack-enabled WPR request is still waiting at the standard secure-desktop UAC consent prompt (PID 22480); WPR is not recording. No elevation bypass was attempted.

## Next check

Run a matched same-generation A/B with only selected repetitive `IMPORTANT` diagnostics sampled or throttled while preserving transitions/errors. Capture whole-host and per-process CPU, thread/function stacks, server tick distribution, every client frame time, physical disk bytes/latency, memory, and journal byte rate. Confirm identical map, mode, player/NPC count, cap, and competing processes. Do not infer improvement from fewer log bytes alone. No source/config changes or builds were made for this evidence record.

## Post-quota host and process sample

A 30-second, one-second sample ran at 2026-11-23 09:27:11.896–09:27:42.184 UTC while the dev-loop reported no build in progress and the existing server/client processes remained alive. The exact current gameplay mode, generation, and NPC/player counts are unknown because the run journals had stopped at the 7 GB quota; the GUI launch identity still listed one NPC and is not proof of the in-game state.

- Host average CPU: 75.69% across 12 logical CPUs.
- Server PID 8804: 323.71% of one core mean (p95 348.44), or 26.98% whole-host capacity.
- Client PID 10148: 372.54% of one core mean (p95 398.44), or 31.05% whole host.
- Pair: 58.03% whole-host capacity, about 76.7% of the measured busy host capacity.
- Mean working set/private bytes: server 3,341.4/4,718.8 MiB; client 709.5/1,889.4 MiB. Pair totals 4,050.9 MiB working set and 6,608.2 MiB private bytes. Host available RAM averaged 3,845 MiB.
- System-wide physical-disk throughput averaged 50,971 B/s read and 270,194 B/s write; Wi-Fi traffic averaged 9,915 B/s and Ethernet was zero. These counters are not process-attributed. GPU utilization and thread CPU distribution remain unknown.

Raw CSV artifacts: `.dev/perf-captures/mimita-postquota-host-20261123T0922Z.csv`, `mimita-postquota-server-20261123T0922Z.csv`, and `mimita-postquota-client-20261123T0922Z.csv`. This is not a matched comparison to the earlier 67-NPC WPR interval and cannot establish current idle usage or game behavior. It does show high pair CPU and memory outside a build, with workload identity now unknown.
