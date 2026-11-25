# Fast build failure review and resident patch rollback

## Request

The user asked: “can u make it so u fail before 19 min ? like how to make it fail super quick dont wait to fial”. The user context is linked to the existing DLL build-stage issue `issue-136d4553ff92`.

## Findings

- The shared `devscripts/game_build_worker.py` working-tree change streams GCC/MinGW output and kills the failing compiler process tree on its first location-prefixed hard error. Focused tests passed **4/4** (`test_game_build_fast_diagnostic_abort` and `test_game_build_failfast_priority`); `py_compile` and scoped `git diff --check` passed. The synthetic error-then-sleep check is not a real compiler failure.
- A cold-cache root DLL attempt, `game-dll-20261125T011406552810Z-170580`, failed after **273.931s** at link. It compiled **198** of 644 translation units and skipped **447**. Changed-source preflight took **13.966s**, object scan **3.668s**, compile **248.211s**, and LLD **3.464s**. The linker reported undefined `lastAiInputFrame()`, `engineRenderGhost(Player const&, Camera const&)`, and `engineTickNet(Engine&, float)`. The cached-object scan recorded 32 dependency invalidations, including 24 from `src/entities/player.h`, 4 from `src/physics/physical-entity.h`, 3 from `src/vehicles/vehicle-types.h`, and 1 from `src/gamemode/gamemode.h`. This is direct fanout evidence; it does not establish that machine pressure caused the compile duration.
- A repeated root DLL attempt, `game-dll-20261125T012133976103Z-b7c31e`, failed on the same undefined symbols in **9.004s**: one TU compiled, 644 skipped, object scan **2.766s**, compilation **0.408s**, and LLD **3.293s**. This demonstrates a sub-10-second cached *repeat failure*, not a successful activation.
- The next root attempt, `game-dll-20261125T012320949649Z-bb7709`, stopped in **2.538s** before compile/link with `codegen check failed: packet_registry_codegen.py`. This is a quick codegen rejection, not proof of a successful DLL update.
- The 273.931-second attempt's resource sidecar recorded mean host CPU **97.546%** and minimum available RAM **22,200,320 bytes**. Treat this as build-overlapped pressure evidence, not proof of causality.

## Failure handling

The fast compiler handler can stop work only after GCC emits a recognized hard diagnostic. It cannot detect a cross-translation-unit undefined symbol before the linker checks the final object set. In the observed linker failures, LLD itself took about 3.3–3.5 seconds; the expensive interval was compiling dependents first. The practical reduction is correct cache reuse and smaller header fanout, plus a sound early symbol consistency check if one can avoid false failures from stale dependent objects. Do not skip required dependents or add a redundant syntax-only compile that merely duplicates parsing work.

The historical camera compile failure was already **95.572s total**, including **77.702s** in changed-source preflight; its records do not capture the first diagnostic timestamp. The existing watchdog was not lowered because valid translation units have taken over 200 seconds under severe pressure. The real diagnostic-to-process-exit latency remains unmeasured.

## Source correction

My unfinished resident ModuleHost refresh patch introduced calls to methods that were not declared, and root EXE attempt `exe-build-20261124T201330-60d8894c` failed after **35.525s** with those compiler errors. I reverted my changes in `src/host/host-runtime.cpp` and `src/hot-reload/module-host.cpp` and released their coordination leases. This compile failure is separate from the game-DLL fail-fast work. The subsequent root cold build completed; no build was requested by this changelog task.

## Status

Root dev-loop currently has a pending game batch and no active build. No DLL generation activation or runtime behavior was proven by these failures. The overall edit-to-live target remains open. All attempt details and this review were appended to `issue-136d4553ff92`.
