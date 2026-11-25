# Fast build failure boundary review

- Date: 2026-11-25 02:03 UTC
- Agent session: `session-6426e997a387`
- Work session: `work-1eb676f1a1c5`
- Issue: `issue-136d4553ff92`
- Result: partial; existing compiler-diagnostic abort is verified, but no safe general pre-link failure check is established.

## Request

> can u make it so u fail before 19 min ? like how to make it fail super quick dont wait to fial

## Findings

- `devscripts/game_build_worker.py` already streams tracked GCC/MinGW output and terminates the compiler process tree on a hard error diagnostic. The existing changed-source preflight compiles directly changed `.cpp` files before the broader object-cache scan. Header-dependent stale units are prioritized before unrelated stale units.
- Focused validation reran `test_game_build_fast_diagnostic_abort` and `test_game_build_failfast_priority`: 4/4 passed in 0.547 seconds. The synthetic compiler emits an error then sleeps for 20 seconds; the tracked worker returns without waiting. This proves the worker-level compile-diagnostic path, not a real failed DLL attempt.
- The gen627 cold link failure (`game-dll-20261125T011406552810Z-170580`) took 273.931 seconds, including 248.211 seconds compiling 198/644 units, then LLD failed in 3.464 seconds on three undefined symbols. A warm repeat failed in 9.004 seconds. A separate codegen rejection failed in 2.538 seconds.
- In the gen627 attempt, the cache scan marked `src/engine/engine-tick-net.cpp` skipped, while LLD later reported `lastAiInputFrame`, `engineRenderGhost`, and `engineTickNet` undefined. The source tree contains definitions for these functions. This narrows the next investigation to the exact gen627 object, its build flags/source snapshot, and link response file; it does not yet prove an object-cache defect.

## Decision and next check

Compile errors can stop as soon as the compiler reports them. Cross-translation-unit link errors are only authoritative after the linker sees the complete current object set. An arbitrary shorter timeout or a speculative partial link could reject valid builds while required dependents are still compiling, so neither was added.

Next, inspect the exact gen627 `engine-tick-net.cpp` object and link response file against the attempt's source snapshot and compiler flags. Only add a pre-link symbol check if it can prove a missing provider without false failures when pending dependents or libraries supply it. In parallel, reduce measured header fan-out and avoid recompiling unrelated units; do not skip required dependency work.

## Scope and evidence limits

- No compiler, full game build, or runtime process control was used in this review.
- The current root dev-loop reported no active or pending build. The isolated Juggernaut run and the separate Bloodtest run were left untouched.
- The persistent edit-to-live goal remains open. Fast failure shortens rejected attempts; it does not establish successful edit-to-live latency.
