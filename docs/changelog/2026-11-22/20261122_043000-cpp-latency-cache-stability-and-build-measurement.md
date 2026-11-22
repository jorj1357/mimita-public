# C++ edit-to-live latency: stable cache and measured builds

## Goal

Continue issue `issue-136d4553ff92`: reduce a small C++ edit to live activation while preserving the MinGW DLL path and only rebuilding correct dependents.

## Changes made

- Replaced generation-specific identity values in the shared module identity header with stable declarations, and emitted the per-generation values in a single generated provider translation unit. This avoids invalidating every wrapper for each DLL generation.
- Compiled the generated provider without the large project PCH. The provider remains generated and linked with each DLL; this does not replace the MinGW compiler or dev-loop.
- Added regression coverage for stable header contents and provider values in `devscripts/quality/test_game_build_snapshot.py`.
- Regenerated `config/hot-reload/packet-ledger.json` after an unrelated concurrent network edit made its generated line references stale. The codegen check then passed.

## Validation

- `python -m py_compile devscripts/game_build_worker.py devscripts/quality/test_game_build_snapshot.py` passed.
- `python -m unittest devscripts.quality.test_game_build_snapshot` passed: 9 tests.
- `git diff --check` passed (with the repository's LF-to-CRLF warnings).
- Generation 451 was the one-time migration from literal header values: 13 compiled units (changed source, 11 wrappers, identity provider); 630 skipped.
- A behavior-neutral comment probe in `src/npc/npc-behavior-module.cpp` plus a concurrent GUI edit produced generation 452: 3 compiled (the two changed sources and provider), 640 skipped, and `identity_wrapper_count=0`. Root server PID 26748 activated generation 452 at `2026-11-22T04:21:06.122Z`; save-to-activation was approximately 112.3 seconds. This confirms cache correctness, not acceptable latency.
- A separate single-source `src/npc/npc.cpp` change produced generation 453: 2 compiled (the changed source and provider), 641 skipped, no cache invalidations, and root PID 26748 activated generation 453 at `2026-11-22T04:24:52.359Z`. File timestamp was `04:23:20Z`, yielding approximately 92 seconds from edit to activation.
- Generation 454 followed four GUI source/header changes. It correctly rebuilt 26 units: four changed sources and 22 dependents of `src/gui/ui-system-internal.h`; 617 were skipped. Cache reasons explicitly identified that header. This is required dependency correctness, not wrapper churn.

## Timings

| Attempt | Manifest snapshot | Changed-source preflight | Object scan | Compile | GNU link | Publish | Total |
|---|---:|---:|---:|---:|---:|---:|---:|
| Generation 452 (two changed sources) | 5.74 s | 19.97 s | 3.08 s | 21.79 s wall (34.4 s summed task time) | 24.62 s | 0.69 s | 85.55 s |
| Generation 453 (one `npc.cpp`) | 12.72 s | 25.19 s | 2.40 s | 26.04 s phase (25.10 s for `npc.cpp`, 0.825 s provider) | 11.79 s | 0.61 s | 89.49 s |
| Generation 454 (four GUI files, one header) | 5.15 s | 10.79 s | 2.17 s | 40.56 s wall (336.2 s summed across 26 tasks) | 9.50 s | 0.55 s | 81.41 s |

Generation 452 provider compile was 1.825 seconds; generation 454 was 0.270 seconds without the project PCH. The provider compile improvement is encouraging but these runs had different system load and are not a matched compiler benchmark. Link time varied from 24.62 to 9.50 seconds across builds; treat it as load-sensitive evidence, not a guaranteed linker improvement.

The generation 452 manifest subphases were `enumerate_cpp` 2.49 seconds, source ownership/digest 1.02 seconds, header digest 1.66 seconds, and mirror update 0.088 seconds (5.51 seconds inside `build_manifest`; 5.74 seconds total snapshot). Generation 453 took 12.72 seconds total / 8.40 seconds inside `build_manifest`, with enumeration 4.65 seconds, source ownership/digest 1.77 seconds, header digest 1.48 seconds, and mirror update 0.27 seconds. Generation 454 took 5.15 seconds total / 1.99 seconds inside `build_manifest`. In all three, C++ inventory was invalidated by the `config` directory and recursively rescanned its 101 directories (2.49, 4.65, and 1.28 seconds). Current manifest paths are all under `src`, `external`, `tools`, `include`, and `launcher`, and a direct search found no `config/**/*.cpp`. The issue now tracks an explicit source-root/incremental refresh investigation while preserving detection of future source files. The 5.15-12.72 second spread under changing resource load means no single run justifies saying manifest snapshot has a fixed cost.

## Runtime and host evidence

- Generation 452 activation and subsequent hotness summaries were in the same server PID 26748; the latter showed 19 NPCs and zero server failures. Generation 453 and 454 activation records also used that same PID. These prove live DLL activation without closing the game, not the intended aggressive-NPC behavior change.
- A short 2-second process CPU delta sample observed 12 logical processors and three `MiMITA.exe` processes using about 6.29 CPU cores combined (about 52% of aggregate logical-processor capacity); `pythonw` processes used about another 0.59 cores. Working sets for the three game processes were roughly 127, 509, and 305 MiB. This sample did not include a total-system CPU counter, did not isolate one runtime, and was not a controlled Juggernaut/AI stress case. It cannot establish that the game alone or the compiler alone saturated the PC.
- A later process inventory showed the dev-loop server and additional isolated test-run processes. They were left untouched to preserve agent-owned runtimes.

## Current conclusions and next work

- Stable module identity fixed the 11-wrapper-per-generation invalidation. A one-file change now compiles the changed unit plus one small provider and skips the other 641 units.
- The one-file `npc.cpp` compile itself took 25.10 seconds. Its compile command/PCH and template/header cost need source-level profiling before proposing a split or compiler change.
- The 1.675-second dependency-index prepass and 2.2-3.1-second object scan are no longer the largest costs in these runs. Manifest snapshot was 5.15-12.72 seconds, changed-source preflight 10.79-25.19 seconds, changed C++ compile 25.10 seconds for `npc.cpp`, and full DLL link 9.50-24.62 seconds. The config-triggered 1.28-4.65-second C++ inventory rescan is one actionable waste, but fixing it alone cannot reach the sub-1-second manifest target.
- A full DLL link below one second is not demonstrated. Smaller/incremental link architecture or a verified faster linker such as LLD remains an experiment; don't assume Ninja alone avoids the full DLL link.
- Blink/function patching could avoid relinking all 642 objects for function-only edits, but compiler/debug format compatibility and state safety remain open. `cr.h` still requires a DLL build and reload, so it does not remove this measured compile/link path. Flecs can help represent migratable game state but does not perform arbitrary C++ layout migration.
- No Juggernaut/AI pressure benchmark ran. Next, profile one representative function compile (including PCH cost), capture all pre-worker intervals under a safe single-runtime pressure run, and compare a validated function-patching prototype with the DLL fallback. Do not claim the <=10-second target until a matched edit-to-activation run demonstrates it.

## Generation 456 follow-up

Generation 456 changed only `src/network/multiplayer-tick.cpp`, compiled that source plus the identity provider, skipped 641 units, and had no cache invalidations. The existing root server PID 26748 activated it at `2026-11-22T04:39:10.897Z`. The attempt took 184.712 seconds. It spent 29.950 seconds creating the source snapshot; its marker records 2,899 hardlinked files. Snapshot pruning then took 8.269 seconds and removed one old snapshot. Manifest snapshot was 6.047 seconds; source snapshot is its own additional stage. Changed-source preflight/compile took 47.005 seconds (`multiplayer-tick.cpp` itself 46.871 seconds; provider 3.478 seconds). GNU link took 75.881 seconds for 642 inputs / 480,242,189 input bytes; publish took 1.593 seconds.

This one-source attempt was far slower than generation 453 despite the same two compiled units and link input count: the main TU rose from 25.10 to 46.87 seconds, and link rose from 11.79 to 75.88 seconds. A separate process sample at `2026-11-22T04:45:04Z`, when no DLL build was active, measured 98.2% total CPU on 12 logical processors. Five MiMITA processes used 8.02 CPU cores combined; root server PID 26748 alone used 3.27 cores and 169 MiB working set. Three surviving processes were identified: shared root server PID 26748, FCS route-proof server PID 15756, and its client PID 31164; two sampled game processes exited before command-line attribution. This is evidence of high live-game CPU demand, but the sample was six minutes after generation 456 and does not prove the cause of that build's slower compiler or linker. Capture an OS CPU/thread profile during a build to establish causality.

The specific next optimization target is `devscripts/dev_loop/game_build.py`: `_create_source_snapshot` at lines 232-265 walks and hardlinks 2,899 files for each changed revision, and `_prune_snapshots` can spend several seconds deleting an old snapshot. Evaluate an immutable incremental overlay or lazy/batched cleanup, while preserving source coherence and the base snapshot required by preview builds. These newly measured stages are now recorded in issue `issue-136d4553ff92`.

## Scope notes

This work did not change the active compiler, main build path, game architecture, or runtime behavior. Concurrent agents modified game sources and generated documentation; those unrelated edits are not attributed to this task. No game processes were stopped and no additional runtime was launched for this measurement task.
