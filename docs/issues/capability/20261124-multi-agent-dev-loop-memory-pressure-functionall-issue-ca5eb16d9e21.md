# issue-ca5eb16d9e21: Multi-agent + dev-loop memory pressure: functionally negligible PC impact as the target

## Human report (verbatim, immutable)

> make a issue that we need to investigate a super effiicent method of working with multiple ai agents and builds, bc meomory resource pressure is slowing funcitons on whole pc down, making swap functions a lot , so we need to passive investigate that furhter over time , the goal is mimita and ai processes are functionally negliible impact on overall computer performance , i acn run heavy programs simultanesously adn its fine , mimita doenst slow, builds dont slow, ai agents dont slow, whole pc is still smooth, using as little cpu and ram as possible


## Agent suggestion

### 2026-11-24T23:03:51.080Z — hot-reload-latency-root

Cross-link evidence from issue-16500baeb0cc (agent hot-reload-latency-root, read-only): sustained hard paging is directly implicated in Bloodtest choppiness.
DIRECT (this session): free physical RAM 56-226 MB of 16,025 MB across 8 samples (~5s apart); pagefile 81.5-82.2% used; "\Memory\Pages Input/sec" 5,512-16,932 sustained (~40 MB/s hard-fault traffic); Memory Compression working set ~3.5 GB. Two transient MiMITA processes (former run 20261124_221823, client PID 19396 + server 37004) exited on their own during the sample; four MiMITA PIDs existed earlier (26492/27372 current run 20261124_221014, plus 19396/37004). Current client PID 26492 sampled at 263% of one logical CPU (2.63 cores); server 27372 at 155% (1.55 cores), ~35% whole-machine across 12 logical CPUs.
CLIENT EFFECT (run 20261124_212828 Bloodtest, client-01): 401 60-frame windows, frame_average p50 19.08 ms but main-thread real CPU (GetThreadTimes) p50 7.29 ms -> 63.7% of frame time the main thread was not executing; frame_max p50 144 ms, max 1297 ms; 75 distinct scopes carry multi-hundred-ms single-frame stalls (Replay 767, PlayerAnimation 1177, Physics::CollisionSteps 792, Network::PacketDispatch 823). Fresh Bloodtest run 20261124_221823 shows the same (frame_average p50 34.28, main CPU 8.33, 75.7% not executing).
INFERENCE: the multi-hundred-ms stalls in unrelated scopes are thread descheduling/hard faults, not algorithm cost; that is the observable "swap" the user reports. During a concurrent game-DLL build (game-dll-20261124T214357271677Z-7f59d1) host CPU was mean 99.98% for 310.98 s and minimum available memory was 38 MiB.
TARGET CHECK FOR THIS ISSUE: establish a documented free-physical-memory floor and a sustained Pages-Input ceiling; re-measure Bloodtest frame_max/p95 after memory pressure is removed. Unknown: per-thread CPU ownership of the ~1.6 non-main-thread cores in the client; do not change hot code before that is measured.

## Agent suggestion

### 2026-11-24T23:08:09.779Z — hot-reload-latency-root

Memory-pressure evidence for this passive investigation (hot-reload-latency-root, read-only; no process stopped, no code edit).

DIRECT OBSERVATIONS (UTC 2026-11-24T23:05:49Z, 12 logical CPUs, 16,025MB physical)
- free physical 831MB (5.2%); system commit 36,825/52,536MB; Memory Compression working set 3,003MB (active compression, not idle).
- Working-set leaders: Memory Compression 3,003MB, MsMpEng 945MB, ChatGPT 686MB, opencode 539MB, opencode 493MB, cpptools 380MB, pythonw 367MB (72,708 CPU-seconds), OpenCode 359MB, opencode-cli 350MB, firefox 311MB, codex 212MB, Code 177MB.
- MiMITA role split at 22:48:12Z: 4 mimita.exe processes, combined 41.04% whole-host CPU and ~2.1GB private (client 26492 private 1,134MB; isolated client 26536 507MB; server 27372 351MB; isolated server 36224 106MB).
- Attribution defect relevant to the target: the isolated pair (run 20261124_224314, Bloodtest/1 NPC) has an empty agent in its run manifest, no claiming coordination activity row, and zero runtime_registry rows, so it is live but unowned. It is not a stale manifest; it is a genuinely running, unattributable pair. A second run (20261124_212828) is reported online with dead PIDs 8940/25432. Full detail appended to issue-b9edd402e658.

INFERENCE
- MiMITA is a material but not dominant resident-memory consumer here (~2.1GB private of 16GB); the over-subscription is driven by the simultaneous agent/editor set (3x opencode + opencode-cli + Code + codex + ChatGPT + cpptools + pythonw) plus MS Defender and Memory Compression. CPU is consistent with the same mixed load, not MiMITA alone.
- Unknown: exact committed bytes per role, page-file read/write rate per role, and whether any single agent session is unbounded over time. Per-role working set and peak growth are not recorded anywhere today.

SMALLEST NEXT CHECK (needs owner agreement; passive)
- Adopt a measured floor as the target definition, e.g. free physical RAM above a documented value with no sustained Memory Compression growth, and record per-role CPU/peak+steady working set for dev-loop daemon, build workers, each agent session, game server, and each client. Record one baseline now and one after each reduction; treat a missing sample as unknown, not zero. Cross-linked CPU/build context stays in issue-ed3ac775283d and issue-136d4553ff92.

## Agent suggestion

### 2026-11-25T00:30:44.174Z — build-wait-reviewer

Resource evidence handoff (build-wait-reviewer, direct observation; read-only, no process stopped).
CROSS-LINK: from review of issue-0567fb769b04 (wait game-dll-build:2026-11-24T22:28:54.886Z). Same run family already reported here by hot-reload-latency-root (run 20261124_221014, client 26492, server 27372).

DIRECT (attempt game-dll-20261124T222855538088Z-1d20e7, .dev/build-timings/game-dll-20261124T222855538088Z-1d20e7.resource.json):
- Sample window 2026-11-24T22:28:55Z -> 22:33:56Z, 199 samples @1s.
- mean host CPU 99.977%, peak 100.0%.
- minimum available memory 2,281,472 B (~2.18 MiB), single-sample minimum.
- Concurrent client pid 26492: performance.fps window 35.40 FPS, 38/38 frames over the 4.167ms budget, max frame 153.70ms, p95 94.52ms; 19 remote NPCs in view window; server tick window 41 ticks/570ms, ai_ms 363.6.

INFERENCE: a game-DLL build ran under ~full-host CPU and at least one sample with near-zero free memory, matching the multi-agent paging class in this issue. Unknown: whether memory pressure caused the 186.7s changed_source_preflight (no prebuild baseline; unmatched_prebuild_resource_baseline is in unavailable_fields), and the per-role commitment. No code edit or claim; owner decision on the free-RAM floor and Pages-Input ceiling remains open.
