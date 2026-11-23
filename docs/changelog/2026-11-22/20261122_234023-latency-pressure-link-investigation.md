# Edit-to-live latency pressure investigation

**Date:** 2026-11-22 UTC  
**Issue:** `issue-136d4553ff92`  
**Run:** `20261122_231306`  
**Build examined:** `game-dll-20261122T231534497789Z-06de62`, generation 545

## Findings

- The Gen545 build took 40.312 s end to end, but it is not a controlled one-file canary: another agent changed `src/network/server-npcs.cpp`. It compiled that source in 22.540 s, compiled the PCH-off `src/npc/npc-world-query.cpp` cache entry in 1.060 s, compiled the generated identity provider in 0.679 s, skipped 640 other objects, linked 644 inputs totaling 483,617,225 bytes in 7.949 s, and published in 0.456 s. The DLL is 226,407,936 bytes. The per-TU result does not prove an end-to-end edit latency.
- The root dev-loop still ran the Juggernaut Horde pair: server PID 29536, client PID 17508, generation 545, 67 NPCs. Its journal continued to report DLL packet handling and NPC phase windows with zero hotness failures/violations.
- An 8-second process sample showed the server consuming 18.83 CPU-seconds (~235% of one logical core) and the client 16.20 CPU-seconds (~203%). Host CPU was reported at 100% across 12 logical processors, with only 0.45 GB of 15.65 GB memory available. This is pressure evidence, not a matched build interval. No additional process was started while memory reserve was below 0.5 GB.
- The current NPC modules are folded into the game DLL. `ModuleHost::startup` loads exports from the active game DLL; `loadLocalModules` sets local-module mode; and `ModuleHost::poll` returns immediately in that mode. The standalone module catalog `config/modules/modules.json` is absent and was removed in commit `7efe9c17`. Therefore the current NPC module path still pays for the monolithic link.
- The current root status later reported a failed `packet_registry_codegen.py` pre-build check. That concurrent generated-ledger failure produced no new candidate and is tracked in the central latency issue; it was not changed in this investigation.

## Next measurable path

The PCH-off leaf compile is near one second, but the cache scan, monolithic LLD link, and pre-worker work leave insufficient budget for <=10 s under pressure. The next candidate is a single coarse behavior module whose source ownership, build request, cache, ABI/state identity, validation, publication, activation, rollback, and timings are all owned by the root dev-loop. Keep the last-good MinGW game DLL as fallback. Prove the path with the same live Juggernaut process, a changed NPC behavior event, and matched save-to-activation timings before increasing to more game processes.

LLD does not currently support incremental linking. An MSVC `/INCREMENTAL` experiment may be useful with MSVC-built objects and stable link inputs, but it cannot reuse the existing MinGW object cache and must remain a separate candidate path. Microsoft documents that LINK falls back to a full link when an object is added or omitted or link settings invalidate the incremental database.

## Work recorded

- Added the pressure/process sample and measured link bottleneck to `issue-136d4553ff92`.
- Added CPU/resource evidence to `issue-ed3ac775283d` for the CPU owner to use.
- No C++ source, active compiler, main build path, process, or game architecture was changed in this investigation. No build or test was started.

## References

- [LLVM LLD Windows support](https://lld.llvm.org/windows_support.html)
- [Microsoft LINK `/INCREMENTAL`](https://learn.microsoft.com/en-us/cpp/build/reference/incremental-link-incrementally?view=msvc-170)
