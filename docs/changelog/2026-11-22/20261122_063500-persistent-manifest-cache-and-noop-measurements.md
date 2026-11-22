# Persistent build-analysis caches and latency measurements

## Goal

Continue issue `issue-136d4553ff92`: reduce MiMITA C++ edit-to-live latency while keeping source/object correctness checks and the MinGW DLL path.

## Changes

- Added a `.dev` packet-reference cache in `devscripts/packet_registry_codegen.py`. Unchanged C++ sources reuse cached packet tokens, while packet ownership precedence and packet-name matching are reconstructed each run. The log now reports input, generation, reference-scan, check/write, and total timings.
- Added persistent C++/header directory inventories and source-owner resolver data in `devscripts/source_manifest.py`. Inventory caches validate stored directory timestamps; resolver caches include owner config, module catalog, C++ source inventory, override existence, and resolved link-input paths in their fingerprint. Cache corruption or write failure falls back to recomputation. Source hashes, coherence validation, and required build stages remain fail-closed.
- Added focused tests for warm-cache reuse, source additions and changes, packet-name changes, owner precedence, and ownership-config invalidation.
- Hardened all optional cache readers against valid-JSON/wrong-shape corruption. Malformed entries trigger a full recomputation instead of aborting codegen or manifest preparation.
- Fixed failure-attempt timing summaries in `devscripts/dev_loop/game_build.py` so failed attempts no longer borrow prior-generation timing values.

## Verification

- `python -m unittest devscripts.quality.test_source_manifest_cache devscripts.quality.test_packet_registry_codegen devscripts.quality.test_game_build_snapshot` passed: 18 tests, including malformed-cache fallback.
- `python -m py_compile devscripts/source_manifest.py devscripts/quality/test_source_manifest_cache.py devscripts/packet_registry_codegen.py` passed.
- `python devscripts/packet_registry_codegen.py --check` passed against the tracked generated header and ledger.
- Restarted the existing root dev-loop through its control path after confirming no active DLL worker. Shared game server PID 26748 remained alive. DLL requests and waits used the root dev-loop only.

## Measurements

| Attempt | Result | Total | Key stages |
|---|---|---:|---|
| `game-dll-20261122T062417602825Z-5ae4cf` | No-op, generation 465 unchanged, 642 skipped | 28.546s | Packet codegen 1.303s; manifest 23.393s; worker 0.827s |
| `game-dll-20261122T062947781426Z-8dc1de` | Published generation 466 during a concurrent NPC source edit; 2 compiled, 641 skipped | 74.127s | Packet codegen 1.295s; manifest 5.876s; snapshot 9.714s; changed preflight 24.852s; object scan 5.340s; GNU link 13.393s; publish 0.392s |
| `game-dll-20261122T063133055665Z-4f45db` | Warm no-op, generation 466 unchanged, 642 skipped, no link | 7.268s | Packet codegen 1.204s; manifest 3.575s; source snapshot reuse 0.009s; worker 0.234s; object scan 0.017s |
| `game-dll-20261122T063925384350Z-ed838c` | Warm no-op, generation 468 unchanged, 642 skipped, no link | 8.975s | Packet codegen 1.305s; manifest 3.938s; source snapshot reuse 0.008s; worker 1.436s; object scan 0.096s |

The initial no-op's manifest breakdown was C++ enumeration 5.704s, owner resolution 12.262s, source ownership/digest 0.591s, header enumeration 3.555s, and header digest 0.987s. Profiling identified repeated globbing as the main resolver cost: 25 `expand_patterns` calls, 1,283 glob operations, and 18,872 `nt.scandir` calls. With persistent caches, the next changed-source attempt resolved owners in 0.146s and the subsequent no-op in 0.127s. Warm no-op C++ inventory validation was 1.309s; source ownership/digest 0.811s; header inventory 0.077s; header digest 1.079s. The warm no-op total crossed 10 seconds, but manifest remains above its sub-second target.

Packet-reference scanning on direct repository checks measured 1.677s cold and 0.511s warm internally over 668 source files and 297 packet tokens. Build attempts after the cache edit measured the whole packet codegen stage at 1.204–1.295 seconds. Older no-op attempts had recorded 15.250–29.704 seconds for packet codegen, but comparisons are load- and revision-confounded.

## Limits and next work

- Generation 466 was triggered by another agent's NPC source edit. It is a real build measurement, not this task's behavior proof or a matched benchmark.
- The warm no-op completed under 10 seconds while the shared server process remained live. It was not an edited-source save-to-activation sample or a controlled high-pressure Juggernaut/AI run.
- Generation 466 publication does not prove live activation or gameplay behavior. No runtime behavior claim is made.
- Next, reduce the warm manifest's directory-stamp and file-stat work while preserving change detection, measure changed-source snapshot/compiler/link stages under matched load, and record save-to-activation plus a behavior event from a controlled one-file edit.
- Required codegen, source coherence, compilation, cache correctness, and linking continue to fail the candidate build on error. Optional profiling/cache-write failures may fail open and trigger recomputation; stale objects are never published as a fallback.
- A third no-op after malformed-cache hardening stayed below ten seconds while the shared server PID remained live; it is still not a controlled pressure or edit-to-activation benchmark.
