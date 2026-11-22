# Generation 440 timings and pre-worker instrumentation

## Changes

- `devscripts/source_manifest.py` reports the exact changed inventory directories (up to 32 names plus a truncation flag). This adds evidence to the existing inventory scan metrics without changing manifest contents or freshness rules.
- `devscripts/dev_loop/game_build.py` now writes bounded `driver.subphase.started/completed` events for snapshot pruning, manifest diffing, change-report writing, attempt progress/context writing, module-slot wait, worker command construction, and worker launch to first output. Current progress also records the active driver subphase. Telemetry write failures do not fail the build.
- The existing `devscripts/dev-loop.py` lifecycle fixes remain: the daemon records the script hash loaded at startup, and server adoption restores the verified executable path needed for a later client start.

## Verification

- `python -m py_compile devscripts/source_manifest.py devscripts/dev_loop/game_build.py` passed (run separately for each file).
- `python -m unittest devscripts.quality.test_game_build_snapshot` passed: 6 tests.
- Generation 440 was built by the root dev-loop and activated in the retained FCS server/client process pair. Attempt: `game-dll-20261122T013844014631Z-d09614`; revision `7e7b5f25b5b9038a9df66a27c961754310d0877221dc1d2decbf7624d4a20fbd`; total 524.297s.
- Server PID 26748 and client PID 30496 both recorded `game.generation.activated` for generation 440 in `logs/2026-11-22/20261122_011031/{server-01,client-01}/events.jsonl`.
- The exact changed-directory telemetry and driver subphase events have not yet been exercised by a live build after these latest Python edits.

## Measurements

- Manifest snapshot: 61.391s; C++ enumeration: 48.675s for 1,008 sources, one changed directory, unstable result. The changed path had not been recorded before this change.
- Source snapshot: 99.235s to create 2,899 hardlinks and zero copies for a changed revision.
- Changed-source preflight: 111.719s for two source units.
- Object-cache scan: 19.922s. Its 642-depfile prepass was 13.672s; path/fanout work was 9.691s, including 9.476s path normalization; 629 objects were skipped and 11 were stale.
- The compile phase rebuilt 11 generated module wrappers in 70.766s of parallel wall time. 13 units total were compiled and 629 skipped.
- Link: 55.281s (GNU linker 54.844s) over 642 objects / 480,144,356 input bytes; staged DLL size 242,552,459 bytes.
- Publish: 8.015s, including 5.157s to regenerate the external dependency manifest and 2.078s to rename/hash the DLL.
- A 38.460s interval remains between source snapshot completion and worker-input validation. The new driver timers are intended to identify its owners on the next stable attempt.
- Five-second pressure sample during the DLL/EXE overlap: host CPU 89.4% across 12 logical processors, 1.23 GiB RAM available; FCS server 139.4% of one core and client 386.5% of one core. The game was active (19 server NPCs; one NPC slice logged at 94.887ms against a 10ms budget). This does not isolate build-caused load.

## Separate EXE request and live state

An already-queued EXE request (`2026-11-22T01:33:57.208Z-bc836913`) ran after generation 440. The build worker reported success (12 compiled, 75 skipped, 190.844s compile, 6.578s link), but the dev-loop remained in `building` after the worker reported `build_complete`. The daemon PID 4008 disappeared and the watcher restarted it as PID 16872 at 02:01:23Z. No new `.dev/builds` artifact was published; the retained server continues to identify the older EXE build. The new daemon adopted server PID 26748. Its one client reattach attempt (PID 9252) crashed during logger startup with an uncaught MinGW `std::filesystem::last_write_time` exception. `addr2line` resolved the throwing comparator to `src/debug/log-manager.cpp:104`, called by `LogManager::init()` at line 260 and `main()` at line 636. The failing path was not captured; a concurrent log-rotation race is plausible but unproven. A safe fix should cache timestamps using the `error_code` overload before sorting. The server was preserved and no further client was launched.

Two subsequent no-source-change DLL requests returned `unable to contact daemon`; neither created a build attempt. A separate FCS test runtime was active, so no further DLL publication/reload was issued while its proof might be in progress. No clean no-op timing has been collected in this activation.

## Next work

