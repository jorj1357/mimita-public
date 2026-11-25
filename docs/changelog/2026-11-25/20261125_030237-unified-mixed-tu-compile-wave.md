# Task

- Task ID: issue-136d4553ff92
- Session ID: session-1424ddc45ff5
- Owner: root-mode3-latency
- Focus: Reduce avoidable compile-wave serialization for mixed source/header DLL batches.
- Summary: Kept early changed-source compilation only for verified `.cpp`-only changes. Mixed `.cpp`/header changes now scan freshness first and compile all stale translation units in one bounded wave. The existing fast GCC diagnostic cancellation remains in the shared worker.
- Status: Implemented and focused checks passed; real build-latency proof pending.
- Date, time, timezone: 2026-11-25T03:02:37Z
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: eb92b02b21e62fd5ec496eee4dcad832f901bb7a
- Final commit: Not committed in this session.

# Pre-existing changes

- The checkout contains extensive unrelated work from concurrent agents. It was preserved.
- `devscripts/game_build_worker.py` already had the GCC hard-error watcher and `_run_tracked_compile_process`; this session reused that shared change and did not originate it.
- No build was active at final status. The latest DLL was generation635; root status had no pending batch at the time of the final check.

# Requested behavior

Reduce edit-to-live C++ time toward the active goal of <=10 seconds for a coordinated 15–20 C++ file batch under live Juggernaut Horde client/server load, with continuous host/process resource data and verified same-process DLL activation. Stop wasting a full compile wave on changed `.cpp` files before compiling header dependents.

# Specification alignment

- Current specification: `docs/specs/dev-loop/build-time-observability.md`.
- Mixed source/header changes must compile every stale TU exactly once, preserve cache freshness decisions and the bounded worker pool, and record phase timing.
- The spec was updated to keep the source-only cache-marker path while putting mixed-batch stale TUs into one compile wave.
- This change does not alter build authority, compiler, DLL architecture, or runtime behavior.

# Exact implementation changes

## File: `devscripts/game_build_worker.py`

- Added `source_only_preflight_eligible`: use the separate early compile wave only when the change report exactly matches the changed existing `.cpp` set and proves there are no header/source-set changes.
- Mixed source/header batches skip the early changed-source compile, scan the dependency cache, and enqueue directly changed and header-dependent stale TUs together. Existing freshness checks and `preflight_handled` filtering remain for the strictly source-only marker path.
- GCC hard-error cancellation in the existing shared diff is retained; it kills the failing compiler and lets the compile worker cancel siblings.

## File: `devscripts/quality/test_game_build_cache_marker_fast_path.py`

- Added coverage that exact source-only edits retain preflight, mismatched source sets reject it, and a mixed header/source report rejects it.

## File: `docs/specs/dev-loop/build-time-observability.md`

- Updated the preflight and cache-scan contract to describe one mixed-batch compile wave and immediate GCC diagnostic cancellation.

# Measured evidence

- Baseline attempt `game-dll-20261125T023802184416Z-39264c`, generation635, ran 2026-11-25T02:38:02.184Z–02:42:17.678Z: 255.459 seconds total.
- Four changed paths (three `.cpp` files and `src/combat/weapon-fire.h`) yielded 17 compiled and 628 skipped TUs.
- Changed-source preflight took 116.777 seconds; the remaining compile phase took 95.041 seconds; LLD link took 26.332 seconds; object scan 2.576 seconds; dependency prepass 0.948 seconds; publication 0.577 seconds.
- `src/combat/weapon-fire.h` caused 13 explicit object invalidations, listed in the attempt JSON. This is independent header-fanout work and remains open.
- The build resource sidecar had 196 samples, mean host CPU 99.93%, minimum available RAM 49.8 MiB, and three live MiMITA pairs. Target Juggernaut server PID36452/client PID25552 averaged 29.02% whole-host CPU; two additional Bloodtest pairs were also present. This is a pressure-correlated baseline, not an isolated single-pair trial.
- Resource gap key: `build-resource-sampling:game-dll-20261125T023802184416Z-39264c`. The sidecar did not contain a matched pre-build resource baseline, host commit-charge/limit, or independent client-mode confirmation. Do not infer that pressure caused the build duration.
- After publication, the Juggernaut server journal reported host DLL generation635 but NPC module generation632. Client FPS at 02:50:09–02:50:15 UTC was 13.8–31.4 with p95 frame times 67–117 ms. Runtime acceptance and same-generation module activation were not met.
- The prior two-stage baseline is recorded in `.dev/build-timings/game-dll-20261125T023802184416Z-39264c.json` and its `.resource.jsonl` sidecar. No post-change DLL build has yet measured the unified-wave policy.

# Validation

- `python -m unittest devscripts.quality.test_game_build_fast_diagnostic_abort devscripts.quality.test_game_build_cache_marker_fast_path` — 8 tests passed in 0.583 seconds.
- `python -m py_compile devscripts/game_build_worker.py devscripts/quality/test_game_build_cache_marker_fast_path.py` — passed.
- `git diff --check -- devscripts/game_build_worker.py devscripts/quality/test_game_build_cache_marker_fast_path.py docs/specs/dev-loop/build-time-observability.md` — passed; only existing LF/CRLF warnings were emitted.
- No game build, runtime command, process restart, or CPU capture was started by this session.

# Risks and next proof

- Mixed batches now pay dependency-scan time before the first compiler starts; the measured scan was about 2.6 seconds, versus the 116.8-second separate compile wave it removes. Source-only edits retain the earlier preflight.
- Link time alone was 26.3 seconds in this baseline, so this optimization cannot meet the 10-second end-to-end target by itself. A coarse replaceable module path or faster link strategy remains necessary.
- The next proof must use one coordinated root Mode3 batch with 15–20 legitimate C++ edits, a live connected Juggernaut pair, exact compile/skip list, sidecar host/process CPU/RAM, source-to-activation elapsed time, and host/NPC module-generation equality. Compare total and per-phase timings to this baseline; do not claim the intended speedup before that build.
- CPU and header-fanout agents received the gen635 evidence and the proposed separation of compile-wave and header-seam experiments. No conflicting edits were made.
