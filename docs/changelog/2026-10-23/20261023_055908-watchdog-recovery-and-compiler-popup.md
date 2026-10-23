# Juggernaut stall watchdog recovery and compiler-popup investigation

Date: 2026-10-23

## User-visible concern

The user showed a Windows `g++.exe - Application Error` dialog with status `0xc0000142`. This is the compiler process, not the game executable. In this session the compiler starts successfully and reports GCC 16.2.0. The dialog's exact trigger is not proven; it appeared amid a release build that exhausted available memory headroom and was stopped. Treat the popup as a compiler-launch/resource incident, not evidence that MiMITA itself crashed.

## Recovery and resource evidence

Two inactive WPR trace files in the user's Temp folder occupied about 17.6 GiB physical space. Their sizes and timestamps were unchanged and WPR reported no recording. They were compressed in place using NTFS compression, preserving their contents. C: free space increased to about 11 GiB. No trace was deleted.

A release build request was sent through the canonical `devscripts/dev-loop.py` for profile `npc-navigation`. It started 12 compiler workers despite an attempted four-job environment cap. Free RAM fell to 287 MiB and the loop became unreachable. The build process tree started by this request was stopped; the loop reported failure (exit code 1), and no release executable was produced. After stopping, free RAM returned to about 5.4 GiB and free C: space was about 10.7 GiB. Do not treat this as a source compilation error. The compiler standalone `--version` check passed.

## Runtime and code evidence

The restored tracked version of `src/perf/perf-frame.cpp` was 601 lines. A prior uncommitted version had been accidentally truncated during a failed patch before this session; that uncommitted content could not be recovered. This file was restored from Git HEAD, then the resource watchdog was reconstructed. The prior uncommitted per-thread CPU/scope-source diagnostics were not all recovered, and no claim is made that they were.

The reconstructed watchdog emits rate-limited `performance.stall-watchdog` events while a frame does not complete. It records last completed frame, active top-level progress stage and source location, and main-thread CPU time. It attempts a stack capture once per stall. The watchdog begins when the render/main thread is registered and stops after engine shutdown while the structured logger remains active.

Debug build 2887's Juggernaut run `logs/2026-10-23/20261023_053635/events.jsonl` reached beyond 80 seconds. The server reported 67 active NPCs during the later sample; client journal logging continued beyond the earlier 50-60-second quiet period in this run, so quieting is intermittent and not proven fixed. Across 6,300 client frames from game time 22-82.3 seconds, mean frame time was 9.320 ms (~107 FPS); 6,034 frames (95.8%) exceeded the 4.17 ms 240-FPS budget. Client private bytes grew from about 361 MiB during startup to about 1.55 GiB by 82 seconds. This is evidence of growth, not proof of a memory leak.

The startup watchdog stack captured blocking file operations in the audio path: `engineTickAudio` -> `audioUpdate` -> `getCachedSoundData`; gameplay misses also reach `getCachedSoundData` through `Player::updateAudio` -> `playWorldSound`. That function synchronously checks/opens/reads a sound file and caches its bytes. This is a strong explanation for some startup or first-use stalls, but does not explain the full late-run 9.3 ms/frame average or establish the cause of memory growth.

Late-run server NPC simulation averaged roughly 1.5-2.6 ms and total server tick roughly 2.4-3.7 ms. This points more strongly toward client-side rendering/presentation work for the observed FPS deficit, while not excluding network and client simulation costs.

## Validation and remaining work

- `g++.exe --version`: passed.
- Debug builds 2885, 2887, and 2888 were published earlier through the dev loop; 2887 produced the cited real-game journal.
- Release build: stopped under low-memory conditions; no release runtime acceptance evidence.
- `git diff --check`: reports pre-existing trailing whitespace in `docs/specs/dev-loop/dev-loop.md`; line-ending warnings are also present. No new test was added or run.
- 240-FPS target: not met. Need a resource-safe, correctly capped release build and a fresh release Juggernaut run before comparing optimized FPS.
- Compiler dialog: standalone compiler starts now; exact cause of the earlier 0xc0000142 remains unproven.
- No commit created. Unrelated existing edits were preserved.
