# Module rebind finding and Mode 3 latency status

- Session ID: work-ea99e632058f

## Work completed

- Rechecked root dev-loop and shared runtime state. No root build or pending batch was active. The root server PID 10164 and daemon PID 30924 were alive; isolated Juggernaut server/client PIDs 22184/32832 were also alive. The root status does not list that isolated client pair.
- Reconciled the latest measured build evidence: attempt `game-dll-20261123T200924195125Z-7401c3` took 829.795 seconds; 754.586 seconds were in compile, 12.657 seconds in link, 46.412 seconds in changed-source preflight, 2.052 seconds in object-cache scan, 0.601 seconds in coherence validation, and 1.009 seconds in publish. These listed stages leave approximately 12.478 seconds unreconciled. The attempt compiled 271 and skipped 372 of 643 recorded units; the source/link input count elsewhere says 642, so one unit remains unexplained.
- Reviewed why unchanged translation units recompiled: `src/config.h` changed, invalidating 263 dependents in the prior attempt. The new `src/debug/config-npc.h` is consumed by six C++ translation units. The split is useful for future NPC-debug-only edits, but its first application still changes `src/config.h` and forces the current broad rebuild; it does not reduce whole-game link time.
- Traced the local NPC module lifecycle. `ModuleHost::loadLocalModules()` resolves exports from the active game HMODULE once, stores API pointers and module generation, and marks `mLocalModules`. `ModuleHost::poll()` returns early for local modules. The resident HostRuntime generation-swap path quiesces and commits a new game module without a ModuleHost rebind call. The NPC module's generated identity provider for game generation 571 reports `571u`, while the matched run's server journals reported host DLL generation571 and NPC module generation570. This is strong source and runtime evidence of a stale local NPC function-table pointer after DLL activation, with the caveat that module versions are not universally required to equal game generations.
- Appended this trace, impact, caveat, and a safe rebind-prototype outline to `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md`.
- Asked the resource-profiler to review the rebind call path and asked the bodycam source owner to declare its compiler-input set ready or not ready. No response/readiness was observed in this session.

## Build and runtime evidence

- No source C++ compiler input was changed by this session. No build, daemon restart, game restart, or runtime capture was requested.
- The native-provider no-build baseline `.dev/build-timings/mode3-prebuild-native-20261123T2102Z.resource.jsonl` covered 15.078 seconds on 12 logical CPUs. Host CPU averaged 99.734%; available RAM averaged 3262.5 MiB and bottomed at 3210.1 MiB. The live pair used 48.29% of whole-host CPU. This shows substantial host load before a build, not that host load caused the 829.795-second build.
- The current DLL build path remains far above the <=10-second target, and the local NPC module staleness means existing host-DLL activation evidence does not prove NPC behavior activation.

## Next work

1. Wait for explicit compiler-input owner readiness and retained build-note IDs before sealing the next Mode 3 batch.
2. On the next actual root build, verify the per-process resource provider records compiler/linker and game PIDs through the full attempt, and compare it with a matched live workload baseline.
3. Design a resident-side, cold-build ModuleHost rebind protocol with safe tick-boundary quiescence, state export/import, rollback, and unload lifetime checks. Prove one same-PID NPC function edit reaches both client and server after the one-time EXE update.
4. After activation proof is correct, tackle the current compile/link critical path. The one-time config-header split can reduce future debug-flag fan-out; it cannot by itself meet the multi-file <=10-second target or eliminate monolithic linking.

## Changed files

- `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md`: appended a source-level local-module generation trace and a rebind prototype outline. There was no earlier section for this exact evidence; the new text is under `2026-11-23T21:29Z — source trace for stale local NPC module generation`.
- `docs/changelog/2026-11-23/20261123_2130-module-rebind-root-cause-and-mode3-status.md`: new session record; there was no prior file.