Use a stable, authorized runtime for a warm unchanged-source attempt and then a one-file attempt. The priority fixes remain (1) remove per-generation identity from the 11 wrapper dependency graph while preserving stable ABI/runtime checks, (2) reuse the persistent dependency index instead of reparsing/normalizing all 642 depfiles, (3) make changed-revision source snapshots incremental or remove the full-tree snapshot requirement safely, and (4) measure and reduce the 642-object GNU link path or prototype function-level patching. A full link below one second is not supported by current measurements. Resolve the EXE worker-to-terminal state boundary and identify the `last_write_time` argument before claiming reliable executable reattachment.

The active compiler/build path remains MinGW and the existing root dev-loop/DLL hot-reload architecture. No dependency library was imported and no build architecture was migrated in this work.

## Follow-up — generation 442 pressure case

While the root dev-loop was active, a one-source attempt for `src/npc/npc.cpp` completed successfully as generation 442. Attempt `game-dll-20261122T024115879642Z-8937c9`, revision `f9b0d4267a56da7c9d96598e979bc4d1863aaf478576ce24a434b6f5ddf8576f`, took 412.492s end to end. It compiled 12 of 642 units (the changed TU plus 11 wrappers invalidated by generation identity), skipped 630, and linked the full 642-object set (480,173,016 input bytes). Server PID 26748 journaled activation from generation 441 at 02:48:14.718Z; behavior of the NPC edit was not separately asserted.

Measured stages: `npc.cpp` changed-source preflight 135.541s; wrapper compile 29.237s; GNU link 144.119s; publish 73.180s. Publish included 63.311s external dependency-manifest generation and 9.503s DLL rename/hash. In the 02:43:04–02:44:00Z preflight sample, host CPU remained 99.6–100% and available RAM ranged 0.19–0.54 GiB; a `cc1plus.exe` sample used 461–682 MiB, and `ld.exe` was later sampled at 1.18 GiB working set during linking. Other MiMITA processes were active, with ownership not re-established for all PIDs. Pressure is a confound, not proven cause.

This is a successful live DLL activation without closing the server, but not a controlled low-latency benchmark. The root dev-loop is configured for 10 jobs. The result confirms that scan/pre-worker work was small (manifest snapshot 2.528s; pre-worker 1.400s) while changed-TU compilation, full linking, and dependency-manifest publication dominate under this workload. Follow-up priority: remove the generation-only wrapper dependency safely, profile and reduce publication manifest work, then prototype a narrow behavior module/function patch path so a one-function edit avoids linking all 642 objects. Benchmark `cr.h` only with a small isolated plugin under MinGW; benchmark Blink separately with its MSVC/PDB requirements. Keep the main compiler/build path unchanged until such trials prove viable.

## Follow-up — generation 446 and publish scan reuse

`devscripts/game_build_worker.py` now takes the external dependency manifest from `scan.external_dependencies`, avoiding a second parse of 642 dependency files after link. Generation 446 exercised this path successfully in the existing root run: attempt `game-dll-20261122T032247328342Z-79f775`, revision `a7150409b063036ea53a031ceaa538c38455d8d5e81e470485ef7c9d5b3b0bd6`, source `src/network/multiplayer-packets.cpp`, 12 compiled / 630 skipped. Syntax compilation and six `test_game_build_snapshot` tests passed.

| Stage | Duration | Notes |
|---|---:|---|
| changed-source preflight | 74.960s | changed translation unit: 74.739s |
| object-cache scan | 6.583s | 642 depfiles; 4.467s dependency prepass; 1,904 unique paths |
| compile | 110.789s | 11 generated identity wrappers recompiled; 630 skipped |
| GNU link | 126.979s | 642 objects, 480,180,314 input bytes |
| publish | 9.681s | rename/hash 9.282s; manifest reuse 0.028s (`source=object_cache_scan`) |
| end to end | 360.398s | worker 331.196s |

Manifest generation measured 0.028s versus 57.828s in generation 443 and 63.311s in generation 442. The repeated depfile work is removed for this path. Server PID 26748's run journal later showed `game.hotness.summary` with generation 446 and zero server failures, proving the DLL activated in the same process. This does not prove the network behavior itself. The dominant remaining costs are wrapper invalidation and a 126.979s full link; those need separate structural work before approaching a 10s target.
